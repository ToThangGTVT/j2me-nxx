#include "websrv.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <SDL.h>

#include "lang.h"
#include "platform.h"

#ifdef MSG_NOSIGNAL
#define SEND_FLAGS MSG_NOSIGNAL
#else
#define SEND_FLAGS 0
#endif

#define PORT_FIRST  8080
#define PORT_TRIES  10
#define IO_TIMEOUT_MS 15000     // điện thoại im lặng lâu hơn thì bỏ kết nối
#define HEADER_MAX  8192
#define CHUNK       (64 * 1024)

static int listen_fd = -1;
static SDL_Thread *thread;
static SDL_atomic_t stop_flag;
static WebHandler handler;

bool web_stopping(void) {
    return SDL_AtomicGet(&stop_flag) != 0;
}

bool web_send_all(int fd, const void *data, size_t len) {
    const char *buf = data;
    while (len > 0) {
        struct pollfd p = { fd, POLLOUT, 0 };
        if (web_stopping() || poll(&p, 1, IO_TIMEOUT_MS) <= 0)
            return false;
        ssize_t n = send(fd, buf, len, SEND_FLAGS);
        if (n <= 0)
            return false;
        buf += n;
        len -= (size_t)n;
    }
    return true;
}

int web_recv_some(int fd, char *buf, int len) {
    for (int waited = 0; waited < IO_TIMEOUT_MS && !web_stopping(); waited += 200) {
        struct pollfd p = { fd, POLLIN, 0 };
        int r = poll(&p, 1, 200);
        if (r < 0)
            return -1;
        if (r > 0) {
            ssize_t n = recv(fd, buf, (size_t)len, 0);
            return n < 0 ? -1 : (int)n;
        }
    }
    return -1;
}

void web_drain(int fd, int64_t left) {
    char buf[4096];
    while (left > 0) {
        int n = web_recv_some(fd, buf, left < (int64_t)sizeof(buf) ? (int)left : (int)sizeof(buf));
        if (n <= 0)
            return;
        left -= n;
    }
}

bool web_send_head(int fd, int code, const char *type, int64_t len, const char *extra) {
    const char *reason = code == 200 ? "OK" : code == 404 ? "Not Found" : code == 411 ? "Length Required"
                       : code == 400 ? "Bad Request" : "Internal Server Error";
    char head[1024];
    int n = snprintf(head, sizeof(head),
                     "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %lld\r\n%s"
                     "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
                     code, reason, type, (long long)len, extra ? extra : "");
    return n > 0 && n < (int)sizeof(head) && web_send_all(fd, head, (size_t)n);
}

void web_send_response(int fd, int code, const char *type, const char *body, size_t len) {
    if (web_send_head(fd, code, type, (int64_t)len, NULL))
        web_send_all(fd, body, len);
}

void web_json_str(char *out, size_t size, const char *s) {
    size_t o = 0;
    if (o + 1 < size)
        out[o++] = '"';
    for (; *s && o + 7 < size; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            out[o++] = '\\';
            out[o++] = (char)c;
        } else if (c < 0x20) {
            o += (size_t)snprintf(out + o, size - o, "\\u%04x", c);
        } else {
            out[o++] = (char)c;
        }
    }
    if (o + 1 < size)
        out[o++] = '"';
    out[o] = '\0';
}

void web_send_json(int fd, int code, bool ok, const char *error) {
    char err[256], body[320];
    web_json_str(err, sizeof(err), error ? error : "");
    int n = snprintf(body, sizeof(body), "{\"ok\":%s,\"error\":%s}", ok ? "true" : "false", err);
    web_send_response(fd, code, "application/json", body, (size_t)n);
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

bool web_query(const char *path, const char *key, char *out, size_t size) {
    const char *q = strchr(path, '?');
    size_t klen = strlen(key);
    while (q) {
        q++;
        if (strncmp(q, key, klen) == 0 && q[klen] == '=') {
            const char *s = q + klen + 1;
            size_t len = strcspn(s, "&"), o = 0;
            for (size_t i = 0; i < len && o + 1 < size; i++) {
                if (s[i] == '%' && i + 2 < len && hex_val(s[i + 1]) >= 0 && hex_val(s[i + 2]) >= 0) {
                    out[o++] = (char)(hex_val(s[i + 1]) * 16 + hex_val(s[i + 2]));
                    i += 2;
                } else {
                    out[o++] = s[i] == '+' ? ' ' : s[i];
                }
            }
            out[o] = '\0';
            return true;
        }
        q = strchr(q, '&');
    }
    return false;
}

WebRecvResult web_recv_to_file(WebRequest *req, FILE *f, void (*progress)(int64_t done, void *arg), void *arg) {
    static char buf[CHUNK];
    int64_t length = req->length, got = 0;
    if (req->body_len > 0) {
        int n = req->body_len < length ? req->body_len : (int)length;
        if (fwrite(req->body, 1, (size_t)n, f) != (size_t)n)
            return WEB_RECV_WRITE;
        got = n;
    }
    while (got < length) {
        int want = length - got < CHUNK ? (int)(length - got) : CHUNK;
        int n = web_recv_some(req->fd, buf, want);
        if (n <= 0)
            return WEB_RECV_NET;
        if (fwrite(buf, 1, (size_t)n, f) != (size_t)n) {
            web_drain(req->fd, length - got - n);
            return WEB_RECV_WRITE;
        }
        got += n;
        if (progress)
            progress(got, arg);
    }
    return WEB_RECV_OK;
}

static void handle_client(int fd) {
    WebRequest req;
    char buf[HEADER_MAX + 1];
    int len = 0;
    char *end = NULL;
    while (!end) {
        if (len >= HEADER_MAX)
            return;
        int n = web_recv_some(fd, buf + len, HEADER_MAX - len);
        if (n <= 0)
            return;
        len += n;
        buf[len] = '\0';
        end = strstr(buf, "\r\n\r\n");
    }
    *end = '\0';
    req.fd = fd;
    req.body = end + 4;
    req.body_len = len - (int)(req.body - buf);
    if (sscanf(buf, "%7s %1023s", req.method, req.path) != 2)
        return;
    req.length = -1;
    for (char *line = strstr(buf, "\r\n"); line; line = strstr(line + 2, "\r\n")) {
        if (strncasecmp(line + 2, "Content-Length:", 15) == 0)
            req.length = strtoll(line + 17, NULL, 10);
    }
    handler(&req);
}

static int server_thread(void *arg) {
    (void)arg;
    while (!web_stopping()) {
        struct pollfd p = { listen_fd, POLLIN, 0 };
        if (poll(&p, 1, 200) <= 0)
            continue;
        int c = accept(listen_fd, NULL, NULL);
        if (c < 0)
            continue;
#ifdef SO_NOSIGPIPE
        int one = 1;
        setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
        handle_client(c);
        shutdown(c, SHUT_RDWR);
        close(c);
    }
    return 0;
}

bool web_start(WebHandler h, char *url, size_t url_size, char *err, size_t err_size) {
    web_stop();
    char ip[64];
    if (!platform_local_ip(ip, sizeof(ip))) {
        snprintf(err, err_size, "%s", tr(S_UPLOAD_NO_NET));
        return false;
    }

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        snprintf(err, err_size, tr(S_UPLOAD_SERVER_FAILED), strerror(errno));
        return false;
    }
    int one = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    int port = 0;
    for (int i = 0; i < PORT_TRIES && !port; i++) {
        struct sockaddr_in a;
        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        a.sin_port = htons((uint16_t)(PORT_FIRST + i));
        if (bind(listen_fd, (struct sockaddr *)&a, sizeof(a)) == 0)
            port = PORT_FIRST + i;
    }
    if (!port || listen(listen_fd, 4) != 0) {
        snprintf(err, err_size, tr(S_UPLOAD_SERVER_FAILED), strerror(errno));
        close(listen_fd);
        listen_fd = -1;
        return false;
    }

    handler = h;
    SDL_AtomicSet(&stop_flag, 0);
    thread = SDL_CreateThread(server_thread, "websrv", NULL);
    if (!thread) {
        snprintf(err, err_size, tr(S_UPLOAD_SERVER_FAILED), SDL_GetError());
        close(listen_fd);
        listen_fd = -1;
        return false;
    }
    snprintf(url, url_size, "http://%s:%d/", ip, port);
    return true;
}

void web_stop(void) {
    if (thread) {
        SDL_AtomicSet(&stop_flag, 1);
        SDL_WaitThread(thread, NULL);
        thread = NULL;
    }
    if (listen_fd >= 0) {
        close(listen_fd);
        listen_fd = -1;
    }
}
