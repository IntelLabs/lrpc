```
mkdir build
cd build
cmake -DFETCH_PROTOBUF=ON ..
make -j 

```
#### how to run
```
./_deps/protobuf-build/protoc --plugin=protoc-gen-lrpc=./lrpc_cpp_plugin --cpp_out=./ --lrpc_out=./ -I ./ pingpong.proto
```
#### how to use it in a CMakeLists.txt
```
set(_PROTOC "${CMAKE_SOURCE_DIR}/build/_deps/protobuf-build/protoc")
set(lrpc_cpp_plugin "${CMAKE_SOURCE_DIR}/build/lrpc_cpp_plugin")

set(protopath	"${CMAKE_CURRENT_SOURCE_DIR}")
set(protofile	"${CMAKE_CURRENT_SOURCE_DIR}/foo.proto")
set(protosrc	"${CMAKE_CURRENT_SOURCE_DIR}/foo.pb.cc")
set(protohdr	"${CMAKE_CURRENT_SOURCE_DIR}/foo.pb.h")
set(lrpcsrc	"${CMAKE_CURRENT_SOURCE_DIR}/foo.lrpc.pb.cc")
set(lrpchdr	"${CMAKE_CURRENT_SOURCE_DIR}/foo.lrpc.pb.h")

add_custom_command(
			OUTPUT "${protosrc}" "${protohdr}" "${lrpcsrc}" "${lrpchdr}"
			COMMAND ${_PROTOC}
			ARGS --plugin=protoc-gen-lrpc=${lrpc_cpp_pllugin}
					--cpp_out="${CMAKE_CURRENT_SOURCE_DIR}" --lrpc_out="${CMAKE_CURRENT_SOURCE_DIR}"
					-I"${protopath}" "${protofile}"
			DEPENDS ${protofile} ${lrpc_cpp_plugin}
)
set(outputsrc ${protosrc} ${lrpcsrc})

add_executable(${target}
	${testsrc}
	${outputsrc})
```

