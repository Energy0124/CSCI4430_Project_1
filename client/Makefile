CC=gcc
CFLAGS=-Wall -I.
LDFLAGS=-lpthread -lrt

EXE = client

OBJ = client.o mtcp_client.o

${EXE}: ${OBJ}
	${CC} ${CFLAGS} -o ${EXE} ${OBJ} ${LDFLAGS}

clean:
	rm -f ${EXE} ${OBJ}
