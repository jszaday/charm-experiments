CHARMC=$(CHARM_HOME)/bin/charmc
OPTS:=$(OPTS) -O2 -g3 -Wall -std=c++11 -language converse++
LIBS:=-latomic -lrt -lxpmem

BINARY=tester

all: $(BINARY)

$(BINARY): $(BINARY).cc
	$(CHARMC) $(OPTS) -o $(BINARY) $(BINARY).cc $(LIBS)

test: $(BINARY)
	./charmrun +p2 ++local ./$(BINARY)

clean:
	rm -f $(BINARY)
