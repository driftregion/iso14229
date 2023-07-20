ifeq "$(TP)" "ISOTP_C"
SRCS += isotp-c/isotp.c examples/isotp-c_on_socketcan.c
CFLAGS += -DUDS_TP=UDS_TP_ISOTP_C
endif

cxx: CFLAGS+=-DUDS_TP=UDS_TP_CUSTOM 
cxx: Makefile iso14229.c iso14229.h
	$(CXX) iso14229.c $(CFLAGS) -c

unit_test: CFLAGS+=-DUDS_TP=UDS_TP_CUSTOM -DUDS_CUSTOM_MILLIS
unit_test: Makefile iso14229.h iso14229.c test_iso14229.c
	$(CC) iso14229.c test_iso14229.c $(CFLAGS) $(LDFLAGS) -o test_iso14229
	$(RUN) ./test_iso14229

client: CFLAGS+=-g -DUDS_DBG_PRINT=printf
client: examples/client.c examples/uds_params.h iso14229.h iso14229.c Makefile $(SRCS)
	$(CC) iso14229.c $(SRCS) $< $(CFLAGS) -o $@

server: CFLAGS+=-g -DUDS_DBG_PRINT=printf
server: examples/server.c examples/uds_params.h iso14229.h iso14229.c Makefile $(SRCS)
	$(CC) iso14229.c $(SRCS) $< $(CFLAGS) -o $@

test_examples: test_examples.py
	$(RUN) ./test_examples.py

uds_prefix: CFLAGS+=-DUDS_TP=UDS_TP_CUSTOM -DUDS_CUSTOM_MILLIS
uds_prefix: iso14229.c iso14229.h
	$(CC) iso14229.c $(CFLAGS) -c -o /tmp/x.o && nm /tmp/x.o | grep ' T ' | grep -v 'UDS' ; test $$? = 1

test_qemu: Makefile iso14229.h iso14229.c test_iso14229.c test_qemu.py
	$(RUN) ./test_qemu.py

test: cxx unit_test test_examples uds_prefix test_qemu

fuzz: CC=clang-14
fuzz: ASAN = -fsanitize=fuzzer,signed-integer-overflow,address,undefined -fprofile-instr-generate -fcoverage-mapping
fuzz: OPTS = -g -DUDS_TP=UDS_TP_CUSTOM -DUDS_CUSTOM_MILLIS
fuzz: iso14229.c iso14229.h fuzz_server.c Makefile
	$(CC) $(OPTS) $(WARN) $(INCS) $(TFLAGS) $(ASAN) fuzz_server.c iso14229.c -o fuzzer 
	$(RUN) ./fuzzer corpus

clean:
	rm -f client server test_iso14229 iso14229.o

.phony: clean test_examples