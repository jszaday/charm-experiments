CXX?=g++
OPTS:=$(OPTS) -O2 -g3 -Wall -std=c++11
LIBS:=-lrt -latomic

BINARY=tester

all: $(BINARY)

$(BINARY): $(BINARY).cc
	$(CXX) $(OPTS) -o $(BINARY) $(BINARY).cc $(LIBS)

test: $(BINARY)
	./$(BINARY)

clean:
	rm -f $(BINARY)
