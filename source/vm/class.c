// Nạp & liên kết class file, lớp mảng, khởi tạo lớp (<clinit>)
#include "vm_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aot.h"

// ---------------------------------------------------------------------------
// Intern string

typedef struct InternEntry {
    struct InternEntry *next;
    uint32_t hash;
    size_t len;
    char str[];
} InternEntry;

#define INTERN_BUCKETS 4096
static InternEntry *intern_table[INTERN_BUCKETS];

static uint32_t str_hash(const char *s, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++)
        h = (h ^ (uint8_t)s[i]) * 16777619u;
    return h;
}

const char *intern(const char *s, size_t len) {
    uint32_t h = str_hash(s, len);
    InternEntry **bucket = &intern_table[h % INTERN_BUCKETS];
    for (InternEntry *e = *bucket; e; e = e->next) {
        if (e->hash == h && e->len == len && memcmp(e->str, s, len) == 0)
            return e->str;
    }
    InternEntry *e = malloc(sizeof(InternEntry) + len + 1);
    e->hash = h;
    e->len = len;
    memcpy(e->str, s, len);
    e->str[len] = '\0';
    e->next = *bucket;
    *bucket = e;
    return e->str;
}

const char *intern_cstr(const char *s) {
    return intern(s, strlen(s));
}

static void intern_free_all(void) {
    for (int i = 0; i < INTERN_BUCKETS; i++) {
        InternEntry *e = intern_table[i];
        while (e) {
            InternEntry *n = e->next;
            free(e);
            e = n;
        }
        intern_table[i] = NULL;
    }
}

// ---------------------------------------------------------------------------
// Bảng lớp đã nạp

#define CLASS_BUCKETS 1024
static Class *class_table[CLASS_BUCKETS];
static Class *cls_object, *cls_string, *cls_class;

static const char *S_init, *S_clinit, *S_void_desc;

static unsigned class_bucket(const char *interned) {
    return (unsigned)(((uintptr_t)interned >> 3) % CLASS_BUCKETS);
}

Class *class_find_loaded(const char *name) {
    name = intern_cstr(name);
    for (Class *c = class_table[class_bucket(name)]; c; c = c->hash_next) {
        if (c->name == name)
            return c;
    }
    return NULL;
}

static void class_register(Class *c) {
    unsigned b = class_bucket(c->name);
    c->hash_next = class_table[b];
    class_table[b] = c;
}

Class *class_object(void) { return cls_object; }
Class *class_string(void) { return cls_string; }
Class *class_class(void)  { return cls_class; }

// ---------------------------------------------------------------------------
// Descriptor

static int desc_arg_slots(const char *desc, char *ret_type) {
    int slots = 0;
    const char *p = desc + 1;
    while (*p && *p != ')') {
        if (*p == 'J' || *p == 'D') {
            slots += 2;
            p++;
        } else {
            slots++;
            while (*p == '[')
                p++;
            if (*p == 'L') {
                while (*p && *p != ';')
                    p++;
            }
            if (*p)
                p++;
        }
    }
    if (*p == ')')
        p++;
    if (ret_type)
        *ret_type = (*p == 'L' || *p == '[') ? 'L' : *p;
    return slots;
}

static bool desc_is_ref(const char *desc) {
    return desc[0] == 'L' || desc[0] == '[';
}

// ---------------------------------------------------------------------------
// Parse class file

typedef struct {
    const uint8_t *p, *end;
    bool error;
} Reader;

static uint8_t r_u1(Reader *r) {
    if (r->p + 1 > r->end) {
        r->error = true;
        return 0;
    }
    return *r->p++;
}

static uint16_t r_u2(Reader *r) {
    if (r->p + 2 > r->end) {
        r->error = true;
        return 0;
    }
    uint16_t v = (uint16_t)((r->p[0] << 8) | r->p[1]);
    r->p += 2;
    return v;
}

static uint32_t r_u4(Reader *r) {
    if (r->p + 4 > r->end) {
        r->error = true;
        return 0;
    }
    uint32_t v = ((uint32_t)r->p[0] << 24) | ((uint32_t)r->p[1] << 16) | ((uint32_t)r->p[2] << 8) | r->p[3];
    r->p += 4;
    return v;
}

static void r_skip(Reader *r, uint32_t n) {
    if (r->p + n > r->end) {
        r->error = true;
        r->p = r->end;
        return;
    }
    r->p += n;
}

static const char *cp_utf8(Class *c, uint16_t idx) {
    if (idx == 0 || idx >= c->cp_count || c->cp[idx].tag != CONST_Utf8)
        return NULL;
    return c->cp[idx].utf8;
}

const char *cp_class_name(Class *c, uint16_t idx) {
    if (idx == 0 || idx >= c->cp_count || c->cp[idx].tag != CONST_Class)
        return NULL;
    return cp_utf8(c, c->cp[idx].a);
}

static bool parse_code(Class *c, Method *m, Reader *r) {
    m->max_stack = r_u2(r);
    m->max_locals = r_u2(r);
    m->code_len = r_u4(r);
    if (r->error || m->code_len == 0 || r->p + m->code_len > r->end)
        return false;
    m->code = malloc(m->code_len);
    memcpy(m->code, r->p, m->code_len);
    r_skip(r, m->code_len);

    m->exc_count = r_u2(r);
    if (m->exc_count) {
        m->exc = calloc(m->exc_count, sizeof(ExceptionEntry));
        for (int i = 0; i < m->exc_count; i++) {
            m->exc[i].start_pc = r_u2(r);
            m->exc[i].end_pc = r_u2(r);
            m->exc[i].handler_pc = r_u2(r);
            m->exc[i].catch_type = r_u2(r);
        }
    }

    uint16_t attr_count = r_u2(r);
    for (int i = 0; i < attr_count && !r->error; i++) {
        const char *name = cp_utf8(c, r_u2(r));
        uint32_t len = r_u4(r);
        if (name && strcmp(name, "LineNumberTable") == 0 && !m->lines) {
            m->line_count = r_u2(r);
            m->lines = calloc(m->line_count ? m->line_count : 1, sizeof(LineEntry));
            for (int k = 0; k < m->line_count; k++) {
                m->lines[k].start_pc = r_u2(r);
                m->lines[k].line = r_u2(r);
            }
        } else {
            r_skip(r, len);
        }
    }
    return !r->error;
}

static Class *parse_class(const uint8_t *data, size_t size) {
    Reader r = { data, data + size, false };
    if (r_u4(&r) != 0xCAFEBABE)
        return NULL;
    r_u2(&r);   // minor
    r_u2(&r);   // major

    Class *c = calloc(1, sizeof(Class));
    c->cp_count = r_u2(&r);
    c->cp = calloc(c->cp_count ? c->cp_count : 1, sizeof(CPEntry));

    for (int i = 1; i < c->cp_count && !r.error; i++) {
        CPEntry *e = &c->cp[i];
        e->tag = r_u1(&r);
        switch (e->tag) {
        case CONST_Utf8: {
            uint16_t len = r_u2(&r);
            if (r.p + len > r.end) {
                r.error = true;
                break;
            }
            e->utf8 = intern((const char *)r.p, len);
            r_skip(&r, len);
            break;
        }
        case CONST_Integer:
            e->i = (jint)r_u4(&r);
            break;
        case CONST_Float: {
            uint32_t bits = r_u4(&r);
            memcpy(&e->f, &bits, 4);
            break;
        }
        case CONST_Long:
        case CONST_Double: {
            uint64_t hi = r_u4(&r), lo = r_u4(&r);
            uint64_t bits = (hi << 32) | lo;
            memcpy(&e->j, &bits, 8);
            i++;
            break;
        }
        case CONST_Class:
        case CONST_String:
            e->a = r_u2(&r);
            break;
        case CONST_Fieldref:
        case CONST_Methodref:
        case CONST_InterfaceMethodref:
        case CONST_NameAndType:
            e->a = r_u2(&r);
            e->b = r_u2(&r);
            break;
        default:
            r.error = true;
            break;
        }
    }
    if (r.error)
        goto fail;

    c->access = r_u2(&r);
    c->name = cp_class_name(c, r_u2(&r));
    uint16_t super_idx = r_u2(&r);
    const char *super_name = super_idx ? cp_class_name(c, super_idx) : NULL;
    if (!c->name)
        goto fail;

    c->iface_count = r_u2(&r);
    const char **iface_names = calloc(c->iface_count ? c->iface_count : 1, sizeof(char *));
    for (int i = 0; i < c->iface_count; i++)
        iface_names[i] = cp_class_name(c, r_u2(&r));

    c->field_count = r_u2(&r);
    c->fields = calloc(c->field_count ? c->field_count : 1, sizeof(Field));
    for (int i = 0; i < c->field_count && !r.error; i++) {
        Field *f = &c->fields[i];
        f->owner = c;
        f->access = r_u2(&r);
        f->name = cp_utf8(c, r_u2(&r));
        f->desc = cp_utf8(c, r_u2(&r));
        uint16_t ac = r_u2(&r);
        for (int k = 0; k < ac && !r.error; k++) {
            const char *an = cp_utf8(c, r_u2(&r));
            uint32_t len = r_u4(&r);
            if (an && strcmp(an, "ConstantValue") == 0 && len == 2)
                f->const_index = r_u2(&r);
            else
                r_skip(&r, len);
        }
        if (!f->name || !f->desc)
            r.error = true;
        else
            f->is_ref = desc_is_ref(f->desc);
    }

    c->method_count = r_u2(&r);
    c->methods = calloc(c->method_count ? c->method_count : 1, sizeof(Method));
    for (int i = 0; i < c->method_count && !r.error; i++) {
        Method *m = &c->methods[i];
        m->owner = c;
        m->access = r_u2(&r);
        m->name = cp_utf8(c, r_u2(&r));
        m->desc = cp_utf8(c, r_u2(&r));
        m->vtable_index = -1;
        if (!m->name || !m->desc) {
            r.error = true;
            break;
        }
        m->arg_slots = desc_arg_slots(m->desc, &m->ret_type) + ((m->access & ACC_STATIC) ? 0 : 1);
        uint16_t ac = r_u2(&r);
        for (int k = 0; k < ac && !r.error; k++) {
            const char *an = cp_utf8(c, r_u2(&r));
            uint32_t len = r_u4(&r);
            if (an && strcmp(an, "Code") == 0) {
                const uint8_t *start = r.p;
                if (!parse_code(c, m, &r)) {
                    r.error = true;
                    break;
                }
                r.p = start;
                r_skip(&r, len);
            } else {
                r_skip(&r, len);
            }
        }
    }

    uint16_t ac = r_u2(&r);
    for (int k = 0; k < ac && !r.error; k++) {
        const char *an = cp_utf8(c, r_u2(&r));
        uint32_t len = r_u4(&r);
        if (an && strcmp(an, "SourceFile") == 0 && len == 2)
            c->source_file = cp_utf8(c, r_u2(&r));
        else
            r_skip(&r, len);
    }

    if (r.error) {
        free(iface_names);
        goto fail;
    }

    // Lưu tạm tên lớp cha / interface vào con trỏ, link sẽ thay bằng Class*
    c->super = (Class *)super_name;
    c->interfaces = (Class **)iface_names;
    return c;

fail:
    vm_log("ClassFormatError khi doc class");
    free(c->cp);
    free(c->fields);
    if (c->methods) {
        for (int i = 0; i < c->method_count; i++) {
            free(c->methods[i].code);
            free(c->methods[i].exc);
            free(c->methods[i].lines);
        }
    }
    free(c->methods);
    free(c);
    return NULL;
}

// ---------------------------------------------------------------------------
// Link

static bool link_class(VMThread *t, Class *c) {
    const char *super_name = (const char *)c->super;
    const char **iface_names = (const char **)c->interfaces;
    c->super = NULL;
    c->interfaces = calloc(c->iface_count ? c->iface_count : 1, sizeof(Class *));

    if (super_name) {
        c->super = class_load(t, super_name);
        if (!c->super) {
            free(iface_names);
            return false;
        }
    }
    for (int i = 0; i < c->iface_count; i++) {
        c->interfaces[i] = iface_names[i] ? class_load(t, iface_names[i]) : NULL;
        if (!c->interfaces[i]) {
            free(iface_names);
            return false;
        }
    }
    free(iface_names);

    // Field instance & static
    int base = c->super ? c->super->instance_slots : 0;
    int inst = base, stat = 0;
    for (int i = 0; i < c->field_count; i++) {
        Field *f = &c->fields[i];
        f->slot = (f->access & ACC_STATIC) ? stat++ : inst++;
    }
    c->instance_slots = inst;
    c->slot_is_ref = calloc(inst ? inst : 1, 1);
    if (base)
        memcpy(c->slot_is_ref, c->super->slot_is_ref, base);
    for (int i = 0; i < c->field_count; i++) {
        Field *f = &c->fields[i];
        if (!(f->access & ACC_STATIC) && f->is_ref)
            c->slot_is_ref[f->slot] = 1;
    }
    c->static_count = stat;
    c->statics = calloc(stat ? stat : 1, sizeof(Value));

    // vtable
    int cap = (c->super ? c->super->vtable_len : 0) + c->method_count;
    c->vtable = calloc(cap ? cap : 1, sizeof(Method *));
    if (c->super) {
        memcpy(c->vtable, c->super->vtable, c->super->vtable_len * sizeof(Method *));
        c->vtable_len = c->super->vtable_len;
    }
    if (!(c->access & ACC_INTERFACE)) {
        for (int i = 0; i < c->method_count; i++) {
            Method *m = &c->methods[i];
            if ((m->access & (ACC_STATIC | ACC_PRIVATE)) || m->name == S_init || m->name == S_clinit)
                continue;
            int idx = -1;
            for (int k = 0; k < c->vtable_len; k++) {
                if (c->vtable[k]->name == m->name && c->vtable[k]->desc == m->desc) {
                    idx = k;
                    break;
                }
            }
            if (idx < 0)
                idx = c->vtable_len++;
            c->vtable[idx] = m;
            m->vtable_index = idx;
        }
    }

    // Native, và mã máy khi bật chế độ AOT
    for (int i = 0; i < c->method_count; i++) {
        Method *m = &c->methods[i];
        if (m->access & ACC_NATIVE)
            m->native = native_lookup(c->name, m->name, m->desc);
        else if (m->code && aot_active())
            aot_compile(c, m);
    }

    c->state = CLASS_LINKED;
    return true;
}

static Class *make_array_class(VMThread *t, const char *name) {
    Class *c = calloc(1, sizeof(Class));
    c->name = intern_cstr(name);
    c->is_array = true;
    c->access = ACC_PUBLIC | ACC_FINAL;
    c->elem_type = name[1];
    switch (c->elem_type) {
    case 'Z': case 'B': c->elem_size = 1; break;
    case 'C': case 'S': c->elem_size = 2; break;
    case 'I': case 'F': c->elem_size = 4; break;
    case 'J': case 'D': c->elem_size = 8; break;
    case 'L': case '[': c->elem_size = sizeof(Object *); break;
    default:
        free(c);
        return NULL;
    }

    if (c->elem_type == 'L') {
        size_t len = strlen(name);
        char *elem = malloc(len);
        memcpy(elem, name + 2, len - 3);
        elem[len - 3] = '\0';
        c->component = class_load(t, elem);
        free(elem);
        if (!c->component) {
            free(c);
            return NULL;
        }
    } else if (c->elem_type == '[') {
        c->component = class_load(t, name + 1);
        if (!c->component) {
            free(c);
            return NULL;
        }
    }

    c->super = cls_object;
    c->interfaces = calloc(1, sizeof(Class *));
    c->slot_is_ref = calloc(1, 1);
    c->statics = calloc(1, sizeof(Value));
    c->vtable = cls_object->vtable;     // dùng chung, không free
    c->vtable_len = cls_object->vtable_len;
    c->state = CLASS_INITIALIZED;
    class_register(c);
    if (c->component)
        c->component->array_class = c;
    return c;
}

Class *class_load(VMThread *t, const char *name) {
    Class *c = class_find_loaded(name);
    if (c)
        return c;

    if (name[0] == '[') {
        c = make_array_class(t, name);
        if (!c && t && !t->exception)
            throw_new(t, "java/lang/NoClassDefFoundError", name);
        return c;
    }

    size_t nlen = strlen(name);
    char *path = malloc(nlen + 7);
    memcpy(path, name, nlen);
    memcpy(path + nlen, ".class", 7);
    size_t size = 0;
    bool from_game = false;
    uint8_t *data = vm_host()->read_file(path, &size, &from_game);
    free(path);
    if (!data) {
        if (t && !t->exception)
            throw_new(t, "java/lang/NoClassDefFoundError", name);
        return NULL;
    }

    c = parse_class(data, size);
    free(data);
    if (!c || c->name != intern_cstr(name)) {
        if (c)
            vm_log("Ten lop khong khop: %s / %s", c->name, name);
        if (t && !t->exception)
            throw_new(t, "java/lang/NoClassDefFoundError", name);
        return NULL;
    }
    c->from_game = from_game;
    // Đăng ký trước khi link để lớp tự tham chiếu (vd field kiểu chính nó) không nạp lại
    class_register(c);

    if (!link_class(t, c)) {
        c->state = CLASS_ERROR;
        if (t && !t->exception)
            throw_new(t, "java/lang/NoClassDefFoundError", name);
        return NULL;
    }
    return c;
}

Class *class_array_of(VMThread *t, Class *component) {
    if (component->array_class)
        return component->array_class;
    size_t len = strlen(component->name);
    char *name = malloc(len + 4);
    if (component->is_array) {
        name[0] = '[';
        memcpy(name + 1, component->name, len + 1);
    } else {
        name[0] = '[';
        name[1] = 'L';
        memcpy(name + 2, component->name, len);
        name[len + 2] = ';';
        name[len + 3] = '\0';
    }
    Class *c = class_load(t, name);
    free(name);
    return c;
}

static Class *prim_array_cache[128];

Class *class_prim_array(char type) {
    if (!prim_array_cache[(int)type]) {
        char name[3] = { '[', type, '\0' };
        prim_array_cache[(int)type] = class_load(NULL, name);
    }
    return prim_array_cache[(int)type];
}

// ---------------------------------------------------------------------------
// Quan hệ kế thừa

bool class_is_subclass(const Class *sub, const Class *super) {
    for (const Class *c = sub; c; c = c->super) {
        if (c == super)
            return true;
    }
    return false;
}

static bool implements(const Class *c, const Class *iface) {
    for (; c; c = c->super) {
        for (int i = 0; i < c->iface_count; i++) {
            const Class *ic = c->interfaces[i];
            if (ic == iface || implements(ic, iface))
                return true;
        }
    }
    return false;
}

bool class_instance_of(const Class *c, const Class *target) {
    if (c == target || target == cls_object)
        return true;
    if (c->is_array) {
        if (!target->is_array)
            return false;   // Cloneable/Serializable không có trong CLDC
        if (c->elem_type == 'L' || c->elem_type == '[') {
            if (target->elem_type != 'L' && target->elem_type != '[')
                return false;
            return class_instance_of(c->component, target->component);
        }
        return c->elem_type == target->elem_type;
    }
    if (target->is_array)
        return false;
    if (target->access & ACC_INTERFACE)
        return implements(c, target);
    return class_is_subclass(c, target);
}

// ---------------------------------------------------------------------------
// Tìm field / method

Field *class_find_field(Class *c, const char *name, const char *desc) {
    name = intern_cstr(name);
    desc = desc ? intern_cstr(desc) : NULL;
    for (; c; c = c->super) {
        for (int i = 0; i < c->field_count; i++) {
            Field *f = &c->fields[i];
            if (f->name == name && (!desc || f->desc == desc))
                return f;
        }
        for (int i = 0; i < c->iface_count; i++) {
            Field *f = class_find_field(c->interfaces[i], name, desc);
            if (f)
                return f;
        }
    }
    return NULL;
}

Method *class_find_declared_method(Class *c, const char *name, const char *desc) {
    for (int i = 0; i < c->method_count; i++) {
        Method *m = &c->methods[i];
        if (m->name == name && m->desc == desc)
            return m;
    }
    return NULL;
}

static Method *find_in_interfaces(Class *c, const char *name, const char *desc) {
    for (; c; c = c->super) {
        for (int i = 0; i < c->iface_count; i++) {
            Method *m = class_find_declared_method(c->interfaces[i], name, desc);
            if (!m)
                m = find_in_interfaces(c->interfaces[i], name, desc);
            if (m)
                return m;
        }
    }
    return NULL;
}

Method *class_find_method(Class *c, const char *name, const char *desc) {
    name = intern_cstr(name);
    desc = intern_cstr(desc);
    for (Class *k = c; k; k = k->super) {
        Method *m = class_find_declared_method(k, name, desc);
        if (m)
            return m;
    }
    return find_in_interfaces(c, name, desc);
}

Method *class_find_interface_method(Class *c, const char *name, const char *desc) {
    for (; c; c = c->super) {
        Method *m = class_find_declared_method(c, name, desc);
        if (m && !(m->access & ACC_ABSTRACT) && !(m->access & ACC_STATIC))
            return m;
    }
    return NULL;
}

int method_line(const Method *m, const uint8_t *pc) {
    if (!m->lines || !m->code || !pc)
        return -1;
    int off = (int)(pc - m->code);
    int line = -1;
    for (int i = 0; i < m->line_count; i++) {
        if (m->lines[i].start_pc <= off)
            line = m->lines[i].line;
    }
    return line;
}

// ---------------------------------------------------------------------------
// Khởi tạo lớp

static void init_constants(VMThread *t, Class *c) {
    for (int i = 0; i < c->field_count; i++) {
        Field *f = &c->fields[i];
        if (!(f->access & ACC_STATIC) || !f->const_index || f->const_index >= c->cp_count)
            continue;
        CPEntry *e = &c->cp[f->const_index];
        Value *v = &c->statics[f->slot];
        switch (e->tag) {
        case CONST_Integer: v->i = e->i; break;
        case CONST_Float:   v->f = e->f; break;
        case CONST_Long:    v->j = e->j; break;
        case CONST_Double:  v->d = e->d; break;
        case CONST_String:
            v->l = jstring_intern_utf8(t, cp_utf8(c, e->a));
            break;
        default:
            break;
        }
    }
}

bool class_ensure_init(VMThread *t, Class *c) {
    if (c->state == CLASS_INITIALIZED)
        return true;
    if (c->state == CLASS_ERROR) {
        throw_new(t, "java/lang/NoClassDefFoundError", c->name);
        return false;
    }
    if (c->state == CLASS_INITIALIZING) {
        if (c->init_thread == t)
            return true;
        t->state = TS_WAIT_INIT;
        return false;
    }

    c->state = CLASS_INITIALIZING;
    c->init_thread = t;
    init_constants(t, c);

    Method *clinit = class_find_declared_method(c, S_clinit, S_void_desc);
    if (clinit) {
        if (!thread_push_frame(t, clinit, NULL)) {
            c->state = CLASS_ERROR;
            return false;
        }
        t->frames[t->frame_count - 1].clinit_of = c;
    }

    bool super_ready = true;
    if (c->super && !(c->access & ACC_INTERFACE))
        super_ready = class_ensure_init(t, c->super);

    if (!clinit) {
        c->state = CLASS_INITIALIZED;
        c->init_thread = NULL;
        return super_ready;
    }
    return false;
}

Object *class_mirror(VMThread *t, Class *c) {
    if (!c->mirror) {
        Object *m = heap_alloc_object(t, cls_class);
        if (!m)
            return NULL;
        OBJ_FIELDS(m)[FS_Class_vmClass].j = (jlong)(intptr_t)c;
        c->mirror = m;
    }
    return c->mirror;
}

Class *class_from_mirror(Object *mirror) {
    if (!mirror)
        return NULL;
    return (Class *)(intptr_t)OBJ_FIELDS(mirror)[FS_Class_vmClass].j;
}

void class_mark_roots(MarkFn mark) {
    for (int b = 0; b < CLASS_BUCKETS; b++) {
        for (Class *c = class_table[b]; c; c = c->hash_next) {
            mark(c->mirror);
            if (c->is_array)
                continue;
            for (int i = 0; i < c->field_count; i++) {
                Field *f = &c->fields[i];
                if ((f->access & ACC_STATIC) && f->is_ref)
                    mark(c->statics[f->slot].l);
            }
            for (int i = 1; i < c->cp_count; i++) {
                if (c->cp[i].tag == CONST_String && c->cp[i].resolved)
                    mark(c->cp[i].str);
            }
        }
    }
}

static void class_free(Class *c) {
    if (!c->is_array) {
        for (int i = 0; i < c->method_count; i++) {
            aot_free_method(&c->methods[i]);
            free(c->methods[i].code);
            free(c->methods[i].exc);
            free(c->methods[i].lines);
        }
        free(c->vtable);
    }
    free(c->methods);
    free(c->fields);
    free(c->cp);
    free(c->interfaces);
    free(c->slot_is_ref);
    free(c->statics);
    free(c);
}

void class_free_all(void) {
    for (int b = 0; b < CLASS_BUCKETS; b++) {
        Class *c = class_table[b];
        while (c) {
            Class *n = c->hash_next;
            class_free(c);
            c = n;
        }
        class_table[b] = NULL;
    }
    cls_object = cls_string = cls_class = NULL;
    memset(prim_array_cache, 0, sizeof(prim_array_cache));
    intern_free_all();
}

bool class_bootstrap(void) {
    S_init = intern_cstr("<init>");
    S_clinit = intern_cstr("<clinit>");
    S_void_desc = intern_cstr("()V");

    cls_object = class_load(NULL, "java/lang/Object");
    cls_class = class_load(NULL, "java/lang/Class");
    cls_string = class_load(NULL, "java/lang/String");
    return cls_object && cls_class && cls_string;
}
