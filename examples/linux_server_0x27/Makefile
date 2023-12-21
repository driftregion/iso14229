SRCS += iso14229.c
HDRS += iso14229.h
TARGETS = server client
CFLAGS = -DUDS_TP_ISOTP_SOCK -DUDS_LINES -g
MBEDTLS ?= /usr/local
LDFLAGS = -L$(MBEDTLS)/lib -lmbedtls -lmbedcrypto -lmbedx509


all: $(TARGETS) private_key.pem public_key.pem

server: $(SRCS) $(HDRS) server.c Makefile
	$(CC) $(CFLAGS) $(SRCS) server.c $(LDFLAGS) -o server

client: $(SRCS) $(HDRS) client.c Makefile
	$(CC) $(CFLAGS)  $(SRCS) client.c $(LDFLAGS) -o client

private_key.pem:
	openssl genrsa -out private_key.pem 4096

public_key.pem: private_key.pem
	openssl rsa -in private_key.pem -pubout -outform PEM -out public_key.pem

clean:
	rm -f $(TARGETS) private_key.pem public_key.pem
