// https://github.com/erpc-io/eRPC/blob/master/LICENSE
// Adopated from eRPC (https://github.com/erpc-io/eRPC)
#pragma once

#include <atomic>
#include "lrpc_common.h"

namespace lrpc
{

  class TlsRegistry
  {
  public:
    TlsRegistry() : cur_etid(0) {}
    std::atomic<size_t> cur_etid;

    /// Initialize all the thread-local registry members
    void init();

    /// Reset all members
    void reset();

    /// Return the eRPC thread ID of the caller
    size_t get_etid() const;
  };
} // namespace lrpc
