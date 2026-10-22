
all :
	$(MAKE) -C src
	$(MAKE) -C examples

test: all
	$(MAKE) -C src/tests test
	$(MAKE) -C examples/python_client_c_server test
	$(MAKE) -C examples/python_client_c_server test-tls

help:
	@echo "Available targets:"
	@echo "  all               - Builds src/libezgrpc2.{a,so} and examples/*"
	@echo "  test              - Runs unit tests and Python client integration tests"
	@echo "  clean             - Removes generated *.o, *.so and executables"
	@echo ""
	@echo "  make D=1 all        enables -ggdb3 and server debug logs for development"
	@echo "  make T=1 all        enables server trace logs for development (may provide heavy overheads)"
	@echo "  make MIMALLOC=1 all enables Microsoft mimalloc allocator"
	@echo "  make JEMALLOC=1 all enables jemalloc allocator"
	@echo "  make ALLOCATOR=name enables specific allocator (default, mimalloc, jemalloc, custom)"
	@echo "  make PROTOBUF_C=1   enables protobuf-c integration (auto-detected by default)"

clean :
	$(MAKE) -C src clean
	$(MAKE) -C examples clean
