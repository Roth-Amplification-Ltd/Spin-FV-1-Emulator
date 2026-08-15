BUILD_DIR ?= build
BUILD_TYPE ?= RelWithDebInfo

.PHONY: all configure build test clean linux-run linux-packages
all: build

configure:
	cmake -S . -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	cmake --build $(BUILD_DIR)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR)


linux-run:
	./linux.sh run

linux-packages:
	./linux.sh package all
