/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef IRPC_INTERNAL_COMPILER_CPP_GENERATOR_H
#define IRPC_INTERNAL_COMPILER_CPP_GENERATOR_H

// cpp_generator.h/.cc do not directly depend on GRPC/ProtoBuf, such that they
// can be used to generate code for other serialization systems, such as
// FlatBuffers.

#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/cpp/options.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>

namespace lrpc_cpp_generator {

// Return the prologue of the generated header file.
std::string GetHeaderPrologue(const google::protobuf::FileDescriptor* file);

// Return the includes needed for generated header file.
std::string GetHeaderIncludes(const google::protobuf::FileDescriptor* file);

// Return the includes needed for generated source file.
std::string GetSourceIncludes(const google::protobuf::FileDescriptor* file);

// Return the epilogue of the generated header file.
std::string GetHeaderEpilogue(const google::protobuf::FileDescriptor* file);

// Return the prologue of the generated source file.
std::string GetSourcePrologue(const google::protobuf::FileDescriptor* file);

// Return the services for generated header file.
std::string GetHeaderServices(const google::protobuf::FileDescriptor* file);

// Return the services for generated source file.
std::string GetSourceServices(const google::protobuf::FileDescriptor* file);

// Return the epilogue of the generated source file.
std::string GetSourceEpilogue(const google::protobuf::FileDescriptor* file);

}  // namespace lrpc_cpp_generator

#endif  // LRPC_INTERNAL_COMPILER_CPP_GENERATOR_H
