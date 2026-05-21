#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define DIR_MATRICES "/mnt/cluster/matrices"

int obtenerTamano(int argc, char *argv[], int rank);
int* reservarMatriz(long elementos);
void calcularReparto(int n, int size, int *sendcounts, int *displs);
void cargarFranja(const char *path, int *buffer, long offset_elementos, long n_elementos);
void multiplicarParcial(const int *Alocal, const int *B, int *Clocal, int filas, int n);

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
    long offset_elementos = (long)displs[rank];

    int *B      = reservarMatriz((long)n * n);
    int *Alocal = reservarMatriz((long)filas_locales * n);
    int *Clocal = reservarMatriz((long)filas_locales * n);
    memset(Clocal, 0, (size_t)filas_locales * n * sizeof(int));

    int *C = NULL;
    if (rank == 0) C = reservarMatriz((long)n * n);

    char rutaA[256], rutaB[256];
    snprintf(rutaA, sizeof(rutaA), "%s/A_%d.bin", DIR_MATRICES, n);
    snprintf(rutaB, sizeof(rutaB), "%s/B_%d.bin", DIR_MATRICES, n);

    cargarFranja(rutaA, Alocal, offset_elementos, (long)filas_locales * n);
    cargarFranja(rutaB, B, 0, (long)n * n);

    MPI_Barrier(MPI_COMM_WORLD);
    double inicio = MPI_Wtime();

    multiplicarParcial(Alocal, B, Clocal, filas_locales, n);

    MPI_Gatherv(Clocal, filas_locales * n, MPI_INT,
                C, sendcounts, displs, MPI_INT,
                0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double fin = MPI_Wtime();

    if (rank == 0) {
        printf("%d %f\n", n, fin - inicio);
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

int* reservarMatriz(long elementos) {
    int *m = (int *)malloc((size_t)elementos * sizeof(int));
    if (!m) {
        fprintf(stderr, "malloc fallo (%ld ints)\n", elementos);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    return m;
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

void cargarFranja(const char *path, int *buffer, long offset_elementos, long n_elementos) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "No se pudo abrir %s\n", path);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if (offset_elementos > 0) {
        if (fseek(f, offset_elementos * (long)sizeof(int), SEEK_SET) != 0) {
            fprintf(stderr, "fseek fallo en %s\n", path);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    size_t leidos = fread(buffer, sizeof(int), (size_t)n_elementos, f);
    if ((long)leidos != n_elementos) {
        fprintf(stderr, "fread incompleto en %s: leidos=%zu esperados=%ld\n", path, leidos, n_elementos);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    fclose(f);
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
