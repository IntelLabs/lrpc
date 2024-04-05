//Copyright(C) 2022 Intel Corporation
//SPDX-License-Identifier: Apache-2.0
#include "lrpc_cpp_generator.h"
#include <google/protobuf/compiler/plugin.h>

int main(int argc, char* argv[]) {
  intel::protobuf::compiler::cpp::LrpcCppGenerator generator;
  return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
