#pragma once

#include "lrpc.h"

namespace lrpc
{

    // Forward declaration
    struct Rpc_message;

    class Lrpc_transport
    {

    public:
        Lrpc_transport() {}

        virtual int lrpc_init(int argc, char **argv) { return 0; }
        virtual int lrpc_snd(lrpc_addr_t *to, Rpc_message *rpc_msg) { return 0; }
        virtual int lrpc_rcv(lrpc_addr_t *from, Rpc_message *rpc_msg) { return 0; }
        virtual int lrpc_signal_cqs() { return 0; }
        virtual int lrpc_new_client(Rpc_message *rpc_msg) { return 0; }
        virtual int lrpc_queue_size(int ch_id) { return 0; }
    };

}
