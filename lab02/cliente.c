#define _POSIX_C_SOURCE 200112L

#include "protocolo.h"
#include "jogo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

static int sockfd = -1;

static void cleanup(void)
{
    if (sockfd >= 0) { close(sockfd); sockfd = -1; }
}

static void sig_handler(int s)
{
    (void)s;
    printf("\nEncerrando...\n");
    cleanup();
    exit(0);
}

/* ── Processar mensagem do servidor ──────────────────────────────── */
/* Retorna 0 para continuar, 1 para encerrar */
static int processar_msg(const char *msg, int *aguardando_palavra,
                          char *letra_rodada, int *timeout_rodada)
{
    /* MSG */
    if (strncmp(msg, "MSG|", 4) == 0) {
        printf("\n  %s\n\n", msg + 4);
        return 0;
    }

    /* NOME */
    if (strcmp(msg, "NOME|") == 0) {
        printf("  Digite seu nome: ");
        fflush(stdout);
        char nome[TAM_NOME];
        if (!fgets(nome, sizeof(nome), stdin)) strcpy(nome, "Jogador");
        size_t l = strlen(nome);
        if (l > 0 && nome[l-1] == '\n') nome[l-1] = '\0';
        char buf[TAM_BUF];
        snprintf(buf, sizeof(buf), "NOME|%s", nome);
        enviar_linha(sockfd, buf);
        return 0;
    }

    /* AGUARDE */
    if (strncmp(msg, "AGUARDE|", 8) == 0) {
        printf("\n  ⏳ %s\n", msg + 8);
        return 0;
    }

    /* RODADA|num|letra|tempo */
    if (strncmp(msg, "RODADA|", 7) == 0) {
        int num, tempo;
        char letra;
        if (sscanf(msg + 7, "%d|%c|%d", &num, &letra, &tempo) == 3) {
            *letra_rodada    = letra;
            *timeout_rodada  = tempo;
            *aguardando_palavra = 1;

            printf("\n  ╔══════════════════════════════════════╗\n");
            printf("  ║        RODADA %d de %d%-18s║\n", num, NUM_RODADAS, "");
            printf("  ║  Letra: [%c]   Tempo: %d seg%-10s║\n", letra, tempo, "");
            printf("  ║  Minimo: %d caracteres%-17s║\n", MIN_PALAVRA, "");
            printf("  ╚══════════════════════════════════════╝\n");
            printf("  Sua palavra: ");
            fflush(stdout);
        }
        return 0;
    }

    /* RESULTADO */
    if (strncmp(msg, "RESULTADO|", 10) == 0) {
        *aguardando_palavra = 0;
        printf("\n  📋 %s\n", msg + 10);
        return 0;
    }

    /* PLACAR|nome1|pts1|nome2|pts2 */
    if (strncmp(msg, "PLACAR|", 7) == 0) {
        char n1[TAM_NOME], n2[TAM_NOME];
        int p1, p2;
        char tmp[TAM_BUF];
        strncpy(tmp, msg + 7, sizeof(tmp) - 1);
        char *tok = strtok(tmp, "|");
        if (tok) { strncpy(n1, tok, sizeof(n1)-1); n1[sizeof(n1)-1]='\0'; }
        tok = strtok(NULL, "|");  p1 = tok ? atoi(tok) : 0;
        tok = strtok(NULL, "|");
        if (tok) { strncpy(n2, tok, sizeof(n2)-1); n2[sizeof(n2)-1]='\0'; }
        tok = strtok(NULL, "|");  p2 = tok ? atoi(tok) : 0;

        printf("  ┌─────────────────────────────────────┐\n");
        printf("  │  PLACAR: %-10s %d  x  %d %-10s│\n", n1, p1, p2, n2);
        printf("  └─────────────────────────────────────┘\n");
        return 0;
    }

    /* TIMEOUT */
    if (strncmp(msg, "TIMEOUT|", 8) == 0) {
        *aguardando_palavra = 0;
        printf("\n  ⌛ Tempo esgotado!\n");
        return 0;
    }

    /* FIM */
    if (strncmp(msg, "FIM|", 4) == 0) {
        printf("\n  🏁 %s\n\n", msg + 4);
        return 1;
    }

    return 0;
}

/* ── main ─────────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    const char *host  = "127.0.0.1";
    int         porta = PORTA_PADRAO;

    if (argc >= 2) host  = argv[1];
    if (argc >= 3) porta = atoi(argv[2]);

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  sig_handler);
    atexit(cleanup);

    printf("╔══════════════════════════════════════╗\n");
    printf("║     BATALHA DE PALAVRAS — Cliente    ║\n");
    printf("╚══════════════════════════════════════╝\n");
    printf("  Conectando a %s:%d...\n", host, porta);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port   = htons((uint16_t)porta);

    struct hostent *he = gethostbyname(host);
    if (!he) { fprintf(stderr, "gethostbyname: host nao encontrado\n"); return 1; }
    memcpy(&srv.sin_addr, he->h_addr_list[0], (size_t)he->h_length);

    if (connect(sockfd, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        perror("connect"); return 1;
    }
    printf("  Conectado!\n\n");

    /* Loop principal usando select() para monitorar socket e stdin */
    int  aguardando_palavra = 0;
    char letra_rodada       = '\0';
    int  timeout_rodada     = TEMPO_RODADA;
    int  palavra_enviada    = 0;

    while (1) {
        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(sockfd, &rset);

        /* Monitora stdin somente se aguardamos palavra e não enviamos ainda */
        if (aguardando_palavra && !palavra_enviada)
            FD_SET(STDIN_FILENO, &rset);

        struct timeval tv;
        tv.tv_sec  = aguardando_palavra ? timeout_rodada : 60;
        tv.tv_usec = 0;

        int maxfd = sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO;
        int r = select(maxfd + 1, &rset, NULL, NULL, &tv);

        if (r < 0) {
            if (errno == EINTR) continue;
            perror("select"); break;
        }

        if (r == 0) {
            /* Timeout de rodada */
            if (aguardando_palavra && !palavra_enviada) {
                printf("\n  ⌛ Tempo esgotado! Enviando TIMEOUT...\n");
                enviar_linha(sockfd, "TIMEOUT|");
                palavra_enviada = 1;
            }
            continue;
        }

        /* Dados do servidor */
        if (FD_ISSET(sockfd, &rset)) {
            char buf[TAM_BUF];
            int ret = receber_com_timeout(sockfd, buf, sizeof(buf), 0);
            if (ret <= 0) {
                /* usa select pra saber se há dados; timeout=0 não bloqueia */
                /* mas receber_com_timeout com 0 pode dar timeout falso: vamos
                   usar recv direto aqui já que select garantiu dado disponível */
                ssize_t n = recv(sockfd, buf, sizeof(buf) - 1, 0);
                if (n <= 0) {
                    printf("  Conexao encerrada pelo servidor.\n");
                    break;
                }
                buf[n] = '\0';
                /* Remove '\n' final */
                while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r'))
                    buf[--n] = '\0';
            }

            int fim = processar_msg(buf, &aguardando_palavra,
                                    &letra_rodada, &timeout_rodada);
            if (fim) break;

            /* Reinicia controle de palavra para nova rodada */
            if (strncmp(buf, "RODADA|", 7) == 0) {
                palavra_enviada = 0;
            }
        }

        /* Leitura do stdin */
        if (FD_ISSET(STDIN_FILENO, &rset) && aguardando_palavra && !palavra_enviada) {
            char palavra[TAM_PALAVRA];
            if (fgets(palavra, sizeof(palavra), stdin)) {
                size_t len = strlen(palavra);
                if (len > 0 && palavra[len-1] == '\n') palavra[len-1] = '\0';

                char msg[TAM_BUF];
                snprintf(msg, sizeof(msg), "PALAVRA|%s", palavra);
                enviar_linha(sockfd, msg);
                palavra_enviada = 1;
                printf("  Enviado: \"%s\" — aguardando resultado...\n", palavra);
            }
        }
    }

    return 0;
}