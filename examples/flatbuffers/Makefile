-include ../../config.mk

CFLAGS += -I../../include
LDFLAGS += -L../../src -l:libezgrpc2.a $(EZGRPC2_DEPS)

PYTHON ?= python3

# flatcc writes greeter_{reader,builder,verifier}.h and the shared flatbuffers_common_*.h
GENERATED = greeter_reader.h greeter_builder.h greeter_verifier.h

all: server.bin

$(GENERATED) &: greeter.fbs
	$(FLATCC_BIN) -a $<

server.bin: server.c $(GENERATED) ../../src/libezgrpc2.a
	$(CC) $(CFLAGS) server.c -o $@ $(LDFLAGS)

run-server: server.bin
	./server.bin

# needs the Python packages grpcio and flatbuffers
test: all
	@echo "Starting FlatBuffers gRPC server..."
	@./server.bin 19029 > server.log 2>&1 & SERVER_PID=$$!; \
	trap 'kill $$SERVER_PID 2>/dev/null || true; wait $$SERVER_PID 2>/dev/null || true' EXIT INT TERM; \
	sleep 1; \
	echo "Running Python FlatBuffers client..."; \
	$(PYTHON) client.py localhost:19029; \
	STATUS=$$?; \
	kill $$SERVER_PID 2>/dev/null || true; \
	wait $$SERVER_PID 2>/dev/null || true; \
	if [ $$STATUS -ne 0 ]; then cat server.log; fi; \
	exit $$STATUS

clean:
	rm -f server.bin *.o $(GENERATED) flatbuffers_common_reader.h flatbuffers_common_builder.h server.log
