CC ?= gcc
AR ?= ar
CXX ?= g++

CONFIG_PATH_MAX_LEN = 64
CONFIG_STREAM_MAX_HEADERS = 32
CONFIG_CACHE_LINE_SIZE = 64

export PATH := $(HOME)/.local/bin:$(PATH)

# Allocator configuration:
#   ALLOCATOR=default   (standard libc malloc)
#   ALLOCATOR=mimalloc  (Microsoft mimalloc, links -lmimalloc)
#   ALLOCATOR=jemalloc  (jemalloc, links -ljemalloc)
#   ALLOCATOR=custom    (runtime pluggable custom allocator via ezgrpc2_set_allocator)
# Convenience aliases:
#   make MIMALLOC=1
#   make JEMALLOC=1
ALLOCATOR ?= default

ifeq ($(MIMALLOC),1)
  ALLOCATOR = mimalloc
endif

ifeq ($(JEMALLOC),1)
  ALLOCATOR = jemalloc
endif

ifeq ($(ALLOCATOR),mimalloc)
  CFLAGS += -DEZGRPC2_USE_MIMALLOC
  LDFLAGS += -lmimalloc
endif

ifeq ($(ALLOCATOR),jemalloc)
  CFLAGS += -DEZGRPC2_USE_JEMALLOC
  LDFLAGS += -ljemalloc
endif

ifeq ($(ALLOCATOR),custom)
  CFLAGS += -DEZGRPC2_USE_CUSTOM_ALLOCATOR
endif

# Protobuf-C integration:
# Set PROTOBUF_C=1 to enable protobuf-c support (or 0 to disable).
# Defaults to 1 if protobuf-c is available.
ifeq ($(origin PROTOBUF_C), undefined)
  PROTOBUF_C := $(shell (pkg-config --exists libprotobuf-c 2>/dev/null || test -f $(HOME)/.local/include/protobuf-c/protobuf-c.h || test -f /usr/include/protobuf-c/protobuf-c.h) && echo 1 || echo 0)
endif

ifeq ($(PROTOBUF_C),1)
  CFLAGS += -DEZGRPC2_ENABLE_PROTOBUF_C
  LDFLAGS += -lprotobuf-c
endif

# FlatBuffers integration (flatcc):
# Set FLATCC=1 to enable the flatcc helpers (or 0 to disable).
# Defaults to 1 if flatcc is found, in FLATCC_PREFIX when it is set.
# FLATCC_PREFIX=<dir> points at a flatcc installed outside the system paths
# (<dir>/bin/flatcc, <dir>/include/flatcc, <dir>/lib/libflatccrt.a), e.g. by
# scripts/build-flatcc.sh. FLATCC_LIBS must follow libezgrpc2.a on the link line.
ifneq ($(FLATCC_PREFIX),)
  FLATCC_BIN ?= $(FLATCC_PREFIX)/bin/flatcc
  FLATCC_INCLUDE_DIRS = $(FLATCC_PREFIX)/include
else
  FLATCC_BIN ?= flatcc
  FLATCC_INCLUDE_DIRS = /usr/include /usr/local/include $(HOME)/.local/include
endif

ifeq ($(origin FLATCC), undefined)
  FLATCC := $(if $(wildcard $(addsuffix /flatcc/flatcc_builder.h,$(FLATCC_INCLUDE_DIRS))),1,0)
endif

ifeq ($(FLATCC),1)
  CFLAGS += -DEZGRPC2_ENABLE_FLATCC
  ifneq ($(FLATCC_PREFIX),)
    CFLAGS += -I$(FLATCC_PREFIX)/include
    FLATCC_LIBS = -L$(FLATCC_PREFIX)/lib -lflatccrt
  else
    FLATCC_LIBS = -lflatccrt
  endif
endif

# Zlib compression support:
ifeq ($(origin ZLIB), undefined)
  ZLIB := $(shell (pkg-config --exists zlib 2>/dev/null || test -f /usr/include/zlib.h) && echo 1 || echo 0)
endif

ifeq ($(ZLIB),1)
  CFLAGS += -DEZGRPC2_ENABLE_ZLIB
  LDFLAGS += -lz
endif

# Libuv event loop:
LIBUV_CFLAGS ?= $(shell pkg-config --cflags libuv 2>/dev/null)
LIBUV_LIBS ?= $(shell pkg-config --libs libuv 2>/dev/null || echo "-luv")

CFLAGS += $(LIBUV_CFLAGS)
LDFLAGS += $(LIBUV_LIBS)

# TLS backend:
#   TLS=openssl                      OpenSSL (the default when it is installed)
#   TLS=libressl|boringssl|awslc     the OpenSSL forks, usually with TLS_PREFIX
#   TLS=wolfssl|gnutls|rustls        wolfSSL, GnuTLS, rustls (rustls-ffi)
#   TLS=none                         no TLS (OPENSSL=0 is an alias)
# TLS_PREFIX=<dir> points at a library installed outside the system paths
# (<dir>/include and <dir>/lib), e.g. by scripts/build-tls-backend.sh.
# Run `make clean` after switching backends.
TLS_BACKENDS = openssl libressl boringssl awslc wolfssl gnutls rustls

ifeq ($(origin TLS), undefined)
  ifeq ($(OPENSSL),0)
    TLS := none
  else
    TLS := $(shell (pkg-config --exists openssl 2>/dev/null || test -f /usr/include/openssl/ssl.h) && echo openssl || echo none)
  endif
endif

ifeq ($(filter $(TLS),$(TLS_BACKENDS) none),)
  $(error unknown TLS backend '$(TLS)'. Use one of: $(TLS_BACKENDS) none)
endif

# TLS_IMPL names the src/ezgrpc2_tls_<impl>.c that drives the library.
# TLS_LIBS must follow libezgrpc2.a on the link line.
ifneq ($(filter $(TLS),openssl libressl boringssl awslc),)
  TLS_IMPL = openssl
  TLS_LIBS = -lssl -lcrypto
else ifeq ($(TLS),wolfssl)
  TLS_IMPL = wolfssl
  TLS_LIBS = -lwolfssl
else ifeq ($(TLS),gnutls)
  TLS_IMPL = gnutls
  TLS_LIBS = -lgnutls
else ifeq ($(TLS),rustls)
  TLS_IMPL = rustls
  # librustls.a is a Rust static library and needs these system libraries
  TLS_LIBS = -lrustls -lgcc_s -lutil -lrt -lpthread -lm -ldl
endif

ifneq ($(TLS),none)
  CFLAGS += -DEZGRPC2_ENABLE_TLS -DEZGRPC2_TLS_$(shell echo $(TLS) | tr a-z A-Z)
endif

ifneq ($(TLS_PREFIX),)
  CFLAGS += -I$(TLS_PREFIX)/include
  TLS_LIBS := -L$(TLS_PREFIX)/lib -Wl,-rpath,$(TLS_PREFIX)/lib $(TLS_LIBS)
endif

CFLAGS += -DCONFIG_PATH_MAX_LEN=$(CONFIG_PATH_MAX_LEN)
CFLAGS += -DCONFIG_STREAM_MAX_HEADERS=$(CONFIG_STREAM_MAX_HEADERS)
CFLAGS += -DCONFIG_CACHE_LINE_SIZE=$(CONFIG_CACHE_LINE_SIZE)

# C23 standard flag with fallback to -std=c2x for GCC < 14
C_STD ?= $(shell $(CC) -std=c23 -E -x c /dev/null >/dev/null 2>&1 && echo "-std=c23" || echo "-std=c2x")

CFLAGS += -Wall $(C_STD) -pedantic
CFLAGS += -D_GNU_SOURCE
CFLAGS += -D_FORTIFY_SOURCE=2

# Ensure nullptr is supported across all compilers (e.g. older clang/gcc with -std=c2x)
HAS_NULLPTR := $(shell printf "int main(void){void *p = nullptr; (void)p; return 0;}\n" | $(CC) $(C_STD) -x c - >/dev/null 2>&1 && echo 1 || echo 0)
ifeq ($(HAS_NULLPTR),0)
  CFLAGS += -Dnullptr="((void*)0)"
endif

# Ensure alignof is supported across all compilers (e.g. older gcc/clang with -std=c2x)
HAS_ALIGNOF := $(shell printf "typedef struct { int x; } t_s; int main(void){return (int)alignof(t_s);}\n" | $(CC) $(C_STD) -x c - >/dev/null 2>&1 && echo 1 || echo 0)
ifeq ($(HAS_ALIGNOF),0)
  CFLAGS += -Dalignof=_Alignof
endif

ifneq ($(wildcard $(HOME)/.local/lib),)
  LDFLAGS += -L$(HOME)/.local/lib -Wl,-rpath,$(HOME)/.local/lib
endif
ifneq ($(wildcard $(HOME)/.local/include),)
  CFLAGS += -I$(HOME)/.local/include
endif
ifneq ($(wildcard $(HOME)/.local/usr/lib/x86_64-linux-gnu),)
  LDFLAGS += -L$(HOME)/.local/usr/lib/x86_64-linux-gnu -Wl,-rpath,$(HOME)/.local/usr/lib/x86_64-linux-gnu
endif

# enable optimization if D is not set
ifeq ($(strip $(D)),)
  CFLAGS += -O2
endif

# used in setting up in command line
EXTRACFLAGS ?=

ifeq ($(D),1)
  CFLAGS += -ggdb3
  CFLAGS += -DEZGRPC2_DEBUG
endif

ifeq ($(T),1)
  CFLAGS += -DEZGRPC2_TRACE
endif

ifeq ($(ASAN),1)
  LDFLAGS += -fsanitize=address
  CFLAGS += -fsanitize=address
endif
