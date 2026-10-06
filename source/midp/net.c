// Native cho j2menx.Net: socket TCP không chặn
#include "midp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../vm/vm.h"

#ifdef MSG_NOSIGNAL
#define SEND_FLAGS MSG_NOSIGNAL
#else
#define SEND_FLAGS 0
#endif

// Socket đang mở, để đóng hết khi thoát game
#define MAX_SOCKETS 64
static int open_fds[MAX_SOCKETS];
static int open_count;

static void track(int fd) {
    if (open_count < MAX_SOCKETS)
        open_fds[open_count++] = fd;
}

static void untrack(int fd) {
    for (int i = 0; i < open_count; i++) {
        if (open_fds[i] == fd) {
            open_fds[i] = open_fds[--open_count];
            return;
        }
    }
}

void midp_net_shutdown(void) {
    for (int i = 0; i < open_count; i++)
        close(open_fds[i]);
    open_count = 0;
}

static NativeResult io_error(VMThread *t, const char *what) {
    char msg[128];
    snprintf(msg, sizeof(msg), "%s: %s", what, strerror(errno));
    throw_new(t, "java/io/IOException", msg);
    return NATIVE_EXCEPTION;
}

static NativeResult Net_socket0(VMThread *t, Value *args, Value *ret) {
    char host[256], port[16];
    if (!args[0].l) {
        throw_null(t);
        return NATIVE_EXCEPTION;
    }
    jstring_to_cstr(args[0].l, host, sizeof(host));
    snprintf(port, sizeof(port), "%d", args[1].i);

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    // Phân giải tên miền là thao tác chặn (thường rất nhanh)
    int rc = getaddrinfo(host, port, &hints, &res);
    if (rc != 0 || !res) {
        char msg[300];
        snprintf(msg, sizeof(msg), "Khong phan giai duoc ten mien %s", host);
        throw_new(t, "java/io/IOException", msg);
        return NATIVE_EXCEPTION;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        return io_error(t, "socket");
    }
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#ifdef SO_NOSIGPIPE
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif

    rc = connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (rc < 0 && errno != EINPROGRESS && errno != EWOULDBLOCK && errno != EAGAIN) {
        NativeResult r = io_error(t, "connect");
        close(fd);
        return r;
    }
    track(fd);
    ret->i = fd;
    return NATIVE_OK;
}

static NativeResult Net_connectPoll0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    struct pollfd p = { .fd = args[0].i, .events = POLLOUT };
    int rc = poll(&p, 1, 0);
    if (rc == 0) {
        ret->i = 0;
        return NATIVE_OK;
    }
    if (rc < 0 || (p.revents & (POLLERR | POLLNVAL))) {
        ret->i = -1;
        return NATIVE_OK;
    }
    int err = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(p.fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
        ret->i = -1;
        return NATIVE_OK;
    }
    ret->i = 1;
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

static NativeResult Net_read0(VMThread *t, Value *args, Value *ret) {
    Object *b = args[1].l;
    jint off = args[2].i, len = args[3].i;
    if (!check_range(t, b, off, len))
        return NATIVE_EXCEPTION;
    ssize_t n = recv(args[0].i, ARRAY_DATA(b, uint8_t) + off, (size_t)len, 0);
    if (n > 0) {
        ret->i = (jint)n;
    } else if (n == 0) {
        ret->i = -1;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        ret->i = 0;
    } else {
        return io_error(t, "recv");
    }
    return NATIVE_OK;
}

static NativeResult Net_write0(VMThread *t, Value *args, Value *ret) {
    Object *b = args[1].l;
    jint off = args[2].i, len = args[3].i;
    if (!check_range(t, b, off, len))
        return NATIVE_EXCEPTION;
    ssize_t n = send(args[0].i, ARRAY_DATA(b, uint8_t) + off, (size_t)len, SEND_FLAGS);
    if (n >= 0) {
        ret->i = (jint)n;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        ret->i = 0;
    } else {
        return io_error(t, "send");
    }
    return NATIVE_OK;
}

static NativeResult Net_available0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    int n = 0;
    if (ioctl(args[0].i, FIONREAD, &n) < 0)
        n = 0;
    ret->i = n;
    return NATIVE_OK;
}

static NativeResult Net_close0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    (void)ret;
    untrack(args[0].i);
    close(args[0].i);
    return NATIVE_OK;
}

static NativeResult Net_localAddress0(VMThread *t, Value *args, Value *ret) {
    struct sockaddr_in a;
    socklen_t len = sizeof(a);
    char buf[64] = "0.0.0.0";
    if (getsockname(args[0].i, (struct sockaddr *)&a, &len) == 0)
        inet_ntop(AF_INET, &a.sin_addr, buf, sizeof(buf));
    ret->l = jstring_new_utf8(t, buf);
    return NATIVE_OK;
}

static NativeResult Net_localPort0(VMThread *t, Value *args, Value *ret) {
    (void)t;
    struct sockaddr_in a;
    socklen_t len = sizeof(a);
    ret->i = getsockname(args[0].i, (struct sockaddr *)&a, &len) == 0 ? ntohs(a.sin_port) : 0;
    return NATIVE_OK;
}

void midp_net_register(void) {
    const char *N = "j2menx/Net";
    native_register(N, "socket0", "(Ljava/lang/String;I)I", Net_socket0);
    native_register(N, "connectPoll0", "(I)I", Net_connectPoll0);
    native_register(N, "read0", "(I[BII)I", Net_read0);
    native_register(N, "write0", "(I[BII)I", Net_write0);
    native_register(N, "available0", "(I)I", Net_available0);
    native_register(N, "close0", "(I)V", Net_close0);
    native_register(N, "localAddress0", "(I)Ljava/lang/String;", Net_localAddress0);
    native_register(N, "localPort0", "(I)I", Net_localPort0);
}
