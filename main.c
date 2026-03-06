/*Carlos Eduardo Rosendo Basseto - 10409941*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Estrutura principal */
typedef struct {
    int    codigo;
    char  *nome;
    float  preco;
    int    quantidade;
} Produto;

/* Limpa o buffer de entrada completamente */
void limpar_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* Protótipos  */
void     adicionar_produto (Produto **lista, int *n, int *proximo_codigo);
void     listar_produtos   (Produto  *lista, int  n);
Produto *buscar_produto    (Produto  *lista, int  n, int codigo);
void     atualizar_estoque (Produto  *lista, int  n);
void     remover_produto   (Produto **lista, int *n, int codigo);
void     liberar_memoria   (Produto  *lista, int  n);
void     exibir_menu       (void);

/*
   MAIN — apenas menu e chamadas às funções
*/
int main(void) {
    Produto *lista          = NULL;
    int      n              = 0;
    int      proximo_codigo = 1;
    int      opcao;

    do {
        exibir_menu();
        printf("Opcao: ");
        scanf("%d", &opcao);
        limpar_buffer();

        switch (opcao) {
            case 1:
                adicionar_produto(&lista, &n, &proximo_codigo);
                break;
            case 2:
                listar_produtos(lista, n);
                break;
            case 3: {
                int cod;
                printf("\n--- Buscar Produto ---\n");
                printf("Codigo do produto: ");
                scanf("%d", &cod);
                Produto *p = buscar_produto(lista, n, cod);
                if (p) {
                    printf("\n[ENCONTRADO]\n");
                    printf("  Codigo     : %d\n",      p->codigo);
                    printf("  Nome       : %s\n",      p->nome);
                    printf("  Preco      : R$ %.2f\n", p->preco);
                    printf("  Quantidade : %d un.\n\n",p->quantidade);
                } else {
                    printf("\nProduto com codigo %d nao encontrado.\n\n", cod);
                }
                break;
            }
            case 4:
                atualizar_estoque(lista, n);
                break;
            case 5: {
                int cod;
                printf("\n--- Remover Produto ---\n");
                printf("Codigo do produto: ");
                scanf("%d", &cod);
                remover_produto(&lista, &n, cod);
                break;
            }
            case 6:
                printf("\nLiberando memoria...\n");
                liberar_memoria(lista, n);
                printf("Programa encerrado.\n");
                break;
            default:
                printf("\nOpcao invalida. Tente novamente.\n\n");
        }

    } while (opcao != 6);

    return 0;
}

/*
   1. ADICIONAR PRODUTO
*/
void adicionar_produto(Produto **lista, int *n, int *proximo_codigo) {
    printf("\n--- Adicionar Produto ---\n");

    /* Expande o vetor de structs em +1 posição */
    Produto *temp = realloc(*lista, (*n + 1) * sizeof(Produto));
    if (temp == NULL) {
        printf("[ERRO] Falha ao realocar vetor de produtos.\n\n");
        return;
    }
    *lista = temp;

    Produto *p = &(*lista)[*n];
    p->codigo = *proximo_codigo;

    /* Lê o nome e aloca com tamanho exato (strlen + 1 para '\0') */
    char buffer[256];
    printf("Nome: ");
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = '\0';

    p->nome = malloc((strlen(buffer) + 1) * sizeof(char));
    if (p->nome == NULL) {
        printf("[ERRO] Falha ao alocar memoria para o nome.\n\n");
        return;
    }
    strcpy(p->nome, buffer);

    printf("Preco: ");
    scanf("%f", &p->preco);
    limpar_buffer();

    printf("Quantidade: ");
    scanf("%d", &p->quantidade);
    limpar_buffer();

    (*n)++;
    (*proximo_codigo)++;

    printf("Produto adicionado com codigo %d!\n\n", p->codigo);
}

/*
   2. LISTAR PRODUTOS
*/
void listar_produtos(Produto *lista, int n) {
    printf("\n--- Lista de Produtos ---\n");

    if (n == 0) {
        printf("Nenhum produto cadastrado.\n\n");
        return;
    }

    printf("+--------+--------------------+----------+------+---------------+\n");
    printf("| Codigo | Nome               | Preco    | Qtd  | Valor Estoque |\n");
    printf("+--------+--------------------+----------+------+---------------+\n");

    float total_estoque = 0.0f;

    for (int i = 0; i < n; i++) {
        float valor_estoque = lista[i].preco * lista[i].quantidade;
        total_estoque += valor_estoque;

        printf("| %6d | %-18s | %8.2f | %4d | %13.2f |\n",
               lista[i].codigo,
               lista[i].nome,
               lista[i].preco,
               lista[i].quantidade,
               valor_estoque);
    }

    printf("+--------+--------------------+----------+------+---------------+\n");
    printf("Valor total do estoque: R$ %.2f\n\n", total_estoque);
}

/*
   3. BUSCAR PRODUTO POR CÓDIGO
*/
Produto *buscar_produto(Produto *lista, int n, int codigo) {
    for (int i = 0; i < n; i++) {
        if (lista[i].codigo == codigo) {
            return &lista[i];
        }
    }
    return NULL;
}

/*
   4. ATUALIZAR ESTOQUE
*/
void atualizar_estoque(Produto *lista, int n) {
    printf("\n--- Atualizar Estoque ---\n");

    int codigo, nova_qtd;
    printf("Codigo do produto: ");
    scanf("%d", &codigo);

    Produto *p = buscar_produto(lista, n, codigo);

    if (p == NULL) {
        printf("Produto com codigo %d nao encontrado.\n\n", codigo);
        return;
    }

    printf("Nova quantidade: ");
    scanf("%d", &nova_qtd);
    limpar_buffer();

    p->quantidade = nova_qtd;

    printf("Estoque atualizado com sucesso!\n\n");
}

/*
   5. REMOVER PRODUTO
*/
void remover_produto(Produto **lista, int *n, int codigo) {
    int idx = -1;

    for (int i = 0; i < *n; i++) {
        if ((*lista)[i].codigo == codigo) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        printf("Produto com codigo %d nao encontrado.\n\n", codigo);
        return;
    }

    /* Salva o nome para a mensagem antes de liberar */
    char nome_removido[256];
    strncpy(nome_removido, (*lista)[idx].nome, sizeof(nome_removido) - 1);
    nome_removido[sizeof(nome_removido) - 1] = '\0';

    /* PASSO 1: libera a memória do nome */
    free((*lista)[idx].nome);
    (*lista)[idx].nome = NULL;

    /* PASSO 2: desloca elementos posteriores */
    for (int i = idx; i < *n - 1; i++) {
        (*lista)[i] = (*lista)[i + 1];
    }

    (*n)--;

    /* PASSO 3: encolhe o vetor */
    if (*n == 0) {
        free(*lista);
        *lista = NULL;
    } else {
        Produto *temp = realloc(*lista, *n * sizeof(Produto));
        if (temp != NULL) *lista = temp;
    }

    printf("Produto \"%s\" removido com sucesso!\n\n", nome_removido);
}

/*
   6. LIBERAR MEMÓRIA
*/
void liberar_memoria(Produto *lista, int n) {
    for (int i = 0; i < n; i++) {
        printf("Memoria do produto \"%s\" liberada.\n", lista[i].nome);
        free(lista[i].nome);
        lista[i].nome = NULL;
    }
    free(lista);
    printf("Vetor de produtos liberado.\n");
}

/*
   MENU
*/
void exibir_menu(void) {
    printf("========================================\n");
    printf("    SISTEMA DE CADASTRO DE PRODUTOS\n");
    printf("========================================\n");
    printf("Menu:\n");
    printf("  1. Adicionar produto\n");
    printf("  2. Listar produtos\n");
    printf("  3. Buscar produto\n");
    printf("  4. Atualizar estoque\n");
    printf("  5. Remover produto\n");
    printf("  6. Sair\n");
    printf("----------------------------------------\n");
}