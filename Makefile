
unit_test: CFLAGS+=-DUDS_TP=UDS_TP_CUSTOM -DUDS_CUSTOM_MILLIS
unit_test: Makefile iso14229.h iso14229.c test_iso14229.c
	$(CC) iso14229.c test_iso14229.c $(CFLAGS) $(LDFLAGS) -o test_iso14229
	$(RUN) ./test_iso14229

client: examples/client.c examples/uds_params.h iso14229.h iso14229.c Makefile
	$(CC) iso14229.c $< $(CFLAGS) -o $@

server: examples/server.c examples/uds_params.h iso14229.h iso14229.c Makefile
	$(CC) iso14229.c $< $(CFLAGS) -o $@

test_examples: client server test_examples.sh
	@./test_examples.sh

uds_prefix: CFLAGS+=-DUDS_TP=UDS_TP_CUSTOM -DUDS_CUSTOM_MILLIS
uds_prefix: iso14229.c iso14229.h
	$(CC) iso14229.c $(CFLAGS) -c -o /tmp/x.o && nm /tmp/x.o | grep ' T ' | grep -v 'UDS' ; test $$? = 1

test: unit_test test_examples uds_prefix 

clean:
	rm -f client server test_iso14229

.phony: clean test_examples