SRCS= \
isotp-c/isotp.c \
iso14229server.c \
iso14229client.c

HDRS= \
iso14229.h \
iso14229server.h \
iso14229serverconfig.h \
isotp-c/isotp.h \
isotp-c/isotp_config.h \
isotp-c/isotp_defines.h

INCLUDES= \
isotp-c

#
# Unit tests
#
DEFINES=\

TEST_CFLAGS += $(foreach i,$(INCLUDES),-I$(i))
TEST_CFLAGS += $(foreach d,$(DEFINES),-D$(d))

TEST_SRCS= \
test_iso14229.c

TEST_CFLAGS += -g

test_bin: $(TEST_SRCS) $(SRCS) $(HDRS) Makefile
	$(CC) -o $@ $(TEST_CFLAGS) $(TEST_SRCS) $(SRCS) 

test: test_bin
	./test_bin

test_interactive: test_bin
	gdb test_bin

clean:
	rm -rf test_bin

#
# Example program
#
EXAMPLE_SRCS=\
examples/client.c \
examples/main.c \
examples/server.c \
examples/port_socketcan.c

EXAMPLE_HDRS=\
examples/client.h \
examples/server.h \
examples/port.h \
examples/shared.h

EXAMPLE_INCLUDES=\
examples

EXAMPLE_CFLAGS += $(foreach i,$(INCLUDES),-I$(i))
EXAMPLE_CFLAGS += $(foreach i,$(EXAMPLE_INCLUDES),-I$(i))
EXAMPLE_CFLAGS += $(foreach d,$(DEFINES),-D$(d))
EXAMPLE_CFLAGS += -g 

example: $(SRCS) $(EXAMPLE_SRCS) $(HDRS) $(EXAMPLE_HDRS) Makefile
	$(CC) $(EXAMPLE_CFLAGS) -o $@ $(EXAMPLE_SRCS) $(SRCS) 
