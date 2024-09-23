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

#ifndef UI_GFX_X_GENERATED_PROTOS_XTEST_H_
#define UI_GFX_X_GENERATED_PROTOS_XTEST_H_

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

class COMPONENT_EXPORT(X11) Test {
 public:
  static constexpr unsigned major_version = 2;
  static constexpr unsigned minor_version = 2;

  Test(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Cursor : int {
    None = 0,
    Current = 1,
  };

  struct GetVersionRequest {
    uint8_t major_version{};
    uint16_t minor_version{};
  };

  struct GetVersionReply {
    uint8_t major_version{};
    uint16_t sequence{};
    uint16_t minor_version{};
  };

  using GetVersionResponse = Response<GetVersionReply>;

  Future<GetVersionReply> GetVersion(const GetVersionRequest& request);

  Future<GetVersionReply> GetVersion(const uint8_t& major_version = {},
                                     const uint16_t& minor_version = {});

  struct CompareCursorRequest {
    Window window{};
    x11::Cursor cursor{};
  };

  struct CompareCursorReply {
    uint8_t same{};
    uint16_t sequence{};
  };

  using CompareCursorResponse = Response<CompareCursorReply>;

  Future<CompareCursorReply> CompareCursor(const CompareCursorRequest& request);

  Future<CompareCursorReply> CompareCursor(const Window& window = {},
                                           const x11::Cursor& cursor = {});

  struct FakeInputRequest {
    uint8_t type{};
    uint8_t detail{};
    uint32_t time{};
    Window root{};
    int16_t rootX{};
    int16_t rootY{};
    uint8_t deviceid{};
  };

  using FakeInputResponse = Response<void>;

  Future<void> FakeInput(const FakeInputRequest& request);

  Future<void> FakeInput(const uint8_t& type = {},
                         const uint8_t& detail = {},
                         const uint32_t& time = {},
                         const Window& root = {},
                         const int16_t& rootX = {},
                         const int16_t& rootY = {},
                         const uint8_t& deviceid = {});

  struct GrabControlRequest {
    uint8_t impervious{};
  };

  using GrabControlResponse = Response<void>;

  Future<void> GrabControl(const GrabControlRequest& request);

  Future<void> GrabControl(const uint8_t& impervious = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Test::Cursor operator|(x11::Test::Cursor l,
                                             x11::Test::Cursor r) {
  using T = std::underlying_type_t<x11::Test::Cursor>;
  return static_cast<x11::Test::Cursor>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Test::Cursor operator&(x11::Test::Cursor l,
                                             x11::Test::Cursor r) {
  using T = std::underlying_type_t<x11::Test::Cursor>;
  return static_cast<x11::Test::Cursor>(static_cast<T>(l) & static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_XTEST_H_
