/*
 * ------------------------------------------------------------------
 * Erlang bindings for ZeroMQ.
 * ------------------------------------------------------------------
 * "THE BEER-WARE LICENSE"
 *  <dhammika@gmail.com> wrote this file. As long as you retain this 
 *  notice you can do whatever you want with this stuff. If we meet 
 *  some day, and you think this stuff is worth it, you can buy me a 
 *  beer in return.
 * ------------------------------------------------------------------
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
