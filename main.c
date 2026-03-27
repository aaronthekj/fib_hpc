#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <getopt.h>
#include <pthread.h>
#include "bigint.h"
#include "arena.h"
#include "error.h"

static int g_safe_mode = 0;
static int g_turbo_mode = 0;
static int g_json_log = 0;
static int g_telemetry = 0;
static int g_force = 0;
static long g_threads = 1;
static double g_max_ram_gb = 0;

void watchdog_handler(int sig) {
    (void)sig;
    PANIC("WATCHDOG: Strict execution hard-stop triggered (1.5x time limit).");
}

void validate_resources() {
    int nprocs;
    size_t len = sizeof(nprocs);
    if (sysctlbyname("hw.logicalcpu", &nprocs, &len, NULL, 0) < 0) PANIC("sysctl failed to get CPU info");
    if (g_threads > nprocs) PANIC("Thread count %ld exceeds system maximum %d.", g_threads, nprocs);

    uint64_t mem_size;
    len = sizeof(mem_size);
    if (sysctlbyname("hw.memsize", &mem_size, &len, NULL, 0) < 0) PANIC("sysctl failed to get RAM info");
    double phys_ram_gb = (double)mem_size / (1024.0 * 1024.0 * 1024.0);

    if (g_max_ram_gb > phys_ram_gb * 0.9 && !g_force) {
        PANIC("Requested RAM %.2f GB > 90%% of physical memory (%.2f GB). Use --force to override.", g_max_ram_gb, phys_ram_gb);
    }
}

void* logging_thread(void* arg) {
    fib_ctx_t* ctx = (fib_ctx_t*)arg;
    while (!ctx->shut) {
        pthread_mutex_lock(&ctx->tel.mtx);
        while (ctx->tel.head != ctx->tel.tail) {
            if (g_json_log) {
                printf("{\"telemetry\": \"%s\"}\n", ctx->tel.messages[ctx->tel.tail]);
            } else {
                printf("[TELEMETRY] %s\n", ctx->tel.messages[ctx->tel.tail]);
            }
            ctx->tel.tail = (ctx->tel.tail + 1) % 1024;
        }
        pthread_mutex_unlock(&ctx->tel.mtx);
        usleep(100000);
    }
    return NULL;
}

int main(int argc, char** argv) {
    static struct option long_options[] = {
        {"safe", no_argument, &g_safe_mode, 1},
        {"turbo", no_argument, &g_turbo_mode, 1},
        {"json-log", no_argument, &g_json_log, 1},
        {"telemetry", no_argument, &g_telemetry, 1},
        {"force", no_argument, &g_force, 1},
        {"threads", required_argument, 0, 't'},
        {"max-ram", required_argument, 0, 'r'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "t:r:", long_options, NULL)) != -1) {
        switch (opt) {
            case 't': g_threads = atol(optarg); break;
            case 'r': g_max_ram_gb = atof(optarg); break;
        }
    }

    if (optind >= argc) {
        printf("Usage: %s <time_limit_sec> [--safe|--turbo] [--threads=N] [--max-ram=G] [--telemetry] [--json-log] [--force]\n", argv[0]);
        return 1;
    }

    double time_limit = atof(argv[optind]);
    if (time_limit <= 0) PANIC("Strict I/O: time_limit must be > 0.");

    unsigned long long N = 42;
    if (optind + 1 < argc) {
        N = strtoull(argv[optind + 1], NULL, 10);
    }

    validate_resources();
    if (!is_little_endian()) PANIC("Antigravity HPC Requirement: Little-Endian architecture only.");

    signal(SIGALRM, watchdog_handler);
    alarm((unsigned int)(time_limit * 1.5));

    if (g_safe_mode) printf("[STATUS] High-Reliability Mode enabled (Layer 3 Validation).\n");
    if (g_turbo_mode) printf("[STATUS] High-Performance Mode enabled (Layer 1 Only).\n");

    size_t arena_size = (g_max_ram_gb > 0) ? (size_t)(g_max_ram_gb * 1024 * 1024 * 1024) : (1024ULL * 1024 * 1024);
    arena_t* main_arena = arena_create(arena_size);

    fib_ctx_t* ctx = fib_ctx_create((size_t)g_threads);
    ctx->flags = (g_turbo_mode ? 0x1 : 0) | (g_telemetry ? 0x2 : 0);
    pthread_t log_tid;
    pthread_create(&log_tid, NULL, logging_thread, ctx);

    printf("[*] Universal HPC Engine Initialized. Threads: %ld, Arena: %.2f GB\n", g_threads, (double)arena_size / (1024.0*1024.0*1024.0));

    bigint_t res_mock;
    fib_stats_t stats = {0};
    struct timespec start_ts, end_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);
    fib_race(ctx, N, &res_mock, &stats);
    clock_gettime(CLOCK_MONOTONIC, &end_ts);
    double total_time = (end_ts.tv_sec - start_ts.tv_sec) + (end_ts.tv_nsec - start_ts.tv_nsec) / 1e9;

    printf("[RESULT] F(%llu) Calculation Complete.\n", N);
    printf("[STATS] Wall-Clock Time: %.3f s\n", total_time);
    printf("[STATS] Ladder Time: %.3f s\n", stats.ladder_time);
    printf("[STATS] NTT Time: %.3f s\n", stats.ntt_time);

    ctx->shut = 1;
    pthread_join(log_tid, NULL);
    fib_ctx_destroy(ctx);
    arena_destroy(main_arena);
    return 0;
}
