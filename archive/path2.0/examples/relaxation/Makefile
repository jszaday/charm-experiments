include ../../common.mk

all: pgm

pgm: pgm.o
	$(CXX) $(OPTS) -o pgm pgm.o

pgm.o: pgm.cpp pgm.decl.h $(INCLUDE_PATH)/workgroup.decl.h
	$(CXX) $(OPTS) -c -o pgm.o pgm.cpp

pgm.decl.h: pgm.ci
	$(CXX) pgm.ci

test: pgm
	./charmrun +p$(CMK_NUM_PES) ./pgm $(TESTOPTS)
