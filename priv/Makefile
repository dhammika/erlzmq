ERL_INTERFACE_DIR=/usr/lib/erlang/lib/erl_interface-3.6.2
ZMQ_DIR=/usr/local/zeromq

LDFLAGS=-shared -fPIC ${ERL_INTERFACE_DIR}/lib/libei.a -lzmq -lpthread -luuid
CFLAGS=-Wall -ggdb -O0 -I${ZMQ_DIR}/include  -L${ZMQ_DIR}/lib

all:
	gcc zmq_drv.c -o zmq_drv.so ${CFLAGS} ${LDFLAGS} 

clean:
	rm -rf zmq_drv.o zmq_drv.so
