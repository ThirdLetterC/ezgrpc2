-include ../../config.mk

# list and timeout test internals, the others the public API
CFLAGS += -I./unity -I../../include -I../../include/internal

EZGRPC2_LIBS = -L.. -l:libezgrpc2.a $(EZGRPC2_DEPS)

PROGS = api list mem metadata timeout compression health interceptor status_details reflection

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

# every test links the static library
$(PROGS): %: %.o unity/unity.o ../libezgrpc2.a
	$(CC) $(CFLAGS) $*.o $(EXTRA_OBJS_$*) unity/unity.o $(EZGRPC2_LIBS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

# the test checks that the library was built with the requested backend
tls.o: tls.c
	$(CC) -c $(CFLAGS) -DEXPECTED_TLS_BACKEND=\"$(TLS)\" $< -o $@

tls: test_cert.pem test_key.pem

test_cert.pem test_key.pem &:
	openssl req -x509 -newkey rsa:2048 -nodes -keyout test_key.pem -out test_cert.pem -days 365 -subj "/CN=localhost" 2>/dev/null

# protobuf-c
test_msg.pb-c.c test_msg.pb-c.h &: test_msg.proto
	protoc-c -I. --c_out=. $<

pb.o: pb.c test_msg.pb-c.h
EXTRA_OBJS_pb = test_msg.pb-c.o
pb: test_msg.pb-c.o

# flatcc writes <schema>_{reader,builder,verifier}.h and the shared flatbuffers_common_*.h
FB_SCHEMAS = test_msg test_noid
FB_HEADERS = $(foreach s,$(FB_SCHEMAS),$(s)_reader.h $(s)_builder.h $(s)_verifier.h)

$(FB_HEADERS) &: $(addsuffix .fbs,$(FB_SCHEMAS))
	$(FLATCC_BIN) -a $^

fb.o: fb.c $(FB_HEADERS)

test: all
	@set -e; for t in $(PROGS); do ./$$t; done

clean:
	rm -f *.o $(PROGS) api list mem pb fb tls $(FB_HEADERS) flatbuffers_common_reader.h flatbuffers_common_builder.h test_msg.pb-c.c test_msg.pb-c.h test_cert.pem test_key.pem
