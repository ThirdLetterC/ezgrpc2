-include ../../config.mk

CFLAGS += -I./unity -I..

PROGS = list pthpool mem roadmap metadata timeout compression health

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

roadmap.o: roadmap.c
	$(CC) -c $(CFLAGS) $< -o $@

roadmap: roadmap.o unity/unity.o ../libezgrpc2.a
	$(CC) $(CFLAGS) roadmap.o unity/unity.o $(LDFLAGS) -L.. -l:libezgrpc2.a -lnghttp2 -lz -lpthread -luuid -o $@

metadata.o: metadata.c
	$(CC) -c $(CFLAGS) $< -o $@

metadata: metadata.o unity/unity.o ../libezgrpc2.a
	$(CC) $(CFLAGS) metadata.o unity/unity.o $(LDFLAGS) -L.. -l:libezgrpc2.a -lnghttp2 -lz -lpthread -luuid -o $@

timeout.o: timeout.c
	$(CC) -c $(CFLAGS) $< -o $@

timeout: timeout.o unity/unity.o ../libezgrpc2.a
	$(CC) $(CFLAGS) timeout.o unity/unity.o $(LDFLAGS) -L.. -l:libezgrpc2.a -lnghttp2 -lz -lpthread -luuid -o $@

compression.o: compression.c
	$(CC) -c $(CFLAGS) $< -o $@

compression: compression.o unity/unity.o ../libezgrpc2.a
	$(CC) $(CFLAGS) compression.o unity/unity.o $(LDFLAGS) -L.. -l:libezgrpc2.a -lnghttp2 -lz -lpthread -luuid -o $@

health.o: health.c
	$(CC) -c $(CFLAGS) $< -o $@

health: health.o unity/unity.o ../libezgrpc2.a
	$(CC) $(CFLAGS) health.o unity/unity.o $(LDFLAGS) -L.. -l:libezgrpc2.a -lnghttp2 -lz -lpthread -luuid -o $@

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
	./roadmap
	./metadata
	./timeout
	./compression
	./health
ifeq ($(PROTOBUF_C),1)
	./pb
endif

clean:
	rm -f *.o list pthpool mem pb roadmap metadata timeout compression health test_msg.pb-c.c test_msg.pb-c.h
