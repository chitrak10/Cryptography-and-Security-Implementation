#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <omp.h>

#define ITER 10000

int cmp_d(const void *a, const void *b) {
    double aa = *(double*)a, bb = *(double*)b;
    return (aa > bb) - (aa < bb);
}

int cmp_ll(const void *a, const void *b) {
    long long aa = *(long long*)a, bb = *(long long*)b;
    return (aa > bb) - (aa < bb);
}

void bubble_sort(int arr[], int n, long long *comp, long long *swp) {
    for (int i = 0; i < n - 1; i++) {
        int swapped = 0;
        for (int j = 0; j < n - i - 1; j++) {
            (*comp)++;
            if (arr[j] > arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
                (*swp)++;
                swapped = 1;
            }
        }
        if (swapped == 0) break;
    }
}

int partition(int arr[], int low, int high, long long *comp, long long *swp) {
    int pivot = arr[high];
    int i = low - 1;
    for (int j = low; j < high; j++) {
        (*comp)++;
        if (arr[j] < pivot) {
            i++;
            int temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
            (*swp)++;
        }
    }
    int temp = arr[i + 1];
    arr[i + 1] = arr[high];
    arr[high] = temp;
    (*swp)++;
    return i + 1;
}

void quick_sort(int arr[], int low, int high, long long *comp, long long *swp) {
    if (low < high) {
        int pi = partition(arr, low, high, comp, swp);
        quick_sort(arr, low, pi - 1, comp, swp);
        quick_sort(arr, pi + 1, high, comp, swp);
    }
}

void merge(int arr[], int l, int m, int r, long long *comp, long long *swp) {
    int n1 = m - l + 1;
    int n2 = r - m;
    int *L = malloc(n1 * sizeof(int));
    int *R = malloc(n2 * sizeof(int));
    for (int i = 0; i < n1; i++) {
        L[i] = arr[l + i];
        (*swp)++;
    }
    for (int j = 0; j < n2; j++) {
        R[j] = arr[m + 1 + j];
        (*swp)++;
    }
    int i = 0, j = 0, k = l;
    while (i < n1 && j < n2) {
        (*comp)++;
        if (L[i] <= R[j]) {
            arr[k] = L[i];
            i++;
        } else {
            arr[k] = R[j];
            j++;
        }
        (*swp)++;
        k++;
    }
    while (i < n1) {
        arr[k] = L[i];
        (*swp)++;
        i++;
        k++;
    }
    while (j < n2) {
        arr[k] = R[j];
        (*swp)++;
        j++;
        k++;
    }
    free(L);
    free(R);
}

void merge_sort(int arr[], int l, int r, long long *comp, long long *swp) {
    if (l < r) {
        int m = l + (r - l) / 2;
        merge_sort(arr, l, m, comp, swp);
        merge_sort(arr, m + 1, r, comp, swp);
        merge(arr, l, m, r, comp, swp);
    }
}

void heapify(int arr[], int n, int i, long long *comp, long long *swp) {
    int largest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;
    if (left < n) {
        (*comp)++;
        if (arr[left] > arr[largest]) largest = left;
    }
    if (right < n) {
        (*comp)++;
        if (arr[right] > arr[largest]) largest = right;
    }
    if (largest != i) {
        int temp = arr[i];
        arr[i] = arr[largest];
        arr[largest] = temp;
        (*swp)++;
        heapify(arr, n, largest, comp, swp);
    }
}

void heap_sort(int arr[], int n, long long *comp, long long *swp) {
    for (int i = n / 2 - 1; i >= 0; i--) {
        heapify(arr, n, i, comp, swp);
    }
    for (int i = n - 1; i > 0; i--) {
        int temp = arr[0];
        arr[0] = arr[i];
        arr[i] = temp;
        (*swp)++;
        heapify(arr, i, 0, comp, swp);
    }
}

int main() {
    srand(time(NULL));
    omp_set_num_threads(12);  // Set to 12 threads for Intel Core i5-12450H
    char *algos[] = {"bubble", "quick", "merge", "heap"};
    int num_algos = 4;
    FILE *fp = fopen("results.txt", "w");
    if (fp == NULL) {
        perror("Error opening file");
        return 1;
    }
    for (int size = 100; size <= 1000; size += 100) {
        for (int a = 0; a < num_algos; a++) {
            char *algo = algos[a];
            double *cycles = malloc(ITER * sizeof(double));
            long long *comps = malloc(ITER * sizeof(long long));
            long long *swaps = malloc(ITER * sizeof(long long));
            long long sum_comps = 0, sum_swaps = 0;
            double sum_cycles = 0.0;
#pragma omp parallel for reduction(+:sum_cycles, sum_comps, sum_swaps)
            for (int iter = 0; iter < ITER; iter++) {
                unsigned int seed = (unsigned int)(time(NULL) ^ (omp_get_thread_num() << 16) ^ iter);
                int *arr = malloc(size * sizeof(int));
                for (int i = 0; i < size; i++) {
                    arr[i] = rand_r(&seed);
                }
                long long c = 0, s = 0;
                clock_t start = clock();
                if (strcmp(algo, "bubble") == 0) {
                    bubble_sort(arr, size, &c, &s);
                } else if (strcmp(algo, "quick") == 0) {
                    quick_sort(arr, 0, size - 1, &c, &s);
                } else if (strcmp(algo, "merge") == 0) {
                    merge_sort(arr, 0, size - 1, &c, &s);
                } else if (strcmp(algo, "heap") == 0) {
                    heap_sort(arr, size, &c, &s);
                }
                clock_t end = clock();
                cycles[iter] = (double)(end - start);
                comps[iter] = c;
                swaps[iter] = s;
                sum_cycles += cycles[iter];
                sum_comps += comps[iter];
                sum_swaps += swaps[iter];
                free(arr);
            }
            qsort(cycles, ITER, sizeof(double), cmp_d);
            qsort(comps, ITER, sizeof(long long), cmp_ll);
            qsort(swaps, ITER, sizeof(long long), cmp_ll);
            double min_cycles = cycles[0];
            double max_cycles = cycles[ITER - 1];
            double avg_cycles = sum_cycles / ITER;
            double med_cycles = (cycles[ITER / 2 - 1] + cycles[ITER / 2]) / 2.0;

            double min_comps = (double)comps[0];
            double max_comps = (double)comps[ITER - 1];
            double avg_comps = (double)sum_comps / ITER;
            double med_comps = (double)(comps[ITER / 2 - 1] + comps[ITER / 2]) / 2.0;

            double min_swaps = (double)swaps[0];
            double max_swaps = (double)swaps[ITER - 1];
            double avg_swaps = (double)sum_swaps / ITER;
            double med_swaps = (double)(swaps[ITER / 2 - 1] + swaps[ITER / 2]) / 2.0;

            double complexity;
            if (strcmp(algo, "bubble") == 0) {
                complexity = (double)size * size;
            } else {
                complexity = (double)size * (log(size) / log(2));
            }
            min_comps /= complexity;
            max_comps /= complexity;
            avg_comps /= complexity;
            med_comps /= complexity;
            min_swaps /= complexity;
            max_swaps /= complexity;
            avg_swaps /= complexity;
            med_swaps /= complexity;

            fprintf(fp, "%s,%d,%.2f,%.2f,%.2f,%.2f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
                   algo, size, min_cycles, max_cycles, avg_cycles, med_cycles,
                   min_comps, max_comps, avg_comps, med_comps,
                   min_swaps, max_swaps, avg_swaps, med_swaps);

            free(cycles);
            free(comps);
            free(swaps);
        }
    }
    fclose(fp);
    return 0;
}