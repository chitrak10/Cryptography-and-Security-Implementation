#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include <sys/stat.h>

// Function to get clock cycles using RDTSC on x86_64
unsigned long long get_cycles() {
    unsigned long long low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return (high << 32) | low;
}

// Function to benchmark prime generation for all bit sizes in batches
void benchmark_prime_gen(gmp_randstate_t rand_state, int total_iterations, int reps) {
    // Create results folder
    struct stat st = {0};
    if (stat("results", &st) == -1) {
        mkdir("results", 0700);
    }

    int bit_sizes[] = {512, 768, 1024};
    int num_bit_sizes = 3;

    unsigned long long min_cycles[3] = {~0ULL, ~0ULL, ~0ULL};
    unsigned long long max_cycles[3] = {0, 0, 0};
    unsigned long long total_cycles[3] = {0, 0, 0};
    int count[3] = {0, 0, 0};

    const int batch_size = 1000;
    const int num_batches = total_iterations / batch_size;

    printf("Benchmarking prime generation in batches of %d iterations across bit sizes (%d batches)...\n", batch_size, num_batches);

    for (int batch = 1; batch <= num_batches; batch++) {
        for (int b = 0; b < num_bit_sizes; b++) {
            int bit_size = bit_sizes[b];

            unsigned long long batch_min = ~0ULL;
            unsigned long long batch_max = 0;
            unsigned long long batch_total = 0;

            #pragma omp parallel num_threads(2) reduction(min:batch_min) reduction(max:batch_max) reduction(+:batch_total)
            {
                gmp_randstate_t local_rand_state;
                gmp_randinit_mt(local_rand_state);
                unsigned int thread_seed = (unsigned long)time(NULL) ^ omp_get_thread_num();
                gmp_randseed_ui(local_rand_state, thread_seed);

                mpz_t p, q;
                mpz_init(p);
                mpz_init(q);

                #pragma omp for schedule(dynamic)
                for (int j = 0; j < batch_size; j++) {
                    unsigned long long start = get_cycles();

                    // Generate prime p
                    int is_prime_p = 0;
                    while (!is_prime_p) {
                        mpz_urandomb(p, local_rand_state, bit_size);
                        mpz_setbit(p, bit_size - 1);
                        if (mpz_even_p(p)) {
                            mpz_add_ui(p, p, 1);
                        }
                        is_prime_p = mpz_probab_prime_p(p, reps);
                    }

                    // Generate prime q
                    int is_prime_q = 0;
                    while (!is_prime_q) {
                        mpz_urandomb(q, local_rand_state, bit_size);
                        mpz_setbit(q, bit_size - 1);
                        if (mpz_even_p(q)) {
                            mpz_add_ui(q, q, 1);
                        }
                        is_prime_q = mpz_probab_prime_p(q, reps);
                    }

                    unsigned long long end = get_cycles();
                    unsigned long long cycles = end - start;

                    if (cycles < batch_min) batch_min = cycles;
                    if (cycles > batch_max) batch_max = cycles;
                    batch_total += cycles;
                }

                mpz_clear(p);
                mpz_clear(q);
                gmp_randclear(local_rand_state);
            }

            // Update global stats
            count[b] += batch_size;
            total_cycles[b] += batch_total;
            if (batch_min < min_cycles[b]) min_cycles[b] = batch_min;
            if (batch_max > max_cycles[b]) max_cycles[b] = batch_max;

            // Save partial results
            char filename[100];
            snprintf(filename, sizeof(filename), "results/result_%d_run_%d.txt", bit_size, count[b]);
            FILE *fp = fopen(filename, "w");
            if (fp) {
                double avg = (double)total_cycles[b] / count[b];
                fprintf(fp, "Partial results up to %d iterations for %d-bit primes:\n", count[b], bit_size);
                fprintf(fp, "Minimum clock cycles: %llu\n", min_cycles[b]);
                fprintf(fp, "Maximum clock cycles: %llu\n", max_cycles[b]);
                fprintf(fp, "Average clock cycles: %.2f\n", avg);
                fclose(fp);
            } else {
                printf("Error: Could not write to %s\n", filename);
            }

            printf("Completed batch %d/%d for %d-bit primes (%d iterations)\n", batch, num_batches, bit_size, count[b]);
        }
    }

    // Final print for Step 1
    for (int b = 0; b < num_bit_sizes; b++) {
        int bit_size = bit_sizes[b];
        double final_avg = (double)total_cycles[b] / count[b];
        printf("\nStep 1: Prime Generation for %d-bit primes (final)\n", bit_size);
        printf("Minimum clock cycles: %llu\n", min_cycles[b]);
        printf("Maximum clock cycles: %llu\n", max_cycles[b]);
        printf("Average clock cycles: %.2f\n", final_avg);
    }
}

// Function to perform RSA operations for a given bit size
void rsa_operations(int bit_size, gmp_randstate_t rand_state, int reps, FILE *fp) {
    mpz_t p, q;
    mpz_init(p);
    mpz_init(q);

    int is_prime_p = 0;
    while (!is_prime_p) {
        mpz_urandomb(p, rand_state, bit_size);
        mpz_setbit(p, bit_size - 1);
        if (mpz_even_p(p)) {
            mpz_add_ui(p, p, 1);
        }
        is_prime_p = mpz_probab_prime_p(p, reps);
    }

    int is_prime_q = 0;
    while (!is_prime_q) {
        mpz_urandomb(q, rand_state, bit_size);
        mpz_setbit(q, bit_size - 1);
        if (mpz_even_p(q)) {
            mpz_add_ui(q, q, 1);
        }
        is_prime_q = mpz_probab_prime_p(q, reps);
        if (is_prime_q && mpz_cmp(p, q) == 0) {
            is_prime_q = 0;
        }
    }

    unsigned long long start_step2 = get_cycles();
    mpz_t N, phi;
    mpz_init(N);
    mpz_mul(N, p, q);
    mpz_t p_minus_1, q_minus_1;
    mpz_init(p_minus_1);
    mpz_init(q_minus_1);
    mpz_sub_ui(p_minus_1, p, 1);
    mpz_sub_ui(q_minus_1, q, 1);
    mpz_init(phi);
    mpz_mul(phi, p_minus_1, q_minus_1);
    unsigned long long end_step2 = get_cycles();
    unsigned long long cycles_step2 = end_step2 - start_step2;

    printf("\nStep 2: Compute N and phi(N) for %d-bit primes\n", bit_size);
    printf("Clock cycles: %llu\n", cycles_step2);
    fprintf(fp, "\nStep 2: Compute N and phi(N) for %d-bit primes\n", bit_size);
    fprintf(fp, "Clock cycles: %llu\n", cycles_step2);

    mpz_t e;
    mpz_init(e);
    mpz_set_ui(e, 65537);

    unsigned long long start_step3 = get_cycles();
    mpz_t d;
    mpz_init(d);
    if (mpz_invert(d, e, phi) == 0) {
        printf("Error: e is not coprime with phi(N) for %d-bit primes.\n", bit_size);
        fprintf(fp, "Error: e is not coprime with phi(N) for %d-bit primes.\n", bit_size);
        mpz_clear(p);
        mpz_clear(q);
        mpz_clear(N);
        mpz_clear(phi);
        mpz_clear(p_minus_1);
        mpz_clear(q_minus_1);
        mpz_clear(e);
        mpz_clear(d);
        return;
    }
    unsigned long long end_step3 = get_cycles();
    unsigned long long cycles_step3 = end_step3 - start_step3;

    printf("\nStep 3: Public and Private Key Generation for %d-bit primes\n", bit_size);
    printf("Clock cycles for computing d: %llu\n", cycles_step3);
    fprintf(fp, "\nStep 3: Public and Private Key Generation for %d-bit primes\n", bit_size);
    fprintf(fp, "Clock cycles for computing d: %llu\n", cycles_step3);

    mpz_t m;
    mpz_init(m);
    mpz_urandomb(m, rand_state, 1023);
    if (mpz_cmp(m, N) >= 0) {
        mpz_sub(m, m, N);
    }

    mpz_t c;
    mpz_init(c);
    unsigned long long start_enc = get_cycles();
    mpz_powm_sec(c, m, e, N);
    unsigned long long end_enc = get_cycles();
    unsigned long long cycles_enc = end_enc - start_enc;

    printf("\nStep 4: Encryption for %d-bit primes (c = m^e mod N)\n", bit_size);
    printf("Clock cycles: %llu\n", cycles_enc);
    fprintf(fp, "\nStep 4: Encryption for %d-bit primes (c = m^e mod N)\n", bit_size);
    fprintf(fp, "Clock cycles: %llu\n", cycles_enc);

    mpz_t m_dec;
    mpz_init(m_dec);
    unsigned long long start_dec = get_cycles();
    mpz_powm_sec(m_dec, c, d, N);
    unsigned long long end_dec = get_cycles();
    unsigned long long cycles_dec = end_dec - start_dec;

    printf("\nStep 4: Decryption for %d-bit primes (m' = c^d mod N)\n", bit_size);
    printf("Clock cycles: %llu\n", cycles_dec);
    fprintf(fp, "\nStep 4: Decryption for %d-bit primes (m' = c^d mod N)\n", bit_size);
    fprintf(fp, "Clock cycles: %llu\n", cycles_dec);

    if (mpz_cmp(m, m_dec) == 0) {
        printf("Verification: Decryption successful (m == m') for %d-bit primes\n", bit_size);
        fprintf(fp, "Verification: Decryption successful (m == m') for %d-bit primes\n", bit_size);
    } else {
        printf("Verification: Decryption failed for %d-bit primes\n", bit_size);
        fprintf(fp, "Verification: Decryption failed for %d-bit primes\n", bit_size);
    }

    mpz_clear(p);
    mpz_clear(q);
    mpz_clear(N);
    mpz_clear(phi);
    mpz_clear(p_minus_1);
    mpz_clear(q_minus_1);
    mpz_clear(e);
    mpz_clear(d);
    mpz_clear(m);
    mpz_clear(c);
    mpz_clear(m_dec);
}

int main() {
    gmp_randstate_t rand_state;
    gmp_randinit_mt(rand_state);
    gmp_randseed_ui(rand_state, (unsigned long)time(NULL));

    const int reps = 5;
    const int iterations = 1000000;

    // Set OpenMP to use 2 threads
    omp_set_num_threads(2);

    // Create results folder
    struct stat st = {0};
    if (stat("results", &st) == -1) {
        mkdir("results", 0700);
    }

    const char *results_file = "results/rsa_results.txt";
    FILE *fp_results = fopen(results_file, "w");
    if (!fp_results) {
        printf("Error: Could not open %s for writing.\n", results_file);
        return 1;
    }

    // Step 1: Prime generation with batching
    benchmark_prime_gen(rand_state, iterations, reps);

    // Steps 2-4 for each bit size
    int bit_sizes[] = {512, 768, 1024};
    for (int i = 0; i < 3; i++) {
        int bit_size = bit_sizes[i];
        fprintf(fp_results, "\n=== Processing %d-bit primes ===\n", bit_size);
        printf("\n=== Processing %d-bit primes ===\n", bit_size);
        rsa_operations(bit_size, rand_state, reps, fp_results);
    }

    fclose(fp_results);
    gmp_randclear(rand_state);
    return 0;
}