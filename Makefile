CC=gcc
CFLAGS=-g3 -Og -Wall -Wextra -Wformat=1 -fdelete-null-pointer-checks -Wnull-dereference -Wuninitialized -fstrict-aliasing -Wstrict-aliasing -fstrict-overflow -Wstrict-overflow=4 -Wsuggest-attribute=format -Wmissing-format-attribute -Wsuggest-final-types -Wsuggest-final-methods -ftree-vrp -Warray-bounds -Wduplicated-cond -Wfloat-equal -Wundef -Wshadow -Wframe-larger-than=8192 -Wstack-usage=8192 -funsafe-loop-optimizations -Wunsafe-loop-optimizations -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion -Wsign-conversion -Wfloat-conversion -Wlogical-op -Wmissing-field-initializers -Wredundant-decls -Wdisabled-optimization
LDFLAGS=
LIBS=-lresolv
#Solaris
#LIBS= -lnsl -lsocket -lresolv
TARGET= nsping

OBJS= nsping.o dns-lib.o dns-rr.o
SRCS= nsping.c dns-lib.c dns-rr.c
HEADERS= nsping.h dns-lib.h dns-rr.h 

all: ${TARGET}

${TARGET} : ${OBJS}
	${CC} ${CFLAGS} -o ${TARGET} ${OBJS} ${LDFLAGS} ${LIBS}

tar : clean
	tar cvf ${TARGET}.tar *
	gzip ${TARGET}.tar

clean :
	rm -f *.o a.out ${TARGET} *.core

install: ${TARGET}
	mv ${TARGET} /usr/local/sbin
