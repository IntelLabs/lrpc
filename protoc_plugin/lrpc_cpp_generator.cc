//Copyright(C) 2022 Intel Corporation
//SPDX-License-Identifier: Apache-2.0
#include "lrpc_cpp_generator.h"

#include "generator_helpers.h"
#include "cpp_generator.h"
#include "cpp_generator_helpers.h"

#include <google/protobuf/compiler/cpp/helpers.h>
#include <google/protobuf/compiler/cpp/cpp_generator.h>

namespace intel::protobuf::compiler::cpp {

bool LrpcCppGenerator::Generate(const google::protobuf::FileDescriptor* file,
                            const std::string& parameter,
                            google::protobuf::compiler::GeneratorContext* generator_context,
                            std::string* error) const {

    if (file->options().cc_generic_services()) {
      *error =
          "cpp grpc proto compiler plugin does not work with generic "
          "services. To generate cpp grpc APIs, please set \""
          "cc_generic_service = false\".";
      return false;
    }

    std::string file_name = lrpc_generator::StripProto(file->name());

    std::string header_code =
        lrpc_cpp_generator::GetHeaderPrologue(file) +
        lrpc_cpp_generator::GetHeaderIncludes(file) +
        lrpc_cpp_generator::GetHeaderServices(file) +
        lrpc_cpp_generator::GetHeaderEpilogue(file);
    std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> header_output(
        generator_context->Open(file_name + ".lrpc.pb.h"));
    google::protobuf::io::CodedOutputStream header_coded_out(header_output.get());
    header_coded_out.WriteRaw(header_code.data(), header_code.size());

    std::string source_code =
        lrpc_cpp_generator::GetSourcePrologue(file) +
        lrpc_cpp_generator::GetSourceIncludes(file) +
        lrpc_cpp_generator::GetSourceServices(file) +
        lrpc_cpp_generator::GetSourceEpilogue(file);
    std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> source_output(
        generator_context->Open(file_name + ".lrpc.pb.cc"));
    google::protobuf::io::CodedOutputStream source_coded_out(source_output.get());
    source_coded_out.WriteRaw(source_code.data(), source_code.size());

	return true;
}


} // namespace intel::protobuf::compiler::cpp
