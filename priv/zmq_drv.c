/*
 * ------------------------------------------------------------------
 * Erlang bindings for ZeroMQ.
 * ------------------------------------------------------------------
 * <dhammika@gmail.com> wrote this code, copyright disclaimed.
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

/* 
 * Macros to decode ErlIOVec
 * FIXME Is there a standard API to decode binary terms?
 */
#define ZMQDRV_DECODE_INT16(ev, idx, offset, i)                                     \
    ((*(idx) < ev->vsize ||                                                         \
      *(offset) + sizeof(short) <= ev->iov[*(idx)].iov_len)                         \
        ?   *(i) =                                                                  \
                ntohs(*(int16_t *)((char *)ev->iov[*(idx)].iov_base + *(offset))),  \
            *(offset) =                                                             \
                (*(offset) + sizeof(short) == ev->iov[*(idx)].iov_len)              \
                    ? ++*(idx), 0 : *(offset) + sizeof(short),                      \
            (0)                                                                     \
        :   (-1))

#define ZMQDRV_DECODE_INT32(ev, idx, offset, i)                                     \
    ((*(idx) < ev->vsize ||                                                         \
      *(offset) + sizeof(int) <= ev->iov[*(idx)].iov_len)                           \
        ?   *(i) =                                                                  \
                ntohl(*(int32_t *)((char *)ev->iov[*(idx)].iov_base + *(offset))),  \
            *(offset) =                                                             \
                (*(offset) + sizeof(int) == ev->iov[*(idx)].iov_len)                \
                    ? ++*(idx), 0 : *(offset) + sizeof(int),                        \
            (0)                                                                     \
        :   (-1))

#define ZMQDRV_DECODE_SOCK(ev, idx, offset, s)                                      \
    ((*(idx) < ev->vsize ||                                                         \
      *(offset) + sizeof(void *) <= ev->iov[*(idx)].iov_len)                        \
        ?   *(s) = *(void **)((char *)ev->iov[*(idx)].iov_base + *(offset)),        \
            *(offset) =                                                             \
                (*(offset) + sizeof(void *) == ev->iov[*(idx)].iov_len)             \
                    ? ++*(idx), 0 : *(offset) + sizeof(void *),                     \
            (0)                                                                     \
        :   (-1))

/* Copy a binary of specified size into a buffer */
#define ZMQDRV_DECODE_BIN(ev, idx, offset, bin, sz)                                 \
    ((*(idx) < ev->vsize ||                                                         \
      *(offset) + sz <= ev->iov[*(idx)].iov_len)                                    \
        ?   memcpy(bin, ((char *)ev->iov[*(idx)].iov_base + *(offset)), sz),        \
            *(offset) =                                                             \
                (*(offset) + sz == ev->iov[*(idx)].iov_len)                         \
                    ? ++*(idx), 0 : *(offset) + sz,                                 \
            (0)                                                                     \
        :   (-1))

/* Return remaining buffer as a blob */
#define ZMQDRV_DECODE_BLOB(ev, idx, offset, blob, sz)                               \
    ((*(idx) < ev->vsize)                                                           \
        ?   *(blob) = ((char *)ev->iov[*(idx)].iov_base + *(offset)),               \
            *(sz) = ev->iov[*(idx)].iov_len - *(offset),                            \
            *(offset) = 0, ++*(idx),                                                \
            (0)                                                                     \
        :   (-1))

/* Return pending buffer size */
#define ZMQDRV_DECODE_PEEK(ev, idx, offset, sz)                                     \
    ((*(idx) < ev->vsize &&                                                         \
      ev->iov[*(idx)].iov_len > *(offset))                                          \
        ?   *(sz) = ev->iov[*(idx)].iov_len - *(offset),                            \
            (0)                                                                     \
        :   (-1))

typedef struct {
    ErlIOVec *ev;
    int index;
    size_t offset;
} zmq_drv_iov_t;

/* Forward declarations */
static ErlDrvData zmqdrv_start(ErlDrvPort port, char* cmd);
static void zmqdrv_stop(ErlDrvData handle);
static void zmqdrv_outputv(ErlDrvData handle, ErlIOVec *ev);
static void zmqdrv_error(zmq_drv_t *zmq_drv, const char *errstr);
static void zmqdrv_ok(zmq_drv_t *zmq_drv);
static void zmqdrv_binary_ok(zmq_drv_t *drv, void *data, size_t size);
static void zmqdrv_init(zmq_drv_t *drv, zmq_drv_iov_t *iov);
static void zmqdrv_term(zmq_drv_t *drv, zmq_drv_iov_t *iov);
static void zmqdrv_socket(zmq_drv_t *drv, zmq_drv_iov_t *iov);
static void zmqdrv_close(zmq_drv_t *drv, zmq_drv_iov_t *iov);
static void zmqdrv_bind(zmq_drv_t *drv, zmq_drv_iov_t *iov);
static void zmqdrv_connect(zmq_drv_t *drv, zmq_drv_iov_t *iov);
static void zmqdrv_send(zmq_drv_t *drv, zmq_drv_iov_t *iov);
static void zmqdrv_recv(zmq_drv_t *drv, zmq_drv_iov_t *iov);
static void zmqdrv_setsockopt(zmq_drv_t *drv, zmq_drv_iov_t *iov);
static void zmqdrv_getsockopt(zmq_drv_t *drv, zmq_drv_iov_t *iov);

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
    if (!drv) {
        /* 
         * erl_errno = ENOMEM;
         * return ERL_DRV_ERROR_ERRNO;
         */
        return NULL;
    }

    drv->port = port;
    drv->zmq_context = NULL;
    return (ErlDrvData)drv;
}

/* Driver Stop, called on port close. */
static void
zmqdrv_stop(ErlDrvData handle)
{
    zmq_drv_t *drv = (zmq_drv_t *)handle;
    if (!drv)
        return;

    if (drv->zmq_context)
        zmq_term(drv->zmq_context);
    driver_free(drv);
}

/* Erlang command, called on binary input from VM. */
static void
zmqdrv_outputv(ErlDrvData handle, ErlIOVec *ev)
{
    int cmd;
    zmq_drv_t *drv = (zmq_drv_t *)handle;
    zmq_drv_iov_t iov = {ev, 1, 0};

    if (ZMQDRV_DECODE_INT32(iov.ev, &iov.index, &iov.offset, &cmd) < 0) {
        zmqdrv_error(drv, strerror(EINVAL));
        return;
    }

    switch (cmd) {
        case ZMQ_INIT :
            zmqdrv_init(drv, &iov);
            break;
        case ZMQ_TERM :
            zmqdrv_term(drv, &iov);
            break;
        case ZMQ_SOCKET :
            zmqdrv_socket(drv, &iov);
            break;
        case ZMQ_CLOSE :
            zmqdrv_close(drv, &iov);
            break;
        case ZMQ_SETSOCKOPT :
            zmqdrv_setsockopt(drv, &iov);
            break;
        case ZMQ_GETSOCKOPT :
            zmqdrv_getsockopt(drv, &iov);
            break;
        case ZMQ_BIND :
            zmqdrv_bind(drv, &iov);
            break;
        case ZMQ_CONNECT :
            zmqdrv_connect(drv, &iov);
            break;
        case ZMQ_SEND :
            zmqdrv_send(drv, &iov);
            break;
        case ZMQ_RECV :
            zmqdrv_recv(drv, &iov);
            break;
        default :
            zmqdrv_error(drv, strerror(EINVAL));
    }
}

static void
zmqdrv_error(zmq_drv_t *drv, const char *errstr)
{
    ErlDrvTermData spec[] =
        {ERL_DRV_ATOM, driver_mk_atom((char *)"error"),
		 ERL_DRV_ATOM, driver_mk_atom((char *)errstr), ERL_DRV_TUPLE, 2};
    driver_output_term(drv->port, spec, sizeof(spec)/sizeof(spec[0]));
}

static void
zmqdrv_ok(zmq_drv_t *drv)
{
    ErlDrvTermData spec[] = 
        {ERL_DRV_ATOM, driver_mk_atom((char *)"ok")};
    driver_output_term(drv->port, spec, sizeof(spec)/sizeof(spec[0]));
}

static void 
zmqdrv_binary_ok(zmq_drv_t *drv, void *data, size_t size)
{
    ErlDrvBinary *ev_data = driver_alloc_binary(size);
    /* Copy payload. */
    ev_data->orig_size = size;
    memcpy(ev_data->orig_bytes, data, size);

    ErlDrvTermData spec[] = 
        {ERL_DRV_ATOM, driver_mk_atom((char *)"ok"),
		 ERL_DRV_BINARY, (ErlDrvTermData)ev_data, size, 0,
		 ERL_DRV_TUPLE, 2};
    
    driver_output_term(drv->port, spec, sizeof(spec)/sizeof(spec[0]));
    driver_free_binary(ev_data);
}

static void
zmqdrv_init(zmq_drv_t *drv, zmq_drv_iov_t *iov)
{
    int app_threads;
    int io_threads; 
    int flags; 

    if (ZMQDRV_DECODE_INT32(iov->ev, &iov->index, &iov->offset, &app_threads) ||
        ZMQDRV_DECODE_INT32(iov->ev, &iov->index, &iov->offset, &io_threads) ||
        ZMQDRV_DECODE_INT32(iov->ev, &iov->index, &iov->offset, &flags)) {
        zmqdrv_error(drv, strerror(EINVAL));
        return;
    }

    zmqdrv_fprintf("appthreads = %u, iothreads = %u\n", app_threads, io_threads);

    drv->zmq_context = (void *)zmq_init(app_threads, io_threads, flags);
    if (!drv->zmq_context) {
        zmqdrv_error(drv, zmq_strerror(zmq_errno()));
        return;
    }

    zmqdrv_ok(drv);
}

static void
zmqdrv_term(zmq_drv_t *drv, zmq_drv_iov_t *iov)
{
    if (!drv->zmq_context) {
        zmqdrv_error(drv, strerror(EINVAL));
        return;
    }

    if (zmq_term(drv->zmq_context) < 0) {
        zmqdrv_error(drv, zmq_strerror(zmq_errno()));
        return;
    }

    zmqdrv_ok(drv);
    drv->zmq_context = NULL;
}

static void
zmqdrv_socket(zmq_drv_t *drv, zmq_drv_iov_t *iov)
{
    int type;
    void *s;
        
    if (ZMQDRV_DECODE_INT32(iov->ev, &iov->index, &iov->offset, &type)) {
        zmqdrv_error(drv->zmq_context, strerror(EINVAL));
        return;
    }

    s = zmq_socket(drv->zmq_context, type);
    if (!s) {
        zmqdrv_error(drv->zmq_context, zmq_strerror(zmq_errno()));
        return;
    }

    zmqdrv_fprintf("socket %p\n", s);
    zmqdrv_binary_ok(drv, (void *)&s, sizeof(s));
}

static void
zmqdrv_close(zmq_drv_t *drv, zmq_drv_iov_t *iov)
{
    void *s;

    if (ZMQDRV_DECODE_SOCK(iov->ev, &iov->index, &iov->offset, &s)) {
        zmqdrv_error(drv->zmq_context, strerror(EINVAL));
        return;
    }

    zmqdrv_fprintf("close %p\n", s);
    if (zmq_close(s) < 0) {
        zmqdrv_error(drv->zmq_context, zmq_strerror(zmq_errno()));
        return;
    }
    
    zmqdrv_ok(drv);
}

static void 
zmqdrv_setsockopt(zmq_drv_t *drv, zmq_drv_iov_t *iov)
{
    void *s;
    int option;
    void *optval; 
    size_t optvallen;

    if (ZMQDRV_DECODE_SOCK(iov->ev, &iov->index, &iov->offset, &s) || !s ||
        ZMQDRV_DECODE_INT32(iov->ev, &iov->index, &iov->offset, &option)) {
        zmqdrv_error(drv->zmq_context, strerror(EINVAL));
        return;
    }
    
    if (ZMQDRV_DECODE_BLOB(iov->ev, &iov->index, &iov->offset, &optval, &optvallen))
        optval = NULL, optvallen = 0;

    zmqdrv_fprintf("setsockopt %p\n", s);
    if (zmq_setsockopt(s, option, optval, optvallen) < 0) {
        zmqdrv_error(drv->zmq_context, zmq_strerror(zmq_errno()));
        return;
    }

    zmqdrv_ok(drv);
}

static void 
zmqdrv_getsockopt(zmq_drv_t *drv, zmq_drv_iov_t *iov)
{
    zmqdrv_error(drv->zmq_context, "Not implemented yet, ha ha..");
}

static void
zmqdrv_bind(zmq_drv_t *drv, zmq_drv_iov_t *iov)
{
    void *s;
    char addr[512];
    size_t size;

    if (ZMQDRV_DECODE_SOCK(iov->ev, &iov->index, &iov->offset, &s) || !s ||
        ZMQDRV_DECODE_INT32(iov->ev, &iov->index, &iov->offset, &size)) {
        zmqdrv_error(drv->zmq_context, strerror(EINVAL));
        return;
    }

    /* Strip off 131 version, 107 binary. */
    size &= 0xFFFFUL;

    if (size >= sizeof(addr) || 
        ZMQDRV_DECODE_BIN(iov->ev, &iov->index, &iov->offset, &addr, size)) {
        zmqdrv_error(drv->zmq_context, strerror(EINVAL));
        return;
    }

    addr[size] = '\0';

    if (!s || !addr[0]) {
        zmqdrv_error(drv->zmq_context, strerror(EINVAL));
        return;
    }

    if (zmq_bind(s, addr) < 0) {
        zmqdrv_error(drv->zmq_context, zmq_strerror(zmq_errno()));
        return;
    }

    zmqdrv_ok(drv);
}

static void
zmqdrv_connect(zmq_drv_t *drv, zmq_drv_iov_t *iov)
{
    void *s;
    char addr[512];
    size_t size;

    if (ZMQDRV_DECODE_SOCK(iov->ev, &iov->index, &iov->offset, &s) || !s ||
        ZMQDRV_DECODE_INT32(iov->ev, &iov->index, &iov->offset, &size)) {
        zmqdrv_error(drv->zmq_context, strerror(EINVAL));
        return;
    }

    /* Strip off 131 version, 107 binary. */
    size &= 0xFFFFUL;
    
    if (size >= sizeof(addr) || 
        ZMQDRV_DECODE_BIN(iov->ev, &iov->index, &iov->offset, &addr, size)) {
        zmqdrv_error(drv->zmq_context, strerror(EINVAL));
        return;
    }

    addr[size] = '\0';

    if (!s || !addr[0]) {
        zmqdrv_error(drv->zmq_context, strerror(EINVAL));
        return;
    }

    if (zmq_connect(s, addr) < 0) {
        zmqdrv_error(drv->zmq_context, zmq_strerror(zmq_errno()));
        return;
    }

    zmqdrv_ok(drv);
}

static void
zmqdrv_send(zmq_drv_t *drv, zmq_drv_iov_t *iov)
{
    void *s;
    void *data;
    void *msg_data;
    size_t size;
    zmq_msg_t msg;

    if (ZMQDRV_DECODE_SOCK(iov->ev, &iov->index, &iov->offset, &s) || !s ||
        ZMQDRV_DECODE_BLOB(iov->ev, &iov->index, &iov->offset, &data, &size)) {
        zmqdrv_error(drv->zmq_context, strerror(EINVAL));
        return;
    }
    
    /* FIXME Is there a way to avoid this? */
    msg_data = malloc(size);
    if (!msg_data) {
        zmqdrv_error(drv->zmq_context, strerror(ENOMEM));
        return;
    }
    
    memcpy(msg_data, data, size);

    if (zmq_msg_init_data(&msg, msg_data, size, NULL, NULL)) {
        zmqdrv_error(drv->zmq_context, zmq_strerror(zmq_errno()));
        free(data);
        return;
    }

    if (zmq_send(s, &msg, 0) < 0) {
        zmqdrv_error(drv->zmq_context, zmq_strerror(zmq_errno()));
        zmq_msg_close(&msg);
        return;
    }

    zmqdrv_ok(drv);
}

static void
zmqdrv_recv(zmq_drv_t *drv, zmq_drv_iov_t *iov)
{
    void *s;
    zmq_msg_t msg;

    if (ZMQDRV_DECODE_SOCK(iov->ev, &iov->index, &iov->offset, &s) || !s) {
        zmqdrv_error(drv->zmq_context, strerror(EINVAL));
        return;
    }

    zmq_msg_init(&msg);

    if (zmq_recv(s, &msg, 0) < 0) {
        zmqdrv_error(drv->zmq_context, zmq_strerror(zmq_errno()));
        zmq_msg_close(&msg);
        return;
    }

    zmqdrv_binary_ok(drv, zmq_msg_data(&msg), zmq_msg_size(&msg));
    zmq_msg_close(&msg);
}
