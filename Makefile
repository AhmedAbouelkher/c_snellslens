BUILD_DIR = build
CC = cc
RAYLIB_FLAGS = $$(pkg-config --cflags --libs raylib)
UNAME_S := $(shell uname -s)
HAS_LIBOMP := $(shell test -d /opt/homebrew/opt/libomp && echo yes)
OMP_FLAGS =
OMP_INCLUDES =
OMP_LIBS =

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
	rm -rf $(BUILD_DIR)/snellslense
	$(CC) -o $(BUILD_DIR)/snellslense main.c $(RAYLIB_FLAGS) $(OMP_INCLUDES) $(OMP_FLAGS) $(OMP_LIBS) -fsanitize=address -g

run: build
	$(BUILD_DIR)/snellslense

clean:
	rm -rf $(BUILD_DIR)

.PHONY: build run run-parallel clean