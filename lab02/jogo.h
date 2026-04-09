#ifndef JOGO_H
#define JOGO_H

#include <stddef.h>

/* ── Validação ────────────────────────────────────────────────────── */
/* Retorna 1 se a palavra é válida para a letra dada, 0 caso contrário */
int validar_palavra(const char *palavra, char letra);

/* ── Geração de letra ─────────────────────────────────────────────── */
/* Retorna uma letra aleatória A-Z */
char gerar_letra(void);

/* ── Comunicação formatada ────────────────────────────────────────── */
/* Envia uma linha terminada por '\n' pelo socket fd.
   Retorna bytes enviados ou -1 em erro. */
int enviar_linha(int fd, const char *msg);

/* Recebe uma linha (terminada por '\n') com timeout em segundos.
   Armazena em buf (tamanho max). Remove o '\n' final.
   Retorna  1 se OK,  0 se timeout,  -1 se erro/conexão fechada. */
int receber_com_timeout(int fd, char *buf, size_t max, int timeout_seg);

#endif /* JOGO_H */