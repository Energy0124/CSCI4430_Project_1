CC=gcc
CFLAGS=-Wall -I.
LDFLAGS=-lpthread -lrt

EXE = server

OBJ = server.o mtcp_server.o

${EXE}: ${OBJ}
	${CC} ${CFLAGS} -o ${EXE} ${OBJ} ${LDFLAGS}

clean:
	rm -f ${EXE} ${OBJ}
