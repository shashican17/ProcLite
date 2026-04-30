# Convenience wrapper — delegates to CMake under the hood.
# Keeps the muscle-memory "make && ./proclite" workflow.

BUILD_DIR  := build
BINARY     := $(BUILD_DIR)/proclite

.PHONY: all debug release clean install run test

all: release

release:
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	@cmake --build $(BUILD_DIR) --parallel

debug:
	@cmake -S . -B $(BUILD_DIR)_debug -DCMAKE_BUILD_TYPE=Debug
	@cmake --build $(BUILD_DIR)_debug --parallel

clean:
	@rm -rf $(BUILD_DIR) $(BUILD_DIR)_debug

install: release
	@cmake --install $(BUILD_DIR) --prefix /usr/local

run: release
	@$(BINARY)

# Quick smoke-test: launch, wait 3s, kill
test: release
	@echo "=== Smoke test: launching ProcLite for 3 seconds ==="
	@timeout 3 $(BINARY) || true
	@echo "=== Done ==="
