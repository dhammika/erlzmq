/*
 * ------------------------------------------------------------------
 * Erlang bindings for ZeroMQ.
 * ------------------------------------------------------------------
 * <dhammika@gmail.com> wrote this code, copyright disclaimed.
 * ------------------------------------------------------------------
 */

#include <erl_driver.h>
#include <ei.h>

/* Erlang driver commands. */
#define ZMQ_INIT        1
#define ZMQ_TERM        2
#define ZMQ_SOCKET      3
#define ZMQ_CLOSE       4
#define ZMQ_SETSOCKOPT  5
#define ZMQ_GETSOCKOPT  6
#define ZMQ_BIND        7
#define ZMQ_CONNECT     8
#define ZMQ_SEND        9
#define ZMQ_RECV        10

typedef struct {
    ErlDrvPort port;
    void *zmq_context;
} zmq_drv_t;
