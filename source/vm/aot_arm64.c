// Chế độ AOT: dịch bytecode của 1 method sang mã máy ARM64 (xem aot.h)
//
// Thanh ghi cố định trong mã sinh ra (đều callee-saved nên gọi hàm C không cần cất):
//   x19 = VMThread *t   x20 = Frame *f   x21 = locals   x22 = đáy operand stack
//   x23 = int *budget   x24 = m->code
// Tạm: x0-x3 (tham số gọi hàm C), x9-x16, s0/s1, d0/d1.
// Hàm: int fn(t, f, budget, entry): cất thanh ghi, nạp x21-x24 rồi nhảy tới entry.
#include "vm_internal.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "aot.h"
#include "opcodes.h"

#if defined(__aarch64__) && (defined(__SWITCH__) || defined(__APPLE__) || defined(__linux__))
#define AOT_ARM64 1
#endif

static bool active;
static int compiled, skipped;

bool aot_available(void) {
#ifdef AOT_ARM64
    return true;
#else
    return false;
#endif
}

bool aot_start(void) {
    active = aot_available() && aot_mem_init();
    aot_mem_reset();
    compiled = skipped = 0;
    return active;
}

void aot_stop(void) {
    if (active)
        vm_log("AOT: %d method dich, %d bo qua, %zuK ma may", compiled, skipped, aot_mem_used() / 1024);
    active = false;
    aot_mem_reset();
}

bool aot_active(void) {
    return active;
}

void aot_free_method(Method *m) {
    free(m->aot_entry);
    free(m->aot_sites);
    m->aot_entry = NULL;
    m->aot_sites = NULL;
    m->aot_code = NULL;
}

#ifndef AOT_ARM64
void aot_compile(Class *c, Method *m) {
    (void)c;
    (void)m;
}
#else

// ---------------------------------------------------------------------------
// Hàm C mà mã máy gọi

// Cache tại 1 lệnh gọi: method đích (NULL = chưa có, đi đường chậm) và với gọi ảo / interface là
// lớp của object lần trước. Đường chậm (rt_invoke) ghi lại, mã máy dùng để gọi thẳng.
typedef struct {
    Method *target;
    Class *cls;
} AotSite;

// Gọi method từ mã máy (invoke đã được trình thông dịch viết đè thành lệnh nhanh, method thường,
// không native / synchronized): đẩy frame như trình thông dịch, method có mã máy thì gọi thẳng.
//   RT_INVOKE_DONE: method đã return, giá trị trả về ở stack caller, chạy tiếp lệnh sau
//   RT_INVOKE_STEP: chưa làm gì, để trình thông dịch chạy lệnh invoke
//   AOT_STEP / AOT_YIELD: frame mới (hoặc sâu hơn) đang chờ trình thông dịch, trả nguyên mã này
enum { RT_INVOKE_STEP = -1, RT_INVOKE_DONE = 100 };
#define AOT_MAX_NEST 32         // số tầng mã máy gọi lồng trên stack C

static int nest;

// Gọi native, xử lý kết quả giống trình thông dịch
static int rt_native(VMThread *t, Frame *f, Method *target, uint8_t *insn, int len, Value *sp) {
    Value *args = sp - target->arg_slots;
    Value ret;
    ret.j = 0;
    f->pc = insn;
    f->sp = sp;
    int fc = t->frame_count;
    switch (target->native(t, args, &ret)) {
    case NATIVE_OK:
        if (target->ret_type == 'J' || target->ret_type == 'D') {
            args[0] = ret;
            f->sp = args + 2;
        } else if (target->ret_type != 'V') {
            args[0] = ret;
            f->sp = args + 1;
        } else {
            f->sp = args;
        }
        f->pc = insn + len;
        // thread bị block / chờ: trả quyền cho scheduler, chạy tiếp ở lệnh sau
        return t->state == TS_RUNNABLE ? RT_INVOKE_DONE : AOT_YIELD;
    case NATIVE_EXCEPTION:
        return AOT_EXCEPTION;
    case NATIVE_RETRY:
        // Chạy lại lệnh gọi sau (đã đẩy frame <clinit> / phải chờ)
        if (t->frame_count != fc)
            f->retry = 1;
        if (t->exception)
            return AOT_EXCEPTION;
        return t->state == TS_RUNNABLE ? AOT_STEP : AOT_YIELD;
    case NATIVE_INVOKE:
        // Native nhờ gọi method Java, kết quả của nó là kết quả lệnh gọi
        f->retry = 0;
        f->sp = args;
        f->pc = insn + len;
        if (!thread_push_frame(t, t->invoke_method, t->invoke_args)) {
            f->pc = insn;
            return AOT_EXCEPTION;
        }
        return AOT_STEP;
    }
    return AOT_STEP;
}

static int rt_invoke(VMThread *t, Frame *f, int pc, int d, int *budget, AotSite *site) {
    Method *m = f->m;
    uint8_t *insn = m->code + pc;
    CPEntry *e = &m->owner->cp[(insn[1] << 8) | insn[2]];
    Value *sp = f->stack_base + d;
    Method *target;
    int len = 3;
    switch (insn[0]) {
    case OP_INVOKEVIRTUAL_Q: {
        Method *rm = e->method;
        Object *o = sp[-rm->arg_slots].l;
        if (!o)
            return RT_INVOKE_STEP;
        target = o->cls->vtable[rm->vtable_index];
        break;
    }
    case OP_INVOKESPECIAL_Q:
        target = e->method;
        if (!sp[-target->arg_slots].l)
            return RT_INVOKE_STEP;
        break;
    case OP_INVOKESTATIC_Q:
        target = e->method;
        break;
    case OP_INVOKEINTERFACE: {
        if (!e->resolved)
            return RT_INVOKE_STEP;
        Object *o = sp[-e->method->arg_slots].l;
        if (!o || !(target = interp_find_virtual(e, o->cls, e->method)))
            return RT_INVOKE_STEP;
        len = 5;
        break;
    }
    default:
        return RT_INVOKE_STEP;
    }
    if (target->access & (ACC_SYNCHRONIZED | ACC_ABSTRACT))
        return RT_INVOKE_STEP;
    if (target->access & ACC_NATIVE)
        return target->native ? rt_native(t, f, target, insn, len, sp) : RT_INVOKE_STEP;
    site->target = target;
    site->cls = insn[0] == OP_INVOKEVIRTUAL_Q || insn[0] == OP_INVOKEINTERFACE ? sp[-target->arg_slots].l->cls : NULL;
    int nargs = target->arg_slots;
    Value *nl = sp - nargs;
    int nlocals = target->max_locals > nargs ? target->max_locals : nargs;
    if (t->frame_count >= THREAD_MAX_FRAMES || nl + nlocals + target->max_stack + 4 > t->stack + THREAD_STACK_SLOTS)
        return RT_INVOKE_STEP;      // trình thông dịch ném StackOverflowError

    f->retry = 0;
    f->pc = insn + len;
    f->sp = nl;
    for (Value *v = nl + nargs; v < nl + nlocals; v++)
        v->j = 0;
    Frame *nf = &t->frames[t->frame_count++];
    nf->m = target;
    nf->pc = target->code;
    nf->locals = nl;
    nf->stack_base = nf->sp = nl + nlocals;
    nf->clinit_of = NULL;
    nf->sync_obj = NULL;
    nf->retry = 0;
    if (--*budget <= 0)
        return AOT_YIELD;
    if (!target->aot_code || nest >= AOT_MAX_NEST)
        return AOT_STEP;            // trình thông dịch chạy tiếp từ frame mới
    nest++;
    int r = ((AotCall)target->aot_code)(t, nf, budget, (uint8_t *)target->aot_code + target->aot_entry[0]);
    nest--;
    return r == AOT_RETURN ? RT_INVOKE_DONE : r;
}

static int rt_instance_of(Class *c, Class *target) {
    return class_instance_of(c, target) ? 1 : 0;
}

// aastore: kiểu phần tử hợp lệ
static int rt_store_ok(Object *a, Object *v) {
    return !v || !a->cls->component || class_instance_of(v->cls, a->cls->component);
}

static float rt_fmodf(float a, float b) {
    return fmodf(a, b);
}

static double rt_fmod(double a, double b) {
    return fmod(a, b);
}

// ---------------------------------------------------------------------------
// Phân tích: độ dài lệnh, độ sâu stack tại mỗi lệnh

#define U2(p) ((uint16_t)(((p)[0] << 8) | (p)[1]))
#define S2(p) ((int16_t)U2(p))
#define S4(p) ((int32_t)(((uint32_t)(p)[0] << 24) | ((uint32_t)(p)[1] << 16) | ((uint32_t)(p)[2] << 8) | (uint32_t)(p)[3]))

static int pad4(int pc) {
    return (4 - (pc + 1) % 4) % 4;
}

static int insn_len(const uint8_t *code, int pc, int len) {
    int op = code[pc];
    if (op == OP_TABLESWITCH) {
        int p = pc + 1 + pad4(pc);
        if (p + 12 > len)
            return -1;
        int64_t n = (int64_t)S4(code + p + 8) - S4(code + p + 4) + 1;
        if (n < 0 || n > 65536)
            return -1;
        return 1 + pad4(pc) + 12 + 4 * (int)n;
    }
    if (op == OP_LOOKUPSWITCH) {
        int p = pc + 1 + pad4(pc);
        if (p + 8 > len)
            return -1;
        int32_t n = S4(code + p + 4);
        if (n < 0 || n > 65536)
            return -1;
        return 1 + pad4(pc) + 8 + 8 * n;
    }
    if (op == OP_WIDE)
        return pc + 1 < len && code[pc + 1] == OP_IINC ? 6 : 4;
    if (op <= 0x0f) return 1;
    if (op == OP_BIPUSH || op == OP_LDC) return 2;
    if (op == OP_SIPUSH || op == OP_LDC_W || op == OP_LDC2_W) return 3;
    if (op <= 0x19) return 2;
    if (op <= 0x35) return 1;
    if (op <= 0x3a) return 2;
    if (op <= 0x83) return 1;
    if (op == OP_IINC) return 3;
    if (op <= 0x98) return 1;
    if (op <= 0xa8) return 3;
    if (op == OP_RET) return 2;
    if (op <= 0xb1) return 1;
    if (op <= 0xb8) return 3;
    if (op == OP_INVOKEINTERFACE || op == OP_INVOKEDYNAMIC) return 5;
    if (op == OP_NEW || op == OP_ANEWARRAY || op == OP_CHECKCAST || op == OP_INSTANCEOF ||
        op == OP_IFNULL || op == OP_IFNONNULL) return 3;
    if (op == OP_NEWARRAY) return 2;
    if (op == OP_ARRAYLENGTH || op == OP_ATHROW || op == OP_MONITORENTER || op == OP_MONITOREXIT) return 1;
    if (op == OP_MULTIANEWARRAY) return 4;
    if (op == OP_GOTO_W || op == OP_JSR_W) return 5;
    return -1;
}

static int type_slots(char t) {
    return t == 'V' ? 0 : (t == 'J' || t == 'D') ? 2 : 1;
}

static int desc_arg_slots(const char *d) {
    int n = 0;
    for (d++; *d && *d != ')'; d++) {
        if (*d == 'L') {
            while (*d && *d != ';')
                d++;
            n++;
        } else if (*d == '[') {
            while (*d == '[')
                d++;
            if (*d == 'L')
                while (*d && *d != ';')
                    d++;
            n++;
        } else {
            n += type_slots(*d);
        }
    }
    return n;
}

static int desc_ret_slots(const char *d) {
    const char *p = strchr(d, ')');
    return p ? type_slots(p[1]) : 0;
}

// Descriptor của Fieldref / Methodref
static const char *ref_desc(Class *c, uint16_t idx) {
    if (idx == 0 || idx >= c->cp_count)
        return NULL;
    CPEntry *e = &c->cp[idx];
    if (e->b == 0 || e->b >= c->cp_count)
        return NULL;
    CPEntry *nat = &c->cp[e->b];
    if (nat->b == 0 || nat->b >= c->cp_count || c->cp[nat->b].tag != CONST_Utf8)
        return NULL;
    return c->cp[nat->b].utf8;
}

static const char *ref_name(Class *c, uint16_t idx) {
    CPEntry *nat = &c->cp[c->cp[idx].b];
    return c->cp[nat->a].utf8;
}

// Độ sâu sau lệnh (lệnh không kết thúc luồng), -1 nếu không dịch được
static int stack_after(Class *c, const uint8_t *code, int pc, int d) {
    int op = code[pc];
    const char *desc;
    switch (op) {
    case OP_NOP: return d;
    case OP_ACONST_NULL: case OP_ICONST_M1: case OP_ICONST_0: case OP_ICONST_1: case OP_ICONST_2:
    case OP_ICONST_3: case OP_ICONST_4: case OP_ICONST_5: return d + 1;
    case OP_LCONST_0: case OP_LCONST_1: return d + 2;
    case OP_FCONST_0: case OP_FCONST_1: case OP_FCONST_2: return d + 1;
    case OP_DCONST_0: case OP_DCONST_1: return d + 2;
    case OP_BIPUSH: case OP_SIPUSH: case OP_LDC: case OP_LDC_W: return d + 1;
    case OP_LDC2_W: return d + 2;
    case OP_ILOAD: case OP_FLOAD: case OP_ALOAD: return d + 1;
    case OP_LLOAD: case OP_DLOAD: return d + 2;
    case OP_IALOAD: case OP_FALOAD: case OP_AALOAD: case OP_BALOAD: case OP_CALOAD: case OP_SALOAD: return d - 1;
    case OP_LALOAD: case OP_DALOAD: return d;
    case OP_ISTORE: case OP_FSTORE: case OP_ASTORE: return d - 1;
    case OP_LSTORE: case OP_DSTORE: return d - 2;
    case OP_IASTORE: case OP_FASTORE: case OP_AASTORE: case OP_BASTORE: case OP_CASTORE: case OP_SASTORE: return d - 3;
    case OP_LASTORE: case OP_DASTORE: return d - 4;
    case OP_POP: return d - 1;
    case OP_POP2: return d - 2;
    case OP_DUP: case OP_DUP_X1: case OP_DUP_X2: return d + 1;
    case OP_DUP2: case OP_DUP2_X1: case OP_DUP2_X2: return d + 2;
    case OP_SWAP: return d;
    case OP_INEG: case OP_LNEG: case OP_FNEG: case OP_DNEG: return d;
    case OP_ISHL: case OP_LSHL: case OP_ISHR: case OP_LSHR: case OP_IUSHR: case OP_LUSHR: return d - 1;
    case OP_IAND: case OP_IOR: case OP_IXOR: return d - 1;
    case OP_LAND: case OP_LOR: case OP_LXOR: return d - 2;
    case OP_IINC: return d;
    case OP_I2L: return d + 1;
    case OP_I2F: return d;
    case OP_I2D: return d + 1;
    case OP_L2I: case OP_L2F: return d - 1;
    case OP_L2D: case OP_F2I: return d;
    case OP_F2L: case OP_F2D: return d + 1;
    case OP_D2I: return d - 1;
    case OP_D2L: return d;
    case OP_D2F: return d - 1;
    case OP_I2B: case OP_I2C: case OP_I2S: return d;
    case OP_LCMP: return d - 3;
    case OP_FCMPL: case OP_FCMPG: return d - 1;
    case OP_DCMPL: case OP_DCMPG: return d - 3;
    case OP_IFEQ: case OP_IFNE: case OP_IFLT: case OP_IFGE: case OP_IFGT: case OP_IFLE:
    case OP_IFNULL: case OP_IFNONNULL: return d - 1;
    case OP_IF_ICMPEQ: case OP_IF_ICMPNE: case OP_IF_ICMPLT: case OP_IF_ICMPGE: case OP_IF_ICMPGT:
    case OP_IF_ICMPLE: case OP_IF_ACMPEQ: case OP_IF_ACMPNE: return d - 2;
    case OP_GOTO: case OP_GOTO_W: return d;
    case OP_TABLESWITCH: case OP_LOOKUPSWITCH: return d - 1;
    case OP_GETSTATIC: case OP_PUTSTATIC: case OP_GETFIELD: case OP_PUTFIELD: {
        if (!(desc = ref_desc(c, U2(code + pc + 1))))
            return -1;
        int n = type_slots(desc[0]);
        return op == OP_GETSTATIC ? d + n : op == OP_PUTSTATIC ? d - n : op == OP_GETFIELD ? d - 1 + n : d - 1 - n;
    }
    case OP_INVOKEVIRTUAL: case OP_INVOKESPECIAL: case OP_INVOKESTATIC: case OP_INVOKEINTERFACE:
        if (!(desc = ref_desc(c, U2(code + pc + 1))))
            return -1;
        return d - desc_arg_slots(desc) - (op == OP_INVOKESTATIC ? 0 : 1) + desc_ret_slots(desc);
    case OP_NEW: return d + 1;
    case OP_NEWARRAY: case OP_ANEWARRAY: case OP_ARRAYLENGTH: case OP_CHECKCAST: case OP_INSTANCEOF: return d;
    case OP_MONITORENTER: case OP_MONITOREXIT: return d - 1;
    case OP_MULTIANEWARRAY: return d - code[pc + 3] + 1;
    case OP_WIDE: {
        int w = code[pc + 1];
        if (w == OP_IINC) return d;
        if (w == OP_ILOAD || w == OP_FLOAD || w == OP_ALOAD) return d + 1;
        if (w == OP_LLOAD || w == OP_DLOAD) return d + 2;
        if (w == OP_ISTORE || w == OP_FSTORE || w == OP_ASTORE) return d - 1;
        if (w == OP_LSTORE || w == OP_DSTORE) return d - 2;
        return -1;
    }
    }
    if (op >= OP_ILOAD_0 && op <= OP_ALOAD_3)
        return (op >= OP_LLOAD_0 && op <= OP_LLOAD_3) || (op >= OP_DLOAD_0 && op <= OP_DLOAD_3) ? d + 2 : d + 1;
    if (op >= OP_ISTORE_0 && op <= OP_ASTORE_3)
        return (op >= OP_LSTORE_0 && op <= OP_LSTORE_3) || (op >= OP_DSTORE_0 && op <= OP_DSTORE_3) ? d - 2 : d - 1;
    if (op >= OP_IADD && op <= OP_DREM)     // i / f: -1, l / d: -2
        return ((op - OP_IADD) & 1) ? d - 2 : d - 1;
    return -1;
}

static bool ends_flow(int op) {
    return op == OP_GOTO || op == OP_GOTO_W || op == OP_TABLESWITCH || op == OP_LOOKUPSWITCH ||
           (op >= OP_IRETURN && op <= OP_RETURN) || op == OP_ATHROW;
}

// Đích nhảy của lệnh nhánh, trả về số đích
static int branch_targets(const uint8_t *code, int pc, int *out, int max) {
    int op = code[pc];
    if ((op >= OP_IFEQ && op <= OP_GOTO) || op == OP_IFNULL || op == OP_IFNONNULL) {
        out[0] = pc + S2(code + pc + 1);
        return 1;
    }
    if (op == OP_GOTO_W) {
        out[0] = pc + S4(code + pc + 1);
        return 1;
    }
    int p = pc + 1 + pad4(pc);
    if (op == OP_TABLESWITCH) {
        int lo = S4(code + p + 4), hi = S4(code + p + 8), n = 0;
        out[n++] = pc + S4(code + p);
        for (int64_t k = 0; k <= (int64_t)hi - lo && n < max; k++)
            out[n++] = pc + S4(code + p + 12 + 4 * (int)k);
        return n;
    }
    if (op == OP_LOOKUPSWITCH) {
        int cnt = S4(code + p + 4), n = 0;
        out[n++] = pc + S4(code + p);
        for (int k = 0; k < cnt && n < max; k++)
            out[n++] = pc + S4(code + p + 12 + 8 * k);
        return n;
    }
    return 0;
}

typedef struct {
    Class *c;
    Method *m;
    const uint8_t *code;
    int len;
    int *ilen;          // độ dài lệnh tại điểm bắt đầu, 0 nếu không phải
    int *depth;         // độ sâu stack trước lệnh, -1 nếu không tới được
} Ana;

static bool analyze(Ana *an) {
    const uint8_t *code = an->code;
    int len = an->len;
    for (int pc = 0; pc < len;) {
        int op = code[pc];
        if (op == OP_JSR || op == OP_RET || op == OP_JSR_W || op == OP_INVOKEDYNAMIC || op > OP_JSR_W)
            return false;
        int l = insn_len(code, pc, len);
        if (l <= 0 || pc + l > len)
            return false;
        if (op == OP_WIDE && code[pc + 1] == OP_RET)
            return false;
        an->ilen[pc] = l;
        pc += l;
    }
    for (int i = 0; i < len; i++)
        an->depth[i] = -1;

    int cap = 64, n = 0;
    int *work = malloc(sizeof(int) * 2 * cap);
    int *tg = malloc(sizeof(int) * 65538);
    bool ok = work && tg;
#define PUSH(p, dd)                                                     \
    do {                                                                \
        if (n == cap) {                                                 \
            int *w2 = realloc(work, sizeof(int) * 4 * cap);             \
            if (!w2) { ok = false; break; }                             \
            work = w2;                                                  \
            cap *= 2;                                                   \
        }                                                               \
        work[n * 2] = (p);                                              \
        work[n * 2 + 1] = (dd);                                         \
        n++;                                                            \
    } while (0)
    if (ok) {
        PUSH(0, 0);
        for (int i = 0; i < an->m->exc_count; i++)
            PUSH(an->m->exc[i].handler_pc, 1);
    }
    while (ok && n > 0) {
        n--;
        int pc = work[n * 2], d = work[n * 2 + 1];
        if (pc < 0 || pc >= len || !an->ilen[pc] || d < 0 || d > an->m->max_stack) {
            ok = false;
            break;
        }
        if (an->depth[pc] >= 0) {
            if (an->depth[pc] != d)
                ok = false;
            continue;
        }
        an->depth[pc] = d;
        int op = code[pc];
        if ((op >= OP_IRETURN && op <= OP_RETURN) || op == OP_ATHROW)
            continue;
        int nd = stack_after(an->c, code, pc, d);
        if (nd < 0 || nd > an->m->max_stack) {
            ok = false;
            break;
        }
        int nt = branch_targets(code, pc, tg, 65538);
        for (int k = 0; k < nt && ok; k++)
            PUSH(tg[k], nd);
        if (!ends_flow(op))
            PUSH(pc + an->ilen[pc], nd);
    }
#undef PUSH
    free(work);
    free(tg);
    return ok;
}

// ---------------------------------------------------------------------------
// Sinh mã ARM64

enum { X0 = 0, X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17,
       RT = 19, RF, RL, RS, RB, RC, FP = 29, LR = 30, ZR = 31, SPR = 31 };
enum { C_EQ, C_NE, C_HS, C_LO, C_MI, C_PL, C_VS, C_VC, C_HI, C_LS, C_GE, C_LT, C_GT, C_LE };

typedef struct {
    int label, pc, depth, kind;     // kind: 0 step, 1 yield, 2 nhánh lùi
} Stub;

typedef struct {
    int at, label;
    bool b26;                       // b / bl (imm26), ngược lại imm19 (b.cond, cbz, cbnz)
} Fix;

typedef struct {
    Ana *an;
    uint32_t *buf;
    int n, cap;
    int *lpos;                      // nhãn -> vị trí (word), -1 chưa đặt. 0..len-1 là lệnh bytecode
    int nl, lcap;
    Fix *fix;
    int nfix, fixcap;
    Stub *stub;
    int nstub, stubcap;
    int *step_lbl, *back_lbl, *yield_lbl;      // theo pc, -1 chưa có
    int lbl_step, lbl_yield, lbl_ret;
    int lbl_step_top, lbl_yield_top;            // trả AOT_STEP / AOT_YIELD, frame đỉnh đã tự ghi
    AotSite *sites;
    int nsites;
    bool fail;
} Gen;

static void emit(Gen *g, uint32_t w) {
    if (g->n == g->cap) {
        int cap = g->cap ? g->cap * 2 : 1024;
        uint32_t *b = realloc(g->buf, sizeof(uint32_t) * cap);
        if (!b) {
            g->fail = true;
            return;
        }
        g->buf = b;
        g->cap = cap;
    }
    g->buf[g->n++] = w;
}

static int new_label(Gen *g) {
    if (g->nl == g->lcap) {
        int cap = g->lcap * 2;
        int *p = realloc(g->lpos, sizeof(int) * cap);
        if (!p) {
            g->fail = true;
            return 0;
        }
        g->lpos = p;
        g->lcap = cap;
    }
    g->lpos[g->nl] = -1;
    return g->nl++;
}

static void bind(Gen *g, int label) {
    g->lpos[label] = g->n;
}

static void add_fix(Gen *g, int label, bool b26) {
    if (g->nfix == g->fixcap) {
        int cap = g->fixcap ? g->fixcap * 2 : 256;
        Fix *f = realloc(g->fix, sizeof(Fix) * cap);
        if (!f) {
            g->fail = true;
            return;
        }
        g->fix = f;
        g->fixcap = cap;
    }
    g->fix[g->nfix++] = (Fix){ g->n, label, b26 };
}

static void b_(Gen *g, int label) {
    add_fix(g, label, true);
    emit(g, 0x14000000);
}

static void bcond(Gen *g, int cond, int label) {
    add_fix(g, label, false);
    emit(g, 0x54000000 | (uint32_t)cond);
}

static void cbz_w(Gen *g, int rt, int label) {
    add_fix(g, label, false);
    emit(g, 0x34000000 | (uint32_t)rt);
}

static void cbnz_w(Gen *g, int rt, int label) {
    add_fix(g, label, false);
    emit(g, 0x35000000 | (uint32_t)rt);
}

static void cbz_x(Gen *g, int rt, int label) {
    add_fix(g, label, false);
    emit(g, 0xB4000000 | (uint32_t)rt);
}

static void cbnz_x(Gen *g, int rt, int label) {
    add_fix(g, label, false);
    emit(g, 0xB5000000 | (uint32_t)rt);
}

static void mov_w(Gen *g, int rd, uint32_t v) {
    emit(g, 0x52800000 | ((v & 0xffff) << 5) | (uint32_t)rd);
    if (v >> 16)
        emit(g, 0x72A00000 | ((v >> 16) << 5) | (uint32_t)rd);
}

static void mov_x(Gen *g, int rd, uint64_t v) {
    emit(g, 0xD2800000 | (uint32_t)((v & 0xffff) << 5) | (uint32_t)rd);
    for (int hw = 1; hw < 4; hw++) {
        uint32_t part = (uint32_t)(v >> (16 * hw)) & 0xffff;
        if (part)
            emit(g, 0xF2800000 | ((uint32_t)hw << 21) | (part << 5) | (uint32_t)rd);
    }
}

static void mov_ptr(Gen *g, int rd, const void *p) {
    mov_x(g, rd, (uint64_t)(uintptr_t)p);
}

// Nạp / ghi với offset dương không dấu, đã nhân theo kích thước
static void ldst(Gen *g, uint32_t op, int scale, int rt, int rn, long off) {
    if (off < 0 || (off & ((1 << scale) - 1)) || (off >> scale) > 4095) {
        g->fail = true;
        return;
    }
    emit(g, op | ((uint32_t)(off >> scale) << 10) | ((uint32_t)rn << 5) | (uint32_t)rt);
}

#define LDR_X(rt, rn, off)  ldst(g, 0xF9400000, 3, rt, rn, off)
#define STR_X(rt, rn, off)  ldst(g, 0xF9000000, 3, rt, rn, off)
#define LDR_W(rt, rn, off)  ldst(g, 0xB9400000, 2, rt, rn, off)
#define STR_W(rt, rn, off)  ldst(g, 0xB9000000, 2, rt, rn, off)
#define LDRSW(rt, rn, off)  ldst(g, 0xB9800000, 2, rt, rn, off)
#define LDRB(rt, rn, off)   ldst(g, 0x39400000, 0, rt, rn, off)
#define LDR_S(rt, rn, off)  ldst(g, 0xBD400000, 2, rt, rn, off)
#define STR_S(rt, rn, off)  ldst(g, 0xBD000000, 2, rt, rn, off)
#define LDR_D(rt, rn, off)  ldst(g, 0xFD400000, 3, rt, rn, off)
#define STR_D(rt, rn, off)  ldst(g, 0xFD000000, 3, rt, rn, off)

// Nạp / ghi [rn + rm(extend) << size]: option 2 = uxtw, 3 = lsl
static void ldst_r(Gen *g, uint32_t op, int rt, int rn, int rm, int option, bool shift) {
    emit(g, op | ((uint32_t)rm << 16) | ((uint32_t)option << 13) | ((uint32_t)shift << 12) | ((uint32_t)rn << 5) | (uint32_t)rt);
}

enum {
    LDR_X_R = 0xF8600800, STR_X_R = 0xF8200800, LDR_W_R = 0xB8600800, STR_W_R = 0xB8200800,
    LDRB_R = 0x38600800, LDRSB_R = 0x38E00800, STRB_R = 0x38200800,
    LDRH_R = 0x78600800, LDRSH_R = 0x78E00800, STRH_R = 0x78200800,
};

static void op3(Gen *g, uint32_t op, int rd, int rn, int rm) {
    emit(g, op | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd);
}

enum {
    ADD_W = 0x0B000000, ADD_X = 0x8B000000, SUB_W = 0x4B000000, SUB_X = 0xCB000000,
    SUBS_W = 0x6B000000, SUBS_X = 0xEB000000,
    AND_W = 0x0A000000, AND_X = 0x8A000000, ORR_W = 0x2A000000, ORR_X = 0xAA000000,
    EOR_W = 0x4A000000, EOR_X = 0xCA000000,
    MUL_W = 0x1B007C00, MUL_X = 0x9B007C00, SDIV_W = 0x1AC00C00, SDIV_X = 0x9AC00C00,
    LSLV_W = 0x1AC02000, LSRV_W = 0x1AC02400, ASRV_W = 0x1AC02800,
    LSLV_X = 0x9AC02000, LSRV_X = 0x9AC02400, ASRV_X = 0x9AC02800,
    FADD_S = 0x1E202800, FSUB_S = 0x1E203800, FMUL_S = 0x1E200800, FDIV_S = 0x1E201800,
    FADD_D = 0x1E602800, FSUB_D = 0x1E603800, FMUL_D = 0x1E600800, FDIV_D = 0x1E601800,
};

static void op2(Gen *g, uint32_t op, int rd, int rn) {
    emit(g, op | ((uint32_t)rn << 5) | (uint32_t)rd);
}

enum {
    FNEG_S = 0x1E214000, FNEG_D = 0x1E614000,
    SCVTF_S_W = 0x1E220000, SCVTF_D_W = 0x1E620000, SCVTF_S_X = 0x9E220000, SCVTF_D_X = 0x9E620000,
    FCVTZS_W_S = 0x1E380000, FCVTZS_X_S = 0x9E380000, FCVTZS_W_D = 0x1E780000, FCVTZS_X_D = 0x9E780000,
    FCVT_D_S = 0x1E22C000, FCVT_S_D = 0x1E624000,
    SXTW = 0x93407C00, SXTB_W = 0x13001C00, SXTH_W = 0x13003C00, UXTH_W = 0x53003C00,
};

static void mov_rr(Gen *g, int rd, int rm) {
    emit(g, 0xAA0003E0 | ((uint32_t)rm << 16) | (uint32_t)rd);
}

static void cmp_w(Gen *g, int rn, int rm) {
    op3(g, SUBS_W, ZR, rn, rm);
}

static void cmp_x(Gen *g, int rn, int rm) {
    op3(g, SUBS_X, ZR, rn, rm);
}

static void cmp_w_imm(Gen *g, int rn, uint32_t imm) {
    if (imm > 4095) {
        mov_w(g, X16, imm);
        cmp_w(g, rn, X16);
        return;
    }
    emit(g, 0x7100001F | (imm << 10) | ((uint32_t)rn << 5));
}

// rd = rn + imm (64 bit)
static void add_x_imm(Gen *g, int rd, int rn, uint64_t imm) {
    if (imm <= 4095) {
        emit(g, 0x91000000 | ((uint32_t)imm << 10) | ((uint32_t)rn << 5) | (uint32_t)rd);
        return;
    }
    mov_x(g, X16, imm);
    op3(g, ADD_X, rd, rn, X16);
}

// rd = rn + (uxtw rm << sh)
static void add_x_uxtw(Gen *g, int rd, int rn, int rm, int sh) {
    emit(g, 0x8B204000 | ((uint32_t)rm << 16) | ((uint32_t)sh << 10) | ((uint32_t)rn << 5) | (uint32_t)rd);
}

static void cset(Gen *g, int rd, int cond) {
    emit(g, 0x1A9F07E0 | ((uint32_t)(cond ^ 1) << 12) | (uint32_t)rd);
}

// rd = cond ? rn : ~zr (-1)
static void csinv_m1(Gen *g, int rd, int rn, int cond) {
    emit(g, 0x5A800000 | ((uint32_t)ZR << 16) | ((uint32_t)cond << 12) | ((uint32_t)rn << 5) | (uint32_t)rd);
}

// rd = cond ? -rn : rn
static void cneg(Gen *g, int rd, int rn, int cond) {
    emit(g, 0x5A800400 | ((uint32_t)rn << 16) | ((uint32_t)(cond ^ 1) << 12) | ((uint32_t)rn << 5) | (uint32_t)rd);
}

static void call(Gen *g, const void *fn) {
    mov_ptr(g, X16, fn);
    emit(g, 0xD63F0000 | ((uint32_t)X16 << 5));     // blr x16
}

#define SO(k) ((long)(k) * 8)       // offset slot k của stack / locals

// --- stub dùng chung theo pc

static Stub *add_stub(Gen *g, int pc, int depth, int kind) {
    if (g->nstub == g->stubcap) {
        int cap = g->stubcap ? g->stubcap * 2 : 128;
        Stub *s = realloc(g->stub, sizeof(Stub) * cap);
        if (!s) {
            g->fail = true;
            return NULL;
        }
        g->stub = s;
        g->stubcap = cap;
    }
    Stub *s = &g->stub[g->nstub++];
    *s = (Stub){ new_label(g), pc, depth, kind };
    return s;
}

// Nhãn nhảy ra trình thông dịch tại lệnh pc (stack sâu d)
static int step_at(Gen *g, int pc) {
    if (g->step_lbl[pc] < 0) {
        Stub *s = add_stub(g, pc, g->an->depth[pc], 0);
        g->step_lbl[pc] = s ? s->label : 0;
    }
    return g->step_lbl[pc];
}

static int yield_at(Gen *g, int pc) {
    if (g->yield_lbl[pc] < 0) {
        Stub *s = add_stub(g, pc, g->an->depth[pc], 1);
        g->yield_lbl[pc] = s ? s->label : 0;
    }
    return g->yield_lbl[pc];
}

// Nhãn để nhảy từ lệnh from tới lệnh to: nhánh lùi qua stub trừ ngân sách
static int target(Gen *g, int from, int to) {
    if (to > from)
        return to;
    if (g->back_lbl[to] < 0) {
        Stub *s = add_stub(g, to, g->an->depth[to], 2);
        g->back_lbl[to] = s ? s->label : 0;
    }
    return g->back_lbl[to];
}

// --- resolve lúc dịch (không nạp lớp mới)

static Class *known_class(Class *c, uint16_t idx) {
    CPEntry *e = &c->cp[idx];
    if (e->resolved)
        return e->cls;
    const char *name = cp_class_name(c, idx);
    if (!name)
        return NULL;
    if (name == c->name)
        return c;
    Class *k = class_find_loaded(name);
    return k && k->state >= CLASS_LINKED && k->state != CLASS_ERROR ? k : NULL;
}

static Field *known_field(Class *c, uint16_t idx) {
    CPEntry *e = &c->cp[idx];
    if (e->resolved)
        return e->field;
    Class *fc = known_class(c, e->a);
    if (!fc)
        return NULL;
    return class_find_field(fc, ref_name(c, idx), ref_desc(c, idx));
}

// --- từng lệnh

static void array_check(Gen *g, int pc, int arr_slot, int idx_slot) {
    int st = step_at(g, pc);
    LDR_X(X9, RS, SO(arr_slot));
    cbz_x(g, X9, st);
    LDR_W(X10, RS, SO(idx_slot));
    LDR_W(X11, X9, offsetof(Array, length));
    cmp_w(g, X10, X11);
    bcond(g, C_HS, st);
    add_x_imm(g, X9, X9, offsetof(Array, data));
}

static void gen_aload(Gen *g, int pc, int d, uint32_t op, bool shift, bool wide) {
    array_check(g, pc, d - 2, d - 1);
    ldst_r(g, op, X12, X9, X10, 2, shift);
    if (wide)
        STR_X(X12, RS, SO(d - 2));
    else
        STR_W(X12, RS, SO(d - 2));
}

static void gen_astore(Gen *g, int pc, int d, uint32_t op, bool shift, int vslots, bool ref) {
    int ai = d - 2 - vslots, ii = d - 1 - vslots, vi = d - vslots;
    if (ref) {
        int st = step_at(g, pc);
        LDR_X(X0, RS, SO(ai));
        cbz_x(g, X0, st);
        LDR_X(X1, RS, SO(vi));
        call(g, (const void *)rt_store_ok);
        cbz_w(g, X0, st);
    }
    array_check(g, pc, ai, ii);
    if (vslots == 2 || ref)
        LDR_X(X12, RS, SO(vi));
    else
        LDR_W(X12, RS, SO(vi));
    ldst_r(g, op, X12, X9, X10, 2, shift);
}

static void gen_bin_w(Gen *g, int d, uint32_t op) {
    LDR_W(X9, RS, SO(d - 2));
    LDR_W(X10, RS, SO(d - 1));
    op3(g, op, X9, X9, X10);
    STR_W(X9, RS, SO(d - 2));
}

static void gen_bin_x(Gen *g, int d, uint32_t op) {
    LDR_X(X9, RS, SO(d - 4));
    LDR_X(X10, RS, SO(d - 2));
    op3(g, op, X9, X9, X10);
    STR_X(X9, RS, SO(d - 4));
}

static void gen_shift_x(Gen *g, int d, uint32_t op) {
    LDR_X(X9, RS, SO(d - 3));
    LDR_W(X10, RS, SO(d - 1));
    op3(g, op, X9, X9, X10);
    STR_X(X9, RS, SO(d - 3));
}

static void gen_bin_s(Gen *g, int d, uint32_t op) {
    LDR_S(0, RS, SO(d - 2));
    LDR_S(1, RS, SO(d - 1));
    op3(g, op, 0, 0, 1);
    STR_S(0, RS, SO(d - 2));
}

static void gen_bin_d(Gen *g, int d, uint32_t op) {
    LDR_D(0, RS, SO(d - 4));
    LDR_D(1, RS, SO(d - 2));
    op3(g, op, 0, 0, 1);
    STR_D(0, RS, SO(d - 4));
}

// rem / div có kiểm tra chia 0 (để trình thông dịch ném ArithmeticException)
static void gen_div(Gen *g, int pc, int d, bool wide, bool rem) {
    int st = step_at(g, pc);
    if (wide) {
        LDR_X(X9, RS, SO(d - 4));
        LDR_X(X10, RS, SO(d - 2));
        cbz_x(g, X10, st);
        op3(g, SDIV_X, X11, X9, X10);
        if (rem)    // x9 = x9 - x11 * x10
            emit(g, 0x9B008000 | ((uint32_t)X10 << 16) | ((uint32_t)X9 << 10) | ((uint32_t)X11 << 5) | X9);
        else
            mov_rr(g, X9, X11);
        STR_X(X9, RS, SO(d - 4));
    } else {
        LDR_W(X9, RS, SO(d - 2));
        LDR_W(X10, RS, SO(d - 1));
        cbz_w(g, X10, st);
        op3(g, SDIV_W, X11, X9, X10);
        if (rem)
            emit(g, 0x1B008000 | ((uint32_t)X10 << 16) | ((uint32_t)X9 << 10) | ((uint32_t)X11 << 5) | X9);
        else
            mov_rr(g, X9, X11);
        STR_W(X9, RS, SO(d - 2));
    }
}

static void gen_fmod(Gen *g, int d, bool wide) {
    if (wide) {
        LDR_D(0, RS, SO(d - 4));
        LDR_D(1, RS, SO(d - 2));
        call(g, (const void *)rt_fmod);
        STR_D(0, RS, SO(d - 4));
    } else {
        LDR_S(0, RS, SO(d - 2));
        LDR_S(1, RS, SO(d - 1));
        call(g, (const void *)rt_fmodf);
        STR_S(0, RS, SO(d - 2));
    }
}

static void gen_if(Gen *g, int pc, int d, int cond, bool two, bool ref) {
    int t = target(g, pc, pc + S2(g->an->code + pc + 1));
    if (two) {
        if (ref) {
            LDR_X(X9, RS, SO(d - 2));
            LDR_X(X10, RS, SO(d - 1));
            cmp_x(g, X9, X10);
        } else {
            LDR_W(X9, RS, SO(d - 2));
            LDR_W(X10, RS, SO(d - 1));
            cmp_w(g, X9, X10);
        }
        bcond(g, cond, t);
        return;
    }
    if (ref) {
        LDR_X(X9, RS, SO(d - 1));
        if (cond == C_EQ)
            cbz_x(g, X9, t);
        else
            cbnz_x(g, X9, t);
        return;
    }
    LDR_W(X9, RS, SO(d - 1));
    if (cond == C_EQ)
        cbz_w(g, X9, t);
    else if (cond == C_NE)
        cbnz_w(g, X9, t);
    else {
        cmp_w_imm(g, X9, 0);
        bcond(g, cond, t);
    }
}

// Lệnh gọi method. Đường nhanh ngay trong mã máy khi cache của lệnh này khớp (cùng method đích,
// cùng lớp object với gọi ảo): đẩy frame như trình thông dịch rồi gọi thẳng mã máy của method
// đích. Còn lại đi rt_invoke (native, chưa resolve, synchronized...), rt_invoke cũng ghi cache.
static void gen_invoke(Gen *g, int pc, int d) {
    const uint8_t *code = g->an->code;
    int op = code[pc];
    const char *desc = ref_desc(g->an->c, U2(code + pc + 1));
    int nargs = desc_arg_slots(desc) + (op == OP_INVOKESTATIC ? 0 : 1);
    int len = op == OP_INVOKEINTERFACE ? 5 : 3;
    int base = d - nargs;
    AotSite *site = &g->sites[g->nsites++];
    int slow = new_label(g), cont = new_label(g), zl = new_label(g), zd = new_label(g);

    mov_ptr(g, X13, site);
    LDR_X(X14, X13, offsetof(AotSite, target));
    cbz_x(g, X14, slow);
    if (op != OP_INVOKESTATIC) {
        LDR_X(X9, RS, SO(base));
        cbz_x(g, X9, slow);
        if (op == OP_INVOKEVIRTUAL || op == OP_INVOKEINTERFACE) {
            LDR_X(X10, X9, offsetof(Object, cls));
            LDR_X(X11, X13, offsetof(AotSite, cls));
            cmp_x(g, X10, X11);
            bcond(g, C_NE, slow);
        }
    }
    // Còn chỗ: số frame, stack (locals + max_stack + 4 slot)
    LDR_W(X15, RT, offsetof(VMThread, frame_count));
    cmp_w_imm(g, X15, THREAD_MAX_FRAMES);
    bcond(g, C_HS, slow);
    ldst(g, 0x79400000, 1, X4, X14, offsetof(Method, max_locals));     // ldrh w4 = max_locals
    mov_w(g, X5, (uint32_t)nargs);
    cmp_w(g, X4, X5);
    emit(g, 0x1A800000 | ((uint32_t)X5 << 16) | ((uint32_t)C_HS << 12) | ((uint32_t)X4 << 5) | X4);  // w4 = max(w4, w5)
    add_x_imm(g, X6, RS, (uint64_t)SO(base));                           // x6 = locals của frame mới
    ldst(g, 0x79400000, 1, X7, X14, offsetof(Method, max_stack));
    op3(g, ADD_W, X7, X7, X4);
    emit(g, 0x11000000 | (4u << 10) | ((uint32_t)X7 << 5) | X7);        // add w7, w7, #4
    add_x_uxtw(g, X7, X6, X7, 3);
    LDR_X(X8, RT, offsetof(VMThread, stack));
    add_x_imm(g, X8, X8, (uint64_t)THREAD_STACK_SLOTS * sizeof(Value));
    cmp_x(g, X7, X8);
    bcond(g, C_HI, slow);

    // Xoá locals ngoài tham số
    add_x_imm(g, X10, X6, (uint64_t)SO(nargs));
    add_x_uxtw(g, X11, X6, X4, 3);
    bind(g, zl);
    cmp_x(g, X10, X11);
    bcond(g, C_HS, zd);
    emit(g, 0xF8000400 | (8u << 12) | ((uint32_t)X10 << 5) | ZR);      // str xzr, [x10], #8
    b_(g, zl);
    bind(g, zd);

    // Frame hiện tại: tiếp tục ở lệnh sau, tham số chuyển sang frame mới
    ldst(g, 0x39000000, 0, ZR, RF, offsetof(Frame, retry));            // strb wzr
    mov_ptr(g, X9, code + pc + len);
    STR_X(X9, RF, offsetof(Frame, pc));
    STR_X(X6, RF, offsetof(Frame, sp));
    // Frame mới: x12 = t->frames + frame_count
    LDR_X(X12, RT, offsetof(VMThread, frames));
    mov_w(g, X9, sizeof(Frame));
    emit(g, 0x9BA00000 | ((uint32_t)X9 << 16) | ((uint32_t)X12 << 10) | ((uint32_t)X15 << 5) | X12);  // umaddl
    emit(g, 0x11000400 | ((uint32_t)X15 << 5) | X15);                   // add w15, w15, #1
    STR_W(X15, RT, offsetof(VMThread, frame_count));
    STR_X(X14, X12, offsetof(Frame, m));
    LDR_X(X9, X14, offsetof(Method, code));
    STR_X(X9, X12, offsetof(Frame, pc));
    STR_X(X6, X12, offsetof(Frame, locals));
    add_x_uxtw(g, X9, X6, X4, 3);
    STR_X(X9, X12, offsetof(Frame, stack_base));
    STR_X(X9, X12, offsetof(Frame, sp));
    STR_X(ZR, X12, offsetof(Frame, clinit_of));
    STR_X(ZR, X12, offsetof(Frame, sync_obj));
    ldst(g, 0x39000000, 0, ZR, X12, offsetof(Frame, retry));
    // Ngân sách như khi vào method trong trình thông dịch
    LDR_W(X9, RB, 0);
    emit(g, 0x71000400 | ((uint32_t)X9 << 5) | X9);                     // subs w9, w9, #1
    STR_W(X9, RB, 0);
    bcond(g, C_LE, g->lbl_yield_top);
    // Method đích chưa có mã máy / lồng quá sâu: trình thông dịch chạy tiếp từ frame mới
    LDR_X(X16, X14, offsetof(Method, aot_code));
    cbz_x(g, X16, g->lbl_step_top);
    mov_ptr(g, X9, &nest);
    LDR_W(X10, X9, 0);
    cmp_w_imm(g, X10, AOT_MAX_NEST);
    bcond(g, C_HS, g->lbl_step_top);
    emit(g, 0x11000400 | ((uint32_t)X10 << 5) | X10);
    STR_W(X10, X9, 0);
    LDR_X(X17, X14, offsetof(Method, aot_entry));
    LDR_W(X17, X17, 0);
    op3(g, ADD_X, X3, X16, X17);
    mov_rr(g, X0, RT);
    mov_rr(g, X1, X12);
    mov_rr(g, X2, RB);
    emit(g, 0xD63F0000 | ((uint32_t)X16 << 5));                         // blr x16
    mov_ptr(g, X9, &nest);
    LDR_W(X10, X9, 0);
    emit(g, 0x51000400 | ((uint32_t)X10 << 5) | X10);                   // sub w10, w10, #1
    STR_W(X10, X9, 0);
    cmp_w_imm(g, X0, AOT_RETURN);
    bcond(g, C_EQ, cont);
    b_(g, g->lbl_ret);                      // frame sâu hơn chờ trình thông dịch: trả nguyên mã

    bind(g, slow);
    mov_rr(g, X0, RT);
    mov_rr(g, X1, RF);
    mov_w(g, X2, (uint32_t)pc);
    mov_w(g, X3, (uint32_t)d);
    mov_rr(g, X4, RB);
    mov_ptr(g, X5, site);
    call(g, (const void *)rt_invoke);
    cmp_w_imm(g, X0, RT_INVOKE_DONE);
    bcond(g, C_EQ, cont);
    emit(g, 0x3100041F);                    // cmn w0, #1
    bcond(g, C_EQ, step_at(g, pc));
    b_(g, g->lbl_ret);
    bind(g, cont);
}

// return từ frame thường (không giữ monitor, không phải <clinit>, có caller): bỏ frame, đẩy giá
// trị lên stack caller. Trường hợp khác để trình thông dịch làm.
static void gen_return(Gen *g, int pc, int d, int slots) {
    int st = step_at(g, pc);
    LDR_X(X9, RF, offsetof(Frame, sync_obj));
    cbnz_x(g, X9, st);
    LDR_X(X9, RF, offsetof(Frame, clinit_of));
    cbnz_x(g, X9, st);
    LDR_W(X10, RT, offsetof(VMThread, frame_count));
    cmp_w_imm(g, X10, 2);
    bcond(g, C_LT, st);
    emit(g, 0xD1000000 | ((uint32_t)sizeof(Frame) << 10) | ((uint32_t)RF << 5) | X11);  // x11 = f - 1
    if (slots) {
        LDR_X(X12, X11, offsetof(Frame, sp));
        LDR_X(X9, RS, SO(d - slots));
        STR_X(X9, X12, 0);
        add_x_imm(g, X12, X12, (uint64_t)SO(slots));
        STR_X(X12, X11, offsetof(Frame, sp));
    }
    emit(g, 0x51000400 | ((uint32_t)X10 << 5) | X10);                   // sub w10, w10, #1
    STR_W(X10, RT, offsetof(VMThread, frame_count));
    mov_w(g, X0, AOT_RETURN);
    b_(g, g->lbl_ret);
}

static void gen_field(Gen *g, int pc, int d, int op) {
    Class *c = g->an->c;
    uint16_t idx = U2(g->an->code + pc + 1);
    bool wide = type_slots(ref_desc(c, idx)[0]) == 2;
    int st = step_at(g, pc);
    Field *fl = known_field(c, idx);

    if (op == OP_GETSTATIC || op == OP_PUTSTATIC) {
        int vslot = op == OP_GETSTATIC ? d : d - (wide ? 2 : 1);
        if (fl && fl->owner->statics) {
            // Lớp chủ chưa khởi tạo xong: để trình thông dịch chạy <clinit> / chờ
            mov_ptr(g, X11, &fl->owner->state);
            LDR_W(X12, X11, 0);
            cmp_w_imm(g, X12, CLASS_INITIALIZED);
            bcond(g, C_NE, st);
            mov_ptr(g, X11, &fl->owner->statics[fl->slot]);
        } else {
            // Chưa biết lúc dịch: chờ trình thông dịch viết đè thành lệnh nhanh (đã resolve + khởi tạo)
            mov_ptr(g, X11, g->an->code + pc);
            LDRB(X12, X11, 0);
            int q = op == OP_GETSTATIC ? (wide ? OP_GETSTATIC2_Q : OP_GETSTATIC_Q) : (wide ? OP_PUTSTATIC2_Q : OP_PUTSTATIC_Q);
            cmp_w_imm(g, X12, (uint32_t)q);
            bcond(g, C_NE, st);
            mov_ptr(g, X11, &c->cp[idx]);
            LDR_X(X12, X11, offsetof(CPEntry, field));
            LDR_X(X13, X12, offsetof(Field, owner));
            LDR_X(X13, X13, offsetof(Class, statics));
            LDRSW(X14, X12, offsetof(Field, slot));
            add_x_uxtw(g, X11, X13, X14, 3);
        }
        if (op == OP_GETSTATIC) {
            LDR_X(X9, X11, 0);
            STR_X(X9, RS, SO(vslot));
        } else {
            LDR_X(X9, RS, SO(vslot));
            STR_X(X9, X11, 0);
        }
        return;
    }

    int oslot = op == OP_GETFIELD ? d - 1 : d - (wide ? 3 : 2);
    if (fl && (long)sizeof(Object) + SO(fl->slot) <= 32760) {
        LDR_X(X9, RS, SO(oslot));
        cbz_x(g, X9, st);
        long off = (long)sizeof(Object) + SO(fl->slot);
        if (op == OP_GETFIELD) {
            LDR_X(X10, X9, off);
            STR_X(X10, RS, SO(d - 1));
        } else {
            LDR_X(X10, RS, SO(oslot + 1));
            STR_X(X10, X9, off);
        }
        return;
    }
    // Chưa biết field lúc dịch: đọc từ constant pool khi đã resolve
    mov_ptr(g, X11, &c->cp[idx]);
    LDRB(X12, X11, offsetof(CPEntry, resolved));
    cbz_w(g, X12, st);
    LDR_X(X12, X11, offsetof(CPEntry, field));
    LDRSW(X13, X12, offsetof(Field, slot));
    LDR_X(X9, RS, SO(oslot));
    cbz_x(g, X9, st);
    add_x_imm(g, X9, X9, sizeof(Object));
    if (op == OP_GETFIELD) {
        ldst_r(g, LDR_X_R, X10, X9, X13, 3, true);
        STR_X(X10, RS, SO(d - 1));
    } else {
        LDR_X(X10, RS, SO(oslot + 1));
        ldst_r(g, STR_X_R, X10, X9, X13, 3, true);
    }
}

// x1 = Class* của constant pool idx (nhảy st nếu chưa resolve)
static void load_class(Gen *g, uint16_t idx, int st) {
    Class *k = known_class(g->an->c, idx);
    if (k) {
        mov_ptr(g, X1, k);
        return;
    }
    mov_ptr(g, X11, &g->an->c->cp[idx]);
    LDRB(X12, X11, offsetof(CPEntry, resolved));
    cbz_w(g, X12, st);
    LDR_X(X1, X11, offsetof(CPEntry, cls));
}

static void gen_checkcast(Gen *g, int pc, int d, bool instof) {
    uint16_t idx = U2(g->an->code + pc + 1);
    int st = step_at(g, pc);
    int done = new_label(g), isnull = new_label(g), yes = new_label(g);
    LDR_X(X9, RS, SO(d - 1));
    cbz_x(g, X9, isnull);
    load_class(g, idx, st);
    LDR_X(X0, X9, offsetof(Object, cls));
    cmp_x(g, X0, X1);
    bcond(g, C_EQ, yes);
    call(g, (const void *)rt_instance_of);
    if (instof) {
        STR_W(X0, RS, SO(d - 1));
        b_(g, done);
        bind(g, yes);
        mov_w(g, X9, 1);
        STR_W(X9, RS, SO(d - 1));
        b_(g, done);
        bind(g, isnull);
        STR_W(ZR, RS, SO(d - 1));
    } else {
        cbz_w(g, X0, st);
        bind(g, yes);
        bind(g, isnull);
    }
    bind(g, done);
}

static void gen_ldc(Gen *g, int pc, int d, uint16_t idx) {
    CPEntry *e = &g->an->c->cp[idx];
    if (e->tag == CONST_Integer || e->tag == CONST_Float) {
        mov_w(g, X9, (uint32_t)e->i);
        STR_W(X9, RS, SO(d));
    } else if (e->tag == CONST_String) {
        int st = step_at(g, pc);
        mov_ptr(g, X11, e);
        LDRB(X12, X11, offsetof(CPEntry, resolved));
        cbz_w(g, X12, st);
        LDR_X(X9, X11, offsetof(CPEntry, str));
        STR_X(X9, RS, SO(d));
    } else {
        b_(g, step_at(g, pc));
    }
}

static void gen_switch(Gen *g, int pc, int d) {
    const uint8_t *code = g->an->code;
    int p = pc + 1 + pad4(pc);
    int def = target(g, pc, pc + S4(code + p));
    LDR_W(X9, RS, SO(d - 1));
    if (code[pc] == OP_LOOKUPSWITCH) {
        int n = S4(code + p + 4);
        for (int k = 0; k < n; k++) {
            mov_w(g, X10, (uint32_t)S4(code + p + 8 + 8 * k));
            cmp_w(g, X9, X10);
            bcond(g, C_EQ, target(g, pc, pc + S4(code + p + 12 + 8 * k)));
        }
        b_(g, def);
        return;
    }
    int32_t lo = S4(code + p + 4), hi = S4(code + p + 8);
    mov_w(g, X10, (uint32_t)lo);
    op3(g, SUB_W, X9, X9, X10);
    mov_w(g, X10, (uint32_t)hi - (uint32_t)lo);
    cmp_w(g, X9, X10);
    bcond(g, C_HI, def);
    emit(g, 0x10000000 | (3u << 5) | X10);      // adr x10, #12 (bảng ngay sau br)
    add_x_uxtw(g, X10, X10, X9, 2);
    emit(g, 0xD61F0000 | ((uint32_t)X10 << 5)); // br x10
    for (int64_t k = 0; k <= (int64_t)hi - lo; k++)
        b_(g, target(g, pc, pc + S4(code + p + 12 + 4 * (int)k)));
}

static void load_local(Gen *g, int d, int n) {
    LDR_X(X9, RL, SO(n));
    STR_X(X9, RS, SO(d));
}

static void store_local(Gen *g, int d, int n, int slots) {
    LDR_X(X9, RS, SO(d - slots));
    STR_X(X9, RL, SO(n));
}

static void iinc(Gen *g, int n, int32_t k) {
    LDR_W(X9, RL, SO(n));
    mov_w(g, X10, (uint32_t)k);
    op3(g, ADD_W, X9, X9, X10);
    STR_W(X9, RL, SO(n));
}

// Nạp cả slot stack vào thanh ghi (dup / swap)
static void ld_slot(Gen *g, int reg, int slot) {
    LDR_X(reg, RS, SO(slot));
}

static void gen_insn(Gen *g, int pc) {
    const uint8_t *code = g->an->code;
    int op = code[pc];
    int d = g->an->depth[pc];
    switch (op) {
    case OP_NOP: return;
    case OP_ACONST_NULL: STR_X(ZR, RS, SO(d)); return;
    case OP_ICONST_M1: case OP_ICONST_0: case OP_ICONST_1: case OP_ICONST_2:
    case OP_ICONST_3: case OP_ICONST_4: case OP_ICONST_5:
        mov_w(g, X9, (uint32_t)(op - OP_ICONST_0));
        STR_W(X9, RS, SO(d));
        return;
    case OP_LCONST_0: case OP_LCONST_1:
        mov_x(g, X9, (uint64_t)(op - OP_LCONST_0));
        STR_X(X9, RS, SO(d));
        return;
    case OP_FCONST_0: case OP_FCONST_1: case OP_FCONST_2: {
        Value v = { .f = (float)(op - OP_FCONST_0) };
        mov_w(g, X9, (uint32_t)v.i);
        STR_W(X9, RS, SO(d));
        return;
    }
    case OP_DCONST_0: case OP_DCONST_1: {
        Value v = { .d = (double)(op - OP_DCONST_0) };
        mov_x(g, X9, (uint64_t)v.j);
        STR_X(X9, RS, SO(d));
        return;
    }
    case OP_BIPUSH: mov_w(g, X9, (uint32_t)(int8_t)code[pc + 1]); STR_W(X9, RS, SO(d)); return;
    case OP_SIPUSH: mov_w(g, X9, (uint32_t)S2(code + pc + 1)); STR_W(X9, RS, SO(d)); return;
    case OP_LDC: gen_ldc(g, pc, d, code[pc + 1]); return;
    case OP_LDC_W: gen_ldc(g, pc, d, U2(code + pc + 1)); return;
    case OP_LDC2_W: {
        CPEntry *e = &g->an->c->cp[U2(code + pc + 1)];
        mov_x(g, X9, (uint64_t)e->j);
        STR_X(X9, RS, SO(d));
        return;
    }
    case OP_ILOAD: case OP_LLOAD: case OP_FLOAD: case OP_DLOAD: case OP_ALOAD:
        load_local(g, d, code[pc + 1]);
        return;
    case OP_ISTORE: case OP_FSTORE: case OP_ASTORE: store_local(g, d, code[pc + 1], 1); return;
    case OP_LSTORE: case OP_DSTORE: store_local(g, d, code[pc + 1], 2); return;

    case OP_IALOAD: case OP_FALOAD: gen_aload(g, pc, d, LDR_W_R, true, false); return;
    case OP_LALOAD: case OP_DALOAD: gen_aload(g, pc, d, LDR_X_R, true, true); return;
    case OP_AALOAD: gen_aload(g, pc, d, LDR_X_R, true, true); return;
    case OP_BALOAD: gen_aload(g, pc, d, LDRSB_R, false, false); return;
    case OP_CALOAD: gen_aload(g, pc, d, LDRH_R, true, false); return;
    case OP_SALOAD: gen_aload(g, pc, d, LDRSH_R, true, false); return;
    case OP_IASTORE: case OP_FASTORE: gen_astore(g, pc, d, STR_W_R, true, 1, false); return;
    case OP_LASTORE: case OP_DASTORE: gen_astore(g, pc, d, STR_X_R, true, 2, false); return;
    case OP_AASTORE: gen_astore(g, pc, d, STR_X_R, true, 1, true); return;
    case OP_BASTORE: gen_astore(g, pc, d, STRB_R, false, 1, false); return;
    case OP_CASTORE: case OP_SASTORE: gen_astore(g, pc, d, STRH_R, true, 1, false); return;

    case OP_POP: case OP_POP2: return;
    case OP_DUP:
        ld_slot(g, X9, d - 1);
        STR_X(X9, RS, SO(d));
        return;
    case OP_DUP_X1:     // v2 v1 -> v1 v2 v1
        ld_slot(g, X9, d - 1);
        ld_slot(g, X10, d - 2);
        STR_X(X9, RS, SO(d - 2));
        STR_X(X10, RS, SO(d - 1));
        STR_X(X9, RS, SO(d));
        return;
    case OP_DUP_X2:     // v3 v2 v1 -> v1 v3 v2 v1
        ld_slot(g, X9, d - 1);
        ld_slot(g, X10, d - 2);
        ld_slot(g, X11, d - 3);
        STR_X(X9, RS, SO(d - 3));
        STR_X(X11, RS, SO(d - 2));
        STR_X(X10, RS, SO(d - 1));
        STR_X(X9, RS, SO(d));
        return;
    case OP_DUP2:
        ld_slot(g, X9, d - 2);
        ld_slot(g, X10, d - 1);
        STR_X(X9, RS, SO(d));
        STR_X(X10, RS, SO(d + 1));
        return;
    case OP_DUP2_X1:    // v3 v2 v1 -> v2 v1 v3 v2 v1
        ld_slot(g, X9, d - 1);
        ld_slot(g, X10, d - 2);
        ld_slot(g, X11, d - 3);
        STR_X(X10, RS, SO(d - 3));
        STR_X(X9, RS, SO(d - 2));
        STR_X(X11, RS, SO(d - 1));
        STR_X(X10, RS, SO(d));
        STR_X(X9, RS, SO(d + 1));
        return;
    case OP_DUP2_X2:    // v4 v3 v2 v1 -> v2 v1 v4 v3 v2 v1
        ld_slot(g, X9, d - 1);
        ld_slot(g, X10, d - 2);
        ld_slot(g, X11, d - 3);
        ld_slot(g, X12, d - 4);
        STR_X(X10, RS, SO(d - 4));
        STR_X(X9, RS, SO(d - 3));
        STR_X(X12, RS, SO(d - 2));
        STR_X(X11, RS, SO(d - 1));
        STR_X(X10, RS, SO(d));
        STR_X(X9, RS, SO(d + 1));
        return;
    case OP_SWAP:
        ld_slot(g, X9, d - 1);
        ld_slot(g, X10, d - 2);
        STR_X(X9, RS, SO(d - 2));
        STR_X(X10, RS, SO(d - 1));
        return;

    case OP_IADD: gen_bin_w(g, d, ADD_W); return;
    case OP_LADD: gen_bin_x(g, d, ADD_X); return;
    case OP_FADD: gen_bin_s(g, d, FADD_S); return;
    case OP_DADD: gen_bin_d(g, d, FADD_D); return;
    case OP_ISUB: gen_bin_w(g, d, SUB_W); return;
    case OP_LSUB: gen_bin_x(g, d, SUB_X); return;
    case OP_FSUB: gen_bin_s(g, d, FSUB_S); return;
    case OP_DSUB: gen_bin_d(g, d, FSUB_D); return;
    case OP_IMUL: gen_bin_w(g, d, MUL_W); return;
    case OP_LMUL: gen_bin_x(g, d, MUL_X); return;
    case OP_FMUL: gen_bin_s(g, d, FMUL_S); return;
    case OP_DMUL: gen_bin_d(g, d, FMUL_D); return;
    case OP_IDIV: gen_div(g, pc, d, false, false); return;
    case OP_LDIV: gen_div(g, pc, d, true, false); return;
    case OP_FDIV: gen_bin_s(g, d, FDIV_S); return;
    case OP_DDIV: gen_bin_d(g, d, FDIV_D); return;
    case OP_IREM: gen_div(g, pc, d, false, true); return;
    case OP_LREM: gen_div(g, pc, d, true, true); return;
    case OP_FREM: gen_fmod(g, d, false); return;
    case OP_DREM: gen_fmod(g, d, true); return;
    case OP_INEG:
        LDR_W(X9, RS, SO(d - 1));
        op3(g, SUB_W, X9, ZR, X9);
        STR_W(X9, RS, SO(d - 1));
        return;
    case OP_LNEG:
        LDR_X(X9, RS, SO(d - 2));
        op3(g, SUB_X, X9, ZR, X9);
        STR_X(X9, RS, SO(d - 2));
        return;
    case OP_FNEG:
        LDR_S(0, RS, SO(d - 1));
        op2(g, FNEG_S, 0, 0);
        STR_S(0, RS, SO(d - 1));
        return;
    case OP_DNEG:
        LDR_D(0, RS, SO(d - 2));
        op2(g, FNEG_D, 0, 0);
        STR_D(0, RS, SO(d - 2));
        return;
    case OP_ISHL: gen_bin_w(g, d, LSLV_W); return;
    case OP_ISHR: gen_bin_w(g, d, ASRV_W); return;
    case OP_IUSHR: gen_bin_w(g, d, LSRV_W); return;
    case OP_LSHL: gen_shift_x(g, d, LSLV_X); return;
    case OP_LSHR: gen_shift_x(g, d, ASRV_X); return;
    case OP_LUSHR: gen_shift_x(g, d, LSRV_X); return;
    case OP_IAND: gen_bin_w(g, d, AND_W); return;
    case OP_LAND: gen_bin_x(g, d, AND_X); return;
    case OP_IOR: gen_bin_w(g, d, ORR_W); return;
    case OP_LOR: gen_bin_x(g, d, ORR_X); return;
    case OP_IXOR: gen_bin_w(g, d, EOR_W); return;
    case OP_LXOR: gen_bin_x(g, d, EOR_X); return;
    case OP_IINC: iinc(g, code[pc + 1], (int8_t)code[pc + 2]); return;

    case OP_I2L:
        LDR_W(X9, RS, SO(d - 1));
        op2(g, SXTW, X9, X9);
        STR_X(X9, RS, SO(d - 1));
        return;
    case OP_I2F: LDR_W(X9, RS, SO(d - 1)); op2(g, SCVTF_S_W, 0, X9); STR_S(0, RS, SO(d - 1)); return;
    case OP_I2D: LDR_W(X9, RS, SO(d - 1)); op2(g, SCVTF_D_W, 0, X9); STR_D(0, RS, SO(d - 1)); return;
    case OP_L2I: LDR_X(X9, RS, SO(d - 2)); STR_W(X9, RS, SO(d - 2)); return;
    case OP_L2F: LDR_X(X9, RS, SO(d - 2)); op2(g, SCVTF_S_X, 0, X9); STR_S(0, RS, SO(d - 2)); return;
    case OP_L2D: LDR_X(X9, RS, SO(d - 2)); op2(g, SCVTF_D_X, 0, X9); STR_D(0, RS, SO(d - 2)); return;
    // fcvtzs: bão hoà, NaN -> 0, đúng như Java
    case OP_F2I: LDR_S(0, RS, SO(d - 1)); op2(g, FCVTZS_W_S, X9, 0); STR_W(X9, RS, SO(d - 1)); return;
    case OP_F2L: LDR_S(0, RS, SO(d - 1)); op2(g, FCVTZS_X_S, X9, 0); STR_X(X9, RS, SO(d - 1)); return;
    case OP_F2D: LDR_S(0, RS, SO(d - 1)); op2(g, FCVT_D_S, 0, 0); STR_D(0, RS, SO(d - 1)); return;
    case OP_D2I: LDR_D(0, RS, SO(d - 2)); op2(g, FCVTZS_W_D, X9, 0); STR_W(X9, RS, SO(d - 2)); return;
    case OP_D2L: LDR_D(0, RS, SO(d - 2)); op2(g, FCVTZS_X_D, X9, 0); STR_X(X9, RS, SO(d - 2)); return;
    case OP_D2F: LDR_D(0, RS, SO(d - 2)); op2(g, FCVT_S_D, 0, 0); STR_S(0, RS, SO(d - 2)); return;
    case OP_I2B: LDR_W(X9, RS, SO(d - 1)); op2(g, SXTB_W, X9, X9); STR_W(X9, RS, SO(d - 1)); return;
    case OP_I2C: LDR_W(X9, RS, SO(d - 1)); op2(g, UXTH_W, X9, X9); STR_W(X9, RS, SO(d - 1)); return;
    case OP_I2S: LDR_W(X9, RS, SO(d - 1)); op2(g, SXTH_W, X9, X9); STR_W(X9, RS, SO(d - 1)); return;

    case OP_LCMP:
        LDR_X(X9, RS, SO(d - 4));
        LDR_X(X10, RS, SO(d - 2));
        cmp_x(g, X9, X10);
        cset(g, X11, C_NE);
        cneg(g, X11, X11, C_LT);
        STR_W(X11, RS, SO(d - 4));
        return;
    case OP_FCMPL: case OP_FCMPG: case OP_DCMPL: case OP_DCMPG: {
        bool dbl = op == OP_DCMPL || op == OP_DCMPG, g_ = op == OP_FCMPG || op == OP_DCMPG;
        int a = dbl ? d - 4 : d - 2, b = dbl ? d - 2 : d - 1;
        if (dbl) {
            LDR_D(0, RS, SO(a));
            LDR_D(1, RS, SO(b));
            emit(g, 0x1E602000 | (1u << 16));   // fcmp d0, d1
        } else {
            LDR_S(0, RS, SO(a));
            LDR_S(1, RS, SO(b));
            emit(g, 0x1E202000 | (1u << 16));   // fcmp s0, s1
        }
        // fcmpg: lớn hơn / NaN -> 1, nhỏ hơn -> -1. fcmpl: lớn hơn -> 1, nhỏ hơn / NaN -> -1
        cset(g, X11, g_ ? C_HI : C_GT);
        csinv_m1(g, X11, X11, g_ ? C_PL : C_GE);
        STR_W(X11, RS, SO(a));
        return;
    }
    case OP_IFEQ: gen_if(g, pc, d, C_EQ, false, false); return;
    case OP_IFNE: gen_if(g, pc, d, C_NE, false, false); return;
    case OP_IFLT: gen_if(g, pc, d, C_LT, false, false); return;
    case OP_IFGE: gen_if(g, pc, d, C_GE, false, false); return;
    case OP_IFGT: gen_if(g, pc, d, C_GT, false, false); return;
    case OP_IFLE: gen_if(g, pc, d, C_LE, false, false); return;
    case OP_IF_ICMPEQ: gen_if(g, pc, d, C_EQ, true, false); return;
    case OP_IF_ICMPNE: gen_if(g, pc, d, C_NE, true, false); return;
    case OP_IF_ICMPLT: gen_if(g, pc, d, C_LT, true, false); return;
    case OP_IF_ICMPGE: gen_if(g, pc, d, C_GE, true, false); return;
    case OP_IF_ICMPGT: gen_if(g, pc, d, C_GT, true, false); return;
    case OP_IF_ICMPLE: gen_if(g, pc, d, C_LE, true, false); return;
    case OP_IF_ACMPEQ: gen_if(g, pc, d, C_EQ, true, true); return;
    case OP_IF_ACMPNE: gen_if(g, pc, d, C_NE, true, true); return;
    case OP_IFNULL: gen_if(g, pc, d, C_EQ, false, true); return;
    case OP_IFNONNULL: gen_if(g, pc, d, C_NE, false, true); return;
    case OP_GOTO: b_(g, target(g, pc, pc + S2(code + pc + 1))); return;
    case OP_GOTO_W: b_(g, target(g, pc, pc + S4(code + pc + 1))); return;
    case OP_TABLESWITCH: case OP_LOOKUPSWITCH: gen_switch(g, pc, d); return;

    case OP_IRETURN: case OP_FRETURN: case OP_ARETURN: gen_return(g, pc, d, 1); return;
    case OP_LRETURN: case OP_DRETURN: gen_return(g, pc, d, 2); return;
    case OP_RETURN: gen_return(g, pc, d, 0); return;

    case OP_GETSTATIC: case OP_PUTSTATIC: case OP_GETFIELD: case OP_PUTFIELD:
        gen_field(g, pc, d, op);
        return;
    case OP_INVOKEVIRTUAL: case OP_INVOKESPECIAL: case OP_INVOKESTATIC: case OP_INVOKEINTERFACE:
        gen_invoke(g, pc, d);
        return;
    case OP_ARRAYLENGTH:
        LDR_X(X9, RS, SO(d - 1));
        cbz_x(g, X9, step_at(g, pc));
        LDR_W(X10, X9, offsetof(Array, length));
        STR_W(X10, RS, SO(d - 1));
        return;
    case OP_CHECKCAST: gen_checkcast(g, pc, d, false); return;
    case OP_INSTANCEOF: gen_checkcast(g, pc, d, true); return;

    case OP_WIDE: {
        int w = code[pc + 1], n = U2(code + pc + 2);
        if (w == OP_IINC)
            iinc(g, n, S2(code + pc + 4));
        else if (w == OP_ILOAD || w == OP_LLOAD || w == OP_FLOAD || w == OP_DLOAD || w == OP_ALOAD)
            load_local(g, d, n);
        else
            store_local(g, d, n, w == OP_LSTORE || w == OP_DSTORE ? 2 : 1);
        return;
    }
    }
    if (op >= OP_ILOAD_0 && op <= OP_ALOAD_3) {
        load_local(g, d, (op - OP_ILOAD_0) % 4);
        return;
    }
    if (op >= OP_ISTORE_0 && op <= OP_ASTORE_3) {
        int kind = (op - OP_ISTORE_0) / 4;      // i l f d a
        store_local(g, d, (op - OP_ISTORE_0) % 4, kind == 1 || kind == 3 ? 2 : 1);
        return;
    }
    // Gọi method, new, newarray, athrow, monitor, multianewarray: trình thông dịch làm
    b_(g, step_at(g, pc));
}

static void gen_stubs(Gen *g) {
    for (int i = 0; i < g->nstub && !g->fail; i++) {
        Stub s = g->stub[i];
        bind(g, s.label);
        if (s.kind == 2) {
            // Nhánh lùi: trừ ngân sách, hết thì nhường
            LDR_W(X9, RB, 0);
            emit(g, 0x71000400 | ((uint32_t)X9 << 5) | X9);    // subs w9, w9, #1
            STR_W(X9, RB, 0);
            bcond(g, C_LE, yield_at(g, s.pc));
            b_(g, s.pc);
            continue;
        }
        mov_w(g, X1, (uint32_t)s.pc);
        mov_w(g, X2, (uint32_t)s.depth);
        b_(g, s.kind == 0 ? g->lbl_step : g->lbl_yield);
    }
    bind(g, g->lbl_step_top);
    mov_w(g, X0, AOT_STEP);
    b_(g, g->lbl_ret);
    bind(g, g->lbl_yield_top);
    mov_w(g, X0, AOT_YIELD);
    b_(g, g->lbl_ret);
    // w1 = pc, w2 = độ sâu stack: ghi f->pc / f->sp
    int common = new_label(g);
    bind(g, g->lbl_step);
    mov_w(g, X0, AOT_STEP);
    b_(g, common);
    bind(g, g->lbl_yield);
    mov_w(g, X0, AOT_YIELD);
    bind(g, common);
    add_x_uxtw(g, X9, RC, X1, 0);
    STR_X(X9, RF, offsetof(Frame, pc));
    add_x_uxtw(g, X9, RS, X2, 3);
    STR_X(X9, RF, offsetof(Frame, sp));
    bind(g, g->lbl_ret);
    emit(g, 0xA9400000 | (2u << 15) | (RF << 10) | (SPR << 5) | RT);    // ldp x19, x20, [sp, #16]
    emit(g, 0xA9400000 | (4u << 15) | (RS << 10) | (SPR << 5) | RL);    // ldp x21, x22, [sp, #32]
    emit(g, 0xA9400000 | (6u << 15) | (RC << 10) | (SPR << 5) | RB);    // ldp x23, x24, [sp, #48]
    emit(g, 0xA8C00000 | (8u << 15) | (LR << 10) | (SPR << 5) | FP);    // ldp x29, x30, [sp], #64
    emit(g, 0xD65F03C0);                                                // ret
}

static bool resolve_fixups(Gen *g) {
    for (int i = 0; i < g->nfix; i++) {
        Fix *f = &g->fix[i];
        int pos = g->lpos[f->label];
        if (pos < 0)
            return false;
        int32_t delta = pos - f->at;
        if (f->b26) {
            g->buf[f->at] |= (uint32_t)delta & 0x3FFFFFF;
        } else {
            if (delta < -(1 << 18) || delta >= (1 << 18))
                return false;
            g->buf[f->at] |= ((uint32_t)delta & 0x7FFFF) << 5;
        }
    }
    return true;
}

void aot_compile(Class *c, Method *m) {
    if (!active || !m->code || m->code_len == 0 || m->max_locals > 2000 || m->max_stack > 2000)
        return;
    int len = (int)m->code_len;
    Ana an = { c, m, m->code, len, calloc(len, sizeof(int)), calloc(len, sizeof(int)) };
    Gen g = { .an = &an };
    uint32_t *entry = calloc(len, sizeof(uint32_t));
    g.lcap = len + 64;
    g.lpos = malloc(sizeof(int) * g.lcap);
    g.step_lbl = malloc(sizeof(int) * len);
    g.back_lbl = malloc(sizeof(int) * len);
    g.yield_lbl = malloc(sizeof(int) * len);
    bool ok = an.ilen && an.depth && entry && g.lpos && g.step_lbl && g.back_lbl && g.yield_lbl && analyze(&an);
    if (ok) {
        for (int i = 0; i < len; i++) {
            g.lpos[i] = -1;
            g.step_lbl[i] = g.back_lbl[i] = g.yield_lbl[i] = -1;
        }
        g.nl = len;
        g.lbl_step = new_label(&g);
        g.lbl_yield = new_label(&g);
        g.lbl_ret = new_label(&g);
        g.lbl_step_top = new_label(&g);
        g.lbl_yield_top = new_label(&g);
        int ninvoke = 0;
        for (int pc = 0; pc < len; pc += an.ilen[pc]) {
            int op = m->code[pc];
            if (an.depth[pc] >= 0 && op >= OP_INVOKEVIRTUAL && op <= OP_INVOKEINTERFACE)
                ninvoke++;
        }
        g.sites = ninvoke ? calloc(ninvoke, sizeof(AotSite)) : NULL;
        ok = !ninvoke || g.sites;

        // Mở đầu: cất x29, x30, x19-x24; nạp thanh ghi cố định; nhảy tới entry (x3)
        emit(&g, 0xA9800000 | (0x78u << 15) | (LR << 10) | (SPR << 5) | FP);   // stp x29, x30, [sp, #-64]!
        emit(&g, 0x910003FD);                                                   // mov x29, sp
        emit(&g, 0xA9000000 | (2u << 15) | (RF << 10) | (SPR << 5) | RT);      // stp x19, x20, [sp, #16]
        emit(&g, 0xA9000000 | (4u << 15) | (RS << 10) | (SPR << 5) | RL);      // stp x21, x22, [sp, #32]
        emit(&g, 0xA9000000 | (6u << 15) | (RC << 10) | (SPR << 5) | RB);      // stp x23, x24, [sp, #48]
        mov_rr(&g, RT, X0);
        mov_rr(&g, RF, X1);
        mov_rr(&g, RB, X2);
        ldst(&g, 0xF9400000, 3, RL, RF, offsetof(Frame, locals));
        ldst(&g, 0xF9400000, 3, RS, RF, offsetof(Frame, stack_base));
        mov_ptr(&g, RC, m->code);
        emit(&g, 0xD61F0000 | (X3 << 5));                                       // br x3

        for (int pc = 0; pc < len && !g.fail; pc += an.ilen[pc]) {
            if (an.depth[pc] < 0)
                continue;
            bind(&g, pc);
            entry[pc] = (uint32_t)g.n * 4;
            gen_insn(&g, pc);
        }
        gen_stubs(&g);
        ok = ok && !g.fail && resolve_fixups(&g);
    }
    void *rx = ok ? aot_mem_put(g.buf, (size_t)g.n * 4) : NULL;
    if (rx) {
        m->aot_code = rx;
        m->aot_entry = entry;
        m->aot_sites = g.sites;
        compiled++;
    } else {
        free(entry);
        free(g.sites);
        skipped++;
    }
    free(an.ilen);
    free(an.depth);
    free(g.buf);
    free(g.lpos);
    free(g.fix);
    free(g.stub);
    free(g.step_lbl);
    free(g.back_lbl);
    free(g.yield_lbl);
}

#endif
