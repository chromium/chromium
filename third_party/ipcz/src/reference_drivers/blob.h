// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_REFERENCE_DRIVERS_BLOB_H_
#define IPCZ_SRC_REFERENCE_DRIVERS_BLOB_H_

#include <string>
#include <string_view>

#include "reference_drivers/object.h"
#include "util/ref_counted.h"

namespace ipcz::reference_drivers {

// A driver-managed object which packages arbitrary string data. Blobs are used
// to exercise driver object boxing in tests.
//
// Note that unlike the transport and memory objects defined by the reference
// drivers, a blob is not a type of object known to ipcz. Instead it is used to
// demonstrate how drivers can define arbitrary new types of transferrable
// objects to extend ipcz.
class Blob : public ObjectImpl<Blob, Object::kBlob> {
 public:
  class RefCountedFlag : public RefCounted {
   public:
    RefCountedFlag();

    bool get() const { return flag_; }
    void set(bool flag) { flag_ = flag; }

   private:
    ~RefCountedFlag() override;
    bool flag_ = false;
  };

  explicit Blob(std::string_view message);

  // Object:
  IpczResult Close() override;

  std::string& message() { return message_; }

  const Ref<RefCountedFlag>& destruction_flag_for_testing() const {
    return destruction_flag_for_testing_;
  }

  static Blob* FromHandle(IpczDriverHandle handle);
  static Ref<Blob> TakeFromHandle(IpczDriverHandle handle);

 protected:
  ~Blob() override;

 private:
  std::string message_;
  const Ref<RefCountedFlag> destruction_flag_for_testing_{
      MakeRefCounted<RefCountedFlag>()};
};

}  // namespace ipcz::reference_drivers

#endif  // IPCZ_SRC_REFERENCE_DRIVERS_BLOB_H_
