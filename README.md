## Intel Lean RPC Research Prototype - lRPC
lRPC is a lean and low-latency RPC (remote procedure call) framework

### lRPC Research Prototoype at a Glance

1. lRPC aims to maintain the use of IDL abstraction in SW design (i.e., Protocol Buffers - protobuf). 
2. lRPC extends the IDL (protobuf) compiler with a plugin that generates lRPC Stubs for the RPC application to invoke RPCs at the remote lRPC endpoint(s), as well as lRPC Skeletons for the RPC service. 
3. An example lRPC IDL definition (protobuf) can be found in ./protobuf-msg/pingpong.proto.  The protobuf compiler is resposible from generating object defintion & access methods using this input .proto file. E.g., pingpong.pb.h and pingpoing.cc.
4. An example of the generated lRPC (protobuf) Stub/Skeleton code can be found in pingpong.lrpc.h 
5. lRPC is designed to support different underlying transport mechanisms. An example implementation is provided, utilizing libfabric/OFI available in lrpc_ofi_transport.h.   
6. lRPC has a multi-theaded runtime component to support the processing of the RPC request and the response.
7. lRPC supports both asynchrnonous and synchronous APIs in the Stub.  In case of async, the application is responsible from polling for the arrival of the response(s).

### Installation
We have tested building and running the lRPC prototype on a few typical Linux distributions. Review below notes that can help installation of specific packages for that distribution required before building lRPC. 


#### Ubuntu 22.04 LTS
```
# Install Required Packages
sudo apt install build-essential autoconf cmake libtool zlib1g-dev
```

#### Fedora 39
```
# Install Required Packages
sudo dnf install -y git gcc make zlib-devel cmake autoconf libtool g++
```

#### Build and Install Required Libraries from Source
```
git clone https://github.com/ofiwg/libfabric.git; cd libfabric; ./autogen.sh; ./configure; sudo make -j install
git clone https://github.com/HdrHistogram/HdrHistogram_c.git; cd HdrHistogram_c; mkdir build; cd build; cmake ..; sudo make -j install
```

#### Build lRPC using CMake
```
cd <lRPC-SRC-DIR>; mkdir build; cd build; cmake ..; make -j 
```


### How to Run Test Code
A simple ping-pong client/server RPC application is included in the repository, named lrpc_ofi_client and lrpc_ofi_server, and built as part of above installation steps. To run, one would start a server instance, and then start the client by pointing it to the IP address of the server. User can choose the addresses belonging to the network interface of choice and further direct OFI to use a specific fabric provider using FI_PROVIDER environment variable. More information on libfabric configuration can be found [here](https://ofiwg.github.io/libfabric/main/man/fabric.7.html). 

```
# USAGE
SERVER: ./lrpc_ofi_server
CLIENT: ./lrpc_ofi_client [SERVER NAME or IP ADDRESS] [ITERATION COUNT] [RPC INTERARRIVAL IN USECOND]
```
For example, sending 10,000 requests, 100 usecond apart from each other to an RPC server instance, using VERBS OFI provider, running on a host reachable at 192.168.1.1 would look like
```
Server: FI_PROVIDER=verbs ./lrpc_ofi_server
Client: FI_PROVIDER=verbs ./lrpc_ofi_client 192.168.1.1 10000 100
```

Upon completing iterations, client will print out RTT latency histogram (values in nanoseconds) for completed RPC requests. RPC latency performance depends on the choice and configuration of the platform, network interface, OFI provider, etc. Using a typical modern pair of Xeon servers connected to each other with a dedicated IB VERBS capable 100 GbE Ethernet interface with ~5us average baseline ping-pong RTT latency performance (i.e. ibv_rc_pingpong) typically results in ~10us median lRPC RTT latency performance in our tests with VERBS OFI provider.

