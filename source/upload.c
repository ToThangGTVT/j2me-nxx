#include "upload.h"

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <SDL.h>

#include "crash.h"
#include "lang.h"
#include "websrv.h"

static char games_dir[512];
static SDL_mutex *lock;
static UploadStatus status;
static bool running;

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
        web_json_str(v, sizeof(v), tr(keys[i].id));
        o += (size_t)snprintf(strs + o, sizeof(strs) - o, "%s%s:%s", i ? "," : "", keys[i].key, v);
        if (o >= sizeof(strs) - 2)
            return;
    }
    // Đuôi file nhận được, để trang kiểm tra trước khi gửi
    o += (size_t)snprintf(strs + o, sizeof(strs) - o, ",exts:[\"jar\",\"jad\"");
    strs[o++] = ']';
    strs[o++] = '}';
    strs[o] = '\0';

    size_t len = sizeof(page_head) - 1 + o + sizeof(page_tail) - 1;
    if (web_send_head(fd, 200, "text/html; charset=utf-8", (int64_t)len, NULL) &&
        web_send_all(fd, page_head, sizeof(page_head) - 1) && web_send_all(fd, strs, o))
        web_send_all(fd, page_tail, sizeof(page_tail) - 1);
}

// ---- Nhận file ----

// Chữ có dấu (Latin-1, tiếng Việt) -> chữ không dấu; 0 = ký tự khác; -1 = dấu rời (NFD, bỏ đi)
static int ascii_fold(uint32_t c) {
    // U+00C0..U+00FF
    static const char latin1[] = "AAAAAAACEEEEIIIIDNOOOOO_OUUUUYTsaaaaaaaceeeeiiiidnooooo_ouuuuyty";
    // U+1EA0..U+1EF9: từng cặp hoa / thường (Ạạ Ảả ... Ỹỹ)
    static const char viet[] = "AAAAAAAAAAAAEEEEEEEEIIOOOOOOOOOOOOUUUUUUUYYYY";
    if (c >= 0xC0 && c <= 0xFF)
        return latin1[c - 0xC0];
    if (c >= 0x1EA0 && c <= 0x1EF9) {
        char b = viet[(c - 0x1EA0) / 2];
        return (c & 1) ? b - 'A' + 'a' : b;
    }
    if (c >= 0x300 && c <= 0x36F)
        return -1;
    switch (c) {
    case 0x102: return 'A';  case 0x103: return 'a';    // Ă ă
    case 0x110: return 'D';  case 0x111: return 'd';    // Đ đ
    case 0x128: return 'I';  case 0x129: return 'i';    // Ĩ ĩ
    case 0x168: return 'U';  case 0x169: return 'u';    // Ũ ũ
    case 0x1A0: return 'O';  case 0x1A1: return 'o';    // Ơ ơ
    case 0x1AF: return 'U';  case 0x1B0: return 'u';    // Ư ư
    }
    return 0;
}

// Tên file chỉ còn ASCII: thẻ SD trên Switch lưu được file tên có dấu (vd "nhẹ.jar", mở theo tên vẫn được)
// nhưng không liệt kê ra khi đọc thư mục, nên game gửi lên không hiện trong danh sách.
// Ký tự không bỏ dấu được (chữ Trung, Nga...) thành '_' và thêm mã từ tên gốc trước đuôi file,
// để 2 game tên khác nhau không ghi đè nhau (vd "帝国.jar" -> "_-1a2b3c.jar", "帝国.jad" -> "_-1a2b3c.jad").
static void to_ascii_name(const char *in, char *out, size_t size) {
    const unsigned char *s = (const unsigned char *)in;
    size_t o = 0;
    bool lossy = false;
    while (*s && o + 1 < size) {
        uint32_t c = *s;
        int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
        if (len > 1) {
            c &= 0x3F >> (len - 1);
            for (int i = 1; i < len; i++) {
                if ((s[i] & 0xC0) != 0x80) {    // UTF-8 hỏng: coi như 1 byte lạ
                    len = 1;
                    c = 0x80;
                    break;
                }
                c = (c << 6) | (s[i] & 0x3F);
            }
        }
        s += len;
        if (c < 0x80) {
            out[o++] = (char)c;
            continue;
        }
        int a = ascii_fold(c);
        if (a > 0)
            out[o++] = (char)a;
        else if (a == 0) {
            lossy = true;
            if (o == 0 || out[o - 1] != '_')
                out[o++] = '_';
        }
    }
    out[o] = '\0';
    char *dot = strrchr(out, '.');
    if (lossy && dot && o + 8 < size) {
        // FNV-1a của phần tên trước đuôi: .jar và .jad cùng tên gốc ra cùng tên mới
        const char *in_dot = strrchr(in, '.');
        uint32_t h = 2166136261u;
        for (const char *p = in; *p && p != in_dot; p++)
            h = (h ^ (unsigned char)*p) * 16777619u;
        char ext[16];
        snprintf(ext, sizeof(ext), "%s", dot);
        snprintf(dot, size - (size_t)(dot - out), "-%06x%s", (unsigned)(h & 0xFFFFFF), ext);
    }
}

// Lấy tên file an toàn từ ?name=...: bỏ thư mục, đổi sang ASCII, chỉ nhận game .jar / .jad.
// orig nhận tên gốc (đã bỏ thư mục) để ghi log / dọn file tên có dấu cũ.
static bool file_name_from_query(const char *path, char *out, size_t size, char *orig, size_t orig_size) {
    char raw[512];
    if (!web_query(path, "name", raw, sizeof(raw)))
        return false;
    const char *base = raw;
    for (const char *p = raw; *p; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    snprintf(orig, orig_size, "%s", base);
    char name[512];
    to_ascii_name(base, name, sizeof(name));
    size_t n = strlen(name);
    if (n < 5 || n >= size || name[0] == '.')
        return false;
    for (const char *p = name; *p; p++) {
        if ((unsigned char)*p < 0x20 || strchr(":*?\"<>|", *p))
            return false;
    }
    if (strcasecmp(name + n - 4, ".jar") != 0 && strcasecmp(name + n - 4, ".jad") != 0)
        return false;
    memcpy(out, name, n + 1);
    return true;
}

// Sau khi lưu: đọc lại thư mục xem file có hiện ra không (để biết danh sách game có thấy nó)
static bool listed_in_dir(const char *dir, const char *name) {
    DIR *d = opendir(dir);
    if (!d)
        return false;
    bool found = false;
    struct dirent *e;
    while (!found && (e = readdir(d)) != NULL)
        found = strcmp(e->d_name, name) == 0;
    closedir(d);
    return found;
}

static void set_error(const char *msg) {
    SDL_LockMutex(lock);
    snprintf(status.error, sizeof(status.error), "%s", msg);
    status.current[0] = '\0';
    SDL_UnlockMutex(lock);
}

static void on_progress(int64_t done, void *arg) {
    (void)arg;
    SDL_LockMutex(lock);
    status.done = done;
    SDL_UnlockMutex(lock);
}

static void handle_upload(WebRequest *req) {
    int fd = req->fd;
    char name[128], orig[512] = "";
    app_logf("[upload] POST %.300s, %lld byte", req->path, (long long)req->length);
    if (!file_name_from_query(req->path, name, sizeof(name), orig, sizeof(orig))) {
        app_logf("[upload] tu choi ten file: \"%s\" (chi nhan .jar / .jad)", orig);
        web_drain(fd, req->length - req->body_len);
        web_send_json(fd, 400, false, tr(S_WEB_WRONG_TYPE));
        return;
    }
    char final_path[768], tmp_path[800];
    snprintf(final_path, sizeof(final_path), "%s/%s", games_dir, name);
    snprintf(tmp_path, sizeof(tmp_path), "%s.part", final_path);
    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        app_logf("[upload] khong mo duoc %s: %s", tmp_path, strerror(errno));
        web_drain(fd, req->length - req->body_len);
        web_send_json(fd, 500, false, strerror(errno));
        set_error(strerror(errno));
        return;
    }

    SDL_LockMutex(lock);
    snprintf(status.current, sizeof(status.current), "%s", name);
    status.done = 0;
    status.total = req->length;
    status.error[0] = '\0';
    SDL_UnlockMutex(lock);

    WebRecvResult r = web_recv_to_file(req, f, on_progress, NULL);
    bool write_err = r == WEB_RECV_WRITE || ferror(f) != 0;
    if (fclose(f) != 0)
        write_err = true;
    if (r != WEB_RECV_OK || write_err) {
        UploadStatus st;
        upload_get_status(&st);
        app_logf("[upload] loi khi nhan %s: %s, da nhan %lld / %lld byte", name,
                 write_err ? "ghi the loi" : r == WEB_RECV_NET ? "mat ket noi" : "loi nhan", (long long)st.done,
                 (long long)req->length);
        remove(tmp_path);
        const char *msg = write_err ? tr(S_UPLOAD_WRITE_FAILED) : tr(S_WEB_NETWORK);
        set_error(msg);
        if (r == WEB_RECV_NET)
            return;     // mất kết nối: không còn ai nhận câu trả lời
        web_send_json(fd, 500, false, msg);
        return;
    }
    // Ghi đè file cũ cùng tên (rename trên thẻ SD của Switch không thay file đã có)
    remove(final_path);
    if (rename(tmp_path, final_path) != 0) {
        app_logf("[upload] doi ten %s -> %s loi: %s", tmp_path, final_path, strerror(errno));
        remove(tmp_path);
        set_error(tr(S_UPLOAD_WRITE_FAILED));
        web_send_json(fd, 500, false, tr(S_UPLOAD_WRITE_FAILED));
        return;
    }
    struct stat st;
    long long saved = stat(final_path, &st) == 0 ? (long long)st.st_size : -1;
    app_logf("[upload] da luu %s (%lld byte tren the, %s trong thu muc)", final_path, saved,
             listed_in_dir(games_dir, name) ? "co" : "KHONG thay");
    if (strcmp(orig, name) != 0) {
        // Bản gửi trước với tên có dấu (không hiện trong danh sách): xoá cho đỡ tốn chỗ
        char old_path[768];
        snprintf(old_path, sizeof(old_path), "%s/%s", games_dir, orig);
        bool had_old = stat(old_path, &st) == 0 && remove(old_path) == 0;
        app_logf("[upload] doi ten \"%s\" -> \"%s\"%s", orig, name, had_old ? ", da xoa ban cu ten co dau" : "");
    }
    SDL_LockMutex(lock);
    status.received++;
    snprintf(status.last, sizeof(status.last), "%s", name);
    status.current[0] = '\0';
    SDL_UnlockMutex(lock);
    web_send_json(fd, 200, true, NULL);
}

static void handle_request(WebRequest *req) {
    if (strcmp(req->method, "GET") == 0 && (strcmp(req->path, "/") == 0 || strncmp(req->path, "/?", 2) == 0)) {
        app_logf("[upload] dien thoai mo trang tai len");
        send_page(req->fd);
    } else if (strcmp(req->method, "POST") == 0 && strncmp(req->path, "/upload", 7) == 0) {
        if (req->length < 0)
            web_send_json(req->fd, 411, false, "Content-Length");
        else
            handle_upload(req);
    } else {
        web_send_response(req->fd, 404, "text/plain", "Not found", 9);
    }
}

bool upload_start(const char *dir, char *url, size_t url_size, char *err, size_t err_size) {
    upload_stop();
    snprintf(games_dir, sizeof(games_dir), "%s", dir);
    mkdir(games_dir, 0777);
    if (!lock)
        lock = SDL_CreateMutex();
    memset(&status, 0, sizeof(status));
    bool ok = web_start(handle_request, url, url_size, err, err_size);
    running = ok;
    if (ok)
        app_logf("[upload] bat server %s, luu vao %s", url, games_dir);
    else
        app_logf("[upload] khong bat duoc server: %s", err);
    return ok;
}

void upload_stop(void) {
    web_stop();
    if (running) {
        running = false;
        UploadStatus st;
        upload_get_status(&st);
        app_logf("[upload] tat server: nhan %d file%s%s", st.received, st.error[0] ? ", loi cuoi: " : "", st.error);
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
