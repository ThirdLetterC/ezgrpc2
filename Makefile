.PHONY: all test help clean format format-check

# for the optional parts (FLATCC, TLS, ...)
-include config.mk


all :
	$(MAKE) -C src
	$(MAKE) -C examples

test: all
	$(MAKE) -C src/tests test
	$(MAKE) -C examples/python_client_c_server test
ifneq ($(TLS),none)
	$(MAKE) -C examples/python_client_c_server test-tls
endif
ifeq ($(FLATCC),1)
	$(MAKE) -C examples/flatbuffers test
endif

# C sources to format; src/tests/unity is vendored (see .clang-format-ignore)
FORMAT_FILES = $(shell git ls-files '*.c' '*.h' | grep -v '^src/tests/unity/')
CLANG_FORMAT ?= clang-format

format:
	$(CLANG_FORMAT) -i $(FORMAT_FILES)

format-check:
	@$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)

help:
	@echo "Available targets:"
	@echo "  all               - Builds src/libezgrpc2.{a,so} and examples/*"
	@echo "  test              - Runs unit tests and Python client integration tests"
	@echo "  clean             - Removes generated *.o, *.so and executables"
	@echo "  format            - Formats the C sources with clang-format"
	@echo "  format-check      - Fails if any C source is not clang-formatted"
	@echo ""
	@echo "  make D=1 all        enables -ggdb3 and server debug logs for development"
	@echo "  make T=1 all        enables server trace logs for development (may provide heavy overheads)"
	@echo "  make MIMALLOC=1 all enables Microsoft mimalloc allocator"
	@echo "  make JEMALLOC=1 all enables jemalloc allocator"
	@echo "  make ALLOCATOR=name enables specific allocator (default, mimalloc, jemalloc, custom)"
	@echo "  make PROTOBUF_C=1   enables protobuf-c integration (auto-detected by default)"
	@echo "  make FLATCC=1       enables FlatBuffers (flatcc) integration (auto-detected by default)"
	@echo "  make FLATCC_PREFIX=dir flatcc installed in dir (scripts/build-flatcc.sh builds it)"
	@echo "  make TLS=name       TLS library: openssl libressl boringssl awslc wolfssl gnutls rustls none"
	@echo "  make TLS_PREFIX=dir TLS library installed in dir/include and dir/lib"
	@echo "                      (scripts/build-tls-backend.sh builds libressl, boringssl, awslc, rustls)"

clean :
	$(MAKE) -C src clean
	$(MAKE) -C examples clean
