#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifdef J2ME_NX_NO_TLS

bool http_supported(void) {
    return false;
}

int http_get(const char *url, const HttpOptions *opt, char *err, size_t err_size) {
    (void)url;
    (void)opt;
    snprintf(err, err_size, "HTTPS not supported (no mbedTLS)");
    return -1;
}

#else

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <SDL.h>

#include <mbedtls/ssl.h>
#include <mbedtls/version.h>
#if MBEDTLS_VERSION_MAJOR >= 3
#include <psa/crypto.h>
#endif
#if MBEDTLS_VERSION_MAJOR < 4
#include <mbedtls/ctr_drbg.h>
#endif
#ifdef __SWITCH__
#include <switch.h>
#endif

#ifdef MSG_NOSIGNAL
#define SEND_FLAGS MSG_NOSIGNAL
#else
#define SEND_FLAGS 0
#endif
#ifndef MBEDTLS_ERR_NET_SEND_FAILED
#define MBEDTLS_ERR_NET_SEND_FAILED -0x004E
#endif
#ifndef MBEDTLS_ERR_NET_RECV_FAILED
#define MBEDTLS_ERR_NET_RECV_FAILED -0x004C
#endif

#define IDLE_TIMEOUT_MS 20000   // không nhận được gì trong ngần này thì bỏ
#define MAX_REDIRECTS   5
#define HEADER_MAX      16384

bool http_supported(void) {
    return true;
}

static SDL_SpinLock init_lock;
static bool ready;
static mbedtls_ssl_config conf;
#if MBEDTLS_VERSION_MAJOR < 4
static mbedtls_ctr_drbg_context drbg;

static int entropy(void *ctx, unsigned char *out, size_t len) {
    (void)ctx;
#ifdef __SWITCH__
    randomGet(out, len);
#else
    arc4random_buf(out, len);
#endif
    return 0;
}
#endif

static bool tls_init(void) {
    SDL_AtomicLock(&init_lock);
    if (!ready) {
        bool ok = true;
#if MBEDTLS_VERSION_MAJOR >= 3
        ok = psa_crypto_init() == PSA_SUCCESS;
#endif
        mbedtls_ssl_config_init(&conf);
        ok = ok && mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                               MBEDTLS_SSL_PRESET_DEFAULT) == 0;
        if (ok) {
            mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
#if MBEDTLS_VERSION_MAJOR < 4
            mbedtls_ctr_drbg_init(&drbg);
            const char *pers = "j2me-nxx-update";
            ok = mbedtls_ctr_drbg_seed(&drbg, entropy, NULL, (const unsigned char *)pers, strlen(pers)) == 0;
            if (ok)
                mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &drbg);
#endif
        }
        ready = ok;
    }
    SDL_AtomicUnlock(&init_lock);
    return ready;
}

typedef struct {
    int fd;
    mbedtls_ssl_context ssl;
    bool tls;
    volatile bool *cancel;
    char *err;
    size_t err_size;
    // Bộ đệm đọc
    uint8_t buf[16384];
    size_t pos, len;
} Conn;

static int bio_send(void *ctx, const unsigned char *buf, size_t len) {
    int fd = (int)(intptr_t)ctx;
    ssize_t n = send(fd, buf, len, SEND_FLAGS);
    if (n >= 0)
        return (int)n;
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    return MBEDTLS_ERR_NET_SEND_FAILED;
}

static int bio_recv(void *ctx, unsigned char *buf, size_t len) {
    int fd = (int)(intptr_t)ctx;
    ssize_t n = recv(fd, buf, len, 0);
    if (n >= 0)
        return (int)n;
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        return MBEDTLS_ERR_SSL_WANT_READ;
    return MBEDTLS_ERR_NET_RECV_FAILED;
}

static bool cancelled(Conn *c) {
    if (c->cancel && *c->cancel) {
        snprintf(c->err, c->err_size, "cancelled");
        return true;
    }
    return false;
}

// Chờ socket sẵn sàng; false nếu bị huỷ hoặc quá thời gian
static bool wait_fd(Conn *c, short events, Uint32 *idle_since) {
    struct pollfd p = { .fd = c->fd, .events = events };
    int rc = poll(&p, 1, 250);
    if (cancelled(c))
        return false;
    if (rc > 0) {
        *idle_since = SDL_GetTicks();
        return true;
    }
    if (rc < 0 && errno != EINTR) {
        snprintf(c->err, c->err_size, "poll failed (%d)", errno);
        return false;
    }
    if (SDL_GetTicks() - *idle_since > IDLE_TIMEOUT_MS) {
        snprintf(c->err, c->err_size, "timeout");
        return false;
    }
    return true;
}

static bool conn_open(Conn *c, const char *host, int port, bool tls) {
    char port_s[16];
    snprintf(port_s, sizeof(port_s), "%d", port);
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port_s, &hints, &res) != 0 || !res) {
        snprintf(c->err, c->err_size, "cannot resolve %s (no internet?)", host);
        return false;
    }
    c->fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (c->fd < 0) {
        freeaddrinfo(res);
        snprintf(c->err, c->err_size, "socket failed");
        return false;
    }
    fcntl(c->fd, F_SETFL, fcntl(c->fd, F_GETFL, 0) | O_NONBLOCK);
    int one = 1;
    setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#ifdef SO_NOSIGPIPE
    setsockopt(c->fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    int rc = connect(c->fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    Uint32 idle = SDL_GetTicks();
    if (rc < 0) {
        if (errno != EINPROGRESS && errno != EWOULDBLOCK && errno != EAGAIN) {
            snprintf(c->err, c->err_size, "cannot connect to %s", host);
            return false;
        }
        for (;;) {
            struct pollfd p = { .fd = c->fd, .events = POLLOUT };
            int r = poll(&p, 1, 250);
            if (cancelled(c))
                return false;
            if (r > 0) {
                int e = 0;
                socklen_t len = sizeof(e);
                if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, &e, &len) < 0 || e != 0) {
                    snprintf(c->err, c->err_size, "cannot connect to %s", host);
                    return false;
                }
                break;
            }
            if (SDL_GetTicks() - idle > IDLE_TIMEOUT_MS) {
                snprintf(c->err, c->err_size, "connect timeout");
                return false;
            }
        }
    }
    c->tls = tls;
    if (!tls)
        return true;
    if (!tls_init()) {
        snprintf(c->err, c->err_size, "TLS init failed");
        return false;
    }
    mbedtls_ssl_init(&c->ssl);
    if (mbedtls_ssl_setup(&c->ssl, &conf) != 0 || mbedtls_ssl_set_hostname(&c->ssl, host) != 0) {
        snprintf(c->err, c->err_size, "TLS setup failed");
        return false;
    }
    mbedtls_ssl_set_bio(&c->ssl, (void *)(intptr_t)c->fd, bio_send, bio_recv, NULL);
    idle = SDL_GetTicks();
    for (;;) {
        rc = mbedtls_ssl_handshake(&c->ssl);
        if (rc == 0)
            return true;
        if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) {
            if (!wait_fd(c, rc == MBEDTLS_ERR_SSL_WANT_READ ? POLLIN : POLLOUT, &idle))
                return false;
            continue;
        }
        snprintf(c->err, c->err_size, "TLS handshake failed (-0x%04x)", (unsigned)-rc);
        return false;
    }
}

static void conn_close(Conn *c) {
    if (c->tls)
        mbedtls_ssl_free(&c->ssl);
    if (c->fd >= 0)
        close(c->fd);
    c->fd = -1;
    c->tls = false;
}

static bool conn_write(Conn *c, const char *data, size_t len) {
    Uint32 idle = SDL_GetTicks();
    while (len > 0) {
        int n;
        if (c->tls) {
            n = mbedtls_ssl_write(&c->ssl, (const unsigned char *)data, len);
            if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) {
                if (!wait_fd(c, n == MBEDTLS_ERR_SSL_WANT_READ ? POLLIN : POLLOUT, &idle))
                    return false;
                continue;
            }
        } else {
            n = bio_send((void *)(intptr_t)c->fd, (const unsigned char *)data, len);
            if (n == MBEDTLS_ERR_SSL_WANT_WRITE) {
                if (!wait_fd(c, POLLOUT, &idle))
                    return false;
                continue;
            }
        }
        if (n <= 0) {
            snprintf(c->err, c->err_size, "send failed");
            return false;
        }
        data += n;
        len -= (size_t)n;
    }
    return true;
}

// Đọc thêm vào bộ đệm. Trả về số byte mới, 0 khi hết kết nối, -1 khi lỗi
static int conn_fill(Conn *c) {
    if (c->pos > 0 && c->pos < c->len) {
        memmove(c->buf, c->buf + c->pos, c->len - c->pos);
        c->len -= c->pos;
        c->pos = 0;
    } else if (c->pos >= c->len) {
        c->pos = c->len = 0;
    }
    if (c->len >= sizeof(c->buf))
        return -1;
    Uint32 idle = SDL_GetTicks();
    for (;;) {
        int n;
        if (c->tls)
            n = mbedtls_ssl_read(&c->ssl, c->buf + c->len, sizeof(c->buf) - c->len);
        else
            n = bio_recv((void *)(intptr_t)c->fd, c->buf + c->len, sizeof(c->buf) - c->len);
        if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) {
            if (!wait_fd(c, n == MBEDTLS_ERR_SSL_WANT_READ ? POLLIN : POLLOUT, &idle))
                return -1;
            continue;
        }
#ifdef MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET
        if (n == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
            continue;   // TLS 1.3: server gửi vé phiên, đọc tiếp
#endif
        if (n == 0 || n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
            return 0;
        if (n < 0) {
            snprintf(c->err, c->err_size, "receive failed (-0x%04x)", (unsigned)-n);
            return -1;
        }
        c->len += (size_t)n;
        return n;
    }
}

// Đọc 1 dòng (bỏ \r\n). false khi lỗi / hết dữ liệu
static bool read_line(Conn *c, char *out, size_t size) {
    size_t n = 0;
    for (;;) {
        while (c->pos < c->len) {
            char ch = (char)c->buf[c->pos++];
            if (ch == '\n') {
                if (n > 0 && out[n - 1] == '\r')
                    n--;
                out[n] = 0;
                return true;
            }
            if (n + 1 < size)
                out[n++] = ch;
        }
        if (conn_fill(c) <= 0)
            return false;
    }
}

// Chuyển tối đa want byte (want < 0: tới hết kết nối) cho sink
static bool pump(Conn *c, int64_t want, const HttpOptions *opt) {
    for (;;) {
        if (c->pos < c->len) {
            size_t n = c->len - c->pos;
            if (want >= 0 && (int64_t)n > want)
                n = (size_t)want;
            if (opt->sink && !opt->sink(opt->ctx, c->buf + c->pos, n)) {
                snprintf(c->err, c->err_size, "cancelled");
                return false;
            }
            c->pos += n;
            if (want >= 0) {
                want -= (int64_t)n;
                if (want == 0)
                    return true;
            }
        }
        if (cancelled(c))
            return false;
        int r = conn_fill(c);
        if (r < 0)
            return false;
        if (r == 0) {
            if (want > 0) {
                snprintf(c->err, c->err_size, "connection closed early");
                return false;
            }
            return true;
        }
    }
}

static bool parse_url(const char *url, bool *tls, char *host, size_t host_size, int *port, const char **path) {
    const char *p;
    if (strncmp(url, "https://", 8) == 0) {
        *tls = true;
        *port = 443;
        p = url + 8;
    } else if (strncmp(url, "http://", 7) == 0) {
        *tls = false;
        *port = 80;
        p = url + 7;
    } else {
        return false;
    }
    const char *end = p + strcspn(p, ":/?");
    size_t hl = (size_t)(end - p);
    if (hl == 0 || hl >= host_size)
        return false;
    memcpy(host, p, hl);
    host[hl] = 0;
    if (*end == ':') {
        *port = atoi(end + 1);
        end += strcspn(end, "/?");
    }
    *path = *end ? end : "/";
    return true;
}

int http_get(const char *url_in, const HttpOptions *opt, char *err, size_t err_size) {
    char url[2048];
    snprintf(url, sizeof(url), "%s", url_in);
    err[0] = 0;
    Conn *c = calloc(1, sizeof(Conn));
    if (!c) {
        snprintf(err, err_size, "out of memory");
        return -1;
    }
    c->cancel = opt->cancel;
    c->err = err;
    c->err_size = err_size;
    int status = -1;

    for (int hop = 0; hop <= MAX_REDIRECTS; hop++) {
        bool tls;
        char host[256];
        int port;
        const char *path;
        if (!parse_url(url, &tls, host, sizeof(host), &port, &path)) {
            snprintf(err, err_size, "bad URL");
            break;
        }
        c->fd = -1;
        c->pos = c->len = 0;
        if (!conn_open(c, host, port, tls)) {
            conn_close(c);
            break;
        }
        char req[3072];
        int rl = snprintf(req, sizeof(req),
                          "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: J2ME-NXX/" APP_VERSION_STR "\r\n"
                          "Accept: %s\r\nConnection: close\r\n\r\n",
                          path, host, opt->accept ? opt->accept : "*/*");
        if (rl <= 0 || rl >= (int)sizeof(req) || !conn_write(c, req, (size_t)rl)) {
            if (!err[0])
                snprintf(err, err_size, "request failed");
            conn_close(c);
            break;
        }

        // Dòng trạng thái + header
        char line[HEADER_MAX];
        if (!read_line(c, line, sizeof(line)) || sscanf(line, "HTTP/%*s %d", &status) != 1) {
            if (!err[0])
                snprintf(err, err_size, "bad HTTP response");
            status = -1;
            conn_close(c);
            break;
        }
        int64_t length = -1;
        bool chunked = false;
        char location[2048] = "";
        for (;;) {
            if (!read_line(c, line, sizeof(line))) {
                if (!err[0])
                    snprintf(err, err_size, "bad HTTP header");
                status = -1;
                break;
            }
            if (!line[0])
                break;
            if (strncasecmp(line, "Content-Length:", 15) == 0)
                length = strtoll(line + 15, NULL, 10);
            else if (strncasecmp(line, "Transfer-Encoding:", 18) == 0 && strstr(line + 18, "chunked"))
                chunked = true;
            else if (strncasecmp(line, "Location:", 9) == 0)
                snprintf(location, sizeof(location), "%s", line + 9 + strspn(line + 9, " \t"));
        }
        if (status < 0) {
            conn_close(c);
            break;
        }
        if (status >= 300 && status < 400 && location[0]) {
            conn_close(c);
            if (location[0] == '/') {
                // Đường dẫn tương đối: giữ nguyên scheme + host
                char base[512];
                snprintf(base, sizeof(base), "%s://%s", tls ? "https" : "http", host);
                if (port != (tls ? 443 : 80))
                    snprintf(base + strlen(base), sizeof(base) - strlen(base), ":%d", port);
                char tmp[2048];
                snprintf(tmp, sizeof(tmp), "%s%s", base, location);
                snprintf(url, sizeof(url), "%s", tmp);
            } else {
                snprintf(url, sizeof(url), "%s", location);
            }
            status = -1;
            continue;
        }

        if (opt->on_length)
            opt->on_length(opt->ctx, chunked ? -1 : length);
        bool ok;
        if (chunked) {
            ok = true;
            for (;;) {
                if (!read_line(c, line, sizeof(line))) {
                    ok = false;
                    break;
                }
                long size = strtol(line, NULL, 16);
                if (size <= 0)
                    break;
                if (!pump(c, size, opt) || !read_line(c, line, sizeof(line))) {
                    ok = false;
                    break;
                }
            }
        } else {
            ok = pump(c, length, opt);
        }
        conn_close(c);
        if (!ok) {
            if (!err[0])
                snprintf(err, err_size, "download failed");
            status = -1;
        }
        break;
    }
    if (status < 0 && !err[0])
        snprintf(err, err_size, "too many redirects");
    free(c);
    return status;
}

#endif
