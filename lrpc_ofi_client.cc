//Copyright(C) 2022 Intel Corporation
//SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <immintrin.h>
#include <time.h>
#include <x86intrin.h>
#include <hdr_histogram.h>

#include "lrpc.h"
#include "lrpc_transport.h"
#include "lrpc_ofi_transport.h"

#include "pingpong.pb.h"
#include "pingpong.lrpc.pb.h"

using namespace lrpc;
using namespace grpc_pingpong_bench;

#define US_PER_REQ 100
#define ITERATION 100000

//
uint64_t rdtsc()
{
	unsigned int lo, hi;
	__asm__ __volatile__("rdtsc"
						 : "=a"(lo), "=d"(hi));
	return ((uint64_t)hi << 32) | lo;
}

// call this function to start a nanosecond-resolution timer
struct timespec now()
{
	struct timespec start_time;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time);
	return start_time;
}

// call this function to end a timer, returning nanoseconds elapsed as a long
long duration(struct timespec start_time)
{
	struct timespec end_time;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);
	long diffInNanos = (end_time.tv_sec - start_time.tv_sec) * (long)1e9 + (end_time.tv_nsec - start_time.tv_nsec);
	return diffInNanos;
}

static inline void
umonitor(volatile void *addr)
{
	asm volatile(".byte 0xf3, 0x48, 0x0f, 0xae, 0xf0"
				 :
				 : "a"(addr));
}

static inline int
umwait(unsigned long timeout, unsigned int state)
{
	uint8_t r;
	uint32_t timeout_low = (uint32_t)timeout;
	uint32_t timeout_high = (uint32_t)(timeout >> 32);

	timeout_low = (uint32_t)timeout;
	timeout_high = (uint32_t)(timeout >> 32);

	asm volatile(".byte 0xf2, 0x48, 0x0f, 0xae, 0xf1\t\n"
				 "setc %0\t\n"
				 : "=r"(r)
				 : "c"(state), "a"(timeout_low), "d"(timeout_high));
	return r;
}

#define UMWAIT_DELAY 100000
/* C0.1 state */
#define UMWAIT_STATE 1

int main(int argc, char **argv)
{
	if (argv[1] == NULL)
	{
		printf("Client needs a server address to interact with (argc=%d) \nExiting...\n",argc);
		return -1;
	}

	// Create iRPC OFI Transport Object (Client)
	// Address of Server from first command line argument
	//(1)
	Lrpc_ofi_transport lrpc_ofi_transport(argv[1]);

	// Initialize The Object
	lrpc_ofi_transport.lrpc_init();

	//(2)
	Lrpc_endpoint lrpc_endpoint(&lrpc_ofi_transport); // 0); // no worker threads -

	//(3) PingPong_Stub foo(lrpc_endpoint, remote_uri ("IP://www.intel.com/pingpong") ); (3) PingPong_Service bar; lrpc.regiser(bar);  // PingPong_Service bar1(lrpc_endpoint1);
	PingPong::Stub stub_(&lrpc_endpoint, "192.168.111.1");

	grpc_pingpong_bench::PingPongRequest ppReq;
	grpc_pingpong_bench::PingPongReply ppRsp;
	ppReq.set_ping_str("James");

	//while (1)
	//{

		long i = 0, j = 0, iteration = ITERATION, us_per_req = US_PER_REQ;
		long cur = 0, que = 0;
		struct hdr_histogram *histogram;
		struct timespec start, ts;
	
		// Use second CL argument as iteration count, if present	
		if ((argc > 2) && (argv[2] != NULL))
		   iteration = strtol(argv[2], NULL, 10);

		if (iteration <= 0 || iteration > 1000000) {
			printf("iteration cannot be less than 0 or greater than 1000000!!!\n");
			exit(-1);
		}

		// Use third CL argument as inter request time in us, if present	
		if ((argc > 3) && (argv[3] != NULL))
		   us_per_req = strtol(argv[3], NULL, 10);

		if (us_per_req <= 0 || us_per_req > 1000000) {
				printf("us_per_req cannot be less than 0 or greater than 1000000!!!\n");
				exit(-1);
		}

		hdr_init(1, INT64_C(3600000000), 3, &histogram);

		printf("Running for iteration = %ld inter_arrival_us = %ld \n", iteration, us_per_req);
		start = now();

		for (i = 0; i < iteration; i++)
		{
			cur = duration(start);

			// Treatment to prevent coordinated omission.
			// The intent is to generate one RPC every US_PER_REQ microseconds.
			// Late is defined to be an arrival beyond halfway to the next scheduled.
			if (cur > ((i * us_per_req * 1000) + us_per_req * 1000 / 2))
			{
				// This RPC is late, record queuing delay and proceed immediately
				que = cur - (i * us_per_req * 1000);
				//printf("i: %ld Cur: %ld Sch: %ld Retard: %ld\n",i,cur,i*us_per_req*1000, que);
			}
			else
			{
				// This RPC is early, spin until the time is right
				while (duration(start) < (i * us_per_req * 1000))
					; // Busy wait
				que = 0;
				j++; // Count non-late RPCs out of i
			}
			// Coordinated omission treatment ends here.

			ts = now();

			//(4) foo.Ping(req, rsp);
			stub_.Ping(ppReq, &ppRsp);

			// Total latency is service time plus queuing time
			// Record this instance's total latency to HDR histogram
			hdr_record_value(histogram, duration(ts) + que);
		}
		hdr_percentiles_print(histogram, stdout, 5, 1.0, CSV);
		printf("Ontime RPCs=%ld/%ld\n", j, i);
	//}
}
