// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/box.h"

#include <utility>

#include "ipcz/ipcz.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz {

Box::Box(DriverObject object) : contents_(std::move(object)) {}

Box::Box(ApplicationObject object) : contents_(std::move(object)) {}

Box::Box(Ref<ParcelWrapper> parcel) : contents_(std::move(parcel)) {}

Box::~Box() = default;

IpczResult Box::Peek(IpczBoxContents& contents) {
  return ExtractContents(kPeek, contents);
}

IpczResult Box::Unbox(IpczBoxContents& contents) {
  return ExtractContents(kUnbox, contents);
}

IpczResult Box::Close() {
  contents_ = Empty{};
  return IPCZ_RESULT_OK;
}

bool Box::CanSendFrom(Router& sender) {
  return absl::visit(
      Overloaded{
          [](const Empty&) { return false; },
          [](const DriverObject& object) {
            return object.is_valid() && object.IsSerializable();
          },
          [](const ApplicationObject& object) { return true; },
          [&sender](const Ref<ParcelWrapper>& wrapper) {
            for (const auto& object : wrapper->parcel().objects_view()) {
              if (!object->CanSendFrom(sender)) {
                return false;
              }
            }
            return true;
          },
      },
      contents_);
}

IpczResult Box::ExtractContents(ExtractMode mode, IpczBoxContents& contents) {
  const bool peek = (mode == kPeek);
  const IpczResult result = absl::visit(
      Overloaded{
          [](const Empty& empty) { return IPCZ_RESULT_INVALID_ARGUMENT; },
          [&contents, peek](DriverObject& object) {
            contents.type = IPCZ_BOX_TYPE_DRIVER_OBJECT;
            contents.object.driver_object =
                peek ? object.handle() : object.release();
            return IPCZ_RESULT_OK;
          },
          [&contents, peek](ApplicationObject& object) {
            contents.type = IPCZ_BOX_TYPE_APPLICATION_OBJECT;
            contents.object.application_object =
                peek ? object.object() : object.ReleaseObject();
            contents.serializer = object.serializer();
            contents.destructor = object.destructor();
            return IPCZ_RESULT_OK;
          },
          [&contents, peek](Ref<ParcelWrapper>& wrapper) {
            contents.type = IPCZ_BOX_TYPE_SUBPARCEL;
            contents.object.subparcel =
                peek ? wrapper->handle()
                     : ipcz::APIObject::ReleaseAsHandle(std::move(wrapper));
            return IPCZ_RESULT_OK;
          },
      },
      contents_);

  if (!peek) {
    Close();
  }

  return result;
}

}  // namespace ipcz
