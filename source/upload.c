#include "upload.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <SDL.h>

#include "lang.h"
#include "websrv.h"

static char games_dir[512];
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

// Lấy tên file an toàn từ ?name=...: bỏ thư mục, chỉ nhận game .jar / .jad
static bool file_name_from_query(const char *path, char *out, size_t size) {
    char raw[512];
    if (!web_query(path, "name", raw, sizeof(raw)))
        return false;
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
    if (strcasecmp(base + n - 4, ".jar") != 0 && strcasecmp(base + n - 4, ".jad") != 0)
        return false;
    memcpy(out, base, n + 1);
    return true;
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
    char name[128];
    if (!file_name_from_query(req->path, name, sizeof(name))) {
        web_drain(fd, req->length - req->body_len);
        web_send_json(fd, 400, false, tr(S_WEB_WRONG_TYPE));
        return;
    }
    char final_path[768], tmp_path[800];
    snprintf(final_path, sizeof(final_path), "%s/%s", games_dir, name);
    snprintf(tmp_path, sizeof(tmp_path), "%s.part", final_path);
    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
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
        remove(tmp_path);
        set_error(tr(S_UPLOAD_WRITE_FAILED));
        web_send_json(fd, 500, false, tr(S_UPLOAD_WRITE_FAILED));
        return;
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
    return web_start(handle_request, url, url_size, err, err_size);
}

void upload_stop(void) {
    web_stop();
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
