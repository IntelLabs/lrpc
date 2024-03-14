#pragma once

#include <unistd.h>
#include <unordered_map>
#include <iostream>
#include <mutex>
#include <cstdlib> // atoi and getenv

//#include "barrier.h"
#include "tls_registry.h"
#include "lrpc_transport.h"
#include "lrpc_common.h"

namespace lrpc
{

  static constexpr size_t kMaxWorkerThreads = 8;
  static constexpr size_t kRpcFuncArraySize = 256;

  class Lrpc_endpoint
  {
  public:
    Lrpc_endpoint(Lrpc_transport *lrpc_transport);
    ~Lrpc_endpoint();

    // enqueue an rpc request/response with associated context
    inline int lrpc_enqueue(lrpc_addr_t *to, Rpc_message *rpc_msg, Rpc_active_call *new_call = NULL)
    {

      // If this is a new outgoing RPC, add to the list of outstanding ones
      if (new_call != NULL)
        add_to_active_rpc_list(rpc_msg->rpc_instance_id, new_call);

      return transport_interface->lrpc_snd(to, rpc_msg);
    }

    // dequeue an rpc request/response from a given channel
    // noop, if the NIC support remote enqueue
    inline int lrpc_dequeue(lrpc_addr_t *from, Rpc_message *rpc_msg)
    {
      return transport_interface->lrpc_rcv(from, rpc_msg);
    }

    inline int lrpc_rpc_queue_size(int ch_id)
    {
      return transport_interface->lrpc_queue_size(ch_id);
    }

  //  inline int lrpc_transport_self_ch_id()
  //  {
  //    return transport_interface->lrpc_get_self_ch_id();
  //  }

    inline int lrpc_transport_signal_cqs()
    {
      return transport_interface->lrpc_signal_cqs();
    }

    inline int lrpc_transport_new_client(Rpc_message *rpc_msg)
    {
      return transport_interface->lrpc_new_client(rpc_msg);
    }

    inline long geniRPCInstanceId() { return Lrpc_endpoint::current_instance_id++; }

    /// Add an RPC slot to the list of active RPCs
    inline void add_to_active_rpc_list(long rpc_instance_id, Rpc_active_call *new_call)
    {
      active_rpc_calls_[rpc_instance_id] = new_call;
    }

    /// Delete an active RPC slot from the list of active RPCs
    inline void delete_from_active_rpc_list(long rpc_instance_id)
    {
      // active_rpc_calls[rpc_instance_id] = NULL;
      active_rpc_calls_.erase(rpc_instance_id);
    }

    inline void get_from_active_rpc_list(long rpc_instance_id, Rpc_active_call **new_call)
    {
      //*new_call = active_rpc_calls.at(rpc_instance_id);
      *new_call = active_rpc_calls_[rpc_instance_id];
    }

    // register an rpc service
    inline void register_rpc_service(rpc_service *service)
    {
      // assert(s != nullptr); //TO-DO: re-enable this after figuring out the dependency of s() C++11 with CMake
      services_.emplace_back(new NamedService(service));
    }

    lrpc::RpcServiceMethod *
    get_handle_for_request(const uint32_t &service_id, const uint32_t method_id)
    { // const uint32_t &request_id) {
      for (auto &p : services_)
      {
        if (service_id == p->service->service_id())
        {
          auto x = p->service->method_for_request_id(service_id ^ method_id);
          if (x != nullptr)
            return x;
          else
            return nullptr;
        }
      }
      return nullptr;
    }

  private:
    
    // lrpc_transport
    Lrpc_transport *transport_interface;

    // worker pool & rpc queues
    size_t num_worker_threads;
    volatile bool kill_switch; ///< Used to turn off SM and background threads
    TlsRegistry tls_registry;  ///< A thread-local registry

    /// Worker thread context
    class ThreadCtx
    {
    public:
      volatile bool *kill_switch;    // stop the worker threads
      TlsRegistry *tls_registry;     // the thread-local registry
      size_t worker_thread_index;    // index of the worker thread
      Lrpc_endpoint *lrpc_interface; // lrpc interface for the worker thread to enqueue/dequeue rpc_msg
    };

    std::thread worker_thread_arr[kMaxWorkerThreads];
    static void worker_thread_func(ThreadCtx ctx);

    // list of rpc_services
    std::vector<std::unique_ptr<NamedService>> services_{};

    // instead of hashmap, we may need to switch to double-linked list
    // std::unordered_map<long, Rpc_active_call*> active_rpc_calls;
    // std::array<Rpc_active_call *, 1000000> active_rpc_calls;
    std::map<long, Rpc_active_call *> active_rpc_calls_;

  private:
    void lock() { return _lock.lock(); }
    void unlock() { return _lock.unlock(); }

  private:
    std::mutex _lock;
    static long current_instance_id;
  };

}
