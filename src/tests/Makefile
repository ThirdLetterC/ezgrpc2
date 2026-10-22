-include ../../config.mk

CFLAGS += -I./unity -I..

PROGS = list pthpool mem

ifeq ($(PROTOBUF_C),1)
  PROGS += pb
endif

all: $(PROGS)

list: ../ezgrpc2_list.o ../ezgrpc2_mem.o list.o unity/unity.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

pthpool: ../ezgrpc2_pthpool.o ../ezgrpc2_list.o ../ezgrpc2_mem.o pthpool.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -lpthread -o $@

mem: ../ezgrpc2_mem.o mem.o unity/unity.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

test_msg.pb-c.c test_msg.pb-c.h: test_msg.proto
	protoc-c -I. --c_out=. $<

test_msg.pb-c.o: test_msg.pb-c.c test_msg.pb-c.h
	$(CC) -c $(CFLAGS) $< -o $@

pb.o: pb.c test_msg.pb-c.h
	$(CC) -c $(CFLAGS) $< -o $@

pb: ../ezgrpc2_protobuf_c.o ../ezgrpc2_message.o ../ezgrpc2_mem.o ../ezgrpc2_list.o test_msg.pb-c.o pb.o unity/unity.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

test: all
	./list
	./pthpool
	./mem
ifeq ($(PROTOBUF_C),1)
	./pb
endif

clean:
	rm -f *.o list pthpool mem pb test_msg.pb-c.c test_msg.pb-c.h
