CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c11
LDFLAGS = -lpthread

OBJS_JOGO    = jogo.o
OBJS_SRV     = servidor.o $(OBJS_JOGO)
OBJS_CLI     = cliente.o  $(OBJS_JOGO)

.PHONY: all clean

all: servidor cliente

servidor: $(OBJS_SRV)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

cliente: $(OBJS_CLI)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Dependências de cabeçalho
servidor.o cliente.o jogo.o: protocolo.h jogo.h

clean:
	rm -f servidor cliente *.o