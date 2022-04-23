# TODO ( switchover to cmake )

CXX=$(CHARM_HOME)/bin/charmc # -language charm++

OPTS?=-g3
INCLUDE_PATH=../../include/hypercomm/
INCLUDES=-I../../include -I$(INCLUDE_PATH)
OPTS:=$(OPTS) $(INCLUDES) -std=c++11
FORMAT_OPTS?=-i --style Google
CMK_NUM_PES?=2
BINARY=pgm

# ../../libs/core.o: ../../src/core.cc
# 	mkdir -p ../../libs
# 	$(CXX) $(OPTS) -c -o ../../libs/core.o ../../src/core.cc

$(INCLUDE_PATH)/workgroup.decl.h: $(INCLUDE_PATH)/workgroup.ci
	charmc $(INCLUDE_PATH)/workgroup.ci
	mv workgroup.de*.h $(INCLUDE_PATH)

format:
	clang-format $(FORMAT_OPTS) ../../src/*.cc ../../include/*.hh ./$(BINARY).cc

clean:
	rm -f $(INCLUDE_PATH)/*.de*.h *.o charmrun $(BINARY)
