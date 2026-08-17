SRCS += main/iso14229.c
HDRS += main/iso14229.h
TARGETS = client
CFLAGS = -DUDS_TP_ISOTP_SOCK -DUDS_LINES -DUDS_LOG_LEVEL=UDS_LOG_INFO -g -Imain

all: $(TARGETS) 

client: $(SRCS) $(HDRS) client.c Makefile
	$(CC) $(CFLAGS)  $(SRCS) client.c -o client

clean:
	rm -f $(TARGETS)
