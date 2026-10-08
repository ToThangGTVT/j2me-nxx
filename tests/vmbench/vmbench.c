// Đo tốc độ trình thông dịch trên desktop, không cần cửa sổ.
//   vmbench <bench.jar> [số lần lặp lại] [hệ số tải]
// Mỗi bài in thời gian tốt nhất và checksum. Tối ưu xong checksum phải giữ nguyên.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "vm/vm.h"
#include "vm/zip.h"

extern const unsigned char classlib_jar[];
extern const size_t classlib_jar_size;

static ZipFile *syslib, *bench;

static uint8_t *read_file(const char *name, size_t *size, bool *from_game) {
    uint8_t *d = zip_read(syslib, name, size);
    if (d) {
        *from_game = false;
        return d;
    }
    *from_game = true;
    return zip_read(bench, name, size);
}

static uint8_t *read_resource(const char *name, size_t *size) {
    return zip_read(bench, name, size);
}

static void exit_request(int status) {
    (void)status;
}

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "vmbench <bench.jar> [lap] [he so tai]\n");
        return 2;
    }
    int reps = argc > 2 ? atoi(argv[2]) : 3;
    double scale = argc > 3 ? atof(argv[3]) : 1.0;

    syslib = zip_open_mem(classlib_jar, classlib_jar_size, false);
    bench = zip_open_file(argv[1]);
    if (!syslib || !bench) {
        fprintf(stderr, "Khong mo duoc jar\n");
        return 1;
    }
    VMHost host = { .read_file = read_file, .read_resource = read_resource, .exit_request = exit_request };
    if (!vm_init(&host)) {
        fprintf(stderr, "vm_init: %s\n", vm_last_error());
        return 1;
    }

    Class *bc = class_load(NULL, "Bench");
    Field *rf = bc ? class_find_field(bc, "result", "I") : NULL;
    if (!rf) {
        fprintf(stderr, "Khong tim thay Bench.result\n");
        return 1;
    }
    Value *result_slot = &bc->statics[rf->slot];

    static const struct {
        const char *name;
        int n;
        jint expect;    // checksum từ JVM thật với tải mặc định
    } tests[] = {
        { "loops", 3000000, -1494167887 }, { "fields", 1000000, -3497990 },
        { "statics", 1000000, 2129123390 }, { "calls", 500000, 18947926 },
        { "arrays", 300, 236734017 },       { "fib", 27, 196418 },
        { "library", 30000, 508853941 },    { "exceptions", 30000, 1708000 },
        { "mixed", 500000, 1815789883 },    { "threads", 20000, 1481370101 },
        { "iface", 1000000, -104393980 },
    };
    int ntests = (int)(sizeof(tests) / sizeof(tests[0]));
    double total = 0;
    int fail = 0;
    for (int i = 0; i < ntests; i++) {
        int n = tests[i].n;
        if (strcmp(tests[i].name, "fib") != 0)
            n = (int)(n * scale);
        double best = 1e30;
        jint sum = 0;
        for (int r = 0; r < reps; r++) {
            Value args[2] = { { .i = i }, { .i = n } };
            double t0 = now_ms();
            if (!vm_spawn_static("Bench", "main", "(II)V", args, 2)) {
                fprintf(stderr, "%s: %s\n", tests[i].name, vm_last_error());
                return 1;
            }
            while (vm_run_slice(1000) != VM_RUN_DEAD)
                ;
            double dt = now_ms() - t0;
            if (vm_last_uncaught()[0]) {
                fprintf(stderr, "%s: loi\n%s\n", tests[i].name, vm_last_uncaught());
                return 1;
            }
            Value ret = result_slot[0];
            if (dt < best)
                best = dt;
            if (r > 0 && ret.i != sum) {
                fprintf(stderr, "%s: checksum doi giua cac lan chay\n", tests[i].name);
                fail = 1;
            }
            sum = ret.i;
        }
        total += best;
        bool ok = scale != 1.0 || sum == tests[i].expect;
        if (!ok)
            fail = 1;
        printf("%-11s %9.1f ms  checksum %d%s\n", tests[i].name, best, (int)sum, ok ? "" : "  SAI");
    }
    printf("%-11s %9.1f ms%s\n", "TONG", total, fail ? "  (CO BAI SAI)" : "");
    vm_shutdown();
    return fail;
}
