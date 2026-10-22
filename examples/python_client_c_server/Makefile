-include ../../config.mk

CFLAGS += -I../../src
LDFLAGS += -L../../src -l:libezgrpc2.a -lnghttp2 -lz -lssl -lcrypto -lpthread -lprotobuf-c -luv

ifeq ($(OS),Windows_NT)
  LDFLAGS += -lWs2_32 -lRpcrt4
else
  LDFLAGS += -luuid
endif

PROTOC_C ?= protoc-c
PYTHON ?= python3

all: proto-c proto-py server.bin

proto-c: helloworld.pb-c.c helloworld.pb-c.h

helloworld.pb-c.c helloworld.pb-c.h: helloworld.proto
	$(PROTOC_C) -I. --c_out=. $<

proto-py: helloworld_pb2.py helloworld_pb2_grpc.py

helloworld_pb2.py helloworld_pb2_grpc.py: helloworld.proto
	$(PYTHON) -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. $<

server.bin: server.c helloworld.pb-c.c ../../src/libezgrpc2.a
	$(CC) $(CFLAGS) server.c helloworld.pb-c.c -o $@ $(LDFLAGS)

run-server: server.bin
	./server.bin

run-client: proto-py
	$(PYTHON) client.py

test: all
	@echo "Starting C gRPC server..."
	@./server.bin 19009 > server.log 2>&1 & SERVER_PID=$$!; \
	trap 'kill $$SERVER_PID 2>/dev/null || true; wait $$SERVER_PID 2>/dev/null || true' EXIT INT TERM; \
	sleep 1; \
	echo "Running Python gRPC client..."; \
	$(PYTHON) client.py localhost:19009; \
	STATUS=$$?; \
	kill $$SERVER_PID 2>/dev/null || true; \
	wait $$SERVER_PID 2>/dev/null || true; \
	if [ $$STATUS -ne 0 ]; then cat server.log; fi; \
	exit $$STATUS

server.crt server.key:
	openssl req -x509 -newkey rsa:2048 -nodes -keyout server.key -out server.crt -days 365 -subj "/CN=localhost" 2>/dev/null

test-tls: all server.crt server.key
	@echo "Starting C gRPC server with TLS..."
	@USE_TLS=1 ./server.bin 19019 > server_tls.log 2>&1 & SERVER_PID=$$!; \
	trap 'kill $$SERVER_PID 2>/dev/null || true; wait $$SERVER_PID 2>/dev/null || true' EXIT INT TERM; \
	sleep 1; \
	echo "Running Python gRPC client over TLS..."; \
	USE_TLS=1 TLS_CERT=server.crt $(PYTHON) client.py localhost:19019 --tls; \
	STATUS=$$?; \
	kill $$SERVER_PID 2>/dev/null || true; \
	wait $$SERVER_PID 2>/dev/null || true; \
	if [ $$STATUS -ne 0 ]; then cat server_tls.log; fi; \
	exit $$STATUS

clean:
	rm -f server.bin *.o helloworld.pb-c.c helloworld.pb-c.h helloworld_pb2.py helloworld_pb2_grpc.py helloworld_pb2.pyi server.log server_tls.log server.crt server.key
	rm -rf __pycache__
