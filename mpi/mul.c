#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int obtenerTamano(int argc, char *argv[]);
int** reservarMatriz(int n);
void liberarMatriz(int **matriz, int n);
void inicializarMatrices(int **A, int **B, int **C, int n);
void multiplicarMatrices(int **A, int **B, int **C, int n);
double medirTiempoMultiplicacion(int **A, int **B, int **C, int n);

int main(int argc, char *argv[]) {

    int n = obtenerTamano(argc, argv);
    if (n <= 0) return 1;

    int **A = reservarMatriz(n);
    int **B = reservarMatriz(n);
    int **C = reservarMatriz(n);

    inicializarMatrices(A, B, C, n);

    double tiempo = medirTiempoMultiplicacion(A, B, C, n);

    printf("%d %f\n", n, tiempo);

    liberarMatriz(A, n);
    liberarMatriz(B, n);
    liberarMatriz(C, n);

    return 0;
}


int obtenerTamano(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <tamano_matriz>\n", argv[0]);
        return -1;
    }

    int n = atoi(argv[1]);

    if (n <= 0) {
        printf("El tamaño debe ser mayor que 0\n");
        return -1;
    }

    return n;
}

int** reservarMatriz(int n) {
    int **matriz = (int **)malloc(n * sizeof(int *));
    for (int i = 0; i < n; i++) {
        matriz[i] = (int *)malloc(n * sizeof(int));
    }
    return matriz;
}

void liberarMatriz(int **matriz, int n) {
    for (int i = 0; i < n; i++) {
        free(matriz[i]);
    }
    free(matriz);
}

void inicializarMatrices(int **A, int **B, int **C, int n) {
    srand(time(NULL));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            A[i][j] = rand() % 10;
            B[i][j] = rand() % 10;
            C[i][j] = 0;
        }
    }
}

void multiplicarMatrices(int **A, int **B, int **C, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            for (int k = 0; k < n; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

double medirTiempoMultiplicacion(int **A, int **B, int **C, int n) {
    struct timespec inicio, fin;

    clock_gettime(CLOCK_MONOTONIC, &inicio);

    multiplicarMatrices(A, B, C, n);

    clock_gettime(CLOCK_MONOTONIC, &fin);

    return (fin.tv_sec - inicio.tv_sec) +
           (fin.tv_nsec - inicio.tv_nsec) / 1e9;
}