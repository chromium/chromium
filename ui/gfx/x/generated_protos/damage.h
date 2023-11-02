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

#ifndef UI_GFX_X_GENERATED_PROTOS_DAMAGE_H_
#define UI_GFX_X_GENERATED_PROTOS_DAMAGE_H_

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
#include "xfixes.h"
#include "xproto.h"

namespace x11 {

class Connection;

template <typename Reply>
struct Response;

template <typename Reply>
class Future;

class COMPONENT_EXPORT(X11) Damage {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 1;

  Damage(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class DamageId : uint32_t {};

  enum class ReportLevel : int {
    RawRectangles = 0,
    DeltaRectangles = 1,
    BoundingBox = 2,
    NonEmpty = 3,
  };

  struct BadDamageError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct NotifyEvent {
    static constexpr int type_id = 1;
    static constexpr uint8_t opcode = 0;
    ReportLevel level{};
    uint16_t sequence{};
    Drawable drawable{};
    DamageId damage{};
    Time timestamp{};
    Rectangle area{};
    Rectangle geometry{};

    x11::Window* GetWindow() {
      return reinterpret_cast<x11::Window*>(&drawable);
    }
  };

  struct QueryVersionRequest {
    uint32_t client_major_version{};
    uint32_t client_minor_version{};
  };

  struct QueryVersionReply {
    uint16_t sequence{};
    uint32_t major_version{};
    uint32_t minor_version{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion(
      const uint32_t& client_major_version = {},
      const uint32_t& client_minor_version = {});

  struct CreateRequest {
    DamageId damage{};
    Drawable drawable{};
    ReportLevel level{};
  };

  using CreateResponse = Response<void>;

  Future<void> Create(const CreateRequest& request);

  Future<void> Create(const DamageId& damage = {},
                      const Drawable& drawable = {},
                      const ReportLevel& level = {});

  struct DestroyRequest {
    DamageId damage{};
  };

  using DestroyResponse = Response<void>;

  Future<void> Destroy(const DestroyRequest& request);

  Future<void> Destroy(const DamageId& damage = {});

  struct SubtractRequest {
    DamageId damage{};
    XFixes::Region repair{};
    XFixes::Region parts{};
  };

  using SubtractResponse = Response<void>;

  Future<void> Subtract(const SubtractRequest& request);

  Future<void> Subtract(const DamageId& damage = {},
                        const XFixes::Region& repair = {},
                        const XFixes::Region& parts = {});

  struct AddRequest {
    Drawable drawable{};
    XFixes::Region region{};
  };

  using AddResponse = Response<void>;

  Future<void> Add(const AddRequest& request);

  Future<void> Add(const Drawable& drawable = {},
                   const XFixes::Region& region = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Damage::ReportLevel operator|(
    x11::Damage::ReportLevel l,
    x11::Damage::ReportLevel r) {
  using T = std::underlying_type_t<x11::Damage::ReportLevel>;
  return static_cast<x11::Damage::ReportLevel>(static_cast<T>(l) |
                                               static_cast<T>(r));
}

inline constexpr x11::Damage::ReportLevel operator&(
    x11::Damage::ReportLevel l,
    x11::Damage::ReportLevel r) {
  using T = std::underlying_type_t<x11::Damage::ReportLevel>;
  return static_cast<x11::Damage::ReportLevel>(static_cast<T>(l) &
                                               static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_DAMAGE_H_
