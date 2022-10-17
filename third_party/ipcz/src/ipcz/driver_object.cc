// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/driver_object.h"

#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

#include "ipcz/driver_transport.h"
#include "ipcz/ipcz.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace ipcz {

DriverObject::DriverObject() = default;

DriverObject::DriverObject(const IpczDriver& driver, IpczDriverHandle handle)
    : driver_(&driver), handle_(handle) {}

DriverObject::DriverObject(DriverObject&& other)
    : driver_(std::exchange(other.driver_, nullptr)),
      handle_(std::exchange(other.handle_, IPCZ_INVALID_DRIVER_HANDLE)) {}

DriverObject& DriverObject::operator=(DriverObject&& other) {
  reset();
  driver_ = std::exchange(other.driver_, nullptr);
  handle_ = std::exchange(other.handle_, IPCZ_INVALID_DRIVER_HANDLE);
  return *this;
}

DriverObject::~DriverObject() {
  reset();
}

void DriverObject::reset() {
  if (is_valid()) {
    ABSL_ASSERT(driver_);
    driver_->Close(handle_, IPCZ_NO_FLAGS, nullptr);
    driver_ = nullptr;
    handle_ = IPCZ_INVALID_DRIVER_HANDLE;
  }
}

IpczDriverHandle DriverObject::release() {
  IpczDriverHandle handle = handle_;
  handle_ = IPCZ_INVALID_DRIVER_HANDLE;
  driver_ = nullptr;
  return handle;
}

bool DriverObject::IsSerializable() const {
  if (!is_valid()) {
    return false;
  }

  const IpczResult result =
      driver_->Serialize(handle_, IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                         nullptr, nullptr, nullptr, nullptr, nullptr);
  return result == IPCZ_RESULT_ABORTED ||
         result == IPCZ_RESULT_RESOURCE_EXHAUSTED;
}

bool DriverObject::CanTransmitOn(const DriverTransport& transport) const {
  if (!is_valid()) {
    return false;
  }

  const IpczResult result = driver_->Serialize(
      handle_, transport.driver_object().handle(), IPCZ_NO_FLAGS, nullptr,
      nullptr, nullptr, nullptr, nullptr);
  return result == IPCZ_RESULT_RESOURCE_EXHAUSTED;
}

DriverObject::SerializedDimensions DriverObject::GetSerializedDimensions(
    const DriverTransport& transport) const {
  DriverObject::SerializedDimensions dimensions = {};
  IpczResult result = driver_->Serialize(
      handle_, transport.driver_object().handle(), IPCZ_NO_FLAGS, nullptr,
      nullptr, &dimensions.num_bytes, nullptr, &dimensions.num_driver_handles);
  ABSL_ASSERT(result == IPCZ_RESULT_RESOURCE_EXHAUSTED);
  ABSL_ASSERT(dimensions.num_bytes > 0 || dimensions.num_driver_handles > 0);
  return dimensions;
}

bool DriverObject::Serialize(const DriverTransport& transport,
                             absl::Span<uint8_t> data,
                             absl::Span<IpczDriverHandle> handles) {
  size_t num_bytes = data.size();
  size_t num_handles = handles.size();
  IpczResult result = driver_->Serialize(
      handle_, transport.driver_object().handle(), IPCZ_NO_FLAGS, nullptr,
      data.data(), &num_bytes, handles.data(), &num_handles);
  if (result == IPCZ_RESULT_OK) {
    release();
    return true;
  }
  return false;
}

// static
DriverObject DriverObject::Deserialize(
    const DriverTransport& transport,
    absl::Span<const uint8_t> data,
    absl::Span<const IpczDriverHandle> handles) {
  IpczDriverHandle handle;
  const IpczDriver* driver = transport.driver_object().driver();
  IpczResult result = driver->Deserialize(
      data.data(), data.size(), handles.data(), handles.size(),
      transport.driver_object().handle(), IPCZ_NO_FLAGS, nullptr, &handle);
  if (result != IPCZ_RESULT_OK) {
    return DriverObject();
  }

  return DriverObject(*driver, handle);
}

}  // namespace ipcz
