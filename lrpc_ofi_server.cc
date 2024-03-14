#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <string.h>
#include <immintrin.h>

#include "lrpc.h"
#include "lrpc_transport.h"
#include "lrpc_ofi_transport.h"

#include "pingpong.pb.h"
#include "pingpong.lrpc.pb.h"

using namespace lrpc;
using namespace grpc_pingpong_bench;

// Logic and data behind the server's behavior.
class PingPongServiceImpl final : public PingPong::Service
{
public:
	PingPongServiceImpl(Lrpc_endpoint *lrpc_endpoint) : PingPong::Service(lrpc_endpoint) {} /* Developers need this because iRPC did not register service via member func, but cntr */

	// Sends a ping
	int32_t Ping(/* ServerContext *context, */ const ::grpc_pingpong_bench::PingPongRequest *request, ::grpc_pingpong_bench::PingPongReply *response) override
	{

		std::string prefix("Hello ");
		response->set_pong_str(prefix + request->ping_str());
		return 0;
	}
};

int main()
{
	int ret;

	// iRPC server
	// Create iRPC OFI Transport Object (Server) (Server)
	Lrpc_ofi_transport lrpc_ofi_transport(NULL);

	// Initialize The Fabric and Spin For Completion Event
	ret = lrpc_ofi_transport.lrpc_init();
	if (ret)
	{
		fprintf(stderr, "OFI Error initializing fabric w/ code = %d.\n", ret);
		return ret;
	}

	// attached the transport to the runtime
	Lrpc_endpoint lrpc_endpoint(&lrpc_ofi_transport);

	// create lrpc service
	PingPongServiceImpl pingpong_service(&lrpc_endpoint);

	while (1)
		; // running RPC server

	return 0;
}
