// TLS cho https:// và ssl:// bằng mbedTLS (2.28 trên Switch, 3.x/4.x trên desktop), chế độ không chặn.
// Không kiểm tra chứng chỉ: Switch không có sẵn kho CA cho homebrew, và game J2ME chỉ cần kết nối được.
#include "midp.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "../vm/vm.h"

#ifdef J2ME_NX_NO_TLS

// Build không có mbedTLS: mọi kết nối TLS báo lỗi rõ ràng
static NativeResult no_tls(VMThread *t, Value *args, Value *ret) {
    (void)args;
    (void)ret;
    throw_new(t, "java/io/IOException", "HTTPS/SSL chua ho tro: ban build nay thieu mbedTLS");
    return NATIVE_EXCEPTION;
}

void midp_tls_register(void) {
    const char *N = "j2menx/Net";
    native_register(N, "tlsNew0", "(ILjava/lang/String;)I", no_tls);
    native_register(N, "tlsHandshake0", "(I)I", no_tls);
    native_register(N, "tlsRead0", "(I[BII)I", no_tls);
    native_register(N, "tlsWrite0", "(I[BII)I", no_tls);
    native_register(N, "tlsAvailable0", "(I)I", no_tls);
    native_register(N, "tlsClose0", "(I)V", no_tls);
    native_register(N, "tlsCipher0", "(I)Ljava/lang/String;", no_tls);
    native_register(N, "tlsVersion0", "(I)Ljava/lang/String;", no_tls);
}

void midp_tls_shutdown(void) {
}

#else

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

#define MAX_TLS 16

typedef struct {
    bool used;
    int fd;
    mbedtls_ssl_context ssl;
} Tls;

static Tls conns[MAX_TLS + 1];     // chỉ số 0 không dùng
static bool ready;
static mbedtls_ssl_config conf;
#if MBEDTLS_VERSION_MAJOR < 4
static mbedtls_ctr_drbg_context drbg;
#endif

#if MBEDTLS_VERSION_MAJOR < 4
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
    if (ready)
        return true;
#if MBEDTLS_VERSION_MAJOR >= 3
    if (psa_crypto_init() != PSA_SUCCESS)
        return false;
#endif
    mbedtls_ssl_config_init(&conf);
    if (mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT) != 0)
        return false;
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
#if MBEDTLS_VERSION_MAJOR < 4
    mbedtls_ctr_drbg_init(&drbg);
    const char *pers = "j2me-nx";
    if (mbedtls_ctr_drbg_seed(&drbg, entropy, NULL, (const unsigned char *)pers, strlen(pers)) != 0)
        return false;
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &drbg);
#endif
    ready = true;
    return true;
}

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

static Tls *get(int h) {
    return h > 0 && h <= MAX_TLS && conns[h].used ? &conns[h] : NULL;
}

static NativeResult tls_error(VMThread *t, const char *what, int rc) {
    char msg[160];
    snprintf(msg, sizeof(msg), "TLS %s loi -0x%04x", what, (unsigned)-rc);
    throw_new(t, "java/io/IOException", msg);
    return NATIVE_EXCEPTION;
}

// tlsNew0(fd, host) -> handle
static NativeResult Net_tlsNew0(VMThread *t, Value *args, Value *ret) {
    if (!tls_init()) {
        throw_new(t, "java/io/IOException", "Khong khoi tao duoc TLS");
        return NATIVE_EXCEPTION;
    }
    int h = 0;
    for (int i = 1; i <= MAX_TLS; i++) {
        if (!conns[i].used) {
            h = i;
            break;
        }
    }
    if (!h) {
        throw_new(t, "java/io/IOException", "Qua nhieu ket noi TLS");
        return NATIVE_EXCEPTION;
    }
    Tls *c = &conns[h];
    memset(c, 0, sizeof(*c));
    mbedtls_ssl_init(&c->ssl);
    int rc = mbedtls_ssl_setup(&c->ssl, &conf);
    if (rc != 0) {
        mbedtls_ssl_free(&c->ssl);
        return tls_error(t, "setup", rc);
    }
    char host[256];
    jstring_to_cstr(args[1].l, host, sizeof(host));
    mbedtls_ssl_set_hostname(&c->ssl, host);
    c->fd = args[0].i;
    mbedtls_ssl_set_bio(&c->ssl, (void *)(intptr_t)c->fd, bio_send, bio_recv, NULL);
    c->used = true;
    ret->i = h;
    return NATIVE_OK;
}

// 1 = xong, 0 = đang bắt tay
static NativeResult Net_tlsHandshake0(VMThread *t, Value *args, Value *ret) {
    Tls *c = get(args[0].i);
    if (!c) {
        throw_new(t, "java/io/IOException", "TLS closed");
        return NATIVE_EXCEPTION;
    }
    int rc = mbedtls_ssl_handshake(&c->ssl);
    if (rc == 0) {
        ret->i = 1;
    } else if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) {
        ret->i = 0;
    } else {
        return tls_error(t, "handshake", rc);
    }
    return NATIVE_OK;
}

static bool check_range(VMThread *t, Object *b, jint off, jint len) {
    if (!b) {
        throw_null(t);
        return false;
    }
    if (off < 0 || len < 0 || off + len > ARRAY_LEN(b)) {
        throw_new(t, "java/lang/IndexOutOfBoundsException", NULL);
        return false;
    }
    return true;
}

// n > 0, 0 = chưa có dữ liệu, -1 = hết
static NativeResult Net_tlsRead0(VMThread *t, Value *args, Value *ret) {
    Tls *c = get(args[0].i);
    Object *b = args[1].l;
    if (!c) {
        throw_new(t, "java/io/IOException", "TLS closed");
        return NATIVE_EXCEPTION;
    }
    if (!check_range(t, b, args[2].i, args[3].i))
        return NATIVE_EXCEPTION;
    int rc = mbedtls_ssl_read(&c->ssl, ARRAY_DATA(b, uint8_t) + args[2].i, (size_t)args[3].i);
    if (rc > 0)
        ret->i = rc;
    else if (rc == 0 || rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
        ret->i = -1;
    else if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE)
        ret->i = 0;
#ifdef MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET
    else if (rc == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
        ret->i = 0;
#endif
    else
        return tls_error(t, "read", rc);
    return NATIVE_OK;
}

static NativeResult Net_tlsWrite0(VMThread *t, Value *args, Value *ret) {
    Tls *c = get(args[0].i);
    Object *b = args[1].l;
    if (!c) {
        throw_new(t, "java/io/IOException", "TLS closed");
        return NATIVE_EXCEPTION;
    }
    if (!check_range(t, b, args[2].i, args[3].i))
        return NATIVE_EXCEPTION;
    int rc = mbedtls_ssl_write(&c->ssl, ARRAY_DATA(b, uint8_t) + args[2].i, (size_t)args[3].i);
    if (rc >= 0)
        ret->i = rc;
    else if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE)
        ret->i = 0;
    else
        return tls_error(t, "write", rc);
    return NATIVE_OK;
}

static NativeResult Net_tlsAvailable0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    Tls *c = get(args[0].i);
    ret->i = c ? (jint)mbedtls_ssl_get_bytes_avail(&c->ssl) : 0;
    return NATIVE_OK;
}

static void tls_free(Tls *c) {
    mbedtls_ssl_close_notify(&c->ssl);
    mbedtls_ssl_free(&c->ssl);
    c->used = false;
}

// Chỉ giải phóng phiên TLS; socket đóng riêng bằng Net.close0
static NativeResult Net_tlsClose0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)ret;
    Tls *c = get(args[0].i);
    if (c)
        tls_free(c);
    return NATIVE_OK;
}

static NativeResult Net_tlsCipher0(VMThread *t, Value *args, Value *ret) {
    Tls *c = get(args[0].i);
    const char *s = c ? mbedtls_ssl_get_ciphersuite(&c->ssl) : NULL;
    ret->l = s ? jstring_new_utf8(t, s) : NULL;
    return NATIVE_OK;
}

static NativeResult Net_tlsVersion0(VMThread *t, Value *args, Value *ret) {
    Tls *c = get(args[0].i);
    const char *s = c ? mbedtls_ssl_get_version(&c->ssl) : NULL;
    ret->l = s ? jstring_new_utf8(t, s) : NULL;
    return NATIVE_OK;
}

void midp_tls_register(void) {
    const char *N = "j2menx/Net";
    native_register(N, "tlsNew0", "(ILjava/lang/String;)I", Net_tlsNew0);
    native_register(N, "tlsHandshake0", "(I)I", Net_tlsHandshake0);
    native_register(N, "tlsRead0", "(I[BII)I", Net_tlsRead0);
    native_register(N, "tlsWrite0", "(I[BII)I", Net_tlsWrite0);
    native_register(N, "tlsAvailable0", "(I)I", Net_tlsAvailable0);
    native_register(N, "tlsClose0", "(I)V", Net_tlsClose0);
    native_register(N, "tlsCipher0", "(I)Ljava/lang/String;", Net_tlsCipher0);
    native_register(N, "tlsVersion0", "(I)Ljava/lang/String;", Net_tlsVersion0);
}

void midp_tls_shutdown(void) {
    for (int i = 1; i <= MAX_TLS; i++) {
        if (conns[i].used)
            tls_free(&conns[i]);
    }
}

#endif
