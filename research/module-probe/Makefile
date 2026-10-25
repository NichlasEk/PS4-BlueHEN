OUT := ../../build/module-probe
CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror
.PHONY: all test
all: $(OUT)/bluehen_module_probe.elf

$(OUT):
	mkdir -p $@

$(OUT)/bluehen_module_probe.elf: probe.c Makefile | $(OUT)
	@test -x "$(PS4_PAYLOAD_SDK)/bin/orbis-clang"
	"$(PS4_PAYLOAD_SDK)/bin/orbis-clang" $(CFLAGS) -lSceSysmodule -o $@ probe.c

test: $(OUT)/test_probe
	$(OUT)/test_probe

$(OUT)/test_probe: probe.c test_probe.c Makefile | $(OUT)
	$(CC) $(CFLAGS) -DBLUEHEN_MODULE_HOST_TEST -fsanitize=address,undefined -g -o $@ test_probe.c
