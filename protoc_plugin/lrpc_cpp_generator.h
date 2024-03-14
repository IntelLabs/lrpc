#ifndef LRPC_INTERNAL_COMPILER_CPP_PLUGIN_H
#define LRPC_INTERNAL_COMPILER_CPP_PLUGIN_H

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/compiler/cpp/helpers.h>

#include <memory>
#include <sstream>


namespace intel
{
    namespace protobuf
    {
        namespace compiler
        {
            namespace cpp
            {


				class LrpcCppGenerator : public google::protobuf::compiler::CodeGenerator {
				public:
					LrpcCppGenerator() {};
					~LrpcCppGenerator() {};

					uint64_t GetSupportedFeatures() const override {
   						 return FEATURE_PROTO3_OPTIONAL;
  					}

					bool Generate(const google::protobuf::FileDescriptor* file,
								const std::string& parameter,
								google::protobuf::compiler::GeneratorContext* generator_context,
								std::string* error) const override;

				};
            } // namespace cpp
        }     // namespace compiler
    }         // namespace protobuf
} // namespace intel   // namespace intel::protobuf::compiler::cpp

#endif  // LRPC_INTERNAL_COMPILER_CPP_PLUGIN_H