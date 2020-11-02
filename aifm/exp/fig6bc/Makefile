AIFM_PATH=../../
SHENANGO_PATH=$(AIFM_PATH)/../shenango
include $(SHENANGO_PATH)/shared.mk

librt_libs = $(SHENANGO_PATH)/bindings/cc/librt++.a
INC += -I$(SHENANGO_PATH)/bindings/cc -I$(AIFM_PATH)/inc -I$(SHENANGO_PATH)/ksched \
-I$(AIFM_PATH)/snappy/build -I$(AIFM_PATH)/snappy

main_src = main.cpp
main_obj = $(main_src:.cpp=.o)

lib_src = $(wildcard $(AIFM_PATH)/src/*.cpp)
lib_src := $(filter-out $(AIFM_PATH)/src/tcp_device_server.cpp,$(lib_src))
lib_obj = $(lib_src:.cpp=.o)

src = $(main_src) $(lib_src)
obj = $(src:.cpp=.o)
dep = $(obj:.o=.d)

CXXFLAGS := $(filter-out -std=gnu++17,$(CXXFLAGS))
override CXXFLAGS += -std=gnu++2a -fconcepts -Wno-unused-function -mcmodel=medium
override RUNTIME_LIBS += -lcryptopp -L$(AIFM_PATH)/snappy/build -lsnappy

#must be first
all: main

main: $(main_obj) $(librt_libs) $(RUNTIME_DEPS) $(main_obj) $(lib_obj)
	$(LDXX) -o $@ $(LDFLAGS) $(main_obj) $(lib_obj) $(librt_libs) $(RUNTIME_LIBS)

ifneq ($(MAKECMDGOALS),clean)
-include $(dep)   # include all dep files in the makefile
endif

#rule to generate a dep file by using the C preprocessor
#(see man cpp for details on the - MM and - MT options)
%.d: %.cpp
	@$(CXX) $(CXXFLAGS) $< -MM -MT $(@:.d=.o) >$@
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f *.o $(dep) main $(AIFM_PATH)/src/*.o
