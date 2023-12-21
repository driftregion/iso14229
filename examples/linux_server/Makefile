SRCS += iso14229.c main.c
HDRS += iso14229.h
TARGET = server
CFLAGS = -DUDS_TP_ISOTP_SOCK=1 -g

all: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)
