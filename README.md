# Cryptography and Security Implementation

This repository contains C and Python implementations developed for a cryptography and security implementation course. It includes implementations, experiments, reports, and benchmark results for symmetric ciphers, RSA, Miller--Rabin primality testing, and sorting-algorithm performance analysis.

The repository is mainly intended for academic learning, experimentation, and performance measurement. It should not be used as production cryptographic software.

---

## Contents

| File                                        | Description                                                                                                 |
| ------------------------------------------- | ----------------------------------------------------------------------------------------------------------- |
| `aes.c`                                     | AES-128 implementation with a FIPS-style single-block test vector and ECB-mode multi-block encryption demo. |
| `rc4.c`                                     | RC4 stream-cipher implementation with a performance benchmark using `rdtsc`.                                |
| `salsa20.c`                                 | Salsa20 stream-cipher implementation with a two-thread benchmark.                                           |
| `chacha20.c`                                | ChaCha20 stream-cipher implementation with a two-thread benchmark.                                          |
| `rsa.c`                                     | RSA key-generation, encryption, decryption, and prime-generation benchmark using GMP and OpenMP.            |
| `miller_rabin.c`                            | Miller--Rabin primality-testing experiment using GMP.                                                       |
| `sorting.c`                                 | Benchmark comparison of bubble sort, quick sort, merge sort, and heap sort.                                 |
| `analysis_sorting.py`                       | Python script for plotting sorting benchmark results.                                                       |
| `*_results.txt`, `results_*.txt`            | Recorded benchmark/result outputs.                                                                          |
| `*_results.png`, `*_comparison_sorting.png` | Generated result plots.                                                                                     |
| `*.pdf`                                     | Assignment reports and supporting documentation.                                                            |

---

## Implemented topics

### Symmetric cryptography

* AES-128 block encryption
* AES ECB-mode demonstration with PKCS#7-style padding
* RC4 stream cipher
* Salsa20 stream cipher
* ChaCha20 stream cipher

### Public-key cryptography

* RSA key generation
* RSA encryption and decryption
* Prime generation for 512-bit, 768-bit, and 1024-bit prime sizes
* Modular arithmetic using GMP

### Primality testing

* Miller--Rabin primality test
* False-positive experiment on composite numbers
* Generation of 256-bit primes

### Algorithm benchmarking

* Bubble sort
* Quick sort
* Merge sort
* Heap sort
* Comparison of clock cycles, comparisons, and swaps
* Python-based result visualization

---

## Requirements

### System requirements

The C programs are written primarily for Linux/x86 systems because some benchmark files use:

* `rdtsc` / `__rdtsc()` for cycle measurement,
* CPU affinity functions,
* real-time scheduling calls,
* memory locking calls,
* OpenMP,
* POSIX threads.

Recommended environment:

```bash
sudo apt update
sudo apt install build-essential libgmp-dev python3 python3-pip
pip install pandas matplotlib
```

For OpenMP support, GCC is recommended.

---

## Building the programs

The repository does not currently include a Makefile, so each program can be compiled manually.

```bash
# AES-128 demo
gcc -O2 aes.c -o aes

# Miller-Rabin primality test
gcc -O2 miller_rabin.c -o miller_rabin -lgmp

# RSA implementation and benchmark
gcc -O2 rsa.c -o rsa -lgmp -fopenmp

# Sorting benchmark
gcc -O2 sorting.c -o sorting -fopenmp -lm

# RC4 benchmark
gcc -O2 rc4.c -o rc4

# ChaCha20 benchmark
gcc -O2 chacha20.c -o chacha20 -pthread

# Salsa20 benchmark
gcc -O2 salsa20.c -o salsa20 -pthread
```

A compile note for Miller--Rabin is also included in `compile_miler_rabin.txt`.

---

## Running the programs

### AES

```bash
./aes
```

This runs:

* a single-block AES-128 test vector, and
* a multi-block ECB-mode encryption example.

---

### Miller--Rabin

```bash
./miller_rabin
```

This generates output in:

```text
results_miller.txt
```

The program verifies Miller--Rabin on a known composite number and then runs a larger experiment using generated 256-bit primes.

---

### RSA

```bash
./rsa
```

This creates a `results/` directory and writes RSA benchmark output such as:

```text
results/rsa_results.txt
results/result_512_run_*.txt
results/result_768_run_*.txt
results/result_1024_run_*.txt
```

The program benchmarks prime generation and then performs RSA encryption/decryption checks for different prime sizes.

---

### Sorting benchmark

```bash
./sorting
```

This writes sorting benchmark data to:

```text
results.txt
```

To generate plots:

```bash
python3 analysis_sorting.py
```

This produces:

```text
cycles_comparison.png
comps_comparison.png
swaps_comparison.png
```

The repository also contains saved sorting-result files and plots, including `results_sorting.txt`, `cycles_comparison_sorting.png`, `comps_comparison_sorting.png`, and `swaps_comparison_sorting.png`.

If you want to plot the saved sorting result file directly, either rename/copy it:

```bash
cp results_sorting.txt results.txt
python3 analysis_sorting.py
```

or update `analysis_sorting.py` to read `results_sorting.txt`.

---

### RC4, Salsa20, and ChaCha20 benchmarks

```bash
./rc4
./salsa20
./chacha20
```

These programs run large benchmark loops and print average cycle counts. They may require Linux-specific permissions because they attempt to set CPU affinity, use real-time scheduling, and lock memory.

If a program exits with errors such as:

```text
Failed to set real-time scheduling
Failed to lock memory
```

run it in an environment where those operations are permitted, or remove/adjust the benchmarking setup code if you only want a normal functional run.

---

## Reports and result files

The repository includes assignment/report PDFs and recorded outputs:

| File                             | Purpose                                          |
| -------------------------------- | ------------------------------------------------ |
| `AES_CRS2404.pdf`                | AES assignment/report material.                  |
| `REPORT_RSA_CRS2404.pdf`         | RSA assignment/report material.                  |
| `RSA_Assignment.pdf`             | RSA assignment statement or supporting material. |
| `assg2_CRS2404_miller_rabin.pdf` | Miller--Rabin assignment/report material.        |
| `sorting_crs2404.pdf`            | Sorting benchmark report material.               |
| `rc4_results.png`                | RC4 benchmark result image.                      |
| `salsa20_results.png`            | Salsa20 benchmark result image.                  |
| `chacha20_results.png`           | ChaCha20 benchmark result image.                 |

---

## Security notes

This repository is for educational use. The code should not be used to protect real data.

Important limitations:

* RC4 is cryptographically broken and should not be used in modern systems.
* AES ECB mode leaks structural patterns and is not secure for real message encryption.
* The benchmark programs use simple pseudo-random data generation for experiments, not secure randomness.
* The implementations are not audited for side-channel resistance.
* The code is not guaranteed to be constant-time.
* RSA examples are for learning and benchmarking, not deployment.
* Performance results depend heavily on CPU, compiler, operating system, and runtime configuration.

For real-world cryptographic applications, use well-reviewed libraries such as OpenSSL, libsodium, BoringSSL, or platform-provided cryptographic APIs.

---

## Reproducibility notes

Some results may differ between machines because the code uses CPU cycle counters, OpenMP parallelism, thread scheduling, and machine-specific performance behavior.

For more stable benchmark comparisons:

* run on the same machine,
* close unnecessary background processes,
* use the same compiler and optimization flags,
* repeat experiments multiple times,
* record CPU model and compiler version,
* avoid comparing results across very different systems.

---

## Acknowledgement

The original repository notes that the implementations were completed with help from course materials and AI tools such as Gemini and Grok. The code and reports should therefore be treated as academic implementation work that may require review, cleanup, and validation before reuse.

---

## License

No license file is currently included in the repository. Add a license before publishing or allowing reuse by others.
