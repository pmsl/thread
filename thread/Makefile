LIB = thread
ifndef PROG_ROOT
export PROG_ROOT := $(subst /$(LIB),,$(CURDIR))
endif

ifndef GXX
GXX = g++
endif
MODULE = threadpoll
OUTDIR = bin

BUILD_PARA += -o 
BUILD_PARA += $(OUTDIR)/$(MODULE)
BUILD_PARA += -lpthread

SRC = \
	$(PROG_ROOT)/common/common.cpp \
	$(PROG_ROOT)/log/log.cpp	\
	threadpool.cpp \
	test.cpp
	
all: $(SRC)
	$(GXX) $(BUILD_PARA) -I$(PROG_ROOT)   $(SRC)

clean:
	-rm -f bin/threadpool