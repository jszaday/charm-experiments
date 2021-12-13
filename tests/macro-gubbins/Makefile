
CXX=clang++

OPTS?=-g3
OPTS:=$(OPTS) -std=c++11

all: pgm

pgm.o: pgm.cc
	$(CXX) $(OPTS) -c -o pgm.o pgm.cc

ep.o: ep.cc
	$(CXX) $(OPTS) -c -o ep.o ep.cc

pgm: pgm.o ep.o
	$(CXX) $(OPTS) ep.o pgm.o -o pgm

test: pgm
	./pgm
 
clean:
	rm -f pgm *.o