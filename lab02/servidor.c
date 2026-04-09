#define _POSIX_C_SOURCE 200112L

#include "protocolo.h"
#include "jogo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <strings.h>

/* ── Estrutura de partida ─────────────────────────────────────────── */
typedef struct {
    int  fd[2];
    char nome[2][TAM_NOME];
    int  pontos[2];
} Partida;

static int partida_id = 0;          /* contador global de partidas */
static pthread_mutex_t id_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── Helpers ──────────────────────────────────────────────────────── */
static void send_fmt(int fd, const char *fmt, ...)
{
    char buf[TAM_BUF];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    enviar_linha(fd, buf);
}

/* Obtém o nome do jogador via protocolo NOME */
static int obter_nome(int fd, char *nome)
{
    enviar_linha(fd, "NOME|");

    char buf[TAM_BUF];
    if (receber_com_timeout(fd, buf, sizeof(buf), 60) != 1) return -1;

    /* Espera "NOME|<nome>" */
    if (strncmp(buf, "NOME|", 5) != 0) return -1;
    char *n = buf + 5;
    if (*n == '\0') strncpy(nome, "Jogador", TAM_NOME - 1);
    else            strncpy(nome, n, TAM_NOME - 1);
    nome[TAM_NOME - 1] = '\0';

    /* Sanitiza: mantém apenas alnum/espaço */
    for (int i = 0; nome[i]; i++)
        if (!isalnum((unsigned char)nome[i]) && nome[i] != ' ')
            nome[i] = '_';

    return 0;
}

/* ── Lógica de uma rodada ─────────────────────────────────────────── */
static void executar_rodada(Partida *p, int rodada)
{
    char letra = gerar_letra();
    char palavra[2][TAM_PALAVRA] = {"", ""};
    int  valido[2] = {0, 0};
    int  timeout_jogador[2] = {0, 0};

    printf("  [Rodada %d] Letra: %c\n", rodada, letra);

    /* Anuncia rodada para os dois */
    for (int i = 0; i < 2; i++) {
        send_fmt(p->fd[i], "RODADA|%d|%c|%d", rodada, letra, TEMPO_RODADA);
    }

    /* Recebe palavras em paralelo usando select (simplificado: sequencial
       com timeout individual — para paralelismo real usaríamos threads por fd) */
    for (int i = 0; i < 2; i++) {
        char buf[TAM_BUF];
        int r = receber_com_timeout(p->fd[i], buf, sizeof(buf), TEMPO_RODADA);
        if (r != 1) {
            /* timeout ou erro */
            timeout_jogador[i] = 1;
            enviar_linha(p->fd[i], "TIMEOUT|");
            strncpy(palavra[i], "", TAM_PALAVRA - 1);
        } else if (strncmp(buf, "PALAVRA|", 8) == 0) {
            strncpy(palavra[i], buf + 8, TAM_PALAVRA - 1);
            palavra[i][TAM_PALAVRA - 1] = '\0';
        } else if (strncmp(buf, "TIMEOUT|", 8) == 0) {
            timeout_jogador[i] = 1;
        }
    }

    /* Valida */
    for (int i = 0; i < 2; i++) {
        if (!timeout_jogador[i])
            valido[i] = validar_palavra(palavra[i], letra);
    }

    /* Palavras iguais → ninguém pontua */
    int repetida = 0;
    if (valido[0] && valido[1] &&
        strcasecmp(palavra[0], palavra[1]) == 0) {
        repetida = 1;
        valido[0] = valido[1] = 0;
    }

    /* Atualiza pontos */
    for (int i = 0; i < 2; i++)
        if (valido[i]) p->pontos[i]++;

    /* Log do servidor */
    printf("  [Rodada %d] %s=\"%s\"(%s) | %s=\"%s\"(%s) | Placar: %d x %d\n",
           rodada,
           p->nome[0], palavra[0], valido[0] ? "ok" : (timeout_jogador[0] ? "timeout" : "invalida"),
           p->nome[1], palavra[1], valido[1] ? "ok" : (timeout_jogador[1] ? "timeout" : "invalida"),
           p->pontos[0], p->pontos[1]);

    /* Envia RESULTADO e PLACAR */
    for (int i = 0; i < 2; i++) {
        int j = 1 - i;  /* oponente */
        char res[TAM_BUF];

        if (repetida) {
            snprintf(res, sizeof(res),
                "RESULTADO|Palavras iguais (\"%s\")! Ninguem pontua. [%s enviou: \"%s\"]",
                palavra[i], p->nome[j], palavra[j]);
        } else if (timeout_jogador[i]) {
            snprintf(res, sizeof(res),
                "RESULTADO|Tempo esgotado! 0 pontos. [%s enviou: \"%s\"]",
                p->nome[j], valido[j] ? palavra[j] : "(invalida)");
        } else if (!valido[i]) {
            snprintf(res, sizeof(res),
                "RESULTADO|Palavra \"%s\" invalida! 0 pontos. [%s enviou: \"%s\"]",
                palavra[i], p->nome[j], valido[j] ? palavra[j] : "(invalida)");
        } else {
            snprintf(res, sizeof(res),
                "RESULTADO|Palavra \"%s\" valida! +1 ponto. [%s enviou: \"%s\"]",
                palavra[i], p->nome[j], palavra[j]);
        }

        enviar_linha(p->fd[i], res);

        send_fmt(p->fd[i], "PLACAR|%s|%d|%s|%d",
                 p->nome[0], p->pontos[0],
                 p->nome[1], p->pontos[1]);
    }
}

/* ── Thread de partida ────────────────────────────────────────────── */
static void *thread_partida(void *arg)
{
    Partida *p = (Partida *)arg;

    pthread_mutex_lock(&id_mutex);
    int pid = ++partida_id;
    pthread_mutex_unlock(&id_mutex);

    /* Coleta nomes */
    for (int i = 0; i < 2; i++) {
        if (obter_nome(p->fd[i], p->nome[i]) < 0) {
            snprintf(p->nome[i], TAM_NOME, "Jogador%d", i + 1);
        }
        send_fmt(p->fd[i], "MSG|Bem-vindo, %s!", p->nome[i]);
    }

    /* Aguarda segundo jogador */
    for (int i = 0; i < 2; i++) {
        send_fmt(p->fd[i], "AGUARDE|Conectado! Aguardando outro jogador para iniciar...");
    }

    printf("[Partida #%d] Jogadores: %s vs %s\n", pid, p->nome[0], p->nome[1]);

    /* Anuncia batalha */
    for (int i = 0; i < 2; i++) {
        send_fmt(p->fd[i],
            "MSG|Batalha de Palavras! %s vs %s - %d rodadas. Boa sorte!",
            p->nome[0], p->nome[1], NUM_RODADAS);
    }

    /* Rodadas */
    for (int r = 1; r <= NUM_RODADAS; r++) {
        executar_rodada(p, r);

        /* Pequena pausa entre rodadas */
        if (r < NUM_RODADAS) sleep(2);
    }

    /* Resultado final */
    char fim[TAM_BUF];
    if (p->pontos[0] > p->pontos[1]) {
        snprintf(fim, sizeof(fim), "FIM|%s venceu! Placar final: %s %d x %d %s",
                 p->nome[0], p->nome[0], p->pontos[0], p->pontos[1], p->nome[1]);
        printf("[Partida #%d] %s venceu! Placar final: %s %d x %d %s\n",
               pid, p->nome[0], p->nome[0], p->pontos[0], p->pontos[1], p->nome[1]);
    } else if (p->pontos[1] > p->pontos[0]) {
        snprintf(fim, sizeof(fim), "FIM|%s venceu! Placar final: %s %d x %d %s",
                 p->nome[1], p->nome[0], p->pontos[0], p->pontos[1], p->nome[1]);
        printf("[Partida #%d] %s venceu! Placar final: %s %d x %d %s\n",
               pid, p->nome[1], p->nome[0], p->pontos[0], p->pontos[1], p->nome[1]);
    } else {
        snprintf(fim, sizeof(fim), "FIM|Empate! Placar final: %s %d x %d %s",
                 p->nome[0], p->pontos[0], p->pontos[1], p->nome[1]);
        printf("[Partida #%d] Empate! Placar final: %s %d x %d %s\n",
               pid, p->nome[0], p->pontos[0], p->pontos[1], p->nome[1]);
    }

    for (int i = 0; i < 2; i++) {
        enviar_linha(p->fd[i], fim);
        close(p->fd[i]);
    }

    free(p);
    return NULL;
}

/* ── main ─────────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    int porta = PORTA_PADRAO;
    if (argc >= 2) porta = atoi(argv[1]);

    /* Ignora SIGPIPE para não encerrar ao escrever em socket fechado */
    signal(SIGPIPE, SIG_IGN);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)porta);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(srv); return 1;
    }
    if (listen(srv, 16) < 0) {
        perror("listen"); close(srv); return 1;
    }

    printf("╔══════════════════════════════════════════════╗\n");
    printf("║      BATALHA DE PALAVRAS — Servidor          ║\n");
    printf("║  Porta: %-4d                                 ║\n", porta);
    printf("║  Aguardando jogadores (pares de 2)...        ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    while (1) {
        /* Aceita dois clientes e forma par */
        int fds[2];
        char ips[2][INET_ADDRSTRLEN];

        for (int i = 0; i < 2; i++) {
            struct sockaddr_in cli = {0};
            socklen_t clen = sizeof(cli);
            fds[i] = accept(srv, (struct sockaddr *)&cli, &clen);
            if (fds[i] < 0) { perror("accept"); continue; }

            inet_ntop(AF_INET, &cli.sin_addr, ips[i], sizeof(ips[i]));
            printf("[+] Jogador conectou: %s:%d (fd=%d)\n",
                   ips[i], ntohs(cli.sin_port), fds[i]);

            if (i == 0) {
                /* Avisa primeiro jogador que aguarda */
                enviar_linha(fds[0], "AGUARDE|Aguardando segundo jogador...");
                printf("[*] Aguardando mais 1 jogador(es)...\n");
            }
        }

        /* Cria partida em thread separada */
        Partida *p = calloc(1, sizeof(Partida));
        if (!p) { close(fds[0]); close(fds[1]); continue; }

        p->fd[0] = fds[0];
        p->fd[1] = fds[1];

        pthread_t tid;
        if (pthread_create(&tid, NULL, thread_partida, p) != 0) {
            perror("pthread_create");
            close(fds[0]); close(fds[1]); free(p);
        } else {
            pthread_detach(tid);
        }
    }

    close(srv);
    return 0;
}