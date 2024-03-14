cmake_minimum_required(VERSION 3.16.3)
project(lrpc_cpp_plugin)
set(EXEC lrpc_cpp_plugin)

project(${EXEC} VERSION 0.1 DESCRIPTION "protoc plugin to support lrpc")

#include(ExternalProject)
include(./cmake/protobuf.cmake)

file(GLOB plugin_SRC "*.cc")

add_executable(${EXEC} 
  ${plugin_SRC})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

#set_property(TARGET ${EXEC} PROPERTY CXX_STANDARD 11)

message("Protobuf_INCLUDE_DIRS is ${Protobuf_INCLUDE_DIRS}")

target_include_directories(${EXEC} PUBLIC ${_PROTOBUF_GOOGLE})
target_link_libraries(${EXEC} PRIVATE libprotoc ${_PROTOBUF_LIBPROTOBUF})

#set(_PLUGIN ${EXEC} PARENT_SCOPE)

set_target_properties(${EXEC} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${EXEC} PROPERTIES SOVERSION 1)