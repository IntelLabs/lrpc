
if(FETCH_PROTOBUF)

include(FetchContent)
FetchContent_Declare(
  protobuf
  GIT_REPOSITORY https://github.com/protocolbuffers/protobuf
  GIT_TAG  v21.9
  )

FetchContent_MakeAvailable(protobuf)

set(_PROTOBUF_GOOGLE "${CMAKE_BINARY_DIR}/_deps/protobuf-src/src/")
include_directories(${_PROTOBUF_GOOGLE})

set(_PROTOBUF_LIBPROTOBUF libprotobuf)

message("_PROTOBUF_GOOGLE is ${_PROTOBUF_GOOGLE}")

endif()
