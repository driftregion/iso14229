SRCS += ../../iso14229.c aes_utils.c
HDRS += ../../iso14229.h aes_utils.h
TARGETS = server client

# Libraries to pull flags from
PKGS = mbedtls mbedx509 mbedcrypto

CFLAGS = -DUDS_TP_ISOTP_SOCK -DUDS_LINES -g -I ../..
CFLAGS += $(shell pkg-config --cflags $(PKGS))

# Use LDLIBS for libraries; order matters (crypto last)
LDLIBS = $(shell pkg-config --libs $(PKGS))

# AES key artifacts
KEY_BIN = uds_aes_key.bin
KEY_HDR = uds_aes_key.h

all: $(TARGETS) $(KEY_HDR)

server: $(SRCS) $(HDRS) $(KEY_HDR) server.c Makefile
	$(CC) $(CFLAGS) $(SRCS) server.c $(LDLIBS) -o $@

client: $(SRCS) $(HDRS) $(KEY_HDR) client.c Makefile
	$(CC) $(CFLAGS) $(SRCS) client.c $(LDLIBS) -o $@

$(KEY_BIN):
	openssl rand -out $(KEY_BIN) 16

# Export key as a C header: uds_aes_key[] and uds_aes_key_len
$(KEY_HDR): $(KEY_BIN)
	{ \
	  echo '#pragma once'; \
	  echo '#include <stddef.h>'; \
	  printf 'const unsigned char uds_aes_key_bin[] = {'; \
	  od -An -tx1 $(KEY_BIN) | tr -d '\n ' | sed 's/../0x&,/g; s/,$$//'; \
	  echo '};'; \
	  echo 'static const unsigned char *const uds_aes_key = uds_aes_key_bin;'; \
	  echo 'static const size_t uds_aes_key_len = 16;'; \
	} > $(KEY_HDR)

regen-key:
	rm -f $(KEY_BIN) $(KEY_HDR)
	$(MAKE) $(KEY_HDR)

clean:
	rm -f $(TARGETS) $(KEY_BIN) $(KEY_HDR)