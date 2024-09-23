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

#ifndef UI_GFX_X_GENERATED_PROTOS_GLX_H_
#define UI_GFX_X_GENERATED_PROTOS_GLX_H_

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

class COMPONENT_EXPORT(X11) Glx {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 4;

  Glx(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Pixmap : uint32_t {};

  enum class Context : uint32_t {};

  enum class PBuffer : uint32_t {};

  enum class Window : uint32_t {};

  enum class FbConfig : uint32_t {};

  enum class Bool32 : uint32_t {};

  enum class ContextTag : uint32_t {};

  enum class Pbcet : int {
    Damaged = 32791,
    Saved = 32792,
  };

  enum class Pbcdt : int {
    Window = 32793,
    Pbuffer = 32794,
  };

  enum class GraphicsContextAttribute : int {
    XPROTO_GL_ALL_ATTRIB_BITS = 16777215,
    XPROTO_GL_CURRENT_BIT = 1 << 0,
    XPROTO_GL_POINT_BIT = 1 << 1,
    XPROTO_GL_LINE_BIT = 1 << 2,
    XPROTO_GL_POLYGON_BIT = 1 << 3,
    XPROTO_GL_POLYGON_STIPPLE_BIT = 1 << 4,
    XPROTO_GL_PIXEL_MODE_BIT = 1 << 5,
    XPROTO_GL_LIGHTING_BIT = 1 << 6,
    XPROTO_GL_FOG_BIT = 1 << 7,
    XPROTO_GL_DEPTH_BUFFER_BIT = 1 << 8,
    XPROTO_GL_ACCUM_BUFFER_BIT = 1 << 9,
    XPROTO_GL_STENCIL_BUFFER_BIT = 1 << 10,
    XPROTO_GL_VIEWPORT_BIT = 1 << 11,
    XPROTO_GL_TRANSFORM_BIT = 1 << 12,
    XPROTO_GL_ENABLE_BIT = 1 << 13,
    XPROTO_GL_COLOR_BUFFER_BIT = 1 << 14,
    XPROTO_GL_HINT_BIT = 1 << 15,
    XPROTO_GL_EVAL_BIT = 1 << 16,
    XPROTO_GL_LIST_BIT = 1 << 17,
    XPROTO_GL_TEXTURE_BIT = 1 << 18,
    XPROTO_GL_SCISSOR_BIT = 1 << 19,
  };

  enum class Rm : int {
    XPROTO_GL_RENDER = 7168,
    XPROTO_GL_FEEDBACK = 7169,
    XPROTO_GL_SELECT = 7170,
  };

  struct Drawable {
    Drawable() : value{} {}

    Drawable(x11::Window value) : value{static_cast<uint32_t>(value)} {}
    operator x11::Window() const { return static_cast<x11::Window>(value); }

    Drawable(PBuffer value) : value{static_cast<uint32_t>(value)} {}
    operator PBuffer() const { return static_cast<PBuffer>(value); }

    Drawable(Pixmap value) : value{static_cast<uint32_t>(value)} {}
    operator Pixmap() const { return static_cast<Pixmap>(value); }

    Drawable(Window value) : value{static_cast<uint32_t>(value)} {}
    operator Window() const { return static_cast<Window>(value); }

    uint32_t value{};
  };

  struct GenericError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadContextError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadContextStateError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadDrawableError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadPixmapError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadContextTagError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadCurrentWindowError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadRenderRequestError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadLargeRequestError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct UnsupportedPrivateRequestError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadFBConfigError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadPbufferError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadCurrentDrawableError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct BadWindowError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct GLXBadProfileARBError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_value{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct PbufferClobberEvent {
    static constexpr uint8_t type_id = 1;
    static constexpr uint8_t opcode = 0;
    uint16_t sequence{};
    uint16_t event_type{};
    uint16_t draw_type{};
    Drawable drawable{};
    uint32_t b_mask{};
    uint16_t aux_buffer{};
    uint16_t x{};
    uint16_t y{};
    uint16_t width{};
    uint16_t height{};
    uint16_t count{};
  };

  struct BufferSwapCompleteEvent {
    static constexpr uint8_t type_id = 2;
    static constexpr uint8_t opcode = 1;
    uint16_t sequence{};
    uint16_t event_type{};
    Drawable drawable{};
    uint32_t ust_hi{};
    uint32_t ust_lo{};
    uint32_t msc_hi{};
    uint32_t msc_lo{};
    uint32_t sbc{};
  };

  struct RenderRequest {
    ContextTag context_tag{};
    std::vector<uint8_t> data{};
  };

  using RenderResponse = Response<void>;

  Future<void> Render(const RenderRequest& request);

  Future<void> Render(const ContextTag& context_tag = {},
                      const std::vector<uint8_t>& data = {});

  struct RenderLargeRequest {
    ContextTag context_tag{};
    uint16_t request_num{};
    uint16_t request_total{};
    std::vector<uint8_t> data{};
  };

  using RenderLargeResponse = Response<void>;

  Future<void> RenderLarge(const RenderLargeRequest& request);

  Future<void> RenderLarge(const ContextTag& context_tag = {},
                           const uint16_t& request_num = {},
                           const uint16_t& request_total = {},
                           const std::vector<uint8_t>& data = {});

  struct CreateContextRequest {
    Context context{};
    VisualId visual{};
    uint32_t screen{};
    Context share_list{};
    uint8_t is_direct{};
  };

  using CreateContextResponse = Response<void>;

  Future<void> CreateContext(const CreateContextRequest& request);

  Future<void> CreateContext(const Context& context = {},
                             const VisualId& visual = {},
                             const uint32_t& screen = {},
                             const Context& share_list = {},
                             const uint8_t& is_direct = {});

  struct DestroyContextRequest {
    Context context{};
  };

  using DestroyContextResponse = Response<void>;

  Future<void> DestroyContext(const DestroyContextRequest& request);

  Future<void> DestroyContext(const Context& context = {});

  struct MakeCurrentRequest {
    Drawable drawable{};
    Context context{};
    ContextTag old_context_tag{};
  };

  struct MakeCurrentReply {
    uint16_t sequence{};
    ContextTag context_tag{};
  };

  using MakeCurrentResponse = Response<MakeCurrentReply>;

  Future<MakeCurrentReply> MakeCurrent(const MakeCurrentRequest& request);

  Future<MakeCurrentReply> MakeCurrent(const Drawable& drawable = {},
                                       const Context& context = {},
                                       const ContextTag& old_context_tag = {});

  struct IsDirectRequest {
    Context context{};
  };

  struct IsDirectReply {
    uint16_t sequence{};
    uint8_t is_direct{};
  };

  using IsDirectResponse = Response<IsDirectReply>;

  Future<IsDirectReply> IsDirect(const IsDirectRequest& request);

  Future<IsDirectReply> IsDirect(const Context& context = {});

  struct QueryVersionRequest {
    uint32_t major_version{};
    uint32_t minor_version{};
  };

  struct QueryVersionReply {
    uint16_t sequence{};
    uint32_t major_version{};
    uint32_t minor_version{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion(const uint32_t& major_version = {},
                                         const uint32_t& minor_version = {});

  struct WaitGLRequest {
    ContextTag context_tag{};
  };

  using WaitGLResponse = Response<void>;

  Future<void> WaitGL(const WaitGLRequest& request);

  Future<void> WaitGL(const ContextTag& context_tag = {});

  struct WaitXRequest {
    ContextTag context_tag{};
  };

  using WaitXResponse = Response<void>;

  Future<void> WaitX(const WaitXRequest& request);

  Future<void> WaitX(const ContextTag& context_tag = {});

  struct CopyContextRequest {
    Context src{};
    Context dest{};
    uint32_t mask{};
    ContextTag src_context_tag{};
  };

  using CopyContextResponse = Response<void>;

  Future<void> CopyContext(const CopyContextRequest& request);

  Future<void> CopyContext(const Context& src = {},
                           const Context& dest = {},
                           const uint32_t& mask = {},
                           const ContextTag& src_context_tag = {});

  struct SwapBuffersRequest {
    ContextTag context_tag{};
    Drawable drawable{};
  };

  using SwapBuffersResponse = Response<void>;

  Future<void> SwapBuffers(const SwapBuffersRequest& request);

  Future<void> SwapBuffers(const ContextTag& context_tag = {},
                           const Drawable& drawable = {});

  struct UseXFontRequest {
    ContextTag context_tag{};
    Font font{};
    uint32_t first{};
    uint32_t count{};
    uint32_t list_base{};
  };

  using UseXFontResponse = Response<void>;

  Future<void> UseXFont(const UseXFontRequest& request);

  Future<void> UseXFont(const ContextTag& context_tag = {},
                        const Font& font = {},
                        const uint32_t& first = {},
                        const uint32_t& count = {},
                        const uint32_t& list_base = {});

  struct CreateGLXPixmapRequest {
    uint32_t screen{};
    VisualId visual{};
    x11::Pixmap pixmap{};
    Pixmap glx_pixmap{};
  };

  using CreateGLXPixmapResponse = Response<void>;

  Future<void> CreateGLXPixmap(const CreateGLXPixmapRequest& request);

  Future<void> CreateGLXPixmap(const uint32_t& screen = {},
                               const VisualId& visual = {},
                               const x11::Pixmap& pixmap = {},
                               const Pixmap& glx_pixmap = {});

  struct GetVisualConfigsRequest {
    uint32_t screen{};
  };

  struct GetVisualConfigsReply {
    uint16_t sequence{};
    uint32_t num_visuals{};
    uint32_t num_properties{};
    std::vector<uint32_t> property_list{};
  };

  using GetVisualConfigsResponse = Response<GetVisualConfigsReply>;

  Future<GetVisualConfigsReply> GetVisualConfigs(
      const GetVisualConfigsRequest& request);

  Future<GetVisualConfigsReply> GetVisualConfigs(const uint32_t& screen = {});

  struct DestroyGLXPixmapRequest {
    Pixmap glx_pixmap{};
  };

  using DestroyGLXPixmapResponse = Response<void>;

  Future<void> DestroyGLXPixmap(const DestroyGLXPixmapRequest& request);

  Future<void> DestroyGLXPixmap(const Pixmap& glx_pixmap = {});

  struct VendorPrivateRequest {
    uint32_t vendor_code{};
    ContextTag context_tag{};
    std::vector<uint8_t> data{};
  };

  using VendorPrivateResponse = Response<void>;

  Future<void> VendorPrivate(const VendorPrivateRequest& request);

  Future<void> VendorPrivate(const uint32_t& vendor_code = {},
                             const ContextTag& context_tag = {},
                             const std::vector<uint8_t>& data = {});

  struct VendorPrivateWithReplyRequest {
    uint32_t vendor_code{};
    ContextTag context_tag{};
    std::vector<uint8_t> data{};
  };

  struct VendorPrivateWithReplyReply {
    uint16_t sequence{};
    uint32_t retval{};
    std::array<uint8_t, 24> data1{};
    std::vector<uint8_t> data2{};
  };

  using VendorPrivateWithReplyResponse = Response<VendorPrivateWithReplyReply>;

  Future<VendorPrivateWithReplyReply> VendorPrivateWithReply(
      const VendorPrivateWithReplyRequest& request);

  Future<VendorPrivateWithReplyReply> VendorPrivateWithReply(
      const uint32_t& vendor_code = {},
      const ContextTag& context_tag = {},
      const std::vector<uint8_t>& data = {});

  struct QueryExtensionsStringRequest {
    uint32_t screen{};
  };

  struct QueryExtensionsStringReply {
    uint16_t sequence{};
    uint32_t n{};
  };

  using QueryExtensionsStringResponse = Response<QueryExtensionsStringReply>;

  Future<QueryExtensionsStringReply> QueryExtensionsString(
      const QueryExtensionsStringRequest& request);

  Future<QueryExtensionsStringReply> QueryExtensionsString(
      const uint32_t& screen = {});

  struct QueryServerStringRequest {
    uint32_t screen{};
    uint32_t name{};
  };

  struct QueryServerStringReply {
    uint16_t sequence{};
    std::string string{};
  };

  using QueryServerStringResponse = Response<QueryServerStringReply>;

  Future<QueryServerStringReply> QueryServerString(
      const QueryServerStringRequest& request);

  Future<QueryServerStringReply> QueryServerString(const uint32_t& screen = {},
                                                   const uint32_t& name = {});

  struct ClientInfoRequest {
    uint32_t major_version{};
    uint32_t minor_version{};
    std::string string{};
  };

  using ClientInfoResponse = Response<void>;

  Future<void> ClientInfo(const ClientInfoRequest& request);

  Future<void> ClientInfo(const uint32_t& major_version = {},
                          const uint32_t& minor_version = {},
                          const std::string& string = {});

  struct GetFBConfigsRequest {
    uint32_t screen{};
  };

  struct GetFBConfigsReply {
    uint16_t sequence{};
    uint32_t num_FB_configs{};
    uint32_t num_properties{};
    std::vector<uint32_t> property_list{};
  };

  using GetFBConfigsResponse = Response<GetFBConfigsReply>;

  Future<GetFBConfigsReply> GetFBConfigs(const GetFBConfigsRequest& request);

  Future<GetFBConfigsReply> GetFBConfigs(const uint32_t& screen = {});

  struct CreatePixmapRequest {
    uint32_t screen{};
    FbConfig fbconfig{};
    x11::Pixmap pixmap{};
    Pixmap glx_pixmap{};
    uint32_t num_attribs{};
    std::vector<uint32_t> attribs{};
  };

  using CreatePixmapResponse = Response<void>;

  Future<void> CreatePixmap(const CreatePixmapRequest& request);

  Future<void> CreatePixmap(const uint32_t& screen = {},
                            const FbConfig& fbconfig = {},
                            const x11::Pixmap& pixmap = {},
                            const Pixmap& glx_pixmap = {},
                            const uint32_t& num_attribs = {},
                            const std::vector<uint32_t>& attribs = {});

  struct DestroyPixmapRequest {
    Pixmap glx_pixmap{};
  };

  using DestroyPixmapResponse = Response<void>;

  Future<void> DestroyPixmap(const DestroyPixmapRequest& request);

  Future<void> DestroyPixmap(const Pixmap& glx_pixmap = {});

  struct CreateNewContextRequest {
    Context context{};
    FbConfig fbconfig{};
    uint32_t screen{};
    uint32_t render_type{};
    Context share_list{};
    uint8_t is_direct{};
  };

  using CreateNewContextResponse = Response<void>;

  Future<void> CreateNewContext(const CreateNewContextRequest& request);

  Future<void> CreateNewContext(const Context& context = {},
                                const FbConfig& fbconfig = {},
                                const uint32_t& screen = {},
                                const uint32_t& render_type = {},
                                const Context& share_list = {},
                                const uint8_t& is_direct = {});

  struct QueryContextRequest {
    Context context{};
  };

  struct QueryContextReply {
    uint16_t sequence{};
    uint32_t num_attribs{};
    std::vector<uint32_t> attribs{};
  };

  using QueryContextResponse = Response<QueryContextReply>;

  Future<QueryContextReply> QueryContext(const QueryContextRequest& request);

  Future<QueryContextReply> QueryContext(const Context& context = {});

  struct MakeContextCurrentRequest {
    ContextTag old_context_tag{};
    Drawable drawable{};
    Drawable read_drawable{};
    Context context{};
  };

  struct MakeContextCurrentReply {
    uint16_t sequence{};
    ContextTag context_tag{};
  };

  using MakeContextCurrentResponse = Response<MakeContextCurrentReply>;

  Future<MakeContextCurrentReply> MakeContextCurrent(
      const MakeContextCurrentRequest& request);

  Future<MakeContextCurrentReply> MakeContextCurrent(
      const ContextTag& old_context_tag = {},
      const Drawable& drawable = {},
      const Drawable& read_drawable = {},
      const Context& context = {});

  struct CreatePbufferRequest {
    uint32_t screen{};
    FbConfig fbconfig{};
    PBuffer pbuffer{};
    uint32_t num_attribs{};
    std::vector<uint32_t> attribs{};
  };

  using CreatePbufferResponse = Response<void>;

  Future<void> CreatePbuffer(const CreatePbufferRequest& request);

  Future<void> CreatePbuffer(const uint32_t& screen = {},
                             const FbConfig& fbconfig = {},
                             const PBuffer& pbuffer = {},
                             const uint32_t& num_attribs = {},
                             const std::vector<uint32_t>& attribs = {});

  struct DestroyPbufferRequest {
    PBuffer pbuffer{};
  };

  using DestroyPbufferResponse = Response<void>;

  Future<void> DestroyPbuffer(const DestroyPbufferRequest& request);

  Future<void> DestroyPbuffer(const PBuffer& pbuffer = {});

  struct GetDrawableAttributesRequest {
    Drawable drawable{};
  };

  struct GetDrawableAttributesReply {
    uint16_t sequence{};
    uint32_t num_attribs{};
    std::vector<uint32_t> attribs{};
  };

  using GetDrawableAttributesResponse = Response<GetDrawableAttributesReply>;

  Future<GetDrawableAttributesReply> GetDrawableAttributes(
      const GetDrawableAttributesRequest& request);

  Future<GetDrawableAttributesReply> GetDrawableAttributes(
      const Drawable& drawable = {});

  struct ChangeDrawableAttributesRequest {
    Drawable drawable{};
    uint32_t num_attribs{};
    std::vector<uint32_t> attribs{};
  };

  using ChangeDrawableAttributesResponse = Response<void>;

  Future<void> ChangeDrawableAttributes(
      const ChangeDrawableAttributesRequest& request);

  Future<void> ChangeDrawableAttributes(
      const Drawable& drawable = {},
      const uint32_t& num_attribs = {},
      const std::vector<uint32_t>& attribs = {});

  struct CreateWindowRequest {
    uint32_t screen{};
    FbConfig fbconfig{};
    x11::Window window{};
    Window glx_window{};
    uint32_t num_attribs{};
    std::vector<uint32_t> attribs{};
  };

  using CreateWindowResponse = Response<void>;

  Future<void> CreateWindow(const CreateWindowRequest& request);

  Future<void> CreateWindow(const uint32_t& screen = {},
                            const FbConfig& fbconfig = {},
                            const x11::Window& window = {},
                            const Window& glx_window = {},
                            const uint32_t& num_attribs = {},
                            const std::vector<uint32_t>& attribs = {});

  struct DeleteWindowRequest {
    Window glxwindow{};
  };

  using DeleteWindowResponse = Response<void>;

  Future<void> DeleteWindow(const DeleteWindowRequest& request);

  Future<void> DeleteWindow(const Window& glxwindow = {});

  struct SetClientInfoARBRequest {
    uint32_t major_version{};
    uint32_t minor_version{};
    uint32_t num_versions{};
    std::vector<uint32_t> gl_versions{};
    std::string gl_extension_string{};
    std::string glx_extension_string{};
  };

  using SetClientInfoARBResponse = Response<void>;

  Future<void> SetClientInfoARB(const SetClientInfoARBRequest& request);

  Future<void> SetClientInfoARB(const uint32_t& major_version = {},
                                const uint32_t& minor_version = {},
                                const uint32_t& num_versions = {},
                                const std::vector<uint32_t>& gl_versions = {},
                                const std::string& gl_extension_string = {},
                                const std::string& glx_extension_string = {});

  struct CreateContextAttribsARBRequest {
    Context context{};
    FbConfig fbconfig{};
    uint32_t screen{};
    Context share_list{};
    uint8_t is_direct{};
    uint32_t num_attribs{};
    std::vector<uint32_t> attribs{};
  };

  using CreateContextAttribsARBResponse = Response<void>;

  Future<void> CreateContextAttribsARB(
      const CreateContextAttribsARBRequest& request);

  Future<void> CreateContextAttribsARB(
      const Context& context = {},
      const FbConfig& fbconfig = {},
      const uint32_t& screen = {},
      const Context& share_list = {},
      const uint8_t& is_direct = {},
      const uint32_t& num_attribs = {},
      const std::vector<uint32_t>& attribs = {});

  struct SetClientInfo2ARBRequest {
    uint32_t major_version{};
    uint32_t minor_version{};
    uint32_t num_versions{};
    std::vector<uint32_t> gl_versions{};
    std::string gl_extension_string{};
    std::string glx_extension_string{};
  };

  using SetClientInfo2ARBResponse = Response<void>;

  Future<void> SetClientInfo2ARB(const SetClientInfo2ARBRequest& request);

  Future<void> SetClientInfo2ARB(const uint32_t& major_version = {},
                                 const uint32_t& minor_version = {},
                                 const uint32_t& num_versions = {},
                                 const std::vector<uint32_t>& gl_versions = {},
                                 const std::string& gl_extension_string = {},
                                 const std::string& glx_extension_string = {});

  struct NewListRequest {
    ContextTag context_tag{};
    uint32_t list{};
    uint32_t mode{};
  };

  using NewListResponse = Response<void>;

  Future<void> NewList(const NewListRequest& request);

  Future<void> NewList(const ContextTag& context_tag = {},
                       const uint32_t& list = {},
                       const uint32_t& mode = {});

  struct EndListRequest {
    ContextTag context_tag{};
  };

  using EndListResponse = Response<void>;

  Future<void> EndList(const EndListRequest& request);

  Future<void> EndList(const ContextTag& context_tag = {});

  struct DeleteListsRequest {
    ContextTag context_tag{};
    uint32_t list{};
    int32_t range{};
  };

  using DeleteListsResponse = Response<void>;

  Future<void> DeleteLists(const DeleteListsRequest& request);

  Future<void> DeleteLists(const ContextTag& context_tag = {},
                           const uint32_t& list = {},
                           const int32_t& range = {});

  struct GenListsRequest {
    ContextTag context_tag{};
    int32_t range{};
  };

  struct GenListsReply {
    uint16_t sequence{};
    uint32_t ret_val{};
  };

  using GenListsResponse = Response<GenListsReply>;

  Future<GenListsReply> GenLists(const GenListsRequest& request);

  Future<GenListsReply> GenLists(const ContextTag& context_tag = {},
                                 const int32_t& range = {});

  struct FeedbackBufferRequest {
    ContextTag context_tag{};
    int32_t size{};
    int32_t type{};
  };

  using FeedbackBufferResponse = Response<void>;

  Future<void> FeedbackBuffer(const FeedbackBufferRequest& request);

  Future<void> FeedbackBuffer(const ContextTag& context_tag = {},
                              const int32_t& size = {},
                              const int32_t& type = {});

  struct SelectBufferRequest {
    ContextTag context_tag{};
    int32_t size{};
  };

  using SelectBufferResponse = Response<void>;

  Future<void> SelectBuffer(const SelectBufferRequest& request);

  Future<void> SelectBuffer(const ContextTag& context_tag = {},
                            const int32_t& size = {});

  struct RenderModeRequest {
    ContextTag context_tag{};
    uint32_t mode{};
  };

  struct RenderModeReply {
    uint16_t sequence{};
    uint32_t ret_val{};
    uint32_t new_mode{};
    std::vector<uint32_t> data{};
  };

  using RenderModeResponse = Response<RenderModeReply>;

  Future<RenderModeReply> RenderMode(const RenderModeRequest& request);

  Future<RenderModeReply> RenderMode(const ContextTag& context_tag = {},
                                     const uint32_t& mode = {});

  struct FinishRequest {
    ContextTag context_tag{};
  };

  struct FinishReply {
    uint16_t sequence{};
  };

  using FinishResponse = Response<FinishReply>;

  Future<FinishReply> Finish(const FinishRequest& request);

  Future<FinishReply> Finish(const ContextTag& context_tag = {});

  struct PixelStorefRequest {
    ContextTag context_tag{};
    uint32_t pname{};
    float datum{};
  };

  using PixelStorefResponse = Response<void>;

  Future<void> PixelStoref(const PixelStorefRequest& request);

  Future<void> PixelStoref(const ContextTag& context_tag = {},
                           const uint32_t& pname = {},
                           const float& datum = {});

  struct PixelStoreiRequest {
    ContextTag context_tag{};
    uint32_t pname{};
    int32_t datum{};
  };

  using PixelStoreiResponse = Response<void>;

  Future<void> PixelStorei(const PixelStoreiRequest& request);

  Future<void> PixelStorei(const ContextTag& context_tag = {},
                           const uint32_t& pname = {},
                           const int32_t& datum = {});

  struct ReadPixelsRequest {
    ContextTag context_tag{};
    int32_t x{};
    int32_t y{};
    int32_t width{};
    int32_t height{};
    uint32_t format{};
    uint32_t type{};
    uint8_t swap_bytes{};
    uint8_t lsb_first{};
  };

  struct ReadPixelsReply {
    uint16_t sequence{};
    std::vector<uint8_t> data{};
  };

  using ReadPixelsResponse = Response<ReadPixelsReply>;

  Future<ReadPixelsReply> ReadPixels(const ReadPixelsRequest& request);

  Future<ReadPixelsReply> ReadPixels(const ContextTag& context_tag = {},
                                     const int32_t& x = {},
                                     const int32_t& y = {},
                                     const int32_t& width = {},
                                     const int32_t& height = {},
                                     const uint32_t& format = {},
                                     const uint32_t& type = {},
                                     const uint8_t& swap_bytes = {},
                                     const uint8_t& lsb_first = {});

  struct GetBooleanvRequest {
    ContextTag context_tag{};
    int32_t pname{};
  };

  struct GetBooleanvReply {
    uint16_t sequence{};
    uint8_t datum{};
    std::vector<uint8_t> data{};
  };

  using GetBooleanvResponse = Response<GetBooleanvReply>;

  Future<GetBooleanvReply> GetBooleanv(const GetBooleanvRequest& request);

  Future<GetBooleanvReply> GetBooleanv(const ContextTag& context_tag = {},
                                       const int32_t& pname = {});

  struct GetClipPlaneRequest {
    ContextTag context_tag{};
    int32_t plane{};
  };

  struct GetClipPlaneReply {
    uint16_t sequence{};
    std::vector<double> data{};
  };

  using GetClipPlaneResponse = Response<GetClipPlaneReply>;

  Future<GetClipPlaneReply> GetClipPlane(const GetClipPlaneRequest& request);

  Future<GetClipPlaneReply> GetClipPlane(const ContextTag& context_tag = {},
                                         const int32_t& plane = {});

  struct GetDoublevRequest {
    ContextTag context_tag{};
    uint32_t pname{};
  };

  struct GetDoublevReply {
    uint16_t sequence{};
    double datum{};
    std::vector<double> data{};
  };

  using GetDoublevResponse = Response<GetDoublevReply>;

  Future<GetDoublevReply> GetDoublev(const GetDoublevRequest& request);

  Future<GetDoublevReply> GetDoublev(const ContextTag& context_tag = {},
                                     const uint32_t& pname = {});

  struct GetErrorRequest {
    ContextTag context_tag{};
  };

  struct GetErrorReply {
    uint16_t sequence{};
    int32_t error{};
  };

  using GetErrorResponse = Response<GetErrorReply>;

  Future<GetErrorReply> GetError(const GetErrorRequest& request);

  Future<GetErrorReply> GetError(const ContextTag& context_tag = {});

  struct GetFloatvRequest {
    ContextTag context_tag{};
    uint32_t pname{};
  };

  struct GetFloatvReply {
    uint16_t sequence{};
    float datum{};
    std::vector<float> data{};
  };

  using GetFloatvResponse = Response<GetFloatvReply>;

  Future<GetFloatvReply> GetFloatv(const GetFloatvRequest& request);

  Future<GetFloatvReply> GetFloatv(const ContextTag& context_tag = {},
                                   const uint32_t& pname = {});

  struct GetIntegervRequest {
    ContextTag context_tag{};
    uint32_t pname{};
  };

  struct GetIntegervReply {
    uint16_t sequence{};
    int32_t datum{};
    std::vector<int32_t> data{};
  };

  using GetIntegervResponse = Response<GetIntegervReply>;

  Future<GetIntegervReply> GetIntegerv(const GetIntegervRequest& request);

  Future<GetIntegervReply> GetIntegerv(const ContextTag& context_tag = {},
                                       const uint32_t& pname = {});

  struct GetLightfvRequest {
    ContextTag context_tag{};
    uint32_t light{};
    uint32_t pname{};
  };

  struct GetLightfvReply {
    uint16_t sequence{};
    float datum{};
    std::vector<float> data{};
  };

  using GetLightfvResponse = Response<GetLightfvReply>;

  Future<GetLightfvReply> GetLightfv(const GetLightfvRequest& request);

  Future<GetLightfvReply> GetLightfv(const ContextTag& context_tag = {},
                                     const uint32_t& light = {},
                                     const uint32_t& pname = {});

  struct GetLightivRequest {
    ContextTag context_tag{};
    uint32_t light{};
    uint32_t pname{};
  };

  struct GetLightivReply {
    uint16_t sequence{};
    int32_t datum{};
    std::vector<int32_t> data{};
  };

  using GetLightivResponse = Response<GetLightivReply>;

  Future<GetLightivReply> GetLightiv(const GetLightivRequest& request);

  Future<GetLightivReply> GetLightiv(const ContextTag& context_tag = {},
                                     const uint32_t& light = {},
                                     const uint32_t& pname = {});

  struct GetMapdvRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t query{};
  };

  struct GetMapdvReply {
    uint16_t sequence{};
    double datum{};
    std::vector<double> data{};
  };

  using GetMapdvResponse = Response<GetMapdvReply>;

  Future<GetMapdvReply> GetMapdv(const GetMapdvRequest& request);

  Future<GetMapdvReply> GetMapdv(const ContextTag& context_tag = {},
                                 const uint32_t& target = {},
                                 const uint32_t& query = {});

  struct GetMapfvRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t query{};
  };

  struct GetMapfvReply {
    uint16_t sequence{};
    float datum{};
    std::vector<float> data{};
  };

  using GetMapfvResponse = Response<GetMapfvReply>;

  Future<GetMapfvReply> GetMapfv(const GetMapfvRequest& request);

  Future<GetMapfvReply> GetMapfv(const ContextTag& context_tag = {},
                                 const uint32_t& target = {},
                                 const uint32_t& query = {});

  struct GetMapivRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t query{};
  };

  struct GetMapivReply {
    uint16_t sequence{};
    int32_t datum{};
    std::vector<int32_t> data{};
  };

  using GetMapivResponse = Response<GetMapivReply>;

  Future<GetMapivReply> GetMapiv(const GetMapivRequest& request);

  Future<GetMapivReply> GetMapiv(const ContextTag& context_tag = {},
                                 const uint32_t& target = {},
                                 const uint32_t& query = {});

  struct GetMaterialfvRequest {
    ContextTag context_tag{};
    uint32_t face{};
    uint32_t pname{};
  };

  struct GetMaterialfvReply {
    uint16_t sequence{};
    float datum{};
    std::vector<float> data{};
  };

  using GetMaterialfvResponse = Response<GetMaterialfvReply>;

  Future<GetMaterialfvReply> GetMaterialfv(const GetMaterialfvRequest& request);

  Future<GetMaterialfvReply> GetMaterialfv(const ContextTag& context_tag = {},
                                           const uint32_t& face = {},
                                           const uint32_t& pname = {});

  struct GetMaterialivRequest {
    ContextTag context_tag{};
    uint32_t face{};
    uint32_t pname{};
  };

  struct GetMaterialivReply {
    uint16_t sequence{};
    int32_t datum{};
    std::vector<int32_t> data{};
  };

  using GetMaterialivResponse = Response<GetMaterialivReply>;

  Future<GetMaterialivReply> GetMaterialiv(const GetMaterialivRequest& request);

  Future<GetMaterialivReply> GetMaterialiv(const ContextTag& context_tag = {},
                                           const uint32_t& face = {},
                                           const uint32_t& pname = {});

  struct GetPixelMapfvRequest {
    ContextTag context_tag{};
    uint32_t map{};
  };

  struct GetPixelMapfvReply {
    uint16_t sequence{};
    float datum{};
    std::vector<float> data{};
  };

  using GetPixelMapfvResponse = Response<GetPixelMapfvReply>;

  Future<GetPixelMapfvReply> GetPixelMapfv(const GetPixelMapfvRequest& request);

  Future<GetPixelMapfvReply> GetPixelMapfv(const ContextTag& context_tag = {},
                                           const uint32_t& map = {});

  struct GetPixelMapuivRequest {
    ContextTag context_tag{};
    uint32_t map{};
  };

  struct GetPixelMapuivReply {
    uint16_t sequence{};
    uint32_t datum{};
    std::vector<uint32_t> data{};
  };

  using GetPixelMapuivResponse = Response<GetPixelMapuivReply>;

  Future<GetPixelMapuivReply> GetPixelMapuiv(
      const GetPixelMapuivRequest& request);

  Future<GetPixelMapuivReply> GetPixelMapuiv(const ContextTag& context_tag = {},
                                             const uint32_t& map = {});

  struct GetPixelMapusvRequest {
    ContextTag context_tag{};
    uint32_t map{};
  };

  struct GetPixelMapusvReply {
    uint16_t sequence{};
    uint16_t datum{};
    std::vector<uint16_t> data{};
  };

  using GetPixelMapusvResponse = Response<GetPixelMapusvReply>;

  Future<GetPixelMapusvReply> GetPixelMapusv(
      const GetPixelMapusvRequest& request);

  Future<GetPixelMapusvReply> GetPixelMapusv(const ContextTag& context_tag = {},
                                             const uint32_t& map = {});

  struct GetPolygonStippleRequest {
    ContextTag context_tag{};
    uint8_t lsb_first{};
  };

  struct GetPolygonStippleReply {
    uint16_t sequence{};
    std::vector<uint8_t> data{};
  };

  using GetPolygonStippleResponse = Response<GetPolygonStippleReply>;

  Future<GetPolygonStippleReply> GetPolygonStipple(
      const GetPolygonStippleRequest& request);

  Future<GetPolygonStippleReply> GetPolygonStipple(
      const ContextTag& context_tag = {},
      const uint8_t& lsb_first = {});

  struct GetStringRequest {
    ContextTag context_tag{};
    uint32_t name{};
  };

  struct GetStringReply {
    uint16_t sequence{};
    std::string string{};
  };

  using GetStringResponse = Response<GetStringReply>;

  Future<GetStringReply> GetString(const GetStringRequest& request);

  Future<GetStringReply> GetString(const ContextTag& context_tag = {},
                                   const uint32_t& name = {});

  struct GetTexEnvfvRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t pname{};
  };

  struct GetTexEnvfvReply {
    uint16_t sequence{};
    float datum{};
    std::vector<float> data{};
  };

  using GetTexEnvfvResponse = Response<GetTexEnvfvReply>;

  Future<GetTexEnvfvReply> GetTexEnvfv(const GetTexEnvfvRequest& request);

  Future<GetTexEnvfvReply> GetTexEnvfv(const ContextTag& context_tag = {},
                                       const uint32_t& target = {},
                                       const uint32_t& pname = {});

  struct GetTexEnvivRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t pname{};
  };

  struct GetTexEnvivReply {
    uint16_t sequence{};
    int32_t datum{};
    std::vector<int32_t> data{};
  };

  using GetTexEnvivResponse = Response<GetTexEnvivReply>;

  Future<GetTexEnvivReply> GetTexEnviv(const GetTexEnvivRequest& request);

  Future<GetTexEnvivReply> GetTexEnviv(const ContextTag& context_tag = {},
                                       const uint32_t& target = {},
                                       const uint32_t& pname = {});

  struct GetTexGendvRequest {
    ContextTag context_tag{};
    uint32_t coord{};
    uint32_t pname{};
  };

  struct GetTexGendvReply {
    uint16_t sequence{};
    double datum{};
    std::vector<double> data{};
  };

  using GetTexGendvResponse = Response<GetTexGendvReply>;

  Future<GetTexGendvReply> GetTexGendv(const GetTexGendvRequest& request);

  Future<GetTexGendvReply> GetTexGendv(const ContextTag& context_tag = {},
                                       const uint32_t& coord = {},
                                       const uint32_t& pname = {});

  struct GetTexGenfvRequest {
    ContextTag context_tag{};
    uint32_t coord{};
    uint32_t pname{};
  };

  struct GetTexGenfvReply {
    uint16_t sequence{};
    float datum{};
    std::vector<float> data{};
  };

  using GetTexGenfvResponse = Response<GetTexGenfvReply>;

  Future<GetTexGenfvReply> GetTexGenfv(const GetTexGenfvRequest& request);

  Future<GetTexGenfvReply> GetTexGenfv(const ContextTag& context_tag = {},
                                       const uint32_t& coord = {},
                                       const uint32_t& pname = {});

  struct GetTexGenivRequest {
    ContextTag context_tag{};
    uint32_t coord{};
    uint32_t pname{};
  };

  struct GetTexGenivReply {
    uint16_t sequence{};
    int32_t datum{};
    std::vector<int32_t> data{};
  };

  using GetTexGenivResponse = Response<GetTexGenivReply>;

  Future<GetTexGenivReply> GetTexGeniv(const GetTexGenivRequest& request);

  Future<GetTexGenivReply> GetTexGeniv(const ContextTag& context_tag = {},
                                       const uint32_t& coord = {},
                                       const uint32_t& pname = {});

  struct GetTexImageRequest {
    ContextTag context_tag{};
    uint32_t target{};
    int32_t level{};
    uint32_t format{};
    uint32_t type{};
    uint8_t swap_bytes{};
  };

  struct GetTexImageReply {
    uint16_t sequence{};
    int32_t width{};
    int32_t height{};
    int32_t depth{};
    std::vector<uint8_t> data{};
  };

  using GetTexImageResponse = Response<GetTexImageReply>;

  Future<GetTexImageReply> GetTexImage(const GetTexImageRequest& request);

  Future<GetTexImageReply> GetTexImage(const ContextTag& context_tag = {},
                                       const uint32_t& target = {},
                                       const int32_t& level = {},
                                       const uint32_t& format = {},
                                       const uint32_t& type = {},
                                       const uint8_t& swap_bytes = {});

  struct GetTexParameterfvRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t pname{};
  };

  struct GetTexParameterfvReply {
    uint16_t sequence{};
    float datum{};
    std::vector<float> data{};
  };

  using GetTexParameterfvResponse = Response<GetTexParameterfvReply>;

  Future<GetTexParameterfvReply> GetTexParameterfv(
      const GetTexParameterfvRequest& request);

  Future<GetTexParameterfvReply> GetTexParameterfv(
      const ContextTag& context_tag = {},
      const uint32_t& target = {},
      const uint32_t& pname = {});

  struct GetTexParameterivRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t pname{};
  };

  struct GetTexParameterivReply {
    uint16_t sequence{};
    int32_t datum{};
    std::vector<int32_t> data{};
  };

  using GetTexParameterivResponse = Response<GetTexParameterivReply>;

  Future<GetTexParameterivReply> GetTexParameteriv(
      const GetTexParameterivRequest& request);

  Future<GetTexParameterivReply> GetTexParameteriv(
      const ContextTag& context_tag = {},
      const uint32_t& target = {},
      const uint32_t& pname = {});

  struct GetTexLevelParameterfvRequest {
    ContextTag context_tag{};
    uint32_t target{};
    int32_t level{};
    uint32_t pname{};
  };

  struct GetTexLevelParameterfvReply {
    uint16_t sequence{};
    float datum{};
    std::vector<float> data{};
  };

  using GetTexLevelParameterfvResponse = Response<GetTexLevelParameterfvReply>;

  Future<GetTexLevelParameterfvReply> GetTexLevelParameterfv(
      const GetTexLevelParameterfvRequest& request);

  Future<GetTexLevelParameterfvReply> GetTexLevelParameterfv(
      const ContextTag& context_tag = {},
      const uint32_t& target = {},
      const int32_t& level = {},
      const uint32_t& pname = {});

  struct GetTexLevelParameterivRequest {
    ContextTag context_tag{};
    uint32_t target{};
    int32_t level{};
    uint32_t pname{};
  };

  struct GetTexLevelParameterivReply {
    uint16_t sequence{};
    int32_t datum{};
    std::vector<int32_t> data{};
  };

  using GetTexLevelParameterivResponse = Response<GetTexLevelParameterivReply>;

  Future<GetTexLevelParameterivReply> GetTexLevelParameteriv(
      const GetTexLevelParameterivRequest& request);

  Future<GetTexLevelParameterivReply> GetTexLevelParameteriv(
      const ContextTag& context_tag = {},
      const uint32_t& target = {},
      const int32_t& level = {},
      const uint32_t& pname = {});

  struct IsEnabledRequest {
    ContextTag context_tag{};
    uint32_t capability{};
  };

  struct IsEnabledReply {
    uint16_t sequence{};
    Bool32 ret_val{};
  };

  using IsEnabledResponse = Response<IsEnabledReply>;

  Future<IsEnabledReply> IsEnabled(const IsEnabledRequest& request);

  Future<IsEnabledReply> IsEnabled(const ContextTag& context_tag = {},
                                   const uint32_t& capability = {});

  struct IsListRequest {
    ContextTag context_tag{};
    uint32_t list{};
  };

  struct IsListReply {
    uint16_t sequence{};
    Bool32 ret_val{};
  };

  using IsListResponse = Response<IsListReply>;

  Future<IsListReply> IsList(const IsListRequest& request);

  Future<IsListReply> IsList(const ContextTag& context_tag = {},
                             const uint32_t& list = {});

  struct FlushRequest {
    ContextTag context_tag{};
  };

  using FlushResponse = Response<void>;

  Future<void> Flush(const FlushRequest& request);

  Future<void> Flush(const ContextTag& context_tag = {});

  struct AreTexturesResidentRequest {
    ContextTag context_tag{};
    std::vector<uint32_t> textures{};
  };

  struct AreTexturesResidentReply {
    uint16_t sequence{};
    Bool32 ret_val{};
    std::vector<uint8_t> data{};
  };

  using AreTexturesResidentResponse = Response<AreTexturesResidentReply>;

  Future<AreTexturesResidentReply> AreTexturesResident(
      const AreTexturesResidentRequest& request);

  Future<AreTexturesResidentReply> AreTexturesResident(
      const ContextTag& context_tag = {},
      const std::vector<uint32_t>& textures = {});

  struct DeleteTexturesRequest {
    ContextTag context_tag{};
    std::vector<uint32_t> textures{};
  };

  using DeleteTexturesResponse = Response<void>;

  Future<void> DeleteTextures(const DeleteTexturesRequest& request);

  Future<void> DeleteTextures(const ContextTag& context_tag = {},
                              const std::vector<uint32_t>& textures = {});

  struct GenTexturesRequest {
    ContextTag context_tag{};
    int32_t n{};
  };

  struct GenTexturesReply {
    uint16_t sequence{};
    std::vector<uint32_t> data{};
  };

  using GenTexturesResponse = Response<GenTexturesReply>;

  Future<GenTexturesReply> GenTextures(const GenTexturesRequest& request);

  Future<GenTexturesReply> GenTextures(const ContextTag& context_tag = {},
                                       const int32_t& n = {});

  struct IsTextureRequest {
    ContextTag context_tag{};
    uint32_t texture{};
  };

  struct IsTextureReply {
    uint16_t sequence{};
    Bool32 ret_val{};
  };

  using IsTextureResponse = Response<IsTextureReply>;

  Future<IsTextureReply> IsTexture(const IsTextureRequest& request);

  Future<IsTextureReply> IsTexture(const ContextTag& context_tag = {},
                                   const uint32_t& texture = {});

  struct GetColorTableRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t format{};
    uint32_t type{};
    uint8_t swap_bytes{};
  };

  struct GetColorTableReply {
    uint16_t sequence{};
    int32_t width{};
    std::vector<uint8_t> data{};
  };

  using GetColorTableResponse = Response<GetColorTableReply>;

  Future<GetColorTableReply> GetColorTable(const GetColorTableRequest& request);

  Future<GetColorTableReply> GetColorTable(const ContextTag& context_tag = {},
                                           const uint32_t& target = {},
                                           const uint32_t& format = {},
                                           const uint32_t& type = {},
                                           const uint8_t& swap_bytes = {});

  struct GetColorTableParameterfvRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t pname{};
  };

  struct GetColorTableParameterfvReply {
    uint16_t sequence{};
    float datum{};
    std::vector<float> data{};
  };

  using GetColorTableParameterfvResponse =
      Response<GetColorTableParameterfvReply>;

  Future<GetColorTableParameterfvReply> GetColorTableParameterfv(
      const GetColorTableParameterfvRequest& request);

  Future<GetColorTableParameterfvReply> GetColorTableParameterfv(
      const ContextTag& context_tag = {},
      const uint32_t& target = {},
      const uint32_t& pname = {});

  struct GetColorTableParameterivRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t pname{};
  };

  struct GetColorTableParameterivReply {
    uint16_t sequence{};
    int32_t datum{};
    std::vector<int32_t> data{};
  };

  using GetColorTableParameterivResponse =
      Response<GetColorTableParameterivReply>;

  Future<GetColorTableParameterivReply> GetColorTableParameteriv(
      const GetColorTableParameterivRequest& request);

  Future<GetColorTableParameterivReply> GetColorTableParameteriv(
      const ContextTag& context_tag = {},
      const uint32_t& target = {},
      const uint32_t& pname = {});

  struct GetConvolutionFilterRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t format{};
    uint32_t type{};
    uint8_t swap_bytes{};
  };

  struct GetConvolutionFilterReply {
    uint16_t sequence{};
    int32_t width{};
    int32_t height{};
    std::vector<uint8_t> data{};
  };

  using GetConvolutionFilterResponse = Response<GetConvolutionFilterReply>;

  Future<GetConvolutionFilterReply> GetConvolutionFilter(
      const GetConvolutionFilterRequest& request);

  Future<GetConvolutionFilterReply> GetConvolutionFilter(
      const ContextTag& context_tag = {},
      const uint32_t& target = {},
      const uint32_t& format = {},
      const uint32_t& type = {},
      const uint8_t& swap_bytes = {});

  struct GetConvolutionParameterfvRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t pname{};
  };

  struct GetConvolutionParameterfvReply {
    uint16_t sequence{};
    float datum{};
    std::vector<float> data{};
  };

  using GetConvolutionParameterfvResponse =
      Response<GetConvolutionParameterfvReply>;

  Future<GetConvolutionParameterfvReply> GetConvolutionParameterfv(
      const GetConvolutionParameterfvRequest& request);

  Future<GetConvolutionParameterfvReply> GetConvolutionParameterfv(
      const ContextTag& context_tag = {},
      const uint32_t& target = {},
      const uint32_t& pname = {});

  struct GetConvolutionParameterivRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t pname{};
  };

  struct GetConvolutionParameterivReply {
    uint16_t sequence{};
    int32_t datum{};
    std::vector<int32_t> data{};
  };

  using GetConvolutionParameterivResponse =
      Response<GetConvolutionParameterivReply>;

  Future<GetConvolutionParameterivReply> GetConvolutionParameteriv(
      const GetConvolutionParameterivRequest& request);

  Future<GetConvolutionParameterivReply> GetConvolutionParameteriv(
      const ContextTag& context_tag = {},
      const uint32_t& target = {},
      const uint32_t& pname = {});

  struct GetSeparableFilterRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t format{};
    uint32_t type{};
    uint8_t swap_bytes{};
  };

  struct GetSeparableFilterReply {
    uint16_t sequence{};
    int32_t row_w{};
    int32_t col_h{};
    std::vector<uint8_t> rows_and_cols{};
  };

  using GetSeparableFilterResponse = Response<GetSeparableFilterReply>;

  Future<GetSeparableFilterReply> GetSeparableFilter(
      const GetSeparableFilterRequest& request);

  Future<GetSeparableFilterReply> GetSeparableFilter(
      const ContextTag& context_tag = {},
      const uint32_t& target = {},
      const uint32_t& format = {},
      const uint32_t& type = {},
      const uint8_t& swap_bytes = {});

  struct GetHistogramRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t format{};
    uint32_t type{};
    uint8_t swap_bytes{};
    uint8_t reset{};
  };

  struct GetHistogramReply {
    uint16_t sequence{};
    int32_t width{};
    std::vector<uint8_t> data{};
  };

  using GetHistogramResponse = Response<GetHistogramReply>;

  Future<GetHistogramReply> GetHistogram(const GetHistogramRequest& request);

  Future<GetHistogramReply> GetHistogram(const ContextTag& context_tag = {},
                                         const uint32_t& target = {},
                                         const uint32_t& format = {},
                                         const uint32_t& type = {},
                                         const uint8_t& swap_bytes = {},
                                         const uint8_t& reset = {});

  struct GetHistogramParameterfvRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t pname{};
  };

  struct GetHistogramParameterfvReply {
    uint16_t sequence{};
    float datum{};
    std::vector<float> data{};
  };

  using GetHistogramParameterfvResponse =
      Response<GetHistogramParameterfvReply>;

  Future<GetHistogramParameterfvReply> GetHistogramParameterfv(
      const GetHistogramParameterfvRequest& request);

  Future<GetHistogramParameterfvReply> GetHistogramParameterfv(
      const ContextTag& context_tag = {},
      const uint32_t& target = {},
      const uint32_t& pname = {});

  struct GetHistogramParameterivRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t pname{};
  };

  struct GetHistogramParameterivReply {
    uint16_t sequence{};
    int32_t datum{};
    std::vector<int32_t> data{};
  };

  using GetHistogramParameterivResponse =
      Response<GetHistogramParameterivReply>;

  Future<GetHistogramParameterivReply> GetHistogramParameteriv(
      const GetHistogramParameterivRequest& request);

  Future<GetHistogramParameterivReply> GetHistogramParameteriv(
      const ContextTag& context_tag = {},
      const uint32_t& target = {},
      const uint32_t& pname = {});

  struct GetMinmaxRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t format{};
    uint32_t type{};
    uint8_t swap_bytes{};
    uint8_t reset{};
  };

  struct GetMinmaxReply {
    uint16_t sequence{};
    std::vector<uint8_t> data{};
  };

  using GetMinmaxResponse = Response<GetMinmaxReply>;

  Future<GetMinmaxReply> GetMinmax(const GetMinmaxRequest& request);

  Future<GetMinmaxReply> GetMinmax(const ContextTag& context_tag = {},
                                   const uint32_t& target = {},
                                   const uint32_t& format = {},
                                   const uint32_t& type = {},
                                   const uint8_t& swap_bytes = {},
                                   const uint8_t& reset = {});

  struct GetMinmaxParameterfvRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t pname{};
  };

  struct GetMinmaxParameterfvReply {
    uint16_t sequence{};
    float datum{};
    std::vector<float> data{};
  };

  using GetMinmaxParameterfvResponse = Response<GetMinmaxParameterfvReply>;

  Future<GetMinmaxParameterfvReply> GetMinmaxParameterfv(
      const GetMinmaxParameterfvRequest& request);

  Future<GetMinmaxParameterfvReply> GetMinmaxParameterfv(
      const ContextTag& context_tag = {},
      const uint32_t& target = {},
      const uint32_t& pname = {});

  struct GetMinmaxParameterivRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t pname{};
  };

  struct GetMinmaxParameterivReply {
    uint16_t sequence{};
    int32_t datum{};
    std::vector<int32_t> data{};
  };

  using GetMinmaxParameterivResponse = Response<GetMinmaxParameterivReply>;

  Future<GetMinmaxParameterivReply> GetMinmaxParameteriv(
      const GetMinmaxParameterivRequest& request);

  Future<GetMinmaxParameterivReply> GetMinmaxParameteriv(
      const ContextTag& context_tag = {},
      const uint32_t& target = {},
      const uint32_t& pname = {});

  struct GetCompressedTexImageARBRequest {
    ContextTag context_tag{};
    uint32_t target{};
    int32_t level{};
  };

  struct GetCompressedTexImageARBReply {
    uint16_t sequence{};
    int32_t size{};
    std::vector<uint8_t> data{};
  };

  using GetCompressedTexImageARBResponse =
      Response<GetCompressedTexImageARBReply>;

  Future<GetCompressedTexImageARBReply> GetCompressedTexImageARB(
      const GetCompressedTexImageARBRequest& request);

  Future<GetCompressedTexImageARBReply> GetCompressedTexImageARB(
      const ContextTag& context_tag = {},
      const uint32_t& target = {},
      const int32_t& level = {});

  struct DeleteQueriesARBRequest {
    ContextTag context_tag{};
    std::vector<uint32_t> ids{};
  };

  using DeleteQueriesARBResponse = Response<void>;

  Future<void> DeleteQueriesARB(const DeleteQueriesARBRequest& request);

  Future<void> DeleteQueriesARB(const ContextTag& context_tag = {},
                                const std::vector<uint32_t>& ids = {});

  struct GenQueriesARBRequest {
    ContextTag context_tag{};
    int32_t n{};
  };

  struct GenQueriesARBReply {
    uint16_t sequence{};
    std::vector<uint32_t> data{};
  };

  using GenQueriesARBResponse = Response<GenQueriesARBReply>;

  Future<GenQueriesARBReply> GenQueriesARB(const GenQueriesARBRequest& request);

  Future<GenQueriesARBReply> GenQueriesARB(const ContextTag& context_tag = {},
                                           const int32_t& n = {});

  struct IsQueryARBRequest {
    ContextTag context_tag{};
    uint32_t id{};
  };

  struct IsQueryARBReply {
    uint16_t sequence{};
    Bool32 ret_val{};
  };

  using IsQueryARBResponse = Response<IsQueryARBReply>;

  Future<IsQueryARBReply> IsQueryARB(const IsQueryARBRequest& request);

  Future<IsQueryARBReply> IsQueryARB(const ContextTag& context_tag = {},
                                     const uint32_t& id = {});

  struct GetQueryivARBRequest {
    ContextTag context_tag{};
    uint32_t target{};
    uint32_t pname{};
  };

  struct GetQueryivARBReply {
    uint16_t sequence{};
    int32_t datum{};
    std::vector<int32_t> data{};
  };

  using GetQueryivARBResponse = Response<GetQueryivARBReply>;

  Future<GetQueryivARBReply> GetQueryivARB(const GetQueryivARBRequest& request);

  Future<GetQueryivARBReply> GetQueryivARB(const ContextTag& context_tag = {},
                                           const uint32_t& target = {},
                                           const uint32_t& pname = {});

  struct GetQueryObjectivARBRequest {
    ContextTag context_tag{};
    uint32_t id{};
    uint32_t pname{};
  };

  struct GetQueryObjectivARBReply {
    uint16_t sequence{};
    int32_t datum{};
    std::vector<int32_t> data{};
  };

  using GetQueryObjectivARBResponse = Response<GetQueryObjectivARBReply>;

  Future<GetQueryObjectivARBReply> GetQueryObjectivARB(
      const GetQueryObjectivARBRequest& request);

  Future<GetQueryObjectivARBReply> GetQueryObjectivARB(
      const ContextTag& context_tag = {},
      const uint32_t& id = {},
      const uint32_t& pname = {});

  struct GetQueryObjectuivARBRequest {
    ContextTag context_tag{};
    uint32_t id{};
    uint32_t pname{};
  };

  struct GetQueryObjectuivARBReply {
    uint16_t sequence{};
    uint32_t datum{};
    std::vector<uint32_t> data{};
  };

  using GetQueryObjectuivARBResponse = Response<GetQueryObjectuivARBReply>;

  Future<GetQueryObjectuivARBReply> GetQueryObjectuivARB(
      const GetQueryObjectuivARBRequest& request);

  Future<GetQueryObjectuivARBReply> GetQueryObjectuivARB(
      const ContextTag& context_tag = {},
      const uint32_t& id = {},
      const uint32_t& pname = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Glx::Pbcet operator|(x11::Glx::Pbcet l,
                                           x11::Glx::Pbcet r) {
  using T = std::underlying_type_t<x11::Glx::Pbcet>;
  return static_cast<x11::Glx::Pbcet>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Glx::Pbcet operator&(x11::Glx::Pbcet l,
                                           x11::Glx::Pbcet r) {
  using T = std::underlying_type_t<x11::Glx::Pbcet>;
  return static_cast<x11::Glx::Pbcet>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Glx::Pbcdt operator|(x11::Glx::Pbcdt l,
                                           x11::Glx::Pbcdt r) {
  using T = std::underlying_type_t<x11::Glx::Pbcdt>;
  return static_cast<x11::Glx::Pbcdt>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Glx::Pbcdt operator&(x11::Glx::Pbcdt l,
                                           x11::Glx::Pbcdt r) {
  using T = std::underlying_type_t<x11::Glx::Pbcdt>;
  return static_cast<x11::Glx::Pbcdt>(static_cast<T>(l) & static_cast<T>(r));
}

inline constexpr x11::Glx::GraphicsContextAttribute operator|(
    x11::Glx::GraphicsContextAttribute l,
    x11::Glx::GraphicsContextAttribute r) {
  using T = std::underlying_type_t<x11::Glx::GraphicsContextAttribute>;
  return static_cast<x11::Glx::GraphicsContextAttribute>(static_cast<T>(l) |
                                                         static_cast<T>(r));
}

inline constexpr x11::Glx::GraphicsContextAttribute operator&(
    x11::Glx::GraphicsContextAttribute l,
    x11::Glx::GraphicsContextAttribute r) {
  using T = std::underlying_type_t<x11::Glx::GraphicsContextAttribute>;
  return static_cast<x11::Glx::GraphicsContextAttribute>(static_cast<T>(l) &
                                                         static_cast<T>(r));
}

inline constexpr x11::Glx::Rm operator|(x11::Glx::Rm l, x11::Glx::Rm r) {
  using T = std::underlying_type_t<x11::Glx::Rm>;
  return static_cast<x11::Glx::Rm>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::Glx::Rm operator&(x11::Glx::Rm l, x11::Glx::Rm r) {
  using T = std::underlying_type_t<x11::Glx::Rm>;
  return static_cast<x11::Glx::Rm>(static_cast<T>(l) & static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_GLX_H_
