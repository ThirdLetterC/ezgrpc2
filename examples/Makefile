-include ../config.mk

CFLAGS += -I../include
LDFLAGS += -L../src -l:libezgrpc2.a -lnghttp2 -lz -lssl -lcrypto -lpthread -luv 

CFLAGS+=$(EXTRACFLAGS)

targets = multi_threaded.bin
targets += single_threaded.bin

LDFLAGS += -luuid

ifeq ($(HAVE_SECCOMP),1)
  LDFLAGS += -lseccomp
  CFLAGS += -DHAVE_SECCOMP
endif

all : $(targets)
ifeq ($(PROTOBUF_C),1)
	$(MAKE) -C python_client_c_server
endif

clean :
	rm -f $(targets) *.o
	$(MAKE) -C python_client_c_server clean

../src/libezgrpc2.a:
	$(MAKE) -C ../src

%.bin : %.o ../src/libezgrpc2.a
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@
