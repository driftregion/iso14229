all: iso14229.c iso14229.h
	mkdir -p release 
	cp iso14229.c iso14229.h release/

clean:
	rm -rf isotp_c_wrapped.c isotp_c_wrapped.h iso14229.c iso14229.h release/

isotp_c_wrapped.c: $(shell find src/tp/isotp-c -name '*.c')
	echo '#if defined(UDS_ISOTP_C)' >> $@ ; for f in $^; do cat $$f >> $@; done ; echo '#endif' >> $@

isotp_c_wrapped.h: $(shell find src/tp/isotp-c -name '*.h')
	echo '#if defined(UDS_ISOTP_C)' >> $@ ; for f in $^; do cat $$f >> $@; done ; echo '#endif' >> $@

iso14229.c: $(shell find src -name '*.c') isotp_c_wrapped.c
	echo '#include "iso14229.h"' > $@ ; for f in $^; do echo; echo '#ifdef UDS_LINES'; echo "#line 1 \"$$f\""; echo '#endif'; cat $$f | sed -e 's,#include ".*,,'; done >> $@

iso14229.h: $(shell find src -name '*.h') isotp_c_wrapped.h
	( echo '#ifndef ISO14229_H'; echo '#define ISO14229_H'; echo; echo '#ifdef __cplusplus'; echo 'extern "C" {'; echo '#endif'; cat src/version.h src/sys.h src/sys_arduino.h src/sys_unix.h src/sys_win32.h src/sys_esp32.h src/config.h src/util.h src/tp.h src/uds.h src/client.h src/server.h isotp_c_wrapped.h src/tp/*.h |sed -e 's,#include ".*,,' -e 's,^#pragma once,,' ; echo '#endif'; echo '#ifdef __cplusplus'; echo '}'; echo '#endif';) > $@

.phony: clean
