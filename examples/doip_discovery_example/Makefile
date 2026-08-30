APP_SRCS += doip_discover.c \
            main/iso14229.c
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
