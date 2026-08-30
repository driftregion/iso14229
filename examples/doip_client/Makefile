CLI_SRCS += iso14229.c doip_test_client.c
CLI_HDRS += iso14229.h
CLI_TARGET = doip_client

SVR_SRCS = doip_server.c doip_test_server.c
SVR_TARGET = doip_server

# Use the verbose log level for debugging
# CFLAGS = -DUDS_TP_DOIP=1 -DUDS_CONFIG_LOG_COLORS=1 -DUDS_LOG_LEVEL=UDS_LOG_VERBOSE -g -Wall -Wpedantic -Wextra
CFLAGS = -DUDS_TP_DOIP=1 -DUDS_CONFIG_LOG_COLORS=1 -DUDS_LOG_LEVEL=UDS_LOG_INFO -g -Wall -Wpedantic -Wextra

all: client server

client: $(CLI_SRCS) $(CLI_HDRS)
	$(CC) $(CFLAGS) $(CLI_SRCS) -o $(CLI_TARGET)

server: $(SVR_SRCS)
	$(CC) $(CFLAGS) $(SVR_SRCS) -o $(SVR_TARGET)

clean:
	rm -f $(CLI_TARGET) $(SVR_TARGET)
