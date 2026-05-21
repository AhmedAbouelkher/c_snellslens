BUILD_DIR = build
CC = cc
RAYLIB_FLAGS = $$(pkg-config --cflags --libs raylib)
UNAME_S := $(shell uname -s)
HAS_LIBOMP := $(shell test -d /opt/homebrew/opt/libomp && echo yes)
OMP_FLAGS =
OMP_INCLUDES =
OMP_LIBS =

WEB_BUILD_DIR = $(BUILD_DIR)/web
RAYLIB_WEB_PATH = ./raylib-web
RAYLIB_WEB_FLAGS = $(RAYLIB_WEB_PATH)/lib/libraylib.a -I$(RAYLIB_WEB_PATH)/include
BUILD_WEB_RESOURCES_PATH  ?= $(dir $<)resources@resources
BUILD_WEB_SHELL       ?= minshell.html
BUILD_WEB_HEAP_SIZE   ?= 128MB
BUILD_WEB_STACK_SIZE  ?= 1MB
BUILD_WEB_ASYNCIFY_STACK_SIZE ?= 1048576

ifeq ($(UNAME_S),Darwin)
	ifeq ($(HAS_LIBOMP),yes)
		OMP_FLAGS = -Xpreprocessor -fopenmp
		OMP_INCLUDES = -I/opt/homebrew/opt/libomp/include
		OMP_LIBS = -L/opt/homebrew/opt/libomp/lib -lomp
	endif
else
	OMP_FLAGS = -fopenmp
endif

build: 
	mkdir -p $(BUILD_DIR)
	rm -rf $(BUILD_DIR)/snellslens
	$(CC) -o $(BUILD_DIR)/snellslens main.c -Wall $(RAYLIB_FLAGS) $(OMP_INCLUDES) $(OMP_FLAGS) $(OMP_LIBS) -g

run: build
	$(BUILD_DIR)/snellslens

run-debug:
	$(BUILD_DIR)/snellslens_debug
	rm -rf $(BUILD_DIR)/snellslens_debug
	$(CC) -o $(BUILD_DIR)/snellslens_debug main.c -Wall $(RAYLIB_FLAGS) $(OMP_INCLUDES) $(OMP_FLAGS) $(OMP_LIBS) -g -fsanitize=address
	$(BUILD_DIR)/snellslens_debug

build-web:
	mkdir -p $(WEB_BUILD_DIR)
	emcc -o $(WEB_BUILD_DIR)/snellslens.html main.c -Os -Wall -DPLATFORM_WEB \
		$(RAYLIB_WEB_FLAGS) -sUSE_GLFW=3 -sFORCE_FILESYSTEM=1 -sMINIFY_HTML=0 \
		-sINITIAL_MEMORY=256MB -sMAXIMUM_MEMORY=2048MB -sALLOW_MEMORY_GROWTH=1 \
		--preload-file $(BUILD_WEB_RESOURCES_PATH) \
		--shell-file $(BUILD_WEB_SHELL)
		
build-web-deploy: build-web
	rm -rf ./docs
	cp -r $(WEB_BUILD_DIR) ./docs
	cp $(WEB_BUILD_DIR)/snellslens.html ./docs/index.html
	rm -rf ./docs/snellslens.html

clean-web:
	rm -rf $(WEB_BUILD_DIR)
	rm -rf ./docs

clean:
	rm -rf $(BUILD_DIR)

.PHONY: build run run-parallel clean build-web