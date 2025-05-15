# Top‑level binary
all: hellonaomi.bin

# List of your C sources (all in the current directory)
SRCS := main.c \
        video_prims.c

# Make sure the compiler can see your local headers
CFLAGS += -I.

# Pull in Naomi’s shared build rules
include ../../Makefile.base

# ROM packaging target
hellonaomi.bin: ${MAKEROM_FILE} ${NAOMI_BIN_FILE}
	${MAKEROM} $@ \
		--title "Hello Naomi" \
		--publisher "DragonMinded" \
		--serial "${SERIAL}" \
		--section ${NAOMI_BIN_FILE},${START_ADDR} \
		--entrypoint ${MAIN_ADDR} \
		--main-binary-includes-test-binary \
		--test-entrypoint ${TEST_ADDR}

.PHONY: clean
clean:
	rm -rf build
	rm -f hellonaomi.bin
