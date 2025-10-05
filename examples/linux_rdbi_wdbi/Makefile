SRCS += iso14229.c
HDRS += iso14229.h
TARGETS = server client
CFLAGS = -DUDS_TP_ISOTP_SOCK -DUDS_LINES -DUDS_LOG_LEVEL=UDS_LOG_INFO -g

all: $(TARGETS) 

server: $(SRCS) $(HDRS) server.c Makefile
	$(CC) $(CFLAGS) $(SRCS) server.c -o server

client: $(SRCS) $(HDRS) client.c Makefile
	$(CC) $(CFLAGS)  $(SRCS) client.c -o client

clean:
	rm -f $(TARGETS)
