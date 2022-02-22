SRCS= \
isotp-c/isotp.c \
iso14229server.c \
iso14229client.c \
iso14229serverappsoftware.c \
iso14229serverbootsoftware.c

HDRS= \
iso14229.h \
iso14229server.h \
iso14229serverconfig.h \
isotp-c/isotp.h \
isotp-c/isotp_config.h \
isotp-c/isotp_defines.h

INCLUDES= \
. \
isotp-c


# Tests (Run locally on linux)
DEFINES=\
ISO14229USERDEBUG=printf

TEST_CFLAGS += $(foreach i,$(INCLUDES),-I$(i))
TEST_CFLAGS += $(foreach d,$(DEFINES),-D$(d))

TEST_SRCS= \
test_iso14229.c

TEST_CFLAGS += -g

test_bin: $(TEST_SRCS) $(SRCS) $(HDRS) Makefile
	$(CC) $(TEST_CFLAGS) $(TEST_SRCS) $(SRCS) -o $@

test: test_bin
	./test_bin

test_interactive: test_bin
	gdb test_bin

clean:
	rm -rf test_bin


# Example

EXAMPLE_SRCS=\
example/simple.c \
example/linux_host.c

EXAMPLE_HDRS=\
example/simple.h

EXAMPLE_INCLUDES=\
example

EXAMPLE_CFLAGS += $(foreach i,$(INCLUDES),-I$(i))
EXAMPLE_CFLAGS += $(foreach i,$(EXAMPLE_INCLUDES),-I$(i))
EXAMPLE_CFLAGS += $(foreach d,$(DEFINES),-D$(d))
EXAMPLE_CFLAGS += -g 

example/linux: $(SRCS) $(EXAMPLE_SRCS) $(HDRS) $(EXAMPLE_HDRS) Makefile
	$(CC) $(EXAMPLE_CFLAGS) -o $@ $(EXAMPLE_SRCS) $(SRCS) 

.phony: py_requirements
