APP_SRCS += main.c \
            ../../src/tp/doip/doip_client.c \
            ../../src/tp/doip/doip_tp_udp.c \
            ../../src/tp/doip/doip_tp_tcp.c
APP_TARGET = doip_discovery

# Use the verbose log level for debugging
# CFLAGS = -DUDS_TP_DOIP=1 -DUDS_CONFIG_LOG_COLORS=1 -DUDS_LOG_LEVEL=UDS_LOG_VERBOSE -g -Wall -Wpedantic -Wextra
CFLAGS = -DUDS_TP_DOIP=1 -DUDS_CONFIG_LOG_COLORS=1 -DUDS_LOG_LEVEL=UDS_LOG_INFO -g -Wall -Wpedantic -Wextra
INCLUDES = -I../../src -I../..

all: app

app: $(APP_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $(APP_SRCS) -o $(APP_TARGET)

clean:
	rm -f $(APP_TARGET)
