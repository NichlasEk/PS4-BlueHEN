OUT := ../../build/native-probe
CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror
.PHONY: all test
all: $(OUT)/bluehen_native_probe.elf

$(OUT):
	mkdir -p $@

$(OUT)/bluehen_native_probe.elf: probe.c Makefile | $(OUT)
	@test -x "$(PS4_PAYLOAD_SDK)/bin/orbis-clang" || { echo 'Set PS4_PAYLOAD_SDK to ps4-payload-dev/sdk (v0.9)'; exit 1; }
	"$(PS4_PAYLOAD_SDK)/bin/orbis-clang" $(CFLAGS) -o $@ probe.c

test: $(OUT)/test_probe
	$(OUT)/test_probe

$(OUT)/test_probe: probe.c test_probe.c Makefile | $(OUT)
	$(CC) $(CFLAGS) -DBLUEHEN_HOST_TEST -fsanitize=address,undefined -g -o $@ test_probe.c
