#include "upload.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <SDL.h>

#include "game_list.h"
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

static char games_dir[512];
static int listen_fd = -1;
static SDL_Thread *thread;
static SDL_atomic_t stop_flag;
static SDL_mutex *lock;
static UploadStatus status;

// ---- Trang tải lên (điện thoại mở bằng trình duyệt) ----

static const char page_head[] =
    "<!doctype html><html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>J2ME-NXX</title><style>"
    "body{margin:0;background:#24262b;color:#eee;font:16px/1.45 -apple-system,system-ui,sans-serif}"
    "main{max-width:560px;margin:0 auto;padding:24px 16px}"
    "h1{font-size:22px;margin:0 0 6px}h1 b{color:#00b4e6}"
    "p{color:#9a9fa8;margin:0 0 20px}"
    "label{display:block;background:#00b4e6;color:#111;text-align:center;font-weight:600;"
    "padding:16px;border-radius:12px;cursor:pointer}"
    "input{display:none}ul{list-style:none;padding:0;margin:20px 0 0}"
    "li{background:#18191d;border-radius:10px;padding:12px 14px;margin-bottom:10px}"
    ".n{word-break:break-all}.s{color:#9a9fa8;font-size:14px;margin-top:4px}"
    ".bar{height:6px;background:#3a3e47;border-radius:3px;margin-top:8px;overflow:hidden}"
    ".bar i{display:block;height:100%;width:0;background:#00b4e6}"
    ".ok .s{color:#5cd67a}.ok .bar i{background:#5cd67a}.err .s{color:#ffc14d}"
    "#all{color:#5cd67a;margin-top:12px;display:none}"
    "</style></head><body><main>"
    "<h1><b>J2ME-NXX</b> <span id=\"t\"></span></h1><p id=\"i\"></p>"
    "<label for=\"f\" id=\"c\"></label><input type=\"file\" id=\"f\" multiple>"
    "<ul id=\"l\"></ul><div id=\"all\"></div></main><script>var T=";

// Không đặt accept: trình chọn file của iOS / một số máy Android làm mờ file không rõ kiểu
static const char page_tail[] =
    ";\n"
    "var $=function(id){return document.getElementById(id)},q=[],busy=0,sent=0;"
    "document.title='J2ME-NXX - '+T.title;$('t').textContent=T.title;$('i').textContent=T.intro;"
    "$('c').textContent=T.choose;"
    "function mb(n){return (n/1048576).toFixed(n<1048576?2:1)+' MB'}"
    "function row(f){var li=document.createElement('li');"
    "li.innerHTML='<div class=n></div><div class=s></div><div class=bar><i></i></div>';"
    "li.firstChild.textContent=f.name;$('l').appendChild(li);return li}"
    "function set(li,cls,text,pct){li.className=cls;li.querySelector('.s').textContent=text;"
    "if(pct!=null)li.querySelector('i').style.width=pct+'%'}"
    "$('f').onchange=function(){var fs=this.files;for(var k=0;k<fs.length;k++){var f=fs[k],li=row(f);"
    "var m=/\\.([^.]+)$/.exec(f.name);if(!m||T.exts.indexOf(m[1].toLowerCase())<0){set(li,'err',T.wrong,0);continue}"
    "set(li,'',T.waiting+' - '+mb(f.size),0);q.push({f:f,li:li})}this.value='';next()};"
    "function next(){if(busy)return;var it=q.shift();if(!it){if(sent){$('all').textContent=T.all;"
    "$('all').style.display='block'}return}busy=1;var x=new XMLHttpRequest();"
    "x.open('POST','/upload?name='+encodeURIComponent(it.f.name));"
    "x.upload.onprogress=function(e){if(e.lengthComputable)"
    "set(it.li,'',T.sending+' '+Math.floor(e.loaded*100/e.total)+'%',e.loaded*100/e.total)};"
    "x.onload=function(){var r={};try{r=JSON.parse(x.responseText)}catch(e){}"
    "if(x.status==200&&r.ok){sent++;set(it.li,'ok',T.done+' - '+mb(it.f.size),100)}"
    "else set(it.li,'err',T.failed+': '+(r.error||x.status),0)};"
    "x.onerror=function(){set(it.li,'err',T.failed+': '+T.network,0)};"
    "x.onloadend=function(){busy=0;next()};x.send(it.f)}"
    "</script></body></html>";

// ---- Gửi / nhận có thời hạn ----

static bool stopping(void) {
    return SDL_AtomicGet(&stop_flag) != 0;
}

static bool send_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        struct pollfd p = { fd, POLLOUT, 0 };
        if (poll(&p, 1, IO_TIMEOUT_MS) <= 0)
            return false;
        ssize_t n = send(fd, buf, len, SEND_FLAGS);
        if (n <= 0)
            return false;
        buf += n;
        len -= (size_t)n;
    }
    return true;
}

// > 0: số byte, 0: điện thoại đóng kết nối, < 0: lỗi / quá hạn / đang tắt server
static int recv_some(int fd, char *buf, int len) {
    for (int waited = 0; waited < IO_TIMEOUT_MS && !stopping(); waited += 200) {
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

static void send_response(int fd, int code, const char *type, const char *body, size_t len) {
    const char *reason = code == 200 ? "OK" : code == 404 ? "Not Found" : code == 411 ? "Length Required"
                       : code == 400 ? "Bad Request" : "Internal Server Error";
    char head[256];
    int n = snprintf(head, sizeof(head),
                     "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
                     "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
                     code, reason, type, len);
    if (send_all(fd, head, (size_t)n))
        send_all(fd, body, len);
}

// Chuỗi JSON: thoát " \ và ký tự điều khiển
static void json_str(char *out, size_t size, const char *s) {
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

static void send_json(int fd, int code, bool ok, const char *error) {
    char err[256], body[320];
    json_str(err, sizeof(err), error ? error : "");
    int n = snprintf(body, sizeof(body), "{\"ok\":%s,\"error\":%s}", ok ? "true" : "false", err);
    send_response(fd, code, "application/json", body, (size_t)n);
}

static void send_page(int fd) {
    static const struct { const char *key; StrId id; } keys[] = {
        { "title", S_WEB_TITLE },     { "intro", S_WEB_INTRO },     { "choose", S_WEB_CHOOSE },
        { "waiting", S_WEB_WAITING }, { "sending", S_WEB_SENDING }, { "done", S_WEB_DONE },
        { "failed", S_WEB_FAILED },   { "network", S_WEB_NETWORK }, { "wrong", S_WEB_WRONG_TYPE },
        { "all", S_WEB_ALL_DONE },
    };
    char strs[4096];
    size_t o = 0;
    strs[o++] = '{';
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        char v[512];
        json_str(v, sizeof(v), tr(keys[i].id));
        o += (size_t)snprintf(strs + o, sizeof(strs) - o, "%s%s:%s", i ? "," : "", keys[i].key, v);
        if (o >= sizeof(strs) - 2)
            return;
    }
    // Đuôi file nhận được, để trang kiểm tra trước khi gửi
    o += (size_t)snprintf(strs + o, sizeof(strs) - o, ",exts:[\"jar\",\"jad\"");
    for (int i = 0; i < game_list_video_ext_count && o < sizeof(strs) - 16; i++)
        o += (size_t)snprintf(strs + o, sizeof(strs) - o, ",\"%s\"", game_list_video_exts[i] + 1);
    strs[o++] = ']';
    strs[o++] = '}';
    strs[o] = '\0';

    size_t len = sizeof(page_head) - 1 + o + sizeof(page_tail) - 1;
    char head[256];
    int n = snprintf(head, sizeof(head),
                     "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %zu\r\n"
                     "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
                     len);
    if (send_all(fd, head, (size_t)n) && send_all(fd, page_head, sizeof(page_head) - 1) && send_all(fd, strs, o))
        send_all(fd, page_tail, sizeof(page_tail) - 1);
}

// ---- Nhận file ----

static int hex_val(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static void url_decode(char *out, size_t size, const char *s, size_t len) {
    size_t o = 0;
    for (size_t i = 0; i < len && o + 1 < size; i++) {
        if (s[i] == '%' && i + 2 < len && hex_val(s[i + 1]) >= 0 && hex_val(s[i + 2]) >= 0) {
            out[o++] = (char)(hex_val(s[i + 1]) * 16 + hex_val(s[i + 2]));
            i += 2;
        } else {
            out[o++] = s[i] == '+' ? ' ' : s[i];
        }
    }
    out[o] = '\0';
}

// Lấy tên file an toàn từ ?name=...: bỏ thư mục, chỉ nhận game .jar / .jad và video
static bool file_name_from_query(const char *path, char *out, size_t size) {
    const char *q = strstr(path, "?name=");
    if (!q)
        q = strstr(path, "&name=");
    if (!q)
        return false;
    q += 6;
    char raw[512];
    url_decode(raw, sizeof(raw), q, strcspn(q, "&"));
    const char *base = raw;
    for (const char *p = raw; *p; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    size_t n = strlen(base);
    if (n < 5 || n >= size || base[0] == '.')
        return false;
    for (const char *p = base; *p; p++) {
        if ((unsigned char)*p < 0x20 || strchr(":*?\"<>|", *p))
            return false;
    }
    if (strcasecmp(base + n - 4, ".jar") != 0 && strcasecmp(base + n - 4, ".jad") != 0 && !game_list_is_video(base))
        return false;
    memcpy(out, base, n + 1);
    return true;
}

// Bỏ phần thân còn lại, để điện thoại đọc được câu trả lời lỗi
static void drain(int fd, int64_t left) {
    char buf[4096];
    while (left > 0) {
        int n = recv_some(fd, buf, left < (int64_t)sizeof(buf) ? (int)left : (int)sizeof(buf));
        if (n <= 0)
            return;
        left -= n;
    }
}

static void set_error(const char *msg) {
    SDL_LockMutex(lock);
    snprintf(status.error, sizeof(status.error), "%s", msg);
    status.current[0] = '\0';
    SDL_UnlockMutex(lock);
}

static void handle_upload(int fd, const char *path, int64_t length, const char *body, int body_len) {
    char name[128];
    if (!file_name_from_query(path, name, sizeof(name))) {
        drain(fd, length - body_len);
        send_json(fd, 400, false, tr(S_WEB_WRONG_TYPE));
        return;
    }
    char final_path[768], tmp_path[800];
    snprintf(final_path, sizeof(final_path), "%s/%s", games_dir, name);
    snprintf(tmp_path, sizeof(tmp_path), "%s.part", final_path);
    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        drain(fd, length - body_len);
        send_json(fd, 500, false, strerror(errno));
        set_error(strerror(errno));
        return;
    }

    SDL_LockMutex(lock);
    snprintf(status.current, sizeof(status.current), "%s", name);
    status.done = 0;
    status.total = length;
    status.error[0] = '\0';
    SDL_UnlockMutex(lock);

    static char buf[CHUNK];
    int64_t got = 0;
    bool ok = true;
    if (body_len > 0) {
        int n = body_len < length ? body_len : (int)length;
        ok = fwrite(body, 1, (size_t)n, f) == (size_t)n;
        got = n;
    }
    while (ok && got < length) {
        int want = length - got < CHUNK ? (int)(length - got) : CHUNK;
        int n = recv_some(fd, buf, want);
        if (n <= 0) {
            ok = false;
            break;
        }
        if (fwrite(buf, 1, (size_t)n, f) != (size_t)n) {
            ok = false;
            break;
        }
        got += n;
        SDL_LockMutex(lock);
        status.done = got;
        SDL_UnlockMutex(lock);
    }
    bool write_err = ferror(f) != 0;
    if (fclose(f) != 0)
        write_err = true;
    if (!ok || write_err) {
        remove(tmp_path);
        const char *msg = write_err ? tr(S_UPLOAD_WRITE_FAILED) : tr(S_WEB_NETWORK);
        set_error(msg);
        if (got < length && !write_err)
            return;     // mất kết nối: không còn ai nhận câu trả lời
        drain(fd, length - got);
        send_json(fd, 500, false, msg);
        return;
    }
    // Ghi đè file cũ cùng tên (rename trên thẻ SD của Switch không thay file đã có)
    remove(final_path);
    if (rename(tmp_path, final_path) != 0) {
        remove(tmp_path);
        set_error(tr(S_UPLOAD_WRITE_FAILED));
        send_json(fd, 500, false, tr(S_UPLOAD_WRITE_FAILED));
        return;
    }
    SDL_LockMutex(lock);
    status.received++;
    snprintf(status.last, sizeof(status.last), "%s", name);
    status.current[0] = '\0';
    SDL_UnlockMutex(lock);
    send_json(fd, 200, true, NULL);
}

static void handle_client(int fd) {
    char req[HEADER_MAX + 1];
    int len = 0;
    char *end = NULL;
    while (!end) {
        if (len >= HEADER_MAX)
            return;
        int n = recv_some(fd, req + len, HEADER_MAX - len);
        if (n <= 0)
            return;
        len += n;
        req[len] = '\0';
        end = strstr(req, "\r\n\r\n");
    }
    *end = '\0';
    char *body = end + 4;
    int body_len = len - (int)(body - req);

    char method[8], path[1024];
    if (sscanf(req, "%7s %1023s", method, path) != 2)
        return;
    int64_t length = -1;
    for (char *line = strstr(req, "\r\n"); line; line = strstr(line + 2, "\r\n")) {
        if (strncasecmp(line + 2, "Content-Length:", 15) == 0)
            length = strtoll(line + 17, NULL, 10);
    }

    if (strcmp(method, "GET") == 0 && (strcmp(path, "/") == 0 || strncmp(path, "/?", 2) == 0)) {
        send_page(fd);
    } else if (strcmp(method, "POST") == 0 && strncmp(path, "/upload", 7) == 0) {
        if (length < 0)
            send_json(fd, 411, false, "Content-Length");
        else
            handle_upload(fd, path, length, body, body_len);
    } else {
        send_response(fd, 404, "text/plain", "Not found", 9);
    }
}

static int server_thread(void *arg) {
    (void)arg;
    while (!stopping()) {
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

bool upload_start(const char *dir, char *url, size_t url_size, char *err, size_t err_size) {
    upload_stop();
    char ip[64];
    if (!platform_local_ip(ip, sizeof(ip))) {
        snprintf(err, err_size, "%s", tr(S_UPLOAD_NO_NET));
        return false;
    }
    snprintf(games_dir, sizeof(games_dir), "%s", dir);
    mkdir(games_dir, 0777);

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

    if (!lock)
        lock = SDL_CreateMutex();
    memset(&status, 0, sizeof(status));
    SDL_AtomicSet(&stop_flag, 0);
    thread = SDL_CreateThread(server_thread, "upload", NULL);
    if (!thread) {
        snprintf(err, err_size, tr(S_UPLOAD_SERVER_FAILED), SDL_GetError());
        close(listen_fd);
        listen_fd = -1;
        return false;
    }
    snprintf(url, url_size, "http://%s:%d/", ip, port);
    return true;
}

void upload_stop(void) {
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

void upload_get_status(UploadStatus *out) {
    if (!lock) {
        memset(out, 0, sizeof(*out));
        return;
    }
    SDL_LockMutex(lock);
    *out = status;
    SDL_UnlockMutex(lock);
}
