BUILD_DIR = build

build: 
	mkdir -p $(BUILD_DIR)
	rm -rf $(BUILD_DIR)/snellslense
	gcc -o $(BUILD_DIR)/snellslense main.c $$(pkg-config --cflags --libs raylib) -fsanitize=address -g

run: build
	$(BUILD_DIR)/snellslense

clean:
	rm -rf $(BUILD_DIR)

.PHONY: build run clean