//Copyright(C) 2022 Intel Corporation
//SPDX-License-Identifier: Apache-2.0
/* lrpc_common.h */
#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cerrno>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

#include <google/protobuf/message.h> // ::google::protobuf::MessageLite - base class for protobuf message needed for the lrpc runtime

#define IRPC_ADDR_UNSPEC		((uint64_t) -1)

namespace lrpc
{

  
  typedef union {
    uint8_t  byte[16];
    uint16_t word[8];
    uint32_t dword[4];
    uint64_t qword[2];
    __uint128_t data;
  } lrpc_addr_t;  /* 128 abstract iRPC address, accessible various ways*/

   enum Rpc_type : int32_t
  {
    kRpcTypeReq = 1,
    kRpcTypeRsp,
    kRpcTypeHi
  };

  // rpc structure for message queue
  // ServiceID: 1969906889 == crc32(ServiceName) - // https://github.com/smfrpc/smf/blob/master/docs/rpc.md (generated by lrpc plugin)
  // MethodID:  3312871568 == crc32(MethodName) - // https://github.com/smfrpc/smf/blob/master/docs/rpc.md (generated by lrpc plugin)
  // RequestID: 1969906889 ^ 3312871568 (This is to uniquely identify a RPC method. It's different from Instance ID of an active RPC call. (generated by lrpc plugin)
  // InstanceID: simliar to HTTP's stream ID to identify an active RPC call. (default implementation as defined in HTTP, may used linear address of Rpc_active_call)
  struct Rpc_message
  {
    long rpc_service_id;          /* rpc service id */
    long rpc_method_id;           /* rpc method id */
    long rpc_instance_id;         /* instance id */
    Rpc_type rpc_type;            /* req or resp */
    uint32_t rpc_parameters_size; /* rpc_parameters_size */
    char rpc_parameters[4096];    /* void *rpc_parameters;	  user-supplied req  - user is responsible for freeing */
  };                              // many techniques (linked-list, KV, etc.) can be used to extend rpc_message to support additonal coustom fields

  // rpc structure for the RPC queue
  struct Rpc_active_call
  {
    ::google::protobuf::MessageLite *response; // google::protobuf::Message *response;
    uint8_t rpc_resp_compl_record;
    std::atomic<bool> resp_ready_flag{false};
  }; /* callback function can be added */

  // Service registration are model after gRPC and borrowed many code / concept from gRPC
  // https://github.com/grpc/grpc/blob/d0fbba52d6e379b76a69016bc264b96a2318315f/include/grpc%2B%2B/impl/codegen/rpc_method.h
  class RpcMethod
  {
  };

  class MethodHandler
  {
  public:
    virtual ~MethodHandler() {}
    virtual int32_t RunHandler(void *req, void *resp) = 0;
  };

  class RpcServiceMethod : public RpcMethod
  {
  public:
    RpcServiceMethod(MethodHandler *handler) : handler_(handler) {}
    MethodHandler *handler() const { return handler_.get(); }

  private:
    std::unique_ptr<MethodHandler> handler_;
  };

  /// A wrapper class of an application provided rpc method handler.
  template <class ServiceType, class RequestType, class ResponseType,
            class BaseRequestType = RequestType,
            class BaseResponseType = ResponseType>
  class RpcMethodHandler : public lrpc::MethodHandler
  {
  public:
    RpcMethodHandler(
        std::function<int32_t(ServiceType *, RequestType *, ResponseType *)> func,
        ServiceType *service)
        : func_(std::move(func)), service_(service) {}

    ~RpcMethodHandler() = default;

    // this is needed as the middle man to take network blob and transform it into a specific object to trigger an RPC function
    int32_t RunHandler(void *req, void *resp) final
    {
      RequestType proto_req;
      ResponseType proto_resp;
      std::string serialized_str;

      proto_req.ParseFromString((char *)req); //- protobuf, will need another abstraction/wrap around serialization/deserialization APIs to be transformation alg neutral
      func_(service_, &proto_req, &proto_resp);
      proto_resp.SerializeToString(&serialized_str); //- protobuf, will need another abstraction/wrap around serialization/deserialization APIs to be transformation alg neutral
      memcpy((char *)resp, &serialized_str[0], serialized_str.size());

      return 0;
    }

    /// Application provided rpc handler function.
    std::function<int32_t(ServiceType *, RequestType *, ResponseType *)> func_;
    // The class the above handler function lives in.
    ServiceType *service_;
  };

  // base class for the iRPC service
  class rpc_service
  {
  public:
    virtual const char *service_name() const = 0;
    virtual uint32_t service_id() const = 0;
    virtual RpcServiceMethod *method_for_request_id(uint32_t idx) = 0;
    virtual ~rpc_service() {}
    rpc_service() {}
  };

  // needed to wrap lrpc::rpc_service, who has virtual functions to be overridden
  struct NamedService
  {
    explicit NamedService(lrpc::rpc_service *s) : service(s) {}
    lrpc::rpc_service *service;
  };

}