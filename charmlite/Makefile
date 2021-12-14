
CXX=$(CHARM_HOME)/bin/charmc -language converse++

OPTS?=-g3
OPTS:=$(OPTS) -std=c++11
FORMAT_OPTS?=-i --style Google
CMK_NUM_PES?=2

all: pgm

pgm.o: pgm.cc
	$(CXX) $(OPTS) -c -o pgm.o pgm.cc

core.o: core.cc
	$(CXX) $(OPTS) -c -o core.o core.cc

pgm: pgm.o core.o
	$(CXX) $(OPTS) core.o pgm.o -o pgm

test: pgm
	./charmrun +p$(CMK_NUM_PES) ./pgm $(TESTOPTS)
 
format:
	clang-format $(FORMAT_OPTS) *.cc *.hh

clean:
	rm -f pgm *.o