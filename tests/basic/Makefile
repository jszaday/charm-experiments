CHARMC=$(CHARM_HOME)/bin/charmc
INCLUDES=-I../../include
OPTS:=$(OPTS) -O2 -g3 -Wall -std=c++11 -language converse++
LIBS:=-latomic -lrt -lxpmem

BINARY=tester
CMK_NUM_PES?=2

all: $(BINARY)

$(BINARY): $(BINARY).cc
	$(CHARMC) $(OPTS) -o $(BINARY) $(BINARY).cc $(LIBS) $(INCLUDES)

test: $(BINARY)
	./charmrun +p$(CMK_NUM_PES) ++local ./$(BINARY)

clean:
	rm -f $(BINARY)
