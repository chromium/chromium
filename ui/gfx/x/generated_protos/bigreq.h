// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was automatically generated with:
// ../../ui/gfx/x/gen_xproto.py \
//    ../../third_party/xcbproto/src \
//    gen/ui/gfx/x \
//    bigreq \
//    dri3 \
//    glx \
//    randr \
//    render \
//    screensaver \
//    shape \
//    shm \
//    sync \
//    xfixes \
//    xinput \
//    xkb \
//    xproto \
//    xtest

#ifndef UI_GFX_X_GENERATED_PROTOS_BIGREQ_H_
#define UI_GFX_X_GENERATED_PROTOS_BIGREQ_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/x/error.h"
#include "ui/gfx/x/ref_counted_fd.h"
#include "ui/gfx/x/xproto_types.h"
#include "xproto.h"

namespace x11 {

class Connection;

template <typename Reply>
struct Response;

template <typename Reply>
class Future;

class COMPONENT_EXPORT(X11) BigRequests {
 public:
  static constexpr unsigned major_version = 0;
  static constexpr unsigned minor_version = 0;

  BigRequests(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  struct EnableRequest {};

  struct EnableReply {
    uint16_t sequence{};
    uint32_t maximum_request_length{};
  };

  using EnableResponse = Response<EnableReply>;

  Future<EnableReply> Enable(const EnableRequest& request);

  Future<EnableReply> Enable();

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

#endif  // UI_GFX_X_GENERATED_PROTOS_BIGREQ_H_
