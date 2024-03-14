#include <iostream>
#include <fstream>
#include <string>
#include <immintrin.h>

#include "pingpong.pb.h"

uint64_t rdtsc(){
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}



int main(int argc, char** argv)
{
        // Verify that the version of the library that we linked against is
        // compatible with the version of the headers we compiled against.
        GOOGLE_PROTOBUF_VERIFY_VERSION;

	uint64_t start, end, latency;  
	std::string serialized_str; 

	grpc_pingpong_bench::PingPongRequest ppReq;

	start = rdtsc();
	ppReq.SerializeToString(&serialized_str); 
	ppReq.ParseFromString(serialized_str); 
	end = rdtsc();
	latency = end - start; 
		
	return 0; 
}
