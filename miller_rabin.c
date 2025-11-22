#include <gmp.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

// Miller-Rabin test for a single base (returns 1 if probably prime, 0 if composite)
int miller_rabin_single(const mpz_t n, const mpz_t a, int debug, FILE *fp) {
    mpz_t nm1, d, x;
    mpz_init(nm1);
    mpz_init(d);
    mpz_init(x);
    mpz_sub_ui(nm1, n, 1); // nm1 = n - 1
    mpz_set(d, nm1); // d = n - 1
    unsigned long s = 0;
    while (mpz_even_p(d)) {
        mpz_divexact_ui(d, d, 2); // d = d / 2
        s++;
    }
    mpz_powm(x, a, d, n); // x = a^d mod n
    if (debug) {
        char *a_str = mpz_get_str(NULL, 10, a);
        char *d_str = mpz_get_str(NULL, 10, d);
        char *x_str = mpz_get_str(NULL, 10, x);
        fprintf(fp, "  Base a = %s, s = %lu, d = %s, x = %s\n", a_str, s, d_str, x_str);
        fprintf(stdout, "  Base a = %s, s = %lu, d = %s, x = %s\n", a_str, s, d_str, x_str);
        free(a_str);
        free(d_str);
        free(x_str);
    }
    if (mpz_cmp_ui(x, 1) == 0 || mpz_cmp(x, nm1) == 0) {
        if (debug && (mpz_cmp_ui(x, 1) == 0 || mpz_cmp(x, nm1) == 0)) {
            fprintf(fp, "  Passes: x = %s\n", mpz_cmp_ui(x, 1) == 0 ? "1" : "n-1");
            fprintf(stdout, "  Passes: x = %s\n", mpz_cmp_ui(x, 1) == 0 ? "1" : "n-1");
        }
        mpz_clear(nm1);
        mpz_clear(d);
        mpz_clear(x);
        return 1;
    }
    // Perform squaring up to s times, checking for n-1
    for (unsigned long r = 0; r < s; r++) {
        if (r > 0 || s == 1) { // Ensure squaring happens unless s=0
            mpz_powm_ui(x, x, 2, n); // x = x^2 mod n
        }
        if (debug) {
            char *x_str = mpz_get_str(NULL, 10, x);
            fprintf(fp, "  r = %lu, x = %s\n", r, x_str);
            fprintf(stdout, "  r = %lu, x = %s\n", r, x_str);
            free(x_str);
        }
        if (mpz_cmp(x, nm1) == 0) {
            if (debug) {
                fprintf(fp, "  Passes: x = n-1 at r = %lu\n", r);
                fprintf(stdout, "  Passes: x = n-1 at r = %lu\n", r);
            }
            mpz_clear(nm1);
            mpz_clear(d);
            mpz_clear(x);
            return 1;
        }
    }
    if (debug) {
        fprintf(fp, "  Fails: no pass condition met\n");
        fprintf(stdout, "  Fails: no pass condition met\n");
    }
    mpz_clear(nm1);
    mpz_clear(d);
    mpz_clear(x);
    return 0;
}

// Generate a random prime of bitlen bits using Miller-Rabin with k rounds
void generate_prime(mpz_t p, unsigned int bitlen, gmp_randstate_t state, int k, FILE *fp) {
    mpz_t a;
    mpz_init(a);
    while (1) {
        mpz_urandomb(p, state, bitlen);
        mpz_setbit(p, bitlen - 1); // Ensure 256-bit length
        mpz_setbit(p, 0); // Make odd
        if (mpz_cmp_ui(p, 3) <= 0) continue; // Skip small numbers
        int is_prime = 1;
        for (int i = 0; i < k; i++) {
            mpz_urandomm(a, state, p);
            if (mpz_cmp_ui(a, 2) < 0) mpz_set_ui(a, 2);
            if (!miller_rabin_single(p, a, 0, fp)) {
                is_prime = 0;
                break;
            }
        }
        if (is_prime) break;
    }
    mpz_clear(a);
}

// Test Miller-Rabin on a known semiprime (221 = 13 * 17)
void test_known_composite(FILE *fp) {
    mpz_t n, a;
    mpz_init_set_ui(n, 221);
    mpz_init(a);
    gmp_randstate_t state;
    gmp_randinit_mt(state);
    gmp_randseed_ui(state, time(NULL));
    int false_positives = 0;
    for (int i = 0; i < 100; i++) {
        mpz_urandomm(a, state, n);
        if (mpz_cmp_ui(a, 2) < 0) mpz_set_ui(a, 2);
        if (miller_rabin_single(n, a, 1, fp)) {
            false_positives++;
        }
    }
    fprintf(fp, "Test on n=221 (13 * 17): %d false positives out of 100 trials\n", false_positives);
    fprintf(stdout, "Test on n=221 (13 * 17): %d false positives out of 100 trials\n", false_positives);
    mpz_clear(n);
    mpz_clear(a);
    gmp_randclear(state);
}

int main() {
    FILE *fp = fopen("results_miller.txt", "w");
    if (fp == NULL) {
        printf("Error opening results_miller.txt\n");
        return 1;
    }

    gmp_randstate_t state;
    gmp_randinit_mt(state);
    gmp_randseed_ui(state, time(NULL) ^ clock()); // Robust seed

    // Test Miller-Rabin on a known semiprime to verify
    fprintf(fp, "Verifying Miller-Rabin on n = 221 (13 * 17)...\n");
    fprintf(stdout, "Verifying Miller-Rabin on n = 221 (13 * 17)...\n");
    test_known_composite(fp);

    // Part (a): Generate two 256-bit primes and compute n
    mpz_t p, q, n, a;
    mpz_init(p);
    mpz_init(q);
    mpz_init(n);
    mpz_init(a);

    fprintf(fp, "\nGenerating two 256-bit primes...\n");
    fprintf(stdout, "\nGenerating two 256-bit primes...\n");
    generate_prime(p, 256, state, 41, fp);
    generate_prime(q, 256, state, 41, fp);
    mpz_mul(n, p, q);

    char *p_str = mpz_get_str(NULL, 10, p);
    char *q_str = mpz_get_str(NULL, 10, q);
    char *n_str = mpz_get_str(NULL, 10, n);
    fprintf(fp, "p: %s\n", p_str);
    fprintf(fp, "q: %s\n", q_str);
    fprintf(fp, "n: %s\n", n_str);
    fprintf(stdout, "p: %s\n", p_str);
    fprintf(stdout, "q: %s\n", q_str);
    fprintf(stdout, "n: %s\n", n_str);
    free(p_str);
    free(q_str);
    free(n_str);

    // Part (b): Run Miller-Rabin 1,000,000 times on n
    fprintf(fp, "\nRunning Miller-Rabin 1,000,000 times on n...\n");
    fprintf(stdout, "\nRunning Miller-Rabin 1,000,000 times on n...\n");
    unsigned long trials = 1000000;
    unsigned long false_positives = 0;

    for (unsigned long i = 0; i < trials; i++) {
        mpz_urandomm(a, state, n); // Random a in [0, n-1]
        if (mpz_cmp_ui(a, 2) < 0) {
            mpz_set_ui(a, 2); // Ensure a >= 2
        }
        if (miller_rabin_single(n, a, (i < 10) ? 1 : 0, fp)) {
            false_positives++;
            char *a_str = mpz_get_str(NULL, 10, a);
            fprintf(fp, "False positive at trial %lu with a = %s\n", i + 1, a_str);
            fprintf(stdout, "False positive at trial %lu with a = %s\n", i + 1, a_str);
            free(a_str);
        }
    }

    fprintf(fp, "Number of false positives: %lu out of %lu\n", false_positives, trials);
    fprintf(stdout, "Number of false positives: %lu out of %lu\n", false_positives, trials);
    double error_rate = (double)false_positives / trials;
    fprintf(fp, "Experimental error rate: %.6f\n", error_rate);
    fprintf(stdout, "Experimental error rate: %.6f\n", error_rate);

    // Cleanup
    mpz_clear(p);
    mpz_clear(q);
    mpz_clear(n);
    mpz_clear(a);
    gmp_randclear(state);
    fclose(fp);
    return 0;
}