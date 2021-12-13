
CXX=$(CHARM_HOME)/bin/charmc -language converse++

OPTS?=-g3
OPTS:=$(OPTS) -std=c++11
FORMAT_OPTS?=-i --style Google

all: pgm

pgm.o: pgm.cc
	$(CXX) $(OPTS) -c -o pgm.o pgm.cc

ep.o: ep.cc
	$(CXX) $(OPTS) -c -o ep.o ep.cc

pgm: pgm.o ep.o
	$(CXX) $(OPTS) ep.o pgm.o -o pgm

test: pgm
	./pgm
 
format:
	clang-format $(FORMAT_OPTS) *.cc *.hh

clean:
	rm -f pgm *.o