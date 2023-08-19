// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/application_object.h"

#include <cstdint>

#include "ipcz/ipcz.h"
#include "ipcz/node_link.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz {

ApplicationObject::ApplicationObject(uintptr_t object,
                                     IpczApplicationObjectSerializer serializer,
                                     IpczApplicationObjectDestructor destructor)
    : object_(object), serializer_(serializer), destructor_(destructor) {}

ApplicationObject::ApplicationObject(ApplicationObject&& other)
    : object_(*other.object_),
      serializer_(other.serializer_),
      destructor_(other.destructor_) {
  other.object_.reset();
}

ApplicationObject::~ApplicationObject() {
  reset();
}

void ApplicationObject::reset() {
  if (object_ && destructor_) {
    destructor_(*object_, IPCZ_NO_FLAGS, nullptr);
  }
}

uintptr_t ApplicationObject::ReleaseObject() {
  uintptr_t object = *object_;
  object_.reset();
  return object;
}

bool ApplicationObject::IsSerializable() const {
  if (!serializer_) {
    return false;
  }

  const IpczResult result = serializer_(object(), IPCZ_NO_FLAGS, nullptr,
                                        nullptr, nullptr, nullptr, nullptr);
  return result != IPCZ_RESULT_FAILED_PRECONDITION;
}

Ref<ParcelWrapper> ApplicationObject::Serialize(NodeLink& link) {
  ABSL_ASSERT(IsSerializable());

  auto parcel = std::make_unique<Parcel>();
  size_t num_bytes = 0;
  size_t num_handles = 0;
  const IpczResult query_result =
      serializer_(object(), IPCZ_NO_FLAGS, nullptr, nullptr, &num_bytes,
                  nullptr, &num_handles);
  if (query_result == IPCZ_RESULT_RESOURCE_EXHAUSTED) {
    parcel->AllocateData(num_bytes, /*allow_partial=*/false, &link.memory());
    std::vector<IpczHandle> handles(num_handles);
    const IpczResult serialize_result = serializer_(
        object(), IPCZ_NO_FLAGS, nullptr, parcel->data_view().data(),
        &num_bytes, handles.data(), &num_handles);
    if (serialize_result != IPCZ_RESULT_OK) {
      return nullptr;
    }
    parcel->CommitData(num_bytes);

    std::vector<Ref<APIObject>> objects(num_handles);
    for (size_t i = 0; i < num_handles; ++i) {
      objects[i] = APIObject::TakeFromHandle(handles[i]);
    }
    parcel->SetObjects(std::move(objects));
  } else if (query_result != IPCZ_RESULT_OK) {
    return nullptr;
  }

  return MakeRefCounted<ParcelWrapper>(std::move(parcel));
}

}  // namespace ipcz
