CHARMC=$(CHARM_HOME)/bin/charmc
CHARMRUN=$(CHARM_HOME)/bin/charmrun
OPTS:=$(OPTS) -O2 -g3 -lrt

CMK_NUM_PES?=2
TESTOPTS:=$(TESTOPTS) +p$(CMK_NUM_PES)

BINARY=tester

all: $(BINARY)

$(BINARY).de*.h: $(BINARY).ci
	$(CHARMC) $(BINARY).ci

$(BINARY).o: $(BINARY).cc $(BINARY).de*.h
	$(CHARMC) -language charm++ $(OPTS) -c -o $(BINARY).o $(BINARY).cc 

$(BINARY): $(BINARY).o
	$(CHARMC) $(OPTS) -o $(BINARY) $(BINARY).o -lxpmem

test: $(BINARY)
	$(CHARMRUN) ./$(BINARY) $(TESTOPTS)

clean:
	rm -f conv-host *.o $(BINARY) $(BINARY).de*.h *.bak cthtest.*.log cthtest.sts *~ charmrun charmrun.exe cthtest.exe cthtest.pdb cthtest.ilk
