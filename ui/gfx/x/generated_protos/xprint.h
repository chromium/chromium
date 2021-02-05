// Copyright 2021 The Chromium Authors. All rights reserved.
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

#ifndef UI_GFX_X_GENERATED_PROTOS_XPRINT_H_
#define UI_GFX_X_GENERATED_PROTOS_XPRINT_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "ui/gfx/x/error.h"
#include "xproto.h"

namespace x11 {

class Connection;

template <typename Reply>
struct Response;

template <typename Reply>
class Future;

class COMPONENT_EXPORT(X11) XPrint {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 0;

  XPrint(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class String8 : char {};

  enum class PContext : uint32_t {};

  enum class GetDoc : int {
    Finished = 0,
    SecondConsumer = 1,
  };

  enum class EvMask : int {
    NoEventMask = 0,
    PrintMask = 1 << 0,
    AttributeMask = 1 << 1,
  };

  enum class Detail : int {
    StartJobNotify = 1,
    EndJobNotify = 2,
    StartDocNotify = 3,
    EndDocNotify = 4,
    StartPageNotify = 5,
    EndPageNotify = 6,
  };

  enum class Attr : int {
    JobAttr = 1,
    DocAttr = 2,
    PageAttr = 3,
    PrinterAttr = 4,
    ServerAttr = 5,
    MediumAttr = 6,
    SpoolerAttr = 7,
  };

  struct Printer {
    std::vector<String8> name{};
    std::vector<String8> description{};
  };

  struct NotifyEvent {
    static constexpr int type_id = 50;
    static constexpr uint8_t opcode = 0;
    bool send_event{};
    uint8_t detail{};
    uint16_t sequence{};
    PContext context{};
    uint8_t cancel{};

    x11::Window* GetWindow() { return nullptr; }
  };

  struct AttributNotifyEvent {
    static constexpr int type_id = 51;
    static constexpr uint8_t opcode = 1;
    bool send_event{};
    uint8_t detail{};
    uint16_t sequence{};
    PContext context{};

    x11::Window* GetWindow() { return nullptr; }
  };

  struct BadContextError : public x11::Error {
    uint16_t sequence{};

    std::string ToString() const override;
  };

  struct BadSequenceError : public x11::Error {
    uint16_t sequence{};

    std::string ToString() const override;
  };

  struct PrintQueryVersionRequest {};

  struct PrintQueryVersionReply {
    uint16_t sequence{};
    uint16_t major_version{};
    uint16_t minor_version{};
  };

  using PrintQueryVersionResponse = Response<PrintQueryVersionReply>;

  Future<PrintQueryVersionReply> PrintQueryVersion(
      const PrintQueryVersionRequest& request);

  struct PrintGetPrinterListRequest {
    std::vector<String8> printer_name{};
    std::vector<String8> locale{};
  };

  struct PrintGetPrinterListReply {
    uint16_t sequence{};
    std::vector<Printer> printers{};
  };

  using PrintGetPrinterListResponse = Response<PrintGetPrinterListReply>;

  Future<PrintGetPrinterListReply> PrintGetPrinterList(
      const PrintGetPrinterListRequest& request);

  struct PrintRehashPrinterListRequest {};

  using PrintRehashPrinterListResponse = Response<void>;

  Future<void> PrintRehashPrinterList(
      const PrintRehashPrinterListRequest& request);

  struct CreateContextRequest {
    uint32_t context_id{};
    std::vector<String8> printerName{};
    std::vector<String8> locale{};
  };

  using CreateContextResponse = Response<void>;

  Future<void> CreateContext(const CreateContextRequest& request);

  struct PrintSetContextRequest {
    uint32_t context{};
  };

  using PrintSetContextResponse = Response<void>;

  Future<void> PrintSetContext(const PrintSetContextRequest& request);

  struct PrintGetContextRequest {};

  struct PrintGetContextReply {
    uint16_t sequence{};
    uint32_t context{};
  };

  using PrintGetContextResponse = Response<PrintGetContextReply>;

  Future<PrintGetContextReply> PrintGetContext(
      const PrintGetContextRequest& request);

  struct PrintDestroyContextRequest {
    uint32_t context{};
  };

  using PrintDestroyContextResponse = Response<void>;

  Future<void> PrintDestroyContext(const PrintDestroyContextRequest& request);

  struct PrintGetScreenOfContextRequest {};

  struct PrintGetScreenOfContextReply {
    uint16_t sequence{};
    Window root{};
  };

  using PrintGetScreenOfContextResponse =
      Response<PrintGetScreenOfContextReply>;

  Future<PrintGetScreenOfContextReply> PrintGetScreenOfContext(
      const PrintGetScreenOfContextRequest& request);

  struct PrintStartJobRequest {
    uint8_t output_mode{};
  };

  using PrintStartJobResponse = Response<void>;

  Future<void> PrintStartJob(const PrintStartJobRequest& request);

  struct PrintEndJobRequest {
    uint8_t cancel{};
  };

  using PrintEndJobResponse = Response<void>;

  Future<void> PrintEndJob(const PrintEndJobRequest& request);

  struct PrintStartDocRequest {
    uint8_t driver_mode{};
  };

  using PrintStartDocResponse = Response<void>;

  Future<void> PrintStartDoc(const PrintStartDocRequest& request);

  struct PrintEndDocRequest {
    uint8_t cancel{};
  };

  using PrintEndDocResponse = Response<void>;

  Future<void> PrintEndDoc(const PrintEndDocRequest& request);

  struct PrintPutDocumentDataRequest {
    Drawable drawable{};
    std::vector<uint8_t> data{};
    std::vector<String8> doc_format{};
    std::vector<String8> options{};
  };

  using PrintPutDocumentDataResponse = Response<void>;

  Future<void> PrintPutDocumentData(const PrintPutDocumentDataRequest& request);

  struct PrintGetDocumentDataRequest {
    PContext context{};
    uint32_t max_bytes{};
  };

  struct PrintGetDocumentDataReply {
    uint16_t sequence{};
    uint32_t status_code{};
    uint32_t finished_flag{};
    std::vector<uint8_t> data{};
  };

  using PrintGetDocumentDataResponse = Response<PrintGetDocumentDataReply>;

  Future<PrintGetDocumentDataReply> PrintGetDocumentData(
      const PrintGetDocumentDataRequest& request);

  struct PrintStartPageRequest {
    Window window{};
  };

  using PrintStartPageResponse = Response<void>;

  Future<void> PrintStartPage(const PrintStartPageRequest& request);

  struct PrintEndPageRequest {
    uint8_t cancel{};
  };

  using PrintEndPageResponse = Response<void>;

  Future<void> PrintEndPage(const PrintEndPageRequest& request);

  struct PrintSelectInputRequest {
    PContext context{};
    uint32_t event_mask{};
  };

  using PrintSelectInputResponse = Response<void>;

  Future<void> PrintSelectInput(const PrintSelectInputRequest& request);

  struct PrintInputSelectedRequest {
    PContext context{};
  };

  struct PrintInputSelectedReply {
    uint16_t sequence{};
    uint32_t event_mask{};
    uint32_t all_events_mask{};
  };

  using PrintInputSelectedResponse = Response<PrintInputSelectedReply>;

  Future<PrintInputSelectedReply> PrintInputSelected(
      const PrintInputSelectedRequest& request);

  struct PrintGetAttributesRequest {
    PContext context{};
    uint8_t pool{};
  };

  struct PrintGetAttributesReply {
    uint16_t sequence{};
    std::vector<String8> attributes{};
  };

  using PrintGetAttributesResponse = Response<PrintGetAttributesReply>;

  Future<PrintGetAttributesReply> PrintGetAttributes(
      const PrintGetAttributesRequest& request);

  struct PrintGetOneAttributesRequest {
    PContext context{};
    uint8_t pool{};
    std::vector<String8> name{};
  };

  struct PrintGetOneAttributesReply {
    uint16_t sequence{};
    std::vector<String8> value{};
  };

  using PrintGetOneAttributesResponse = Response<PrintGetOneAttributesReply>;

  Future<PrintGetOneAttributesReply> PrintGetOneAttributes(
      const PrintGetOneAttributesRequest& request);

  struct PrintSetAttributesRequest {
    PContext context{};
    uint32_t stringLen{};
    uint8_t pool{};
    uint8_t rule{};
    std::vector<String8> attributes{};
  };

  using PrintSetAttributesResponse = Response<void>;

  Future<void> PrintSetAttributes(const PrintSetAttributesRequest& request);

  struct PrintGetPageDimensionsRequest {
    PContext context{};
  };

  struct PrintGetPageDimensionsReply {
    uint16_t sequence{};
    uint16_t width{};
    uint16_t height{};
    uint16_t offset_x{};
    uint16_t offset_y{};
    uint16_t reproducible_width{};
    uint16_t reproducible_height{};
  };

  using PrintGetPageDimensionsResponse = Response<PrintGetPageDimensionsReply>;

  Future<PrintGetPageDimensionsReply> PrintGetPageDimensions(
      const PrintGetPageDimensionsRequest& request);

  struct PrintQueryScreensRequest {};

  struct PrintQueryScreensReply {
    uint16_t sequence{};
    std::vector<Window> roots{};
  };

  using PrintQueryScreensResponse = Response<PrintQueryScreensReply>;

  Future<PrintQueryScreensReply> PrintQueryScreens(
      const PrintQueryScreensRequest& request);

  struct PrintSetImageResolutionRequest {
    PContext context{};
    uint16_t image_resolution{};
  };

  struct PrintSetImageResolutionReply {
    uint8_t status{};
    uint16_t sequence{};
    uint16_t previous_resolutions{};
  };

  using PrintSetImageResolutionResponse =
      Response<PrintSetImageResolutionReply>;

  Future<PrintSetImageResolutionReply> PrintSetImageResolution(
      const PrintSetImageResolutionRequest& request);

  struct PrintGetImageResolutionRequest {
    PContext context{};
  };

  struct PrintGetImageResolutionReply {
    uint16_t sequence{};
    uint16_t image_resolution{};
  };

  using PrintGetImageResolutionResponse =
      Response<PrintGetImageResolutionReply>;

  Future<PrintGetImageResolutionReply> PrintGetImageResolution(
      const PrintGetImageResolutionRequest& request);

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::XPrint::GetDoc operator|(x11::XPrint::GetDoc l,
                                               x11::XPrint::GetDoc r) {
  using T = std::underlying_type_t<x11::XPrint::GetDoc>;
  return static_cast<x11::XPrint::GetDoc>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::XPrint::GetDoc operator&(x11::XPrint::GetDoc l,
                                               x11::XPrint::GetDoc r) {
  using T = std::underlying_type_t<x11::XPrint::GetDoc>;
  return static_cast<x11::XPrint::GetDoc>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::XPrint::EvMask operator|(x11::XPrint::EvMask l,
                                               x11::XPrint::EvMask r) {
  using T = std::underlying_type_t<x11::XPrint::EvMask>;
  return static_cast<x11::XPrint::EvMask>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::XPrint::EvMask operator&(x11::XPrint::EvMask l,
                                               x11::XPrint::EvMask r) {
  using T = std::underlying_type_t<x11::XPrint::EvMask>;
  return static_cast<x11::XPrint::EvMask>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::XPrint::Detail operator|(x11::XPrint::Detail l,
                                               x11::XPrint::Detail r) {
  using T = std::underlying_type_t<x11::XPrint::Detail>;
  return static_cast<x11::XPrint::Detail>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::XPrint::Detail operator&(x11::XPrint::Detail l,
                                               x11::XPrint::Detail r) {
  using T = std::underlying_type_t<x11::XPrint::Detail>;
  return static_cast<x11::XPrint::Detail>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::XPrint::Attr operator|(x11::XPrint::Attr l,
                                             x11::XPrint::Attr r) {
  using T = std::underlying_type_t<x11::XPrint::Attr>;
  return static_cast<x11::XPrint::Attr>(static_cast<T>(l) | static_cast<T>(r));
}

inline constexpr x11::XPrint::Attr operator&(x11::XPrint::Attr l,
                                             x11::XPrint::Attr r) {
  using T = std::underlying_type_t<x11::XPrint::Attr>;
  return static_cast<x11::XPrint::Attr>(static_cast<T>(l) & static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_XPRINT_H_
