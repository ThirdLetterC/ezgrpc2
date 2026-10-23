-include ../../config.mk

CFLAGS += -I./unity -I../../include

EZGRPC2_LIBS = -L.. -l:libezgrpc2.a -lnghttp2 -lz $(TLS_LIBS) -lpthread -luuid -luv

PROGS = list pthpool mem roadmap metadata timeout compression health interceptor status_details reflection

ifneq ($(TLS),none)
  PROGS += tls
endif

ifeq ($(PROTOBUF_C),1)
  PROGS += pb
endif

ifeq ($(FLATCC),1)
  PROGS += fb
endif

all: $(PROGS)

list: ../ezgrpc2_list.o ../ezgrpc2_mem.o list.o unity/unity.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

pthpool: ../ezgrpc2_pthpool.o ../ezgrpc2_list.o ../ezgrpc2_mem.o pthpool.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -lpthread -o $@

mem: ../ezgrpc2_mem.o mem.o unity/unity.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

roadmap.o: roadmap.c
	$(CC) -c $(CFLAGS) $< -o $@

roadmap: roadmap.o unity/unity.o ../libezgrpc2.a
	$(CC) $(CFLAGS) roadmap.o unity/unity.o $(EZGRPC2_LIBS) $(LDFLAGS) -o $@

metadata.o: metadata.c
	$(CC) -c $(CFLAGS) $< -o $@

metadata: metadata.o unity/unity.o ../libezgrpc2.a
	$(CC) $(CFLAGS) metadata.o unity/unity.o $(EZGRPC2_LIBS) $(LDFLAGS) -o $@

timeout.o: timeout.c
	$(CC) -c $(CFLAGS) $< -o $@

timeout: timeout.o unity/unity.o ../libezgrpc2.a
	$(CC) $(CFLAGS) timeout.o unity/unity.o $(EZGRPC2_LIBS) $(LDFLAGS) -o $@

compression.o: compression.c
	$(CC) -c $(CFLAGS) $< -o $@

compression: compression.o unity/unity.o ../libezgrpc2.a
	$(CC) $(CFLAGS) compression.o unity/unity.o $(EZGRPC2_LIBS) $(LDFLAGS) -o $@

health.o: health.c
	$(CC) -c $(CFLAGS) $< -o $@

health: health.o unity/unity.o ../libezgrpc2.a
	$(CC) $(CFLAGS) health.o unity/unity.o $(EZGRPC2_LIBS) $(LDFLAGS) -o $@

interceptor.o: interceptor.c
	$(CC) -c $(CFLAGS) $< -o $@

interceptor: interceptor.o unity/unity.o ../libezgrpc2.a
	$(CC) $(CFLAGS) interceptor.o unity/unity.o $(EZGRPC2_LIBS) $(LDFLAGS) -o $@

status_details.o: status_details.c
	$(CC) -c $(CFLAGS) $< -o $@

status_details: status_details.o unity/unity.o ../libezgrpc2.a
	$(CC) $(CFLAGS) status_details.o unity/unity.o $(EZGRPC2_LIBS) $(LDFLAGS) -o $@

reflection.o: reflection.c
	$(CC) -c $(CFLAGS) $< -o $@

reflection: reflection.o unity/unity.o ../libezgrpc2.a
	$(CC) $(CFLAGS) reflection.o unity/unity.o $(EZGRPC2_LIBS) $(LDFLAGS) -o $@

test_cert.pem test_key.pem:
	openssl req -x509 -newkey rsa:2048 -nodes -keyout test_key.pem -out test_cert.pem -days 365 -subj "/CN=localhost" 2>/dev/null

# the test checks that the library was built with the requested backend
tls.o: tls.c
	$(CC) -c $(CFLAGS) -DEXPECTED_TLS_BACKEND=\"$(TLS)\" $< -o $@

tls: tls.o unity/unity.o ../libezgrpc2.a test_cert.pem test_key.pem
	$(CC) $(CFLAGS) tls.o unity/unity.o $(EZGRPC2_LIBS) $(LDFLAGS) -o $@

test_msg.pb-c.c test_msg.pb-c.h: test_msg.proto
	protoc-c -I. --c_out=. $<

test_msg.pb-c.o: test_msg.pb-c.c test_msg.pb-c.h
	$(CC) -c $(CFLAGS) $< -o $@

pb.o: pb.c test_msg.pb-c.h
	$(CC) -c $(CFLAGS) $< -o $@

pb: ../ezgrpc2_protobuf_c.o ../ezgrpc2_message.o ../ezgrpc2_mem.o ../ezgrpc2_list.o test_msg.pb-c.o pb.o unity/unity.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# flatcc writes <schema>_{reader,builder,verifier}.h and the shared flatbuffers_common_*.h
FB_SCHEMAS = test_msg test_noid
FB_HEADERS = $(foreach s,$(FB_SCHEMAS),$(s)_reader.h $(s)_builder.h $(s)_verifier.h)

$(FB_HEADERS) &: $(addsuffix .fbs,$(FB_SCHEMAS))
	$(FLATCC_BIN) -a $^

fb.o: fb.c $(FB_HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

fb: ../ezgrpc2_flatcc.o ../ezgrpc2_message.o ../ezgrpc2_mem.o ../ezgrpc2_list.o fb.o unity/unity.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(FLATCC_LIBS) -o $@

test: all
	./list
	./pthpool
	./mem
	./roadmap
	./metadata
	./timeout
	./compression
	./health
	./interceptor
	./status_details
	./reflection
ifneq ($(TLS),none)
	./tls
endif
ifeq ($(PROTOBUF_C),1)
	./pb
endif
ifeq ($(FLATCC),1)
	./fb
endif

clean:
	rm -f *.o list pthpool mem pb fb $(FB_HEADERS) flatbuffers_common_reader.h flatbuffers_common_builder.h roadmap metadata timeout compression health interceptor status_details reflection tls test_msg.pb-c.c test_msg.pb-c.h test_cert.pem test_key.pem
