#include <stdio.h>
#include <mpi.h>

#define N 40

int main(int argc, char *argv[]) {
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* Verifica se N é divisível pelo número de processos */
    if (N % size != 0) {
        if (rank == 0) {
            fprintf(stderr, "[ERRO] N=%d não é divisível pelo número de processos (%d).\n", N, size);
        }
        MPI_Finalize();
        return 1;
    }

    int chunk = N / size;          /* Quantidade de elementos por processo */
    int vetor_global[N];           /* Apenas o root usa este vetor          */
    int vetor_local[chunk];        /* Cada processo recebe sua fatia        */

    /* Passo 1: root inicializa o vetor global com 1..N */
    if (rank == 0) {
        for (int i = 0; i < N; i++) {
            vetor_global[i] = i + 1;
        }
    }

    /* Passo 2: distribui igualmente entre os processos */
    MPI_Scatter(vetor_global, chunk, MPI_INT,
                vetor_local,  chunk, MPI_INT,
                0, MPI_COMM_WORLD);

    /* Exibe o vetor local de cada processo */
    printf("Processo %d recebeu:", rank);
    for (int i = 0; i < chunk; i++) {
        printf(" %d", vetor_local[i]);
    }
    printf("\n");

    /* Passo 3: cada processo calcula a soma dos quadrados locais */
    long long soma_local = 0;
    for (int i = 0; i < chunk; i++) {
        soma_local += (long long)vetor_local[i] * vetor_local[i];
    }

    printf("Processo %d: soma local dos quadrados = %lld\n", rank, soma_local);

    /* Passo 4: reduz todas as somas locais no root */
    long long soma_global = 0;
    MPI_Reduce(&soma_local, &soma_global, 1, MPI_LONG_LONG,
               MPI_SUM, 0, MPI_COMM_WORLD);

    /* Passo 5 e 6: root valida e exibe o resultado final */
    if (rank == 0) {
        /* Soma sequencial usando a fórmula N*(N+1)*(2N+1)/6 */
        long long soma_sequencial = (long long)N * (N + 1) * (2 * N + 1) / 6;

        printf("\n[RESULTADO] Processo root (0):\n");
        printf("  Soma global (via MPI_Reduce) = %lld\n", soma_global);
        printf("  Soma esperada (via fórmula)  = %lld\n", soma_sequencial);

        if (soma_global == soma_sequencial) {
            printf("  ✅ Resultado validado com sucesso.\n");
        } else {
            printf("  ❌ Divergência detectada! Verifique a implementação.\n");
        }
    }

    MPI_Finalize();
    return 0;
}
