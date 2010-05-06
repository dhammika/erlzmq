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

#include <stdio.h>
#include <string.h>
#include <zmq.h>
#include "zmq_drv.h"

#ifdef ZMQDRV_DEBUG
#define zmqdrv_fprintf(...)   fprintf(stderr, __VA_ARGS__)
#else
#define zmqdrv_fprintf(...)
#endif

/* Forward declarations */
static ErlDrvData zmqdrv_start(ErlDrvPort port, char* cmd);
static void zmqdrv_stop(ErlDrvData handle);
static void zmqdrv_outputv(ErlDrvData handle, ErlIOVec *ev);
static void zmqdrv_error(zmq_drv_t *zmq_drv, const char *errstr);
static void zmqdrv_ok(zmq_drv_t *zmq_drv);
static void zmqdrv_binary_ok(zmq_drv_t *zmq_drv, void *data, size_t size);
static void zmqdrv_init(zmq_drv_t *zmq_drv, ErlIOVec *ev);
static void zmqdrv_term(zmq_drv_t *zmq_drv, ErlIOVec *ev);
static void zmqdrv_socket(zmq_drv_t *zmq_drv, ErlIOVec *ev);
static void zmqdrv_close(zmq_drv_t *zmq_drv, ErlIOVec *ev);
static void zmqdrv_bind(zmq_drv_t *zmq_drv, ErlIOVec *ev);
static void zmqdrv_connect(zmq_drv_t *zmq_drv, ErlIOVec *ev);
static void zmqdrv_send(zmq_drv_t *zmq_drv, ErlIOVec *ev);
static void zmqdrv_recv(zmq_drv_t *zmq_drv, ErlIOVec *ev);
static void zmqdrv_setsockopt(zmq_drv_t *zmq_drv, ErlIOVec *ev);
static void zmqdrv_getsockopt(zmq_drv_t *zmq_drv, ErlIOVec *ev);

/* Callbacks */
static ErlDrvEntry basic_driver_entry = {
    NULL,                               /* init */
    zmqdrv_start,                       /* startup (defined below) */
    zmqdrv_stop,                        /* shutdown (defined below) */
    NULL,                               /* output */
    NULL,                               /* ready_input */
    NULL,                               /* ready_output */
    "zmq_drv",                          /* driver name */
    NULL,                               /* finish */
    NULL,                               /* handle */
    NULL,                               /* control */
    NULL,                               /* timeout */
    zmqdrv_outputv,                     /* outputv, binary output */
    NULL,                               /* ready_async */
    NULL,                               /* flush */
    NULL,                               /* call */
    NULL,                               /* event */
    ERL_DRV_EXTENDED_MARKER,            /* ERL_DRV_EXTENDED_MARKER */
    ERL_DRV_EXTENDED_MAJOR_VERSION,     /* ERL_DRV_EXTENDED_MAJOR_VERSION */
    ERL_DRV_EXTENDED_MAJOR_VERSION,     /* ERL_DRV_EXTENDED_MINOR_VERSION */
    ERL_DRV_FLAG_USE_PORT_LOCKING       /* ERL_DRV_FLAGs */
};

/* Driver internal, hook to driver API. */
DRIVER_INIT(basic_driver)
{
    return &basic_driver_entry;
}

/* Driver Start, called on port open. */
static ErlDrvData
zmqdrv_start(ErlDrvPort port, char* cmd)
{
    zmq_drv_t* drv = (zmq_drv_t*)driver_alloc(sizeof(zmq_drv_t));
    drv->port = port;
    drv->zmq_context = NULL;
    return (ErlDrvData)drv;
}

/* Driver Stop, called on port close. */
static void
zmqdrv_stop(ErlDrvData handle)
{
    zmq_drv_t *drv = (zmq_drv_t *)handle;

    if (drv->zmq_context)
        zmq_term(drv->zmq_context);
    driver_free(drv);
}

/* Erlang command, called on binary input from VM. */
static void
zmqdrv_outputv(ErlDrvData handle, ErlIOVec *ev)
{
    uint32_t cmd;

    zmq_drv_t *drv = (zmq_drv_t *)handle;

    cmd = ntohl(*(uint32_t *)ev->iov[1].iov_base);

    switch (cmd) {
        case ZMQ_INIT :
            zmqdrv_init(drv, ev);
            break;
        case ZMQ_TERM :
            zmqdrv_term(drv, ev);
            break;
        case ZMQ_SOCKET :
            zmqdrv_socket(drv, ev);
            break;
        case ZMQ_CLOSE :
            zmqdrv_close(drv, ev);
            break;
        case ZMQ_SETSOCKOPT :
            zmqdrv_setsockopt(drv, ev);
            break;
        case ZMQ_GETSOCKOPT :
            zmqdrv_getsockopt(drv, ev);
            break;
        case ZMQ_BIND :
            zmqdrv_bind(drv, ev);
            break;
        case ZMQ_CONNECT :
            zmqdrv_connect(drv, ev);
            break;
        case ZMQ_SEND :
            zmqdrv_send(drv, ev);
            break;
        case ZMQ_RECV :
            zmqdrv_recv(drv, ev);
            break;
        default :
            zmqdrv_error(drv, "Invalid driver command");
    }
}

static void
zmqdrv_error(zmq_drv_t *zmq_drv, const char *errstr)
{
    ErlDrvTermData spec[] =
        {ERL_DRV_ATOM, driver_mk_atom((char *)"error"),
		 ERL_DRV_ATOM, driver_mk_atom((char *)errstr), ERL_DRV_TUPLE, 2};
    driver_output_term(zmq_drv->port, spec, sizeof(spec)/sizeof(spec[0]));
}

static void
zmqdrv_ok(zmq_drv_t *zmq_drv)
{
    ErlDrvTermData spec[] = 
        {ERL_DRV_ATOM, driver_mk_atom((char *)"ok")};
    driver_output_term(zmq_drv->port, spec, sizeof(spec)/sizeof(spec[0]));
}

static void 
zmqdrv_binary_ok(zmq_drv_t *zmq_drv, void *data, size_t size)
{
    ErlDrvBinary *ev_data = driver_alloc_binary(size);
    /* Copy payload. */
    ev_data->orig_size = size;
    memcpy(ev_data->orig_bytes, data, size);

    ErlDrvTermData spec[] = 
        {ERL_DRV_ATOM, driver_mk_atom((char *)"ok"),
		 ERL_DRV_BINARY, (ErlDrvTermData)ev_data, size, 0,
		 ERL_DRV_TUPLE, 2};
    
    driver_output_term(zmq_drv->port, spec, sizeof(spec)/sizeof(spec[0]));
    driver_free_binary(ev_data);
}

static void
zmqdrv_init(zmq_drv_t *zmq_drv, ErlIOVec *ev)
{
#if 0
    /* 
     * FIXME 
     * Use ei_decode_* to decode input from erlang VM.
     * This stuff is not documented anywhere, for now 
     * binary ErlIOVec is decoded by poking in iov struct.
     */

    ErlDrvBinary *ev_app_threads = ev->binv[2];
    ErlDrvBinary *ev_io_threads = ev->binv[3];
    ErlDrvBinary *ev_flags = ev->binv[4];
    int i = 0;
    
    if (ei_decode_long(ev_app_threads->orig_bytes, &i, &app_threads) < 0) {
        zmqdrv_error(zmq_drv, "Invalid command");
        return;
    }

    i = 0;
    if (ei_decode_long(ev_io_threads->orig_bytes, &i, &io_threads) < 0) {
        zmqdrv_error(zmq_drv, "Invalid command");
        return;
    }

    i = 0;
    if (ei_decode_long(ev_flags->orig_bytes, &i, &flags) < 0) {
        zmqdrv_error(zmq_drv, "Invalid command");
        return;
    }
#endif

    uint32_t app_threads;
    uint32_t io_threads; 
    uint32_t flags; 

    app_threads = ntohl(*(uint32_t *)(ev->iov[1].iov_base + 4));
    io_threads = ntohl(*(uint32_t *)(ev->iov[1].iov_base + 8));
    flags = ntohl(*(uint32_t *)(ev->iov[1].iov_base + 12));

    zmqdrv_fprintf("appthreads = %u, iothreads = %u\n", app_threads, io_threads);

    zmq_drv->zmq_context = (void *)zmq_init(app_threads, io_threads, flags);
    if (!zmq_drv->zmq_context) {
        zmqdrv_error(zmq_drv, zmq_strerror(zmq_errno()));
        return;
    }

    zmqdrv_ok(zmq_drv);
}

static void
zmqdrv_term(zmq_drv_t *zmq_drv, ErlIOVec *ev)
{
    if (!zmq_drv->zmq_context) {
        zmqdrv_error(zmq_drv, "Uninitialized context");
        return;
    }

    if (zmq_term(zmq_drv->zmq_context) < 0) {
        zmqdrv_error(zmq_drv, zmq_strerror(zmq_errno()));
        return;
    }

    zmqdrv_ok(zmq_drv);
    zmq_drv->zmq_context = NULL;
}

static void
zmqdrv_socket(zmq_drv_t *zmq_drv, ErlIOVec *ev)
{
    uint32_t type;
    void *s;
        
    type = ntohl(*(uint32_t *)(ev->iov[1].iov_base + 4));

    s = zmq_socket(zmq_drv->zmq_context, type);
    if (!s) {
        zmqdrv_error(zmq_drv->zmq_context, zmq_strerror(zmq_errno()));
        return;
    }

    zmqdrv_fprintf("socket %p\n", s);
    zmqdrv_binary_ok(zmq_drv, (void *)&s, sizeof(s));
}

static void
zmqdrv_close(zmq_drv_t *zmq_drv, ErlIOVec *ev)
{
    void *s;

    memcpy(&s, (void *)(ev->iov[1].iov_base + 4), sizeof(s));

    zmqdrv_fprintf("close %p\n", s);
    if (zmq_close(s) < 0) {
        zmqdrv_error(zmq_drv->zmq_context, zmq_strerror(zmq_errno()));
        return;
    }
    
    zmqdrv_ok(zmq_drv);
}

static void 
zmqdrv_setsockopt(zmq_drv_t *zmq_drv, ErlIOVec *ev)
{
    void *s;
    uint32_t option;
    void *optval; 
    size_t optvallen;

    memcpy(&s, (void *)(ev->iov[1].iov_base + 4), sizeof(s));
    option = ntohl(*(uint32_t *)(ev->iov[1].iov_base + 4 + sizeof(s)));
    optval = (void *)(ev->iov[1].iov_base + 4 + sizeof(s) + 4);
    optvallen = ev->iov[1].iov_len - (4 + sizeof(s) + 4);

    if (!s) {
        zmqdrv_error(zmq_drv->zmq_context, "Invalid socket");
        return;
    }

    zmqdrv_fprintf("setsockopt %p\n", s);
    if (zmq_setsockopt(s, option, optval, optvallen) < 0) {
        zmqdrv_error(zmq_drv->zmq_context, zmq_strerror(zmq_errno()));
        return;
    }

    zmqdrv_ok(zmq_drv);
}

static void 
zmqdrv_getsockopt(zmq_drv_t *zmq_drv, ErlIOVec *ev)
{
    zmqdrv_error(zmq_drv->zmq_context, "Not implemented yet, ha ha..");
}

static void
zmqdrv_bind(zmq_drv_t *zmq_drv, ErlIOVec *ev)
{
    void *s;
    char addr[512];
    uint16_t size;

    memcpy(&s, (void *)(ev->iov[1].iov_base + 4), sizeof(s));
    /*
     * FIXME 
     * Address is prefixed with 131,107,x,x 4 byte header!
     * 131 - version, 107 - binary format, x,x string length.
     */
    size = ntohs(*(uint16_t *)(ev->iov[1].iov_base + 14));
    if (size > sizeof(addr) - 1) {
        zmqdrv_error(zmq_drv->zmq_context, "Invalid address");
        return;
    }

    memcpy(addr, ev->iov[1].iov_base + 16, size);
    addr[size] = '\0';

    if (!s || !addr[0]) {
        zmqdrv_error(zmq_drv->zmq_context, "Invalid argument");
        return;
    }

    if (zmq_bind(s, addr) < 0) {
        zmqdrv_error(zmq_drv->zmq_context, zmq_strerror(zmq_errno()));
        return;
    }

    zmqdrv_ok(zmq_drv);
}

static void
zmqdrv_connect(zmq_drv_t *zmq_drv, ErlIOVec *ev)
{
    void *s;
    char addr[512];
    uint16_t size;

    memcpy(&s, (void *)(ev->iov[1].iov_base + 4), sizeof(s));
    /*
     * FIXME 
     * Address is prefixed with 131,107,x,x 4 byte header!
     * 131 - version, 107 - binary format, x,x string length.
     */
    size = ntohs(*(uint16_t *)(ev->iov[1].iov_base + 14));
    if (size > sizeof(addr) - 1) {
        zmqdrv_error(zmq_drv->zmq_context, "Invalid address");
        return;
    }

    memcpy(addr, ev->iov[1].iov_base + 16, size);
    addr[size] = '\0';

    if (!s || !addr[0]) {
        zmqdrv_error(zmq_drv->zmq_context, "Invalid argument");
        return;
    }

    if (zmq_connect(s, addr) < 0) {
        zmqdrv_error(zmq_drv->zmq_context, zmq_strerror(zmq_errno()));
        return;
    }

    zmqdrv_ok(zmq_drv);
}

static void
zmqdrv_send(zmq_drv_t *zmq_drv, ErlIOVec *ev)
{
    void *s;
    void *data;
    size_t size;
    void *buf;
    zmq_msg_t msg;

    memcpy(&s, (void *)(ev->iov[1].iov_base + 4), sizeof(s));
    data = (void *)(ev->iov[1].iov_base + 12);
    size = ev->iov[1].iov_len - 12;

    /* FIXME Is there a way to avoid this? */
    buf = malloc(size);
    if (!buf) {
        zmqdrv_error(zmq_drv->zmq_context, strerror(ENOMEM));
        return;
    }
    memcpy(buf, data, size);

    if (zmq_msg_init_data(&msg, buf, size, NULL, NULL)) {
        zmqdrv_error(zmq_drv->zmq_context, zmq_strerror(zmq_errno()));
        return;
    }

    if (zmq_send(s, &msg, 0) < 0) {
        zmqdrv_error(zmq_drv->zmq_context, zmq_strerror(zmq_errno()));
        zmq_msg_close(&msg);
        return;
    }

    zmqdrv_ok(zmq_drv);
}

static void
zmqdrv_recv(zmq_drv_t *zmq_drv, ErlIOVec *ev)
{
    void *s;
    zmq_msg_t msg;

    memcpy(&s, (void *)(ev->iov[1].iov_base + 4), sizeof(s));

    zmq_msg_init(&msg);

    if (zmq_recv(s, &msg, 0) < 0) {
        zmqdrv_error(zmq_drv->zmq_context, zmq_strerror(zmq_errno()));
        zmq_msg_close(&msg);
        return;
    }

    zmqdrv_binary_ok(zmq_drv, zmq_msg_data(&msg), zmq_msg_size(&msg));
    zmq_msg_close(&msg);
}
