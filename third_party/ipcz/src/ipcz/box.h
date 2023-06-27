// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_BOX_
#define IPCZ_SRC_IPCZ_BOX_

#include "ipcz/api_object.h"
#include "ipcz/application_object.h"
#include "ipcz/driver_object.h"
#include "ipcz/parcel_wrapper.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "util/overloaded.h"

namespace ipcz {

// Generic handle wrapper around a DriverObject, ApplicationObject, or Parcel,
// allowing those types to be passed wherever IpczHandles are accepted.
class Box : public APIObjectImpl<Box, APIObject::kBox> {
 public:
  enum class Type {
    // An empty box.
    kEmpty,

    // A driver object, potentially serializable by the IpczDriver.
    kDriverObject,

    // An opaque application object, optionally accompanied by custom
    // serialization and destruction routines.
    kApplicationObject,

    // A Parcel object which does not stand on its own in any portal's inbound
    // queue, but which can be embedded within another such Parcel.
    kSubparcel,
  };

  explicit Box(DriverObject object);
  explicit Box(ApplicationObject object);
  explicit Box(Ref<ParcelWrapper> parcel);

  Type type() const {
    return absl::visit(
        Overloaded{
            [](const Empty&) { return Type::kEmpty; },
            [](const DriverObject&) { return Type::kDriverObject; },
            [](const ApplicationObject&) { return Type::kApplicationObject; },
            [](const Ref<ParcelWrapper>&) { return Type::kSubparcel; },
        },
        contents_);
  }

  bool is_empty() { return absl::holds_alternative<Empty>(contents_); }
  DriverObject& driver_object() { return absl::get<DriverObject>(contents_); }
  ApplicationObject& application_object() {
    return absl::get<ApplicationObject>(contents_);
  }
  Ref<ParcelWrapper>& subparcel() {
    return absl::get<Ref<ParcelWrapper>>(contents_);
  }

  IpczResult Peek(IpczBoxContents& contents);
  IpczResult Unbox(IpczBoxContents& contents);

  // APIObject:
  IpczResult Close() override;
  bool CanSendFrom(Router& sender) override;

 private:
  ~Box() override;

  enum ExtractMode { kPeek, kUnbox };
  IpczResult ExtractContents(ExtractMode mode, IpczBoxContents& contents);

  using Empty = absl::monostate;
  using Contents =
      absl::variant<Empty, DriverObject, ApplicationObject, Ref<ParcelWrapper>>;

  Contents contents_;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_BOX_
