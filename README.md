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
To run, typically, one would start server, and then point the client to it. Client arguments are ./lrpc_of_client [server name/IP] [iteration_count] [RPC_interarrival_us]

```
Server: ./lrpc_ofi_server
Client: ./lrpc_ofi_client 192.168.1.3 
```
```
Server: FI_PROVIDER=verbs ./lrpc_ofi_server
Client: FI_PROVIDER=verbs ./lrpc_ofi_client 192.168.1.3
```
Using FI_PROVIDER environment variable indicates OFI provider preference to libfabric (verbs), hence the lRPC OFI transport will be using verbs provider.
```
Server: FI_PROVIDER=verbs ./lrpc_ofi_server
Client: FI_PROVIDER=verbs ./lrpc_ofi_client 192.168.1.3 10000 100
```
Above client instance will initiate a total of 10,000 RPCs one after the other every 100us

