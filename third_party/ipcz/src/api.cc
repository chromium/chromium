// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstring>
#include <memory>

#include "api.h"
#include "ipcz/api_context.h"
#include "ipcz/api_object.h"
#include "ipcz/application_object.h"
#include "ipcz/box.h"
#include "ipcz/driver_object.h"
#include "ipcz/ipcz.h"
#include "ipcz/node.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/parcel.h"
#include "ipcz/parcel_wrapper.h"
#include "ipcz/router.h"
#include "util/ref_counted.h"

extern "C" {

IpczResult Close(IpczHandle handle, uint32_t flags, const void* options) {
  const ipcz::APIContext api_context;
  const ipcz::Ref<ipcz::APIObject> doomed_object =
      ipcz::APIObject::TakeFromHandle(handle);
  if (!doomed_object) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }
  return doomed_object->Close();
}

IpczResult CreateNode(const IpczDriver* driver,
                      IpczCreateNodeFlags flags,
                      const IpczCreateNodeOptions* options,
                      IpczHandle* node) {
  if (!node || !driver || driver->size < sizeof(IpczDriver)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  if (options && options->size < sizeof(IpczCreateNodeOptions)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  if (!driver->Close || !driver->Serialize || !driver->Deserialize ||
      !driver->CreateTransports || !driver->ActivateTransport ||
      !driver->DeactivateTransport || !driver->Transmit ||
      !driver->AllocateSharedMemory || !driver->GetSharedMemoryInfo ||
      !driver->DuplicateSharedMemory || !driver->MapSharedMemory ||
      !driver->GenerateRandomBytes) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  // ipcz relies on lock-free implementations of both 32-bit and 64-bit atomics,
  // assuming any applicable alignment requirements are met. This is not
  // required by the standard, but it is a reasonable expectation for modern
  // std::atomic implementations on supported architectures. We verify here just
  // in case, as CreateNode() is a common API which will in practice always be
  // called before ipcz would do any work that might rely on such atomics.
  std::atomic<uint32_t> atomic32;
  std::atomic<uint64_t> atomic64;
  if (!atomic32.is_lock_free() || !atomic64.is_lock_free()) {
    return IPCZ_RESULT_UNIMPLEMENTED;
  }

  const ipcz::APIContext api_context;
  auto node_ptr = ipcz::MakeRefCounted<ipcz::Node>(
      (flags & IPCZ_CREATE_NODE_AS_BROKER) != 0 ? ipcz::Node::Type::kBroker
                                                : ipcz::Node::Type::kNormal,
      *driver, options);
  *node = ipcz::Node::ReleaseAsHandle(std::move(node_ptr));
  return IPCZ_RESULT_OK;
}

IpczResult ConnectNode(IpczHandle node_handle,
                       IpczDriverHandle driver_transport,
                       size_t num_initial_portals,
                       IpczConnectNodeFlags flags,
                       const void* options,
                       IpczHandle* initial_portals) {
  ipcz::Node* node = ipcz::Node::FromHandle(node_handle);
  if (!node || driver_transport == IPCZ_INVALID_HANDLE) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  if (num_initial_portals == 0 || !initial_portals) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  if (num_initial_portals > ipcz::NodeLinkMemory::kMaxInitialPortals) {
    return IPCZ_RESULT_OUT_OF_RANGE;
  }

  const ipcz::APIContext api_context;
  return node->ConnectNode(
      driver_transport, flags,
      absl::Span<IpczHandle>(initial_portals, num_initial_portals));
}

IpczResult OpenPortals(IpczHandle node_handle,
                       uint32_t flags,
                       const void* options,
                       IpczHandle* portal0,
                       IpczHandle* portal1) {
  ipcz::Node* node = ipcz::Node::FromHandle(node_handle);
  if (!node || !portal0 || !portal1) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const ipcz::APIContext api_context;
  ipcz::Router::Pair routers = ipcz::Router::CreatePair();
  *portal0 = ipcz::Router::ReleaseAsHandle(std::move(routers.first));
  *portal1 = ipcz::Router::ReleaseAsHandle(std::move(routers.second));
  return IPCZ_RESULT_OK;
}

IpczResult MergePortals(IpczHandle portal0,
                        IpczHandle portal1,
                        uint32_t flags,
                        const void* options) {
  ipcz::Router* first = ipcz::Router::FromHandle(portal0);
  ipcz::Router* second = ipcz::Router::FromHandle(portal1);
  if (!first || !second) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const ipcz::APIContext api_context;
  ipcz::Ref<ipcz::Router> one(ipcz::kAdoptExistingRef, first);
  ipcz::Ref<ipcz::Router> two(ipcz::kAdoptExistingRef, second);
  IpczResult result = one->MergeRoute(two);
  if (result != IPCZ_RESULT_OK) {
    one.release();
    two.release();
    return result;
  }

  return IPCZ_RESULT_OK;
}

IpczResult QueryPortalStatus(IpczHandle portal_handle,
                             uint32_t flags,
                             const void* options,
                             IpczPortalStatus* status) {
  ipcz::Router* router = ipcz::Router::FromHandle(portal_handle);
  if (!router) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }
  if (!status || status->size < sizeof(IpczPortalStatus)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const ipcz::APIContext api_context;
  router->QueryStatus(*status);
  return IPCZ_RESULT_OK;
}

IpczResult Put(IpczHandle portal_handle,
               const void* data,
               size_t num_bytes,
               const IpczHandle* handles,
               size_t num_handles,
               uint32_t flags,
               const void* options) {
  ipcz::Router* router = ipcz::Router::FromHandle(portal_handle);
  if (!router) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const ipcz::APIContext api_context;
  return router->Put(
      absl::MakeSpan(static_cast<const uint8_t*>(data), num_bytes),
      absl::MakeSpan(handles, num_handles));
}

IpczResult BeginPut(IpczHandle portal_handle,
                    IpczBeginPutFlags flags,
                    const void* options,
                    volatile void** data,
                    size_t* num_bytes,
                    IpczTransaction* transaction) {
  ipcz::Router* router = ipcz::Router::FromHandle(portal_handle);
  if (!router || !transaction) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const ipcz::APIContext api_context;
  return router->BeginPut(flags, data, num_bytes, transaction);
}

IpczResult EndPut(IpczHandle portal_handle,
                  IpczTransaction transaction,
                  size_t num_bytes_produced,
                  const IpczHandle* handles,
                  size_t num_handles,
                  IpczEndPutFlags flags,
                  const void* options) {
  ipcz::Router* router = ipcz::Router::FromHandle(portal_handle);
  if (!router || !transaction || (num_handles > 0 && !handles)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const ipcz::APIContext api_context;
  return router->EndPut(transaction, num_bytes_produced,
                        absl::MakeSpan(handles, num_handles), flags);
}

IpczResult Get(IpczHandle source,
               IpczGetFlags flags,
               const void* options,
               void* data,
               size_t* num_bytes,
               IpczHandle* handles,
               size_t* num_handles,
               IpczHandle* parcel) {
  const ipcz::APIContext api_context;
  if (ipcz::Router* router = ipcz::Router::FromHandle(source)) {
    return router->Get(flags, data, num_bytes, handles, num_handles, parcel);
  }

  if (ipcz::ParcelWrapper* wrapper = ipcz::ParcelWrapper::FromHandle(source)) {
    return wrapper->Get(flags, data, num_bytes, handles, num_handles, parcel);
  }

  return IPCZ_RESULT_INVALID_ARGUMENT;
}

IpczResult BeginGet(IpczHandle source,
                    uint32_t flags,
                    const void* options,
                    const volatile void** data,
                    size_t* num_bytes,
                    IpczHandle* handles,
                    size_t* num_handles,
                    IpczTransaction* transaction) {
  if (!transaction) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const ipcz::APIContext api_context;
  if (ipcz::Router* router = ipcz::Router::FromHandle(source)) {
    return router->BeginGet(flags, data, num_bytes, handles, num_handles,
                            transaction);
  }

  if (ipcz::ParcelWrapper* parcel = ipcz::ParcelWrapper::FromHandle(source)) {
    return parcel->BeginGet(flags, data, num_bytes, handles, num_handles,
                            transaction);
  }

  return IPCZ_RESULT_INVALID_ARGUMENT;
}

IpczResult EndGet(IpczHandle source,
                  IpczTransaction transaction,
                  IpczEndGetFlags flags,
                  const void* options,
                  IpczHandle* parcel) {
  const ipcz::APIContext api_context;
  if (ipcz::Router* router = ipcz::Router::FromHandle(source)) {
    return router->EndGet(transaction, flags, parcel);
  }

  if (ipcz::ParcelWrapper* wrapper = ipcz::ParcelWrapper::FromHandle(source)) {
    return wrapper->EndGet(transaction, flags, parcel);
  }

  return IPCZ_RESULT_INVALID_ARGUMENT;
}

IpczResult Trap(IpczHandle portal_handle,
                const IpczTrapConditions* conditions,
                IpczTrapEventHandler handler,
                uintptr_t context,
                uint32_t flags,
                const void* options,
                IpczTrapConditionFlags* satisfied_condition_flags,
                IpczPortalStatus* status) {
  ipcz::Router* router = ipcz::Router::FromHandle(portal_handle);
  if (!router || !handler || !conditions ||
      conditions->size < sizeof(*conditions)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  if (status && status->size < sizeof(*status)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const ipcz::APIContext api_context;
  return router->Trap(*conditions, handler, context, satisfied_condition_flags,
                      status);
}

IpczResult Reject(IpczHandle parcel_handle,
                  uintptr_t context,
                  uint32_t flags,
                  const void* options) {
  ipcz::ParcelWrapper* parcel = ipcz::ParcelWrapper::FromHandle(parcel_handle);
  if (!parcel) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const ipcz::APIContext api_context;
  return parcel->Reject(context);
}

IpczResult Box(IpczHandle node_handle,
               const IpczBoxContents* contents,
               uint32_t flags,
               const void* options,
               IpczHandle* handle) {
  ipcz::Node* node = ipcz::Node::FromHandle(node_handle);
  if (!node || !handle || !contents ||
      contents->size < sizeof(IpczBoxContents)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const ipcz::APIContext api_context;
  ipcz::Ref<ipcz::Box> box;
  switch (contents->type) {
    case IPCZ_BOX_TYPE_DRIVER_OBJECT:
      if (contents->object.driver_object == IPCZ_INVALID_DRIVER_HANDLE) {
        return IPCZ_RESULT_INVALID_ARGUMENT;
      }
      box = ipcz::MakeRefCounted<ipcz::Box>(
          ipcz::DriverObject(node->driver(), contents->object.driver_object));
      break;

    case IPCZ_BOX_TYPE_APPLICATION_OBJECT:
      box = ipcz::MakeRefCounted<ipcz::Box>(
          ipcz::ApplicationObject(contents->object.application_object,
                                  contents->serializer, contents->destructor));
      break;

    default:
      // NOTE: Explicit boxing of parcel fragments is not supported, but it
      // could be in the future.
      return IPCZ_RESULT_UNIMPLEMENTED;
  }

  *handle = ipcz::Box::ReleaseAsHandle(std::move(box));
  return IPCZ_RESULT_OK;
}

IpczResult Unbox(IpczHandle handle,
                 IpczUnboxFlags flags,
                 const void* options,
                 IpczBoxContents* contents) {
  if (!contents || contents->size < sizeof(IpczBoxContents)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const ipcz::APIContext api_context;
  ipcz::Ref<ipcz::Box> box = ipcz::Box::TakeFromHandle(handle);
  if (!box) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  if (flags & IPCZ_UNBOX_PEEK) {
    return box.release()->Peek(*contents);
  }

  return box->Unbox(*contents);
}

constexpr IpczAPI kCurrentAPI = {
    sizeof(kCurrentAPI),
    Close,
    CreateNode,
    ConnectNode,
    OpenPortals,
    MergePortals,
    QueryPortalStatus,
    Put,
    BeginPut,
    EndPut,
    Get,
    BeginGet,
    EndGet,
    Trap,
    Reject,
    Box,
    Unbox,
};

constexpr size_t kVersion0APISize =
    offsetof(IpczAPI, Unbox) + sizeof(kCurrentAPI.Unbox);

IPCZ_EXPORT IpczResult IPCZ_API IpczGetAPI(IpczAPI* api) {
  if (!api || api->size < kVersion0APISize) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  memcpy(api, &kCurrentAPI, kVersion0APISize);
  return IPCZ_RESULT_OK;
}

}  // extern "C"
