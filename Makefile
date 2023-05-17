###############################################################################
# Flags & Folders
###############################################################################
FOLDER_BIN=bin
FOLDER_BUILD=build
FOLDER_BUILD_CPP=build/cpp
FOLDER_LIB=lib
FOLDER_TESTS=tests

UNAME=$(shell uname)

CC:=$(CC)
CPP:=$(CXX)

CC_FLAGS=-Wall -g

AR=ar
AR_FLAGS=-rsc

ifndef BUILD_EXAMPLES 
BUILD_EXAMPLES=0
endif
ifndef BUILD_TOOLS 
BUILD_TOOLS=1
endif

###############################################################################
# Configuration rules
###############################################################################
LIB=$(FOLDER_LIB)/libquicked.a
LIB_CPP=$(FOLDER_LIB)/libquickedcpp.a
SUBDIRS=alignment \
        system \
        utils
ifeq ($(BUILD_TOOLS),1)        
    APPS+=tools/generate_dataset \
          tools/align_benchmark
endif
ifeq ($(BUILD_EXAMPLES),1)        
    APPS+=examples
endif

all: CC_FLAGS+=-O3 -march=native -mavx2 -mbmi2 -mpopcnt -mfma -mcx16 -flto
all: build

debug: build

ASAN_OPT=-fsanitize=address -fsanitize=undefined -fsanitize=shift -fsanitize=alignment
ASAN_OPT+=-fsanitize=signed-integer-overflow -fsanitize=bool -fsanitize=enum
ASAN_OPT+=-fsanitize=pointer-compare -fsanitize=pointer-overflow -fsanitize=builtin

# ASAN: ASAN_OPTIONS=detect_leaks=1:symbolize=1 LSAN_OPTIONS=verbosity=2:log_threads=1
asan: CC_FLAGS+=$(ASAN_OPT) -fno-omit-frame-pointer -fno-common
asan: build

###############################################################################
# Build rules
###############################################################################
build: setup
build: $(SUBDIRS) 
build: lib_quicked
build: $(APPS)

setup:
	@mkdir -p $(FOLDER_BIN) $(FOLDER_BUILD) $(FOLDER_BUILD_CPP) $(FOLDER_LIB)
ifeq (,$(wildcard ./tools/align_benchmark/external/.built))
	$(MAKE) --directory=tools/align_benchmark/external all
	@touch ./tools/align_benchmark/external/.built
endif
	
lib_quicked: $(SUBDIRS)
	$(AR) $(AR_FLAGS) $(LIB) $(FOLDER_BUILD)/*.o 2> /dev/null
	#$(AR) $(AR_FLAGS) $(LIB_CPP) $(FOLDER_BUILD)/*.o $(FOLDER_BUILD_CPP)/*.o 2> /dev/null

clean:
	rm -rf $(FOLDER_BIN) $(FOLDER_BUILD) $(FOLDER_LIB) 2> /dev/null
	$(MAKE) --directory=tools/align_benchmark clean
	#$(MAKE) --directory=examples clean
	rm -rf $(FOLDER_TESTS)/*.alg $(FOLDER_TESTS)/*.log* 2> /dev/null
	
###############################################################################
# Build external libs (for align-benchmark)
###############################################################################
external: external-all
	
external-all:
	$(MAKE) --directory=tools/align_benchmark/external all
	@touch ./tools/align_benchmark/external/.built
	
external-clean:
	$(MAKE) --directory=tools/align_benchmark/external clean
	@rm ./tools/align_benchmark/external/.built
	
###############################################################################
# Subdir rule
###############################################################################
export
$(SUBDIRS):
	$(MAKE) --directory=$@ all
	
$(APPS):
	$(MAKE) --directory=$@ all

.PHONY: $(SUBDIRS) $(APPS)

