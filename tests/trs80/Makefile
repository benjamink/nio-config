# Host (gcc) tests for the TRS-80 widgets: make -C tests/trs80 test
CFLAGS = -std=c99 -Wall -Wextra -I../../src/platform/trs80
test: build/test_widgets
	./build/test_widgets
build/test_widgets: test_widgets.c fake_console.c ../../src/platform/trs80/ui_widgets.c
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^
clean:
	rm -rf build
.PHONY: test clean
