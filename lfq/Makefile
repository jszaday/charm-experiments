CXX?=g++
OPTS:=$(OPTS) -O2 -g3 -Wall -std=c++11
LIBS:=-lrt -latomic

BINARY=tester

all: $(BINARY)

$(BINARY): $(BINARY).cc
	$(CXX) $(OPTS) -o $(BINARY) $(BINARY).cc $(LIBS)

test: $(BINARY)
	./$(BINARY) 0 &
	./$(BINARY) 1

clean:
	rm -f $(BINARY)
