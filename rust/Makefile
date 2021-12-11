
all: pgm

pgm: main.rs
	rustc main.rs -L$(CHARM_HOME)/lib -lconverse -lmemory-default -lthreads-default -lconverse -lm -ltrace-converse -lmemory-default -lthreads-default -lldb-rand -lconverse -lckqt -lz -lpthread -lz -lm -l conv-static.o -l c++ -o pgm
	cp $(CHARM_HOME)/bin/charmrun .

test: pgm
	./charmrun +p4 ./pgm $(TESTOPTS)
