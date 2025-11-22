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

// Constants for 256-bit key
static const uint32_t sigma[4] = {0x61707865, 0x3320646e, 0x79622d32, 0x6b206574};

static uint32_t u8to32(const uint8_t *p) {
    return ((uint32_t)(p[0]) | ((uint32_t)(p[1]) << 8) | ((uint32_t)(p[2]) << 16) | ((uint32_t)(p[3]) << 24));
}

static void u32to8(uint32_t v, uint8_t *p) {
    p[0] = (v) & 0xff;
    p[1] = (v >> 8) & 0xff;
    p[2] = (v >> 16) & 0xff;
    p[3] = (v >> 24) & 0xff;
}

static void quarterround(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    *b ^= ROTL(*a + *d, 7);
    *c ^= ROTL(*b + *a, 9);
    *d ^= ROTL(*c + *b, 13);
    *a ^= ROTL(*d + *c, 18);
}

static void rowround(uint32_t y[16]) {
    quarterround(&y[0], &y[1], &y[2], &y[3]);
    quarterround(&y[4], &y[5], &y[6], &y[7]);
    quarterround(&y[8], &y[9], &y[10], &y[11]);
    quarterround(&y[12], &y[13], &y[14], &y[15]);
}

static void columnround(uint32_t y[16]) {
    quarterround(&y[0], &y[4], &y[8], &y[12]);
    quarterround(&y[5], &y[9], &y[13], &y[1]);
    quarterround(&y[10], &y[14], &y[2], &y[6]);
    quarterround(&y[15], &y[3], &y[7], &y[11]);
}

static void doubleround(uint32_t y[16]) {
    rowround(y);
    columnround(y);
}

static void salsa20_core(uint8_t out[64], const uint32_t in[16]) {
    uint32_t x[16];
    for (int i = 0; i < 16; ++i) {
        x[i] = in[i];
    }
    for (int i = 0; i < 10; ++i) {
        doubleround(x);
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
} salsa20_state_t;

void salsa20_init(salsa20_state_t *state, const uint8_t *key, const uint8_t *nonce) {
    state->input[0] = sigma[0];
    state->input[1] = u8to32(key + 0);
    state->input[2] = u8to32(key + 4);
    state->input[3] = u8to32(key + 8);
    state->input[4] = u8to32(key + 12);
    state->input[5] = sigma[1];
    state->input[6] = u8to32(nonce + 0);
    state->input[7] = u8to32(nonce + 4);
    state->input[8] = 0;
    state->input[9] = 0;
    state->input[10] = sigma[2];
    state->input[11] = u8to32(key + 16);
    state->input[12] = u8to32(key + 20);
    state->input[13] = u8to32(key + 24);
    state->input[14] = u8to32(key + 28);
    state->input[15] = sigma[3];
}

void salsa20_crypt(salsa20_state_t *state, uint8_t *data, size_t len) {
    uint8_t keystream[64];
    size_t pos = 0;
    while (pos < len) {
        salsa20_core(keystream, state->input);
        size_t bytes = (len - pos > 64) ? 64 : (len - pos);
        for (size_t i = 0; i < bytes; ++i) {
            data[pos + i] ^= keystream[i];
        }
        pos += bytes;
        state->input[8]++;
        if (state->input[8] == 0) {
            state->input[9]++;
        }
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
    salsa20_state_t state;
    uint8_t *data;
    size_t len;
    uint32_t counter_low;
    uint32_t counter_high;
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

    td->state.input[8] = td->counter_low;
    td->state.input[9] = td->counter_high;
    uint64_t start = __rdtsc();
    salsa20_crypt(&td->state, td->data, td->len);
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
    uint8_t nonce[8];
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
    salsa20_state_t state;
    salsa20_init(&state, key, nonce);
    salsa20_crypt(&state, data, data_len);

    // Run iterations
    for (int i = 0; i < runs; ++i) {
        generate_random(data, data_len);
        generate_random(key, sizeof(key));
        generate_random(nonce, sizeof(nonce));

        thread_data_t thread_data[2];
        pthread_t threads[2];
        for (int t = 0; t < 2; t++) {
            salsa20_init(&thread_data[t].state, key, nonce);
            thread_data[t].data = data + (t * chunk_len);
            thread_data[t].len = chunk_len;
            thread_data[t].counter_low = t * (chunk_len / 64);  // 512 KB = 8192 blocks
            thread_data[t].counter_high = (thread_data[t].counter_low >> 32);
            thread_data[t].counter_low = thread_data[t].counter_low & 0xFFFFFFFF;
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