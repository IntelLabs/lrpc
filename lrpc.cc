//Copyright(C) 2022 Intel Corporation
//SPDX-License-Identifier: Apache-2.0
#include "lrpc.h"

namespace lrpc
{
    long Lrpc_endpoint::current_instance_id = 1;
    Lrpc_endpoint::Lrpc_endpoint(Lrpc_transport *lrpc_transport)
        : transport_interface(lrpc_transport),
          num_worker_threads(1)
    {

        kill_switch = false;

        char* env_num_worker_threads = getenv("IRPC_NUM_WORKERS");
        if (env_num_worker_threads == nullptr) 
            num_worker_threads = 1; 
        else
            num_worker_threads = atoi(env_num_worker_threads); 

        printf("Number of worker threads = %ld\n", num_worker_threads);

        if (num_worker_threads > kMaxWorkerThreads || num_worker_threads < 1)
            return;
            
        // create worker thread(s)
        for (size_t i = 0; i < num_worker_threads; i++)
        {
            // assert(tls_registry.cur_etid == i);

            ThreadCtx worker_thread_ctx;

            worker_thread_ctx.kill_switch = &kill_switch;
            worker_thread_ctx.tls_registry = &tls_registry;
            worker_thread_ctx.worker_thread_index = i;
            worker_thread_ctx.lrpc_interface = this;

            worker_thread_arr[i] = std::thread(worker_thread_func, worker_thread_ctx);

            // Wait for the launched thread to grab a RPC thread ID, otherwise later
            // background threads or the foreground thread can grab ID = i.
            // while (tls_registry.cur_etid == i) usleep(1);
        }
    }

    Lrpc_endpoint::~Lrpc_endpoint()
    {
        kill_switch = true;
        lrpc_transport_signal_cqs();
        tls_registry.reset();

        if (num_worker_threads > kMaxWorkerThreads || num_worker_threads < 1)
            return;

        for (size_t i = 0; i < num_worker_threads; i++)
           worker_thread_arr[i].join();
    }

    void Lrpc_endpoint::worker_thread_func(ThreadCtx ctx)
    {
        //int i = 0;

        // thread_end != true TODO: add unlikely() to help with branch prediciton
        while (*ctx.kill_switch == false)
        {
            // Dequeue from the RPC run queue
            Rpc_message rpc_msg;
            lrpc_addr_t from;

            rpc_msg.rpc_instance_id = 0;
            rpc_msg.rpc_method_id = 0;
            rpc_msg.rpc_service_id = 0;
            
            if (ctx.lrpc_interface->lrpc_dequeue(&from, &rpc_msg) != 1)
            {
                // Error in dequeue, move on to next job
                // printf("Error: Dequeue failed!\n");
                continue;
            }

            //printf(".");

            // If dequeued a response
            if (rpc_msg.rpc_type == kRpcTypeRsp)
            {
                Rpc_active_call *return_call;
                //i+=1;
                //printf("%d ",i);
                // Attempt to find the corresponding request
                ctx.lrpc_interface->get_from_active_rpc_list(rpc_msg.rpc_instance_id, &return_call);
                if (return_call == NULL)
                {
                    // Cannot find whose request was this for, move on to next job
                    printf("Error: Matched a response to a NULL request!\n");
                    //ctx.lrpc_interface->lrpc_transport_signal_cqs();
                    continue;
                }
                // De-serialize the protobuf input from wire
                return_call->response->ParseFromString((char *)&rpc_msg.rpc_parameters[0]);

                // Remove this RPC from outstanding list
                ctx.lrpc_interface->delete_from_active_rpc_list(rpc_msg.rpc_instance_id);

                // Signal the waiting party that the response is ready
                return_call->resp_ready_flag.store(true);
                return_call->resp_ready_flag.notify_one();
            }
            // If dequeued a request
            else if (rpc_msg.rpc_type == kRpcTypeReq)
            {
                // Attempt to find and invoke the registered RPC handler
                Rpc_message ret_rpc_msg;
                ret_rpc_msg.rpc_instance_id = rpc_msg.rpc_instance_id;
                ret_rpc_msg.rpc_service_id = rpc_msg.rpc_service_id; 
                ret_rpc_msg.rpc_method_id = rpc_msg.rpc_method_id;
                ret_rpc_msg.rpc_type = kRpcTypeRsp;

                lrpc::RpcServiceMethod *pRpcMethod = ctx.lrpc_interface->get_handle_for_request(rpc_msg.rpc_service_id, rpc_msg.rpc_method_id);
                if (!pRpcMethod)
                {
                    // RPC handler missing, move on to next job
                    printf("Error: Could not find a response handler to run!\n");
                    continue;
                }

                // Run the RPC Handler
                pRpcMethod->handler()->RunHandler((void *)&rpc_msg.rpc_parameters[0], (void *)&ret_rpc_msg.rpc_parameters[0]);
                // ret_rpc_msg.rpc_parameters_size = ret_rpc_resp.rpc_resp_size;

                // Send the RPC response back to where it came from
                if (ctx.lrpc_interface->lrpc_enqueue(&from, &ret_rpc_msg) != 1)
                {
                    // Error enqueuing a response, move on to next job
                    printf("Error: Enqueue failed!\n");
                    continue;
                }
            }
            // If a brand new client says Hi!
            else if (rpc_msg.rpc_type == kRpcTypeHi)
            {
                printf("New Client Arrived: Welcoming %.32s!\n", rpc_msg.rpc_parameters);
                ctx.lrpc_interface->lrpc_transport_new_client(&rpc_msg);
                continue;
            }
            // If dequeued an unknown message
            else
            {
                // not iRPC message, move on to the next job
                printf("Error: Dequeued not an iRPC message!\n");
                continue;
            }
        }

        printf("A worker thread is kill switched!\n");
    } // worker thread

}
