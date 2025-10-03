all: taiko-explosion
	@if [ x$(DEFRINGE) = x1 ]; then $(MAKE) defringe; fi

.SUFFIXES:
.SUFFIXES: .c .o .glsl .spv
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.glsl.spv:
	glslang -V100 -o $@ $<

TAIKO_EXPLOSION_OBJ=\
	../animations/taiko-hitexplosion.png \
	demultiply.vert.spv \
	demultiply.frag.spv \
	taiko-explosion.o \
	taiko-explosion.vert.spv \
	taiko-explosion.frag.spv \
	textured.vert.spv \
	textured.frag.spv

taiko-explosion: $(TAIKO_EXPLOSION_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ taiko-explosion.o -lm -lSDL3 -lSDL3_image

defringe: defringe.o defringe.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ defringe.o -lpng

clean:
	-rm defringe
	-rm defringe.exe
	-rm defringe.o
	-rm taiko-explosion
	-rm taiko-explosion.exe
	-rm taiko-explosion.o
	-rm taiko-explosion.vert.spv
	-rm taiko-explosion.frag.spv
	-rm textured.vert.spv
	-rm textured.frag.spv
