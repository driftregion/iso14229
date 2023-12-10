
The transport layer / session layer interface is a little complex.

- see iso14229.h `UDSTpHandle_t`
    - implement all callback functions
- do sanity checks on init

```c
#include "assert.h"
assert(cfg->source_addr != cfg->target_addr);
assert(cfg->target_addr != cfg->source_addr_func);
assert(cfg->source_addr_func != cfg->source_addr);

assert(cfg->tp->recv);
assert(cfg->tp->send);
assert(cfg->tp->poll);

#if foo
#elif UDS_TP == UDS_TP_ISOTP_C
    assert(cfg->target_addr != cfg->source_addr_func && cfg->source_addr_func != cfg->source_addr);
    UDSTpIsoTpC_t *tp = &self->tp_impl;
    isotp_init_link(&tp->phys_link, cfg->target_addr, self->send_buf, self->send_buf_size,
                    self->recv_buf, self->recv_buf_size);
    isotp_init_link(&tp->func_link, cfg->target_addr, tp->func_send_buf, sizeof(tp->func_send_buf),
                    tp->func_recv_buf, sizeof(tp->func_recv_buf));
    self->tp = (UDSTpHandle_t *)tp;
    self->tp->poll = tp_poll;
    self->tp->send = tp_send;
    self->tp->recv = tp_recv;
#elif UDS_TP == UDS_TP_ISOTP_SOCKET
    self->tp = (UDSTpHandle_t *)&self->tp_impl;
    if (LinuxSockTpOpen(self->tp, cfg->if_name, cfg->source_addr, cfg->target_addr,
                        cfg->source_addr_func, cfg->target_addr)) {
        return UDS_ERR;
    }

// client

#if UDS_TP == UDS_TP_CUSTOM
    assert(cfg->tp);
    assert(cfg->tp->recv);
    assert(cfg->tp->send);
    assert(cfg->tp->poll);
    client->tp = cfg->tp;
#elif UDS_TP == UDS_TP_ISOTP_C
    assert(cfg->source_addr != cfg->target_addr_func && cfg->target_addr_func != cfg->target_addr);
    UDSTpIsoTpC_t *tp = (UDSTpIsoTpC_t *)&client->tp_impl;
    isotp_init_link(&tp->phys_link, cfg->target_addr, client->send_buf, client->send_buf_size,
                    client->recv_buf, client->recv_buf_size);
    isotp_init_link(&tp->func_link, cfg->target_addr_func, tp->func_send_buf,
                    sizeof(tp->func_send_buf), tp->func_recv_buf, sizeof(tp->func_recv_buf));
    client->tp = (UDSTpHandle_t *)tp;
    client->tp->poll = tp_poll;
    client->tp->send = tp_send;
    client->tp->recv = tp_recv;
#elif UDS_TP == UDS_TP_ISOTP_SOCKET
    client->tp = (UDSTpHandle_t *)&client->tp_impl;
    if (LinuxSockTpOpen(client->tp, cfg->if_name, cfg->source_addr, cfg->target_addr,
                        cfg->source_addr, cfg->target_addr_func)) {
        return UDS_ERR;
    }
    assert(client->tp);
#endif


```