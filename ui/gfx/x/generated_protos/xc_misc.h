// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was automatically generated with:
// ../../ui/gfx/x/gen_xproto.py \
//    ../../third_party/xcbproto/src \
//    gen/ui/gfx/x \
//    bigreq \
//    composite \
//    damage \
//    dpms \
//    dri2 \
//    dri3 \
//    ge \
//    glx \
//    present \
//    randr \
//    record \
//    render \
//    res \
//    screensaver \
//    shape \
//    shm \
//    sync \
//    xc_misc \
//    xevie \
//    xf86dri \
//    xf86vidmode \
//    xfixes \
//    xinerama \
//    xinput \
//    xkb \
//    xprint \
//    xproto \
//    xselinux \
//    xtest \
//    xv \
//    xvmc

#ifndef UI_GFX_X_GENERATED_PROTOS_XC_MISC_H_
#define UI_GFX_X_GENERATED_PROTOS_XC_MISC_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/x/error.h"
#include "ui/gfx/x/ref_counted_fd.h"
#include "xproto.h"

namespace x11 {

class Connection;

template <typename Reply>
struct Response;

template <typename Reply>
class Future;

class COMPONENT_EXPORT(X11) XCMisc {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 1;

  XCMisc(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  struct GetVersionRequest {
    uint16_t client_major_version{};
    uint16_t client_minor_version{};
  };

  struct GetVersionReply {
    uint16_t sequence{};
    uint16_t server_major_version{};
    uint16_t server_minor_version{};
  };

  using GetVersionResponse = Response<GetVersionReply>;

  Future<GetVersionReply> GetVersion(const GetVersionRequest& request);

  Future<GetVersionReply> GetVersion(const uint16_t& client_major_version = {},
                                     const uint16_t& client_minor_version = {});

  struct GetXIDRangeRequest {};

  struct GetXIDRangeReply {
    uint16_t sequence{};
    uint32_t start_id{};
    uint32_t count{};
  };

  using GetXIDRangeResponse = Response<GetXIDRangeReply>;

  Future<GetXIDRangeReply> GetXIDRange(const GetXIDRangeRequest& request);

  Future<GetXIDRangeReply> GetXIDRange();

  struct GetXIDListRequest {
    uint32_t count{};
  };

  struct GetXIDListReply {
    uint16_t sequence{};
    std::vector<uint32_t> ids{};
  };

  using GetXIDListResponse = Response<GetXIDListReply>;

  Future<GetXIDListReply> GetXIDList(const GetXIDListRequest& request);

  Future<GetXIDListReply> GetXIDList(const uint32_t& count = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

#endif  // UI_GFX_X_GENERATED_PROTOS_XC_MISC_H_
