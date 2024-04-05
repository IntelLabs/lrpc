// https://github.com/erpc-io/eRPC/blob/master/LICENSE
// Adopated from eRPC (https://github.com/erpc-io/eRPC)
#include "tls_registry.h"

namespace lrpc
{

  thread_local bool tls_initialized = false;
  thread_local size_t etid = SIZE_MAX;

  void TlsRegistry::init()
  {
    assert(!tls_initialized);
    tls_initialized = true;
    etid = cur_etid++;
  }

  void TlsRegistry::reset()
  {
    tls_initialized = false;
    etid = SIZE_MAX;
    cur_etid = 0;
  }

  size_t TlsRegistry::get_etid() const
  {
    assert(tls_initialized);
    return etid;
  }

} // namespace lrpc
