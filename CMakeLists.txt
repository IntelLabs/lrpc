cmake_minimum_required(VERSION 3.10)
project(lrpc_ofi)

set (CMAKE_CXX_STANDARD 20)
option(FETCH_PROTOBUF "Fetch protobuf src" ON)
add_definitions(-w)

#### Build protoc and lrpc protoc plugin
message(STATUS "[lrpc]: Pulling protoc from repo and compiling protoc and lrpc protoc plugin")
add_subdirectory(protoc_plugin)

set(protopath	        "${CMAKE_CURRENT_SOURCE_DIR}/protobuf-msg")
set(output_dir          "${CMAKE_CURRENT_SOURCE_DIR}")
set(protoc_             "${CMAKE_CURRENT_BINARY_DIR}/_deps/protobuf-build/protoc")
set(lrpcplugin_         "${CMAKE_CURRENT_BINARY_DIR}/protoc_plugin/lrpc_cpp_plugin")

#### geneate *.lrpc.pb.h / *.lrpc.pb.cc files
file(GLOB proto_files "${protopath}/*.proto")
foreach(proto_file ${proto_files}) 
    get_filename_component (filename ${proto_file} NAME_WLE)
    set(proto_src "${output_dir}/${filename}.pb.cc")
    set(proto_hdr "${output_dir}/${filename}.pb.h")
    set(proto_lrpcsrc "${output_dir}/${filename}.lrpc.pb.cc")
    set(proto_lrpchdr "${output_dir}/${filename}.lrpc.pb.h")

    add_custom_command(
        OUTPUT "${proto_src}" "${proto_hdr}" "${proto_lrpcsrc}" "${proto_lrpchdr}"
        COMMAND ${protoc_}
        ARGS    --plugin=protoc-gen-lrpc=${lrpcplugin_}
                --cpp_out="${output_dir}" --lrpc_out="${output_dir}"
                -I"${protopath}" "${proto_file}"
        DEPENDS ${proto_file} ${protoc_} ${lrpcplugin_}
    )

    list(APPEND output_sources ${proto_src} ${proto_lrpcsrc}) 
endforeach()

# Find the libfabric library
find_library(FABRIC_LIB fabric)
find_library(HDR_LIB hdr_histogram)

# Include the libfabric header files
include_directories(/usr/include/rdma)
include_directories(/usr/local/include/hdr)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/_deps/protobuf-src/src")

add_definitions(-O2 -g -Wall -Wno-unused-variable -Wno-deprecated-declarations)

# Add the executable target
add_executable(lrpc_ofi_client lrpc_ofi_client.cc lrpc.cc tls_registry.cc ${output_sources})
add_executable(lrpc_ofi_server lrpc_ofi_server.cc lrpc.cc tls_registry.cc ${output_sources})

# Link the target with the libfabric library
target_link_libraries(lrpc_ofi_client ${FABRIC_LIB} "${CMAKE_CURRENT_BINARY_DIR}/_deps/protobuf-build/libprotobuf.a" ${HDR_LIB})
target_link_libraries(lrpc_ofi_server ${FABRIC_LIB} "${CMAKE_CURRENT_BINARY_DIR}/_deps/protobuf-build/libprotobuf.a" ${HDR_LIB})

message(STATUS "Build Type: ${CMAKE_BUILD_TYPE} with options: ${CMAKE_CXX_FLAGS}")
