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

#ifndef UI_GFX_X_GENERATED_PROTOS_XVMC_H_
#define UI_GFX_X_GENERATED_PROTOS_XVMC_H_

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
#include "xv.h"

namespace x11 {

class Connection;

template <typename Reply>
struct Response;

template <typename Reply>
class Future;

class COMPONENT_EXPORT(X11) XvMC {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 1;

  XvMC(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Context : uint32_t {};

  enum class Surface : uint32_t {};

  enum class SubPicture : uint32_t {};

  struct SurfaceInfo {
    bool operator==(const SurfaceInfo& other) const {
      return id == other.id && chroma_format == other.chroma_format &&
             pad0 == other.pad0 && max_width == other.max_width &&
             max_height == other.max_height &&
             subpicture_max_width == other.subpicture_max_width &&
             subpicture_max_height == other.subpicture_max_height &&
             mc_type == other.mc_type && flags == other.flags;
    }

    Surface id{};
    uint16_t chroma_format{};
    uint16_t pad0{};
    uint16_t max_width{};
    uint16_t max_height{};
    uint16_t subpicture_max_width{};
    uint16_t subpicture_max_height{};
    uint32_t mc_type{};
    uint32_t flags{};
  };

  struct QueryVersionRequest {};

  struct QueryVersionReply {
    uint16_t sequence{};
    uint32_t major{};
    uint32_t minor{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion();

  struct ListSurfaceTypesRequest {
    Xv::Port port_id{};
  };

  struct ListSurfaceTypesReply {
    uint16_t sequence{};
    std::vector<SurfaceInfo> surfaces{};
  };

  using ListSurfaceTypesResponse = Response<ListSurfaceTypesReply>;

  Future<ListSurfaceTypesReply> ListSurfaceTypes(
      const ListSurfaceTypesRequest& request);

  Future<ListSurfaceTypesReply> ListSurfaceTypes(const Xv::Port& port_id = {});

  struct CreateContextRequest {
    Context context_id{};
    Xv::Port port_id{};
    Surface surface_id{};
    uint16_t width{};
    uint16_t height{};
    uint32_t flags{};
  };

  struct CreateContextReply {
    uint16_t sequence{};
    uint16_t width_actual{};
    uint16_t height_actual{};
    uint32_t flags_return{};
    std::vector<uint32_t> priv_data{};
  };

  using CreateContextResponse = Response<CreateContextReply>;

  Future<CreateContextReply> CreateContext(const CreateContextRequest& request);

  Future<CreateContextReply> CreateContext(const Context& context_id = {},
                                           const Xv::Port& port_id = {},
                                           const Surface& surface_id = {},
                                           const uint16_t& width = {},
                                           const uint16_t& height = {},
                                           const uint32_t& flags = {});

  struct DestroyContextRequest {
    Context context_id{};
  };

  using DestroyContextResponse = Response<void>;

  Future<void> DestroyContext(const DestroyContextRequest& request);

  Future<void> DestroyContext(const Context& context_id = {});

  struct CreateSurfaceRequest {
    Surface surface_id{};
    Context context_id{};
  };

  struct CreateSurfaceReply {
    uint16_t sequence{};
    std::vector<uint32_t> priv_data{};
  };

  using CreateSurfaceResponse = Response<CreateSurfaceReply>;

  Future<CreateSurfaceReply> CreateSurface(const CreateSurfaceRequest& request);

  Future<CreateSurfaceReply> CreateSurface(const Surface& surface_id = {},
                                           const Context& context_id = {});

  struct DestroySurfaceRequest {
    Surface surface_id{};
  };

  using DestroySurfaceResponse = Response<void>;

  Future<void> DestroySurface(const DestroySurfaceRequest& request);

  Future<void> DestroySurface(const Surface& surface_id = {});

  struct CreateSubpictureRequest {
    SubPicture subpicture_id{};
    Context context{};
    uint32_t xvimage_id{};
    uint16_t width{};
    uint16_t height{};
  };

  struct CreateSubpictureReply {
    uint16_t sequence{};
    uint16_t width_actual{};
    uint16_t height_actual{};
    uint16_t num_palette_entries{};
    uint16_t entry_bytes{};
    std::array<uint8_t, 4> component_order{};
    std::vector<uint32_t> priv_data{};
  };

  using CreateSubpictureResponse = Response<CreateSubpictureReply>;

  Future<CreateSubpictureReply> CreateSubpicture(
      const CreateSubpictureRequest& request);

  Future<CreateSubpictureReply> CreateSubpicture(
      const SubPicture& subpicture_id = {},
      const Context& context = {},
      const uint32_t& xvimage_id = {},
      const uint16_t& width = {},
      const uint16_t& height = {});

  struct DestroySubpictureRequest {
    SubPicture subpicture_id{};
  };

  using DestroySubpictureResponse = Response<void>;

  Future<void> DestroySubpicture(const DestroySubpictureRequest& request);

  Future<void> DestroySubpicture(const SubPicture& subpicture_id = {});

  struct ListSubpictureTypesRequest {
    Xv::Port port_id{};
    Surface surface_id{};
  };

  struct ListSubpictureTypesReply {
    uint16_t sequence{};
    std::vector<Xv::ImageFormatInfo> types{};
  };

  using ListSubpictureTypesResponse = Response<ListSubpictureTypesReply>;

  Future<ListSubpictureTypesReply> ListSubpictureTypes(
      const ListSubpictureTypesRequest& request);

  Future<ListSubpictureTypesReply> ListSubpictureTypes(
      const Xv::Port& port_id = {},
      const Surface& surface_id = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

#endif  // UI_GFX_X_GENERATED_PROTOS_XVMC_H_
