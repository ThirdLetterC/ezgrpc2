-include ../config.mk

CFLAGS += -I.
CFLAGS += -fPIC
CFLAGS += -fvisibility=hidden
CFLAGS += $(EXTRACFLAGS)

.PHONY: push test html host_html


CFLAGS += -Wall

#CFLAGS += -Wextra

tests = tests/test_listc.bin
tests += tests/test_pthpoolc.bin
tests += tests/test_ezgrpc2c.bin

targets = libezgrpc2.so
targets += libezgrpc2.a

objects = thpool.o

objects += ezgrpc2_arena_event.o
objects += ezgrpc2_arena_message.o
objects += ezgrpc2_compression.o
objects += ezgrpc2_event.o
objects += ezgrpc2_events.o
objects += ezgrpc2_global.o
objects += ezgrpc2_grpc_status.o
objects += ezgrpc2_header.o
objects += ezgrpc2_health.o
objects += ezgrpc2_interceptor.o
objects += ezgrpc2_metadata.o
objects += ezgrpc2_http2_settings.o
objects += ezgrpc2_list.o
objects += ezgrpc2_mem.o
ifeq ($(PROTOBUF_C),1)
  objects += ezgrpc2_protobuf_c.o
endif
objects += ezgrpc2_message.o
objects += ezgrpc2_pthpool.o
objects += ezgrpc2_reflection.o
objects += ezgrpc2_server.o
objects += ezgrpc2_server_settings.o
objects += ezgrpc2_session.o
objects += ezgrpc2_session_info.o
objects += ezgrpc2_session_uuid.o
objects += ezgrpc2_status_details.o
objects += ezgrpc2_timeout.o
objects += ezgrpc2_tls.o
objects += internal_helpers.o
objects += internal_nghttp2_callbacks.o
#targets += examples/hello_worldcpp.bin

all: $(targets)

libezgrpc2.a : $(objects)
	$(Q)$(AR) rcs $@ $^

libezgrpc2.so : $(objects)
	$(Q)$(CC) -shared $(LDFLAGS) $^ -lnghttp2 -lz -luv -o $@

# -MMD: rebuild objects when a header (e.g. the structs in core.h) changes
%.o : %.c
	$(CC) -c $(CFLAGS) -MMD -MP $< -o $@

-include $(objects:.o=.d)

clean :
	rm -f $(targets) *.o *.d
	$(MAKE) -C tests clean

test:
	make -C tests test

