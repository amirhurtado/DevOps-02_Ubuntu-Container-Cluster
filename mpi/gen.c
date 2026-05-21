#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <n> <archivoA> <archivoB>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    if (n <= 0) {
        fprintf(stderr, "n debe ser positivo\n");
        return 1;
    }

    FILE *fA = fopen(argv[2], "wb");
    FILE *fB = fopen(argv[3], "wb");
    if (!fA || !fB) {
        perror("fopen");
        return 1;
    }

    int *bufA = (int *)malloc((size_t)n * sizeof(int));
    int *bufB = (int *)malloc((size_t)n * sizeof(int));
    if (!bufA || !bufB) {
        fprintf(stderr, "malloc fallo\n");
        return 1;
    }

    srand(42 + n);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            bufA[j] = rand() % 10;
            bufB[j] = rand() % 10;
        }
        fwrite(bufA, sizeof(int), n, fA);
        fwrite(bufB, sizeof(int), n, fB);
    }

    free(bufA);
    free(bufB);
    fclose(fA);
    fclose(fB);
    return 0;
}
