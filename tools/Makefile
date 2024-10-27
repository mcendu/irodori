.POSIX:

all:
	@if [ x$(DEFRINGE) = x1 ]; then $(MAKE) defringe; fi

.SUFFIXES:
.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

defringe: defringe.o defringe.h
	$(CC) $(CFLAGS) -o $@ defringe.o -lpng

clean:
	-rm defringe
	-rm defringe.o
