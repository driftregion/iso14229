include vars.mk

unquote = $(subst ",,$(strip $(1)))

CFLAGS ?= -Wall -g -ggdb $(STD)
CPPFLAGS ?=
LDFLAGS ?= -shared
BIN ?= ./bin
CPPCHECK ?= cppcheck
INFER ?= infer
CMAKE ?= cmake
CMAKE_CONFIGURE_ARGS ?=
CTEST ?= ctest
CLANG ?= clang

LIB_BASENAME := $(call unquote,$(LIB_NAME))
LIB_MAJOR := $(call unquote,$(MAJOR_VER))
LIB_MINOR := $(call unquote,$(MINOR_VER))
LIB_REVISION := $(call unquote,$(REVISION))
SHARED_LIBRARY := $(BIN)/$(LIB_BASENAME)
SHARED_LIBRARY_MAJOR := $(SHARED_LIBRARY).$(LIB_MAJOR)
SHARED_LIBRARY_FULL := $(SHARED_LIBRARY_MAJOR).$(LIB_MINOR).$(LIB_REVISION)
STATIC_BASENAME := $(if $(filter %.so,$(LIB_BASENAME)),$(patsubst %.so,%.a,$(LIB_BASENAME)),$(LIB_BASENAME).a)
STATIC_LIBRARY := $(BIN)/$(STATIC_BASENAME)
OBJECT := $(BIN)/libisotp.o
INCLUDE_DIR := $(BIN)/include/isotp_c
PUBLIC_HEADERS := $(wildcard *.h)
STAGED_HEADERS := $(addprefix $(INCLUDE_DIR)/,$(PUBLIC_HEADERS))

BOOLEAN_VARIABLES := \
	CMAKE_BUILD_BY_DEFAULT \
	USE_INCLUDE_DIR \
	USE_STATIC_LIBRARY \
	ENABLE_STATIC_LIBRARY_PIC \
	ENABLE_STREAMING \
	ENABLE_FRAME_PADDING \
	ENABLE_CAN_SEND_ARG \
	ENABLE_CAN_SEND_FLAGS \
	ENABLE_CAN_FD_BRS \
	ENABLE_TRANSCEIVE_EVENTS \
	ENABLE_TRANSMIT_COMPLETE_CALLBACK \
	ENABLE_RECEIVE_COMPLETE_CALLBACK \
	NO_FORMATTED_ERRORS
INVALID_BOOLEAN_VARIABLES := $(foreach variable,$(BOOLEAN_VARIABLES),$(if $(filter ON OFF,$($(variable))),,$(variable)))

ifneq ($(strip $(INVALID_BOOLEAN_VARIABLES)),)
$(error $(foreach variable,$(INVALID_BOOLEAN_VARIABLES),$(variable) must be ON or OFF (got '$($(variable))')))
endif

VALID_CAN_FRAME_SIZES := 8 12 16 20 24 32 48 64

ifeq ($(filter $(MAX_CAN_FRAME_SIZE),$(VALID_CAN_FRAME_SIZES)),)
$(error MAX_CAN_FRAME_SIZE must be one of: $(VALID_CAN_FRAME_SIZES) (got '$(MAX_CAN_FRAME_SIZE)'))
endif

ifeq ($(filter $(DEFAULT_TX_DL),$(VALID_CAN_FRAME_SIZES)),)
$(error DEFAULT_TX_DL must be one of: $(VALID_CAN_FRAME_SIZES) (got '$(DEFAULT_TX_DL)'))
endif

ifneq ($(shell test $(DEFAULT_TX_DL) -le $(MAX_CAN_FRAME_SIZE) && printf yes),yes)
$(error DEFAULT_TX_DL must not exceed MAX_CAN_FRAME_SIZE (got '$(DEFAULT_TX_DL)' > '$(MAX_CAN_FRAME_SIZE)'))
endif

ifeq ($(ENABLE_CAN_FD_BRS),ON)
ifneq ($(ENABLE_CAN_SEND_FLAGS),ON)
$(warning ENABLE_CAN_FD_BRS has no effect without ENABLE_CAN_SEND_FLAGS=ON)
endif
endif

FEATURE_CPPFLAGS := \
	-DISO_TP_MAX_CAN_FRAME_SIZE=$(MAX_CAN_FRAME_SIZE) \
	-DISO_TP_DEFAULT_TX_DL=$(DEFAULT_TX_DL)

ifeq ($(ENABLE_FRAME_PADDING),ON)
FEATURE_CPPFLAGS += -DISO_TP_FRAME_PADDING -DISO_TP_FRAME_PADDING_VALUE=$(CAN_FRAME_PAD_VALUE)
endif
ifeq ($(ENABLE_CAN_SEND_ARG),ON)
FEATURE_CPPFLAGS += -DISO_TP_USER_SEND_CAN_ARG
endif
ifeq ($(ENABLE_CAN_SEND_FLAGS),ON)
FEATURE_CPPFLAGS += -DISO_TP_USER_SEND_CAN_FLAGS
ifeq ($(ENABLE_CAN_FD_BRS),ON)
FEATURE_CPPFLAGS += -DISO_TP_CAN_FD_USE_BRS
endif
endif
ifeq ($(ENABLE_TRANSCEIVE_EVENTS),ON)
ifeq ($(ENABLE_TRANSMIT_COMPLETE_CALLBACK),ON)
FEATURE_CPPFLAGS += -DISO_TP_TRANSMIT_COMPLETE_CALLBACK
endif
ifeq ($(ENABLE_RECEIVE_COMPLETE_CALLBACK),ON)
FEATURE_CPPFLAGS += -DISO_TP_RECEIVE_COMPLETE_CALLBACK
endif
endif
ifeq ($(ENABLE_STREAMING),ON)
FEATURE_CPPFLAGS += -DISO_TP_ENABLE_STREAMING
endif
ifeq ($(NO_FORMATTED_ERRORS),ON)
FEATURE_CPPFLAGS += -DISO_TP_NO_FORMATTED_ERRORS
endif
ifeq ($(USE_INCLUDE_DIR),ON)
FEATURE_CPPFLAGS += -DISOTP_USE_INCLUDE_DIR=1
NATIVE_HEADERS := $(STAGED_HEADERS)
endif

ifeq ($(USE_STATIC_LIBRARY),ON)
NATIVE_LIBRARY := $(STATIC_LIBRARY)
INSTALL_ARTIFACTS := $(STATIC_LIBRARY)
PIC_CFLAGS := $(if $(filter ON,$(ENABLE_STATIC_LIBRARY_PIC)),-fPIC)
else
NATIVE_LIBRARY := $(SHARED_LIBRARY)
INSTALL_ARTIFACTS := $(SHARED_LIBRARY_FULL) $(SHARED_LIBRARY_MAJOR) $(SHARED_LIBRARY)
PIC_CFLAGS := -fPIC
endif

CMAKE_OPTION_ARGS := \
	-Disotpc_USE_INCLUDE_DIR=$(USE_INCLUDE_DIR) \
	-Disotpc_STATIC_LIBRARY=$(USE_STATIC_LIBRARY) \
	-Disotpc_STATIC_LIBRARY_PIC=$(ENABLE_STATIC_LIBRARY_PIC) \
	-Disotpc_PAD_CAN_FRAMES=$(ENABLE_FRAME_PADDING) \
	-Disotpc_CAN_FRAME_PAD_VALUE=$(CAN_FRAME_PAD_VALUE) \
	-Disotpc_ENABLE_CAN_SEND_ARG=$(ENABLE_CAN_SEND_ARG) \
	-Disotpc_ENABLE_CAN_SEND_FLAGS=$(ENABLE_CAN_SEND_FLAGS) \
	-Disotpc_ENABLE_CAN_FD_BRS=$(ENABLE_CAN_FD_BRS) \
	-Disotpc_MAX_CAN_FRAME_SIZE=$(MAX_CAN_FRAME_SIZE) \
	-Disotpc_DEFAULT_TX_DL=$(DEFAULT_TX_DL) \
	-Disotpc_ENABLE_TRANSCEIVE_EVENTS=$(ENABLE_TRANSCEIVE_EVENTS) \
	-Disotpc_ENABLE_TRANSMIT_COMPLETE_CALLBACK=$(ENABLE_TRANSMIT_COMPLETE_CALLBACK) \
	-Disotpc_ENABLE_RECEIVE_COMPLETE_CALLBACK=$(ENABLE_RECEIVE_COMPLETE_CALLBACK) \
	-Disotpc_ENABLE_STREAMING=$(ENABLE_STREAMING) \
	-Disotpc_NO_FORMATTED_ERRORS=$(NO_FORMATTED_ERRORS)

.PHONY: all native-all clean fPIC no_opt install cmake tests fuzzing version-gate static-analysis FORCE

ifeq ($(CMAKE_BUILD_BY_DEFAULT),ON)
all: cmake
else
all: native-all
endif

native-all: $(NATIVE_LIBRARY) $(NATIVE_HEADERS)
	@printf "########## BUILT $^ ##########\n\n\n"

# Preserve the legacy entry points while making them perform a build.
fPIC: PIC_CFLAGS := -fPIC
fPIC: native-all

no_opt: CFLAGS += -g -O0
no_opt: native-all

clean:
	-rm -f *.o $(OBJECT) $(SHARED_LIBRARY) $(SHARED_LIBRARY_MAJOR) $(SHARED_LIBRARY_FULL) $(STATIC_LIBRARY) $(STAGED_HEADERS)
	-rmdir $(INCLUDE_DIR) $(BIN)/include $(BIN) 2>/dev/null || true

version-gate:
	@sh .github/scripts/check-version.sh

static-analysis:
	@CPPCHECK="$(CPPCHECK)" INFER="$(INFER)" sh .github/scripts/static-analysis.sh

$(SHARED_LIBRARY): $(SHARED_LIBRARY_MAJOR)
	ln -sfn $(notdir $<) $@
	@printf "Linked $< --> $@...\n"

$(SHARED_LIBRARY_MAJOR): $(SHARED_LIBRARY_FULL)
	ln -sfn $(notdir $<) $@
	@printf "Linked $< --> $@...\n"

$(SHARED_LIBRARY_FULL): $(OBJECT) | $(BIN)
	$(COMP) $< -o $@ $(LDFLAGS)

$(STATIC_LIBRARY): $(OBJECT) | $(BIN)
	$(AR) rcs $@ $<

# The object is intentionally refreshed for each requested build. This keeps
# command-line feature changes from silently reusing an incompatible object.
$(OBJECT): isotp.c $(PUBLIC_HEADERS) FORCE | $(BIN)
	$(COMP) -c $< -o $@ $(CPPFLAGS) $(FEATURE_CPPFLAGS) $(CFLAGS) $(PIC_CFLAGS)

$(BIN):
	mkdir -p $@

$(INCLUDE_DIR)/%.h: %.h
	mkdir -p $(INCLUDE_DIR)
	cp $< $@

install: native-all
	@printf "Installing $(notdir $(NATIVE_LIBRARY)) to $(INSTALL_DIR)...\n"
	cp $(INSTALL_ARTIFACTS) $(INSTALL_DIR)
	@printf "Library was installed...\n"

cmake:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release $(CMAKE_OPTION_ARGS) $(CMAKE_CONFIGURE_ARGS)
	$(CMAKE) --build $(BUILD_DIR) --config Release

tests:
	$(CMAKE) -S . -B $(TEST_BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -Disotpc_ENABLE_TESTING=ON $(CMAKE_OPTION_ARGS) $(CMAKE_CONFIGURE_ARGS)
	$(CMAKE) --build $(TEST_BUILD_DIR) --config Debug
	$(CTEST) --test-dir $(TEST_BUILD_DIR) -C Debug --output-on-failure

fuzzing:
	$(CMAKE) -S . -B $(FUZZ_BUILD_DIR) -DCMAKE_C_COMPILER=$(CLANG) -DCMAKE_BUILD_TYPE=Debug -Disotpc_ENABLE_FUZZING=ON $(CMAKE_OPTION_ARGS) $(CMAKE_CONFIGURE_ARGS)
	$(CMAKE) --build $(FUZZ_BUILD_DIR) --config Debug --target isotp_fuzz_receive

FORCE:
