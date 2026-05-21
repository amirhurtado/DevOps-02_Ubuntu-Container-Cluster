#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

int obtenerTamano(int argc, char *argv[], int rank);
int* reservarMatriz(int elementos);
void inicializarMatrices(int *A, int *B, int n);
void multiplicarParcial(const int *Alocal, const int *B, int *Clocal, int filas, int n);
void calcularReparto(int n, int size, int *sendcounts, int *displs);

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int n = obtenerTamano(argc, argv, rank);
    if (n <= 0) { MPI_Finalize(); return 1; }

    int *sendcounts = (int *)malloc(size * sizeof(int));
    int *displs     = (int *)malloc(size * sizeof(int));
    calcularReparto(n, size, sendcounts, displs);

    int filas_locales = sendcounts[rank] / n;

    int *A = NULL, *C = NULL;
    int *B      = reservarMatriz(n * n);
    int *Alocal = reservarMatriz(filas_locales * n);
    int *Clocal = reservarMatriz(filas_locales * n);
    memset(Clocal, 0, filas_locales * n * sizeof(int));

    if (rank == 0) {
        A = reservarMatriz(n * n);
        C = reservarMatriz(n * n);
        inicializarMatrices(A, B, n);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double inicio = MPI_Wtime();

    MPI_Bcast(B, n * n, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatterv(A, sendcounts, displs, MPI_INT,
                 Alocal, filas_locales * n, MPI_INT,
                 0, MPI_COMM_WORLD);

    multiplicarParcial(Alocal, B, Clocal, filas_locales, n);

    MPI_Gatherv(Clocal, filas_locales * n, MPI_INT,
                C, sendcounts, displs, MPI_INT,
                0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double fin = MPI_Wtime();

    if (rank == 0) {
        printf("%d %f\n", n, fin - inicio);
        free(A);
        free(C);
    }

    free(B);
    free(Alocal);
    free(Clocal);
    free(sendcounts);
    free(displs);

    MPI_Finalize();
    return 0;
}

int obtenerTamano(int argc, char *argv[], int rank) {
    if (argc != 2) {
        if (rank == 0) fprintf(stderr, "Uso: %s <tamano_matriz>\n", argv[0]);
        return -1;
    }
    int n = atoi(argv[1]);
    if (n <= 0) {
        if (rank == 0) fprintf(stderr, "El tamaño debe ser mayor que 0\n");
        return -1;
    }
    return n;
}

int* reservarMatriz(int elementos) {
    int *m = (int *)malloc((size_t)elementos * sizeof(int));
    if (!m) {
        fprintf(stderr, "malloc fallo (%d ints)\n", elementos);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    return m;
}

void inicializarMatrices(int *A, int *B, int n) {
    srand(42);
    for (int i = 0; i < n * n; i++) {
        A[i] = rand() % 10;
        B[i] = rand() % 10;
    }
}

void multiplicarParcial(const int *Alocal, const int *B, int *Clocal, int filas, int n) {
    for (int i = 0; i < filas; i++) {
        for (int k = 0; k < n; k++) {
            int aik = Alocal[i * n + k];
            for (int j = 0; j < n; j++) {
                Clocal[i * n + j] += aik * B[k * n + j];
            }
        }
    }
}

void calcularReparto(int n, int size, int *sendcounts, int *displs) {
    int base = n / size;
    int resto = n % size;
    int offset = 0;
    for (int i = 0; i < size; i++) {
        int filas_i = base + (i < resto ? 1 : 0);
        sendcounts[i] = filas_i * n;
        displs[i] = offset;
        offset += filas_i * n;
    }
}
