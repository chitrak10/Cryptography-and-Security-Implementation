#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <x86intrin.h>
#include <pthread.h>

// Macro for rotation
#define ROTL(a, b) (((a) << (b)) | ((a) >> (32 - (b))))

// Constants for ChaCha20
static const uint32_t constants[4] = {0x61707865, 0x3320646e, 0x79622d32, 0x6b206574};

static uint32_t u8to32(const uint8_t *p) {
    return ((uint32_t)(p[0]) | ((uint32_t)(p[1]) << 8) | ((uint32_t)(p[2]) << 16) | ((uint32_t)(p[3]) << 24));
}

static void u32to8(uint32_t v, uint8_t *p) {
    p[0] = (v) & 0xff;
    p[1] = (v >> 8) & 0xff;
    p[2] = (v >> 16) & 0xff;
    p[3] = (v >> 24) & 0xff;
}

static void chacha_quarterround(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    *a += *b; *d ^= *a; *d = ROTL(*d, 16);
    *c += *d; *b ^= *c; *b = ROTL(*b, 12);
    *a += *b; *d ^= *a; *d = ROTL(*d, 8);
    *c += *d; *b ^= *c; *b = ROTL(*b, 7);
}

static void chacha_columnround(uint32_t x[16]) {
    chacha_quarterround(&x[0], &x[4], &x[8], &x[12]);
    chacha_quarterround(&x[1], &x[5], &x[9], &x[13]);
    chacha_quarterround(&x[2], &x[6], &x[10], &x[14]);
    chacha_quarterround(&x[3], &x[7], &x[11], &x[15]);
}

static void chacha_diagonalround(uint32_t x[16]) {
    chacha_quarterround(&x[0], &x[5], &x[10], &x[15]);
    chacha_quarterround(&x[1], &x[6], &x[11], &x[12]);
    chacha_quarterround(&x[2], &x[7], &x[8], &x[13]);
    chacha_quarterround(&x[3], &x[4], &x[9], &x[14]);
}

static void chacha_doubleround(uint32_t x[16]) {
    chacha_columnround(x);
    chacha_diagonalround(x);
}

static void chacha20_core(uint8_t out[64], const uint32_t in[16]) {
    uint32_t x[16];
    for (int i = 0; i < 16; ++i) {
        x[i] = in[i];
    }
    for (int i = 0; i < 10; ++i) {
        chacha_doubleround(x);
    }
    for (int i = 0; i < 16; ++i) {
        x[i] += in[i];
    }
    for (int i = 0; i < 16; ++i) {
        u32to8(x[i], out + 4 * i);
    }
}

typedef struct {
    uint32_t input[16];
} chacha20_state_t;

void chacha20_init(chacha20_state_t *state, const uint8_t *key, const uint8_t *nonce) {
    state->input[0] = constants[0];
    state->input[1] = constants[1];
    state->input[2] = constants[2];
    state->input[3] = constants[3];
    state->input[4] = u8to32(key + 0);
    state->input[5] = u8to32(key + 4);
    state->input[6] = u8to32(key + 8);
    state->input[7] = u8to32(key + 12);
    state->input[8] = u8to32(key + 16);
    state->input[9] = u8to32(key + 20);
    state->input[10] = u8to32(key + 24);
    state->input[11] = u8to32(key + 28);
    state->input[12] = 0;  // counter
    state->input[13] = u8to32(nonce + 0);
    state->input[14] = u8to32(nonce + 4);
    state->input[15] = u8to32(nonce + 8);
}

void chacha20_crypt(chacha20_state_t *state, uint8_t *data, size_t len) {
    uint8_t keystream[64];
    size_t pos = 0;
    while (pos < len) {
        chacha20_core(keystream, state->input);
        size_t bytes = (len - pos > 64) ? 64 : (len - pos);
        for (size_t i = 0; i < bytes; ++i) {
            data[pos + i] ^= keystream[i];
        }
        pos += bytes;
        state->input[12]++;
    }
}

void setup_no_interruptions() {
    int num_cores = get_nprocs();
    if (num_cores < 2) {
        fprintf(stderr, "System has only 1 core; cannot assign multiple cores.\n");
        exit(1);
    }

    cpu_set_t cpu_mask;
    CPU_ZERO(&cpu_mask);
    CPU_SET(0, &cpu_mask);
    CPU_SET(1, &cpu_mask);
    if (sched_setaffinity(0, sizeof(cpu_mask), &cpu_mask) == -1) {
        perror("Failed to set CPU affinity");
        exit(1);
    }

    struct sched_param sched_params;
    sched_params.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &sched_params) == -1) {
        perror("Failed to set real-time scheduling");
        exit(1);
    }

    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("Failed to lock memory");
        exit(1);
    }
}

typedef struct {
    chacha20_state_t state;
    uint8_t *data;
    size_t len;
    uint32_t counter_start;
    int core_id;
    uint64_t cycles;
} thread_data_t;

void *encrypt_chunk(void *arg) {
    thread_data_t *td = (thread_data_t *)arg;

    cpu_set_t cpu_mask;
    CPU_ZERO(&cpu_mask);
    CPU_SET(td->core_id, &cpu_mask);
    if (sched_setaffinity(0, sizeof(cpu_mask), &cpu_mask) == -1) {
        perror("Failed to set thread CPU affinity");
        exit(1);
    }

    td->state.input[12] = td->counter_start;
    uint64_t start = __rdtsc();
    chacha20_crypt(&td->state, td->data, td->len);
    uint64_t end = __rdtsc();
    td->cycles = end - start;

    return NULL;
}

static uint32_t lcg_seed = 123456789;
uint32_t lcg_rand() {
    lcg_seed = (1103515245 * lcg_seed + 12345) & 0x7fffffff;
    return lcg_seed;
}

void generate_random(uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        buf[i] = (uint8_t)(lcg_rand() & 0xff);
    }
}

int main() {
    setup_no_interruptions();

    size_t data_len = 1024 * 1024;  // 1 MB
    size_t chunk_len = data_len / 2;  // 512 KB per thread
    uint8_t *data = malloc(data_len);
    uint8_t key[32];
    uint8_t nonce[12];
    if (!data) {
        perror("Failed to allocate memory");
        exit(1);
    }

    const int runs = 1000000;
    uint64_t total_cycles = 0;

    // Warm-up run
    generate_random(data, data_len);
    generate_random(key, sizeof(key));
    generate_random(nonce, sizeof(nonce));
    chacha20_state_t state;
    chacha20_init(&state, key, nonce);
    chacha20_crypt(&state, data, data_len);

    // Run iterations
    for (int i = 0; i < runs; ++i) {
        generate_random(data, data_len);
        generate_random(key, sizeof(key));
        generate_random(nonce, sizeof(nonce));

        thread_data_t thread_data[2];
        pthread_t threads[2];
        for (int t = 0; t < 2; t++) {
            chacha20_init(&thread_data[t].state, key, nonce);
            thread_data[t].data = data + (t * chunk_len);
            thread_data[t].len = chunk_len;
            thread_data[t].counter_start = t * (chunk_len / 64);
            thread_data[t].core_id = t;  // Cores 0 and 1
            thread_data[t].cycles = 0;
        }

        uint64_t start = __rdtsc();
        for (int t = 0; t < 2; t++) {
            if (pthread_create(&threads[t], NULL, encrypt_chunk, &thread_data[t]) != 0) {
                perror("Failed to create thread");
                exit(1);
            }
        }
        for (int t = 0; t < 2; t++) {
            if (pthread_join(threads[t], NULL) != 0) {
                perror("Failed to join thread");
                exit(1);
            }
        }
        uint64_t end = __rdtsc();
        uint64_t max_thread_cycles = thread_data[0].cycles > thread_data[1].cycles ? thread_data[0].cycles : thread_data[1].cycles;
        total_cycles += (end - start) + max_thread_cycles;

        if ((i + 1) % 100000 == 0) {
            printf("Completed %d runs\n", i + 1);
            fflush(stdout);
        }
    }

    double avg_cycles = (double)total_cycles / runs;

    printf("Last encrypted data sample (first 16 bytes, hex): ");
    for (size_t i = 0; i < 16; ++i) {
        printf("%02x ", data[i]);
    }
    printf("\n");

    printf("Data size: %zu bytes\n", data_len);
    printf("Total runs: %d\n", runs);
    printf("Average cycles (rdtsc, total including thread overhead): %.2f\n", avg_cycles);
    printf("Average cycles per byte: %.2f\n", avg_cycles / data_len);

    free(data);
    return 0;
}