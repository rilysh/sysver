PROGRAM		= sysver
CFLAGS		= -O2

all:
	${CC} ${CFLAGS} ${PROGRAM}.c -o ${PROGRAM}

clean:
	rm -f ${PROGRAM}
