-include ../config.mk

CFLAGS += -I../include
LDFLAGS += -L../src -l:libezgrpc2.a $(EZGRPC2_DEPS)

CFLAGS+=$(EXTRACFLAGS)

targets = multi_threaded.bin
targets += single_threaded.bin


ifeq ($(HAVE_SECCOMP),1)
  LDFLAGS += -lseccomp
  CFLAGS += -DHAVE_SECCOMP
endif

all : $(targets)
ifeq ($(PROTOBUF_C),1)
	$(MAKE) -C python_client_c_server
endif
ifeq ($(FLATCC),1)
	$(MAKE) -C flatbuffers
endif

PYTHON ?= python3

# starts an example, runs its test, stops it
define run_example_test
	@./$(1) $(2) > $(1).log 2>&1 & PID=$$!; \
	trap 'kill $$PID 2>/dev/null || true; wait $$PID 2>/dev/null || true' EXIT INT TERM; \
	sleep 1; \
	$(PYTHON) test_examples.py $(3) $(2); STATUS=$$?; \
	kill $$PID 2>/dev/null || true; wait $$PID 2>/dev/null || true; \
	if [ $$STATUS -ne 0 ]; then cat $(1).log; fi; \
	exit $$STATUS
endef

test: $(targets)
	$(call run_example_test,single_threaded.bin,19041,single)
	$(call run_example_test,multi_threaded.bin,19042,multi)

clean :
	rm -f $(targets) *.o *.log
	$(MAKE) -C python_client_c_server clean
	$(MAKE) -C flatbuffers clean

../src/libezgrpc2.a:
	$(MAKE) -C ../src

%.bin : %.o ../src/libezgrpc2.a
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@
