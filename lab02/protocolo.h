#ifndef PROTOCOLO_H
#define PROTOCOLO_H

/* ── Configurações gerais ─────────────────────────────────────────── */
#define PORTA_PADRAO    7070
#define MAX_JOGADORES   2
#define NUM_RODADAS     5
#define TEMPO_RODADA    10      /* segundos */
#define MIN_PALAVRA     5       /* mínimo de caracteres */
#define TAM_BUF         512
#define TAM_NOME        64
#define TAM_PALAVRA     128

/* ── Prefixos servidor → cliente ──────────────────────────────────── */
#define S_MSG           "MSG"
#define S_NOME          "NOME"
#define S_AGUARDE       "AGUARDE"
#define S_RODADA        "RODADA"
#define S_RESULTADO     "RESULTADO"
#define S_PLACAR        "PLACAR"
#define S_FIM           "FIM"

/* ── Prefixos cliente → servidor ──────────────────────────────────── */
#define C_NOME          "NOME"
#define C_PALAVRA       "PALAVRA"
#define C_TIMEOUT       "TIMEOUT"

/* ── Separador de campos ───────────────────────────────────────────── */
#define SEP             "|"
#define SEP_C           '|'

#endif /* PROTOCOLO_H */