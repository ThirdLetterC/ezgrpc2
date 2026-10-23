-include ../config.mk

CFLAGS += -I../include -I../include/internal
CFLAGS += -fPIC
CFLAGS += -fvisibility=hidden
CFLAGS += $(EXTRACFLAGS)

.PHONY: push test html host_html


CFLAGS += -Wall

#CFLAGS += -Wextra

targets = libezgrpc2.so
targets += libezgrpc2.a

objects = ezgrpc2_call.o
objects += ezgrpc2_compression.o
objects += ezgrpc2_health.o
objects += ezgrpc2_interceptor.o
objects += ezgrpc2_list.o
objects += ezgrpc2_mem.o
objects += ezgrpc2_message.o
objects += ezgrpc2_metadata.o
objects += ezgrpc2_reflection.o
objects += ezgrpc2_server.o
objects += ezgrpc2_status.o
objects += ezgrpc2_status_details.o
objects += ezgrpc2_timeout.o
objects += ezgrpc2_tls.o
ifneq ($(TLS),none)
  objects += ezgrpc2_tls_$(TLS_IMPL).o
endif
ifeq ($(PROTOBUF_C),1)
  objects += ezgrpc2_protobuf_c.o
endif
ifeq ($(FLATCC),1)
  objects += ezgrpc2_flatcc.o
endif
objects += internal_helpers.o
objects += internal_nghttp2_callbacks.o

all: $(targets)

libezgrpc2.a : $(objects)
	$(Q)$(AR) rcs $@ $^

libezgrpc2.so : $(objects)
	$(Q)$(CC) -shared $(LDFLAGS) $^ $(EZGRPC2_DEPS) -o $@

# -MMD: rebuild objects when a header (e.g. the structs in core.h) changes
%.o : %.c
	$(CC) -c $(CFLAGS) -MMD -MP $< -o $@

-include $(objects:.o=.d)

clean :
	rm -f $(targets) *.o *.d
	$(MAKE) -C tests clean

test:
	make -C tests test

