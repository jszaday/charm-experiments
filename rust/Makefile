# TODO ( in the spirit of Rust, we should use cargo! )

all: pgm

ffi.o: ffi.c
	charmc -language converse ffi.c -c -o ffi.o

pgm: main.rs ffi.o
	rustc main.rs -L$(CHARM_HOME)/lib -lconverse -lmemory-default -lthreads-default -lconverse -lm -ltrace-converse -lmemory-default -lthreads-default -lldb-rand -lconverse -lckqt -lz -lpthread -lz -lm -l conv-static.o -l c++ -L$(shell pwd) -l ffi.o -o pgm
	cp $(CHARM_HOME)/bin/charmrun .

test: pgm
	./charmrun +p4 ./pgm $(TESTOPTS)

clean:
	rm -f charmrun ffi.o pgm
