#define _POSIX_C_SOURCE 200112L

#include "jogo.h"
#include "protocolo.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>

/* ── Geração de letra ─────────────────────────────────────────────── */
char gerar_letra(void)
{
    static int seed_ok = 0;
    if (!seed_ok) { srand((unsigned)time(NULL)); seed_ok = 1; }
    return (char)('A' + rand() % 26);
}

/* ── Validação ────────────────────────────────────────────────────── */
int validar_palavra(const char *palavra, char letra)
{
    if (!palavra || !*palavra) return 0;

    size_t len = strlen(palavra);
    if (len < MIN_PALAVRA) return 0;

    /* Primeira letra (case insensitive) */
    if (toupper((unsigned char)palavra[0]) != toupper((unsigned char)letra))
        return 0;

    /* Apenas letras */
    for (size_t i = 0; i < len; i++) {
        if (!isalpha((unsigned char)palavra[i])) return 0;
    }
    return 1;
}

/* ── Envio ────────────────────────────────────────────────────────── */
int enviar_linha(int fd, const char *msg)
{
    char buf[TAM_BUF];
    int n = snprintf(buf, sizeof(buf), "%s\n", msg);
    if (n <= 0 || n >= (int)sizeof(buf)) return -1;

    ssize_t sent = 0, total = (ssize_t)n;
    while (sent < total) {
        ssize_t r = send(fd, buf + sent, (size_t)(total - sent), MSG_NOSIGNAL);
        if (r <= 0) return -1;
        sent += r;
    }
    return (int)sent;
}

/* ── Recepção com timeout ─────────────────────────────────────────── */
int receber_com_timeout(int fd, char *buf, size_t max, int timeout_seg)
{
    fd_set rset;
    struct timeval tv;

    FD_ZERO(&rset);
    FD_SET(fd, &rset);
    tv.tv_sec  = timeout_seg;
    tv.tv_usec = 0;

    int r = select(fd + 1, &rset, NULL, NULL, &tv);
    if (r < 0)  return -1;   /* erro */
    if (r == 0) return  0;   /* timeout */

    /* Lê byte a byte até '\n' para não consumir dados futuros */
    size_t pos = 0;
    while (pos + 1 < max) {
        ssize_t n = recv(fd, buf + pos, 1, 0);
        if (n <= 0) return -1;
        if (buf[pos] == '\n') { buf[pos] = '\0'; return 1; }
        pos++;
    }
    buf[pos] = '\0';
    return 1;
}