# include this Makefile in all subprojects
# define SHENANGO_PATH before including
ifndef SHENANGO_PATH
$(error SHENANGO_PATH is not set)
endif

# build configuration options (set to y for "yes", n for "no")
CONFIG_MLX5=y
CONFIG_MLX4=n
CONFIG_SPDK=n
CONFIG_DEBUG=n
CONFIG_NATIVE=y
CONFIG_DIRECTPATH=y

# shared toolchain definitions
INC = -I$(SHENANGO_PATH)/inc
FLAGS  = -g -Wall -D_GNU_SOURCE $(INC)
LDFLAGS = -T $(SHENANGO_PATH)/base/base.ld
LD      = gcc-9
CC      = gcc-9
LDXX	= g++-9
CXX	= g++-9
AR      = ar
SPARSE  = sparse

# libraries to include
RUNTIME_DEPS = $(SHENANGO_PATH)/libruntime.a $(SHENANGO_PATH)/libnet.a \
	       $(SHENANGO_PATH)/libbase.a
RUNTIME_LIBS = $(SHENANGO_PATH)/libruntime.a $(SHENANGO_PATH)/libnet.a \
	       $(SHENANGO_PATH)/libbase.a -lpthread

# parse configuration options
ifeq ($(CONFIG_DEBUG),y)
FLAGS += -DDEBUG -DCCAN_LIST_DEBUG -rdynamic -O0 -ggdb -mssse3
LDFLAGS += -rdynamic
else
FLAGS += -DNDEBUG -O3
ifeq ($(CONFIG_NATIVE),y)
FLAGS += -march=native
else
FLAGS += -mssse3
endif
endif
ifeq ($(CONFIG_MLX5),y)
FLAGS += -DMLX5
else
ifeq ($(CONFIG_MLX4),y)
FLAGS += -DMLX4
endif
endif
ifeq ($(CONFIG_SPDK),y)
FLAGS += -DDIRECT_STORAGE
RUNTIME_LIBS += -L$(SHENANGO_PATH)/spdk/build/lib -L$(SHENANGO_PATH)/spdk/dpdk/build/lib
RUNTIME_LIBS += -lspdk_nvme -lspdk_util -lspdk_env_dpdk -lspdk_log -lspdk_sock \
		-ldpdk -lpthread -lrt -luuid -lcrypto -lnuma -ldl
INC += -I$(SHENANGO_PATH)/spdk/include
endif
ifeq ($(CONFIG_DIRECTPATH),y)
RUNTIME_LIBS += -L$(SHENANGO_PATH)/rdma-core/build/lib/statics/
RUNTIME_LIBS += -lmlx5 -libverbs -lnl-3 -lnl-route-3
INC += -I$(SHENANGO_PATH)/rdma-core/build/include
FLAGS += -DDIRECTPATH
endif

CFLAGS = -std=gnu11 $(FLAGS)
override CXXFLAGS += -std=gnu++17 -Wno-subobject-linkage $(FLAGS)

# handy for debugging
print-%  : ; @echo $* = $($*) 
