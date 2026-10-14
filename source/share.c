#include "share.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <SDL.h>

#include "backup.h"
#include "lang.h"
#include "platform.h"
#include "websrv.h"

#define CHUNK (64 * 1024)

static SDL_mutex *lock;
static ShareStatus status;

// ---- Trang backup (điện thoại mở bằng trình duyệt) ----

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
    "input{display:none}ul{list-style:none;padding:0;margin:16px 0 0}"
    "li{background:#18191d;border-radius:10px;padding:12px 14px;margin-bottom:10px}"
    ".n{word-break:break-all}.s{color:#9a9fa8;font-size:14px;margin-top:4px}"
    ".bar{height:6px;background:#3a3e47;border-radius:3px;margin-top:8px;overflow:hidden}"
    ".bar i{display:block;height:100%;width:0;background:#00b4e6}"
    ".ok .s{color:#5cd67a}.ok .bar i{background:#5cd67a}.err .s{color:#ffc14d}"
    "section{margin-top:28px}h2{font-size:18px;margin:0;word-break:break-all}section ul{margin-top:10px}"
    "li.b{display:flex;align-items:center;justify-content:space-between;gap:12px}"
    "a.dl{flex:none;background:#3a3e47;color:#fff;text-decoration:none;font-weight:600;"
    "padding:9px 16px;border-radius:8px}"
    "</style></head><body><main>"
    "<h1><b>J2ME-NXX</b> <span id=\"t\"></span></h1><p id=\"i\"></p>"
    "<label for=\"f\" id=\"c\"></label><input type=\"file\" id=\"f\" multiple>"
    "<ul id=\"l\"></ul><div id=\"g\"></div></main><script>var T=";

static const char page_tail[] =
    ";\n"
    "var $=function(id){return document.getElementById(id)},q=[],busy=0;"
    "document.title='J2ME-NXX - '+T.title;$('t').textContent=T.title;$('i').textContent=T.intro;"
    "$('c').textContent=T.choose;"
    "function mb(n){return n<1048576?Math.ceil(n/1024)+' KB':(n/1048576).toFixed(1)+' MB'}"
    "function el(tag,cls,text){var e=document.createElement(tag);if(cls)e.className=cls;"
    "if(text!=null)e.textContent=text;return e}"
    "function load(){var x=new XMLHttpRequest();x.open('GET','/list');"
    "x.onload=function(){var d={games:[]};try{d=JSON.parse(x.responseText)}catch(e){}show(d)};x.send()}"
    "function show(d){var g=$('g');g.innerHTML='';"
    "if(!d.games.length){var e=el('p','',T.empty);e.style.marginTop='28px';g.appendChild(e);return}"
    "d.games.forEach(function(gm){var s=el('section');s.appendChild(el('h2','',gm.title));"
    "if(gm.id!=gm.title)s.appendChild(el('div','s',gm.id));"
    "var ul=el('ul');gm.backups.forEach(function(b){var li=el('li','b'),t=el('div');"
    "t.appendChild(el('div','n',b.when));t.appendChild(el('div','s',mb(b.size)));li.appendChild(t);"
    "var a=el('a','dl',T.download);"
    "a.href='/dl?id='+encodeURIComponent(gm.id)+'&file='+encodeURIComponent(b.file);"
    "a.setAttribute('download',gm.id+'_'+b.file);li.appendChild(a);ul.appendChild(li)});"
    "s.appendChild(ul);g.appendChild(s)})}"
    "function row(f){var li=el('li');li.appendChild(el('div','n',f.name));li.appendChild(el('div','s'));"
    "var bar=el('div','bar');bar.appendChild(el('i'));li.appendChild(bar);$('l').appendChild(li);return li}"
    "function set(li,cls,text,pct){li.className=cls;li.querySelector('.s').textContent=text;"
    "if(pct!=null)li.querySelector('i').style.width=pct+'%'}"
    "$('f').onchange=function(){var fs=this.files;for(var k=0;k<fs.length;k++){var f=fs[k],li=row(f);"
    "if(!/\\.zip$/i.test(f.name)){set(li,'err',T.wrong,0);continue}"
    "set(li,'',T.waiting+' - '+mb(f.size),0);q.push({f:f,li:li})}this.value='';next()};"
    "function next(){if(busy)return;var it=q.shift();if(!it)return;busy=1;var x=new XMLHttpRequest();"
    "x.open('POST','/upload?name='+encodeURIComponent(it.f.name));"
    "x.upload.onprogress=function(e){if(e.lengthComputable)"
    "set(it.li,'',T.sending+' '+Math.floor(e.loaded*100/e.total)+'%',e.loaded*100/e.total)};"
    "x.onload=function(){var r={};try{r=JSON.parse(x.responseText)}catch(e){}"
    "if(x.status==200&&r.ok){set(it.li,'ok',T.done+' - '+mb(it.f.size),100);load()}"
    "else set(it.li,'err',T.failed+': '+(r.error||x.status),0)};"
    "x.onerror=function(){set(it.li,'err',T.failed+': '+T.network,0)};"
    "x.onloadend=function(){busy=0;next()};x.send(it.f)}"
    "load();"
    "</script></body></html>";

static void send_page(int fd) {
    static const struct { const char *key; StrId id; } keys[] = {
        { "title", S_WEB_BACKUP_TITLE },  { "intro", S_WEB_BACKUP_INTRO }, { "choose", S_WEB_BACKUP_CHOOSE },
        { "empty", S_WEB_BACKUP_EMPTY },  { "download", S_WEB_BACKUP_DOWNLOAD },
        { "waiting", S_WEB_WAITING },     { "sending", S_WEB_SENDING },    { "done", S_WEB_DONE },
        { "failed", S_WEB_FAILED },       { "network", S_WEB_NETWORK },    { "wrong", S_WEB_BACKUP_WRONG },
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
    strs[o++] = '}';
    strs[o] = '\0';

    size_t len = sizeof(page_head) - 1 + o + sizeof(page_tail) - 1;
    if (web_send_head(fd, 200, "text/html; charset=utf-8", (int64_t)len, NULL) &&
        web_send_all(fd, page_head, sizeof(page_head) - 1) && web_send_all(fd, strs, o))
        web_send_all(fd, page_tail, sizeof(page_tail) - 1);
}

// ---- Danh sách backup (JSON) ----

typedef struct {
    char *data;
    size_t len, cap;
} Buf;

static void buf_add(Buf *b, const char *s) {
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 4096;
        while (b->len + n + 1 > cap)
            cap *= 2;
        char *d = realloc(b->data, cap);
        if (!d)
            return;
        b->data = d;
        b->cap = cap;
    }
    memcpy(b->data + b->len, s, n + 1);
    b->len += n;
}

static void send_list(int fd) {
    Buf b = { 0 };
    buf_add(&b, "{\"games\":[");
    char **ids;
    int count = backup_ids(&ids), shown = 0;
    for (int i = 0; i < count; i++) {
        BackupList l;
        backup_list(ids[i], &l);
        if (l.count == 0) {
            backup_list_free(&l);
            continue;
        }
        // Tên ứng dụng lấy từ backup mới nhất
        char path[768], s[512];
        BackupMeta meta;
        if (!backup_file_path(ids[i], l.items[0].file, path, sizeof(path)) || !backup_read_meta(path, &meta))
            snprintf(meta.title, sizeof(meta.title), "%s", ids[i]);
        buf_add(&b, shown++ ? ",{\"id\":" : "{\"id\":");
        web_json_str(s, sizeof(s), ids[i]);
        buf_add(&b, s);
        buf_add(&b, ",\"title\":");
        web_json_str(s, sizeof(s), meta.title);
        buf_add(&b, s);
        buf_add(&b, ",\"backups\":[");
        for (int k = 0; k < l.count; k++) {
            snprintf(s, sizeof(s), "%s{\"file\":\"%s\",\"when\":\"%s\",\"size\":%ld}", k ? "," : "", l.items[k].file,
                     l.items[k].when, l.items[k].size);
            buf_add(&b, s);
        }
        buf_add(&b, "]}");
        backup_list_free(&l);
    }
    backup_ids_free(ids, count);
    buf_add(&b, "]}");
    if (b.data)
        web_send_response(fd, 200, "application/json", b.data, b.len);
    else
        web_send_json(fd, 500, false, "out of memory");
    free(b.data);
}

// ---- Gửi file backup ----

static void set_error(const char *msg) {
    SDL_LockMutex(lock);
    snprintf(status.error, sizeof(status.error), "%s", msg);
    status.current[0] = '\0';
    SDL_UnlockMutex(lock);
}

static void send_backup(int fd, const char *path_query) {
    char id[256], file[64], path[768];
    if (!web_query(path_query, "id", id, sizeof(id)) || !web_query(path_query, "file", file, sizeof(file)) ||
        !backup_file_path(id, file, path, sizeof(path))) {
        web_send_response(fd, 404, "text/plain", "Not found", 9);
        return;
    }
    FILE *f = fopen(path, "rb");
    struct stat st;
    if (!f || fstat(fileno(f), &st) != 0) {
        if (f)
            fclose(f);
        web_send_response(fd, 404, "text/plain", "Not found", 9);
        return;
    }

    // Tên file tải về: <id>_<thời gian>.zip (chữ không phải ASCII để ở filename*)
    char name[320], ascii[320], pct[960], extra[1400];
    snprintf(name, sizeof(name), "%s_%s", id, file);
    size_t a = 0, p = 0;
    for (const unsigned char *c = (const unsigned char *)name; *c; c++) {
        bool plain = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') ||
                     *c == '-' || *c == '_' || *c == '.';
        ascii[a++] = plain ? (char)*c : '_';
        if (plain)
            pct[p++] = (char)*c;
        else
            p += (size_t)snprintf(pct + p, sizeof(pct) - p, "%%%02X", *c);
    }
    ascii[a] = '\0';
    pct[p] = '\0';
    snprintf(extra, sizeof(extra), "Content-Disposition: attachment; filename=\"%s\"; filename*=UTF-8''%s\r\n", ascii,
             pct);

    SDL_LockMutex(lock);
    snprintf(status.current, sizeof(status.current), "%s", name);
    status.receiving = false;
    status.done = 0;
    status.total = (int64_t)st.st_size;
    status.error[0] = '\0';
    SDL_UnlockMutex(lock);

    static char buf[CHUNK];
    bool ok = web_send_head(fd, 200, "application/zip", (int64_t)st.st_size, extra);
    int64_t sent = 0;
    while (ok && sent < (int64_t)st.st_size) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n == 0 || !web_send_all(fd, buf, n)) {
            ok = false;
            break;
        }
        sent += (int64_t)n;
        SDL_LockMutex(lock);
        status.done = sent;
        SDL_UnlockMutex(lock);
    }
    fclose(f);
    if (!ok) {
        set_error(tr(S_WEB_NETWORK));
        return;
    }
    SDL_LockMutex(lock);
    status.sent++;
    snprintf(status.last, sizeof(status.last), "%s", name);
    status.current[0] = '\0';
    SDL_UnlockMutex(lock);
}

// ---- Nhận file backup ----

static void on_progress(int64_t done, void *arg) {
    (void)arg;
    SDL_LockMutex(lock);
    status.done = done;
    SDL_UnlockMutex(lock);
}

static void receive_backup(WebRequest *req) {
    int fd = req->fd;
    char name[160];
    size_t n;
    if (!web_query(req->path, "name", name, sizeof(name)) || (n = strlen(name)) < 5 ||
        strcasecmp(name + n - 4, ".zip") != 0) {
        web_drain(fd, req->length - req->body_len);
        web_send_json(fd, 400, false, tr(S_WEB_BACKUP_WRONG));
        return;
    }
    char dir[600], tmp[700];
    snprintf(dir, sizeof(dir), "%s/backups", platform_data_dir());
    mkdir(platform_data_dir(), 0777);
    mkdir(dir, 0777);
    snprintf(tmp, sizeof(tmp), "%s/incoming.part", dir);
    FILE *f = fopen(tmp, "wb");
    if (!f) {
        web_drain(fd, req->length - req->body_len);
        web_send_json(fd, 500, false, strerror(errno));
        set_error(strerror(errno));
        return;
    }

    SDL_LockMutex(lock);
    snprintf(status.current, sizeof(status.current), "%s", name);
    status.receiving = true;
    status.done = 0;
    status.total = req->length;
    status.error[0] = '\0';
    SDL_UnlockMutex(lock);

    WebRecvResult r = web_recv_to_file(req, f, on_progress, NULL);
    bool write_err = r == WEB_RECV_WRITE || ferror(f) != 0;
    if (fclose(f) != 0)
        write_err = true;
    if (r != WEB_RECV_OK || write_err) {
        remove(tmp);
        const char *msg = write_err ? tr(S_UPLOAD_WRITE_FAILED) : tr(S_WEB_NETWORK);
        set_error(msg);
        if (r != WEB_RECV_NET)
            web_send_json(fd, 500, false, msg);
        return;
    }

    char id[256], file[64], err[256];
    if (!backup_import(tmp, id, sizeof(id), file, sizeof(file), err, sizeof(err))) {
        set_error(err);
        web_send_json(fd, 400, false, err);
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
    bool get = strcmp(req->method, "GET") == 0;
    if (get && (strcmp(req->path, "/") == 0 || strncmp(req->path, "/?", 2) == 0)) {
        send_page(req->fd);
    } else if (get && strcmp(req->path, "/list") == 0) {
        send_list(req->fd);
    } else if (get && strncmp(req->path, "/dl?", 4) == 0) {
        send_backup(req->fd, req->path);
    } else if (strcmp(req->method, "POST") == 0 && strncmp(req->path, "/upload", 7) == 0) {
        if (req->length < 0)
            web_send_json(req->fd, 411, false, "Content-Length");
        else
            receive_backup(req);
    } else {
        web_send_response(req->fd, 404, "text/plain", "Not found", 9);
    }
}

bool share_start(char *url, size_t url_size, char *err, size_t err_size) {
    share_stop();
    if (!lock)
        lock = SDL_CreateMutex();
    memset(&status, 0, sizeof(status));
    return web_start(handle_request, url, url_size, err, err_size);
}

void share_stop(void) {
    web_stop();
}

void share_get_status(ShareStatus *out) {
    if (!lock) {
        memset(out, 0, sizeof(*out));
        return;
    }
    SDL_LockMutex(lock);
    *out = status;
    SDL_UnlockMutex(lock);
}
