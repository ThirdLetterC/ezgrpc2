
all :
	$(MAKE) -C src
	$(MAKE) -C examples

help:
	@echo "Available targets:"
	@echo "  all               - Builds src/libezgrpc.{a,so} and examples/*"
	@echo "  clean             - Removes generated *.o, *.so and executables"
	@echo ""
	@echo "  make D=1 all        enables -ggdb3 and server debug logs for development"
	@echo "  make T=1 all        enables server trace logs for development (may provide heavy overheads)"
	@echo "  make MIMALLOC=1 all enables Microsoft mimalloc allocator"
	@echo "  make JEMALLOC=1 all enables jemalloc allocator"
	@echo "  make ALLOCATOR=name enables specific allocator (default, mimalloc, jemalloc, custom)"

clean :
	$(MAKE) -C src clean
	$(MAKE) -C examples clean
