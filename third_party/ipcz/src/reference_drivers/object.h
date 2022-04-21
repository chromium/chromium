// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_REFERENCE_DRIVERS_OBJECT_H_
#define IPCZ_SRC_REFERENCE_DRIVERS_OBJECT_H_

#include <cstdint>

#include "ipcz/ipcz.h"
#include "util/ref_counted.h"

namespace ipcz::reference_drivers {

// Base class for all driver-managed objects used by both reference drivers.
class Object : public RefCounted {
 public:
  enum Type : uint32_t {
    kTransport,
    kMemory,
    kMapping,
  };

  explicit Object(Type type);

  Type type() const { return type_; }

  static Object* FromHandle(IpczDriverHandle handle) {
    return reinterpret_cast<Object*>(static_cast<uintptr_t>(handle));
  }

  static IpczDriverHandle ReleaseAsHandle(Ref<Object> object) {
    return static_cast<IpczDriverHandle>(
        reinterpret_cast<uintptr_t>(object.release()));
  }

  static Ref<Object> TakeFromHandle(IpczDriverHandle handle) {
    return AdoptRef(FromHandle(handle));
  }

  virtual IpczResult Close();

 protected:
  ~Object() override;

 private:
  const Type type_;
};

template <typename T, Object::Type kType>
class ObjectImpl : public Object {
 public:
  ObjectImpl() : Object(kType) {}

  static T* FromHandle(IpczDriverHandle handle) {
    Object* object = Object::FromHandle(handle);
    if (!object || object->type() != kType) {
      return nullptr;
    }
    return static_cast<T*>(object);
  }

 protected:
  ~ObjectImpl() override = default;
};

}  // namespace ipcz::reference_drivers

#endif  // IPCZ_SRC_REFERENCE_DRIVERS_OBJECT_H_
