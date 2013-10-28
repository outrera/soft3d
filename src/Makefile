CC=gcc
CFLAGS=$(shell sdl-config --cflags --libs) -lSDL_image -march=pentium4 -O4 -Wall
SHARED_OBJS= sdld3d.o bootstrap.o font.o

%.o: %.c
	${CC} ${CFLAGS} -c -o $@ $<

all: crate stars

crate: crate.o ${SHARED_OBJS}
	${CC} ${CFLAGS} $@.o ${SHARED_OBJS} -o $@
	strip --strip-all $@

stars: stars.o ${SHARED_OBJS}
	${CC} ${CFLAGS} $@.o ${SHARED_OBJS} -o $@
	strip --strip-all $@

.PHONY: test stars clean

clean:
	$(RM) -f *.o *.s crate stars
