CC=gcc
CFLAGS=-Wall -Wextra -O2
#Solaris
#CFLAGS= -g -Dsys5
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
