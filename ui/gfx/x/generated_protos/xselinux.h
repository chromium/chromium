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

#ifndef UI_GFX_X_GENERATED_PROTOS_XSELINUX_H_
#define UI_GFX_X_GENERATED_PROTOS_XSELINUX_H_

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

class COMPONENT_EXPORT(X11) SELinux {
 public:
  static constexpr unsigned major_version = 1;
  static constexpr unsigned minor_version = 0;

  SELinux(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  struct ListItem {
    bool operator==(const ListItem& other) const {
      return name == other.name && object_context == other.object_context &&
             data_context == other.data_context;
    }

    Atom name{};
    std::string object_context{};
    std::string data_context{};
  };

  struct QueryVersionRequest {
    uint8_t client_major{};
    uint8_t client_minor{};
  };

  struct QueryVersionReply {
    uint16_t sequence{};
    uint16_t server_major{};
    uint16_t server_minor{};
  };

  using QueryVersionResponse = Response<QueryVersionReply>;

  Future<QueryVersionReply> QueryVersion(const QueryVersionRequest& request);

  Future<QueryVersionReply> QueryVersion(const uint8_t& client_major = {},
                                         const uint8_t& client_minor = {});

  struct SetDeviceCreateContextRequest {
    std::string context{};
  };

  using SetDeviceCreateContextResponse = Response<void>;

  Future<void> SetDeviceCreateContext(
      const SetDeviceCreateContextRequest& request);

  Future<void> SetDeviceCreateContext(const std::string& context = {});

  struct GetDeviceCreateContextRequest {};

  struct GetDeviceCreateContextReply {
    uint16_t sequence{};
    std::string context{};
  };

  using GetDeviceCreateContextResponse = Response<GetDeviceCreateContextReply>;

  Future<GetDeviceCreateContextReply> GetDeviceCreateContext(
      const GetDeviceCreateContextRequest& request);

  Future<GetDeviceCreateContextReply> GetDeviceCreateContext();

  struct SetDeviceContextRequest {
    uint32_t device{};
    std::string context{};
  };

  using SetDeviceContextResponse = Response<void>;

  Future<void> SetDeviceContext(const SetDeviceContextRequest& request);

  Future<void> SetDeviceContext(const uint32_t& device = {},
                                const std::string& context = {});

  struct GetDeviceContextRequest {
    uint32_t device{};
  };

  struct GetDeviceContextReply {
    uint16_t sequence{};
    std::string context{};
  };

  using GetDeviceContextResponse = Response<GetDeviceContextReply>;

  Future<GetDeviceContextReply> GetDeviceContext(
      const GetDeviceContextRequest& request);

  Future<GetDeviceContextReply> GetDeviceContext(const uint32_t& device = {});

  struct SetWindowCreateContextRequest {
    std::string context{};
  };

  using SetWindowCreateContextResponse = Response<void>;

  Future<void> SetWindowCreateContext(
      const SetWindowCreateContextRequest& request);

  Future<void> SetWindowCreateContext(const std::string& context = {});

  struct GetWindowCreateContextRequest {};

  struct GetWindowCreateContextReply {
    uint16_t sequence{};
    std::string context{};
  };

  using GetWindowCreateContextResponse = Response<GetWindowCreateContextReply>;

  Future<GetWindowCreateContextReply> GetWindowCreateContext(
      const GetWindowCreateContextRequest& request);

  Future<GetWindowCreateContextReply> GetWindowCreateContext();

  struct GetWindowContextRequest {
    Window window{};
  };

  struct GetWindowContextReply {
    uint16_t sequence{};
    std::string context{};
  };

  using GetWindowContextResponse = Response<GetWindowContextReply>;

  Future<GetWindowContextReply> GetWindowContext(
      const GetWindowContextRequest& request);

  Future<GetWindowContextReply> GetWindowContext(const Window& window = {});

  struct SetPropertyCreateContextRequest {
    std::string context{};
  };

  using SetPropertyCreateContextResponse = Response<void>;

  Future<void> SetPropertyCreateContext(
      const SetPropertyCreateContextRequest& request);

  Future<void> SetPropertyCreateContext(const std::string& context = {});

  struct GetPropertyCreateContextRequest {};

  struct GetPropertyCreateContextReply {
    uint16_t sequence{};
    std::string context{};
  };

  using GetPropertyCreateContextResponse =
      Response<GetPropertyCreateContextReply>;

  Future<GetPropertyCreateContextReply> GetPropertyCreateContext(
      const GetPropertyCreateContextRequest& request);

  Future<GetPropertyCreateContextReply> GetPropertyCreateContext();

  struct SetPropertyUseContextRequest {
    std::string context{};
  };

  using SetPropertyUseContextResponse = Response<void>;

  Future<void> SetPropertyUseContext(
      const SetPropertyUseContextRequest& request);

  Future<void> SetPropertyUseContext(const std::string& context = {});

  struct GetPropertyUseContextRequest {};

  struct GetPropertyUseContextReply {
    uint16_t sequence{};
    std::string context{};
  };

  using GetPropertyUseContextResponse = Response<GetPropertyUseContextReply>;

  Future<GetPropertyUseContextReply> GetPropertyUseContext(
      const GetPropertyUseContextRequest& request);

  Future<GetPropertyUseContextReply> GetPropertyUseContext();

  struct GetPropertyContextRequest {
    Window window{};
    Atom property{};
  };

  struct GetPropertyContextReply {
    uint16_t sequence{};
    std::string context{};
  };

  using GetPropertyContextResponse = Response<GetPropertyContextReply>;

  Future<GetPropertyContextReply> GetPropertyContext(
      const GetPropertyContextRequest& request);

  Future<GetPropertyContextReply> GetPropertyContext(const Window& window = {},
                                                     const Atom& property = {});

  struct GetPropertyDataContextRequest {
    Window window{};
    Atom property{};
  };

  struct GetPropertyDataContextReply {
    uint16_t sequence{};
    std::string context{};
  };

  using GetPropertyDataContextResponse = Response<GetPropertyDataContextReply>;

  Future<GetPropertyDataContextReply> GetPropertyDataContext(
      const GetPropertyDataContextRequest& request);

  Future<GetPropertyDataContextReply> GetPropertyDataContext(
      const Window& window = {},
      const Atom& property = {});

  struct ListPropertiesRequest {
    Window window{};
  };

  struct ListPropertiesReply {
    uint16_t sequence{};
    std::vector<ListItem> properties{};
  };

  using ListPropertiesResponse = Response<ListPropertiesReply>;

  Future<ListPropertiesReply> ListProperties(
      const ListPropertiesRequest& request);

  Future<ListPropertiesReply> ListProperties(const Window& window = {});

  struct SetSelectionCreateContextRequest {
    std::string context{};
  };

  using SetSelectionCreateContextResponse = Response<void>;

  Future<void> SetSelectionCreateContext(
      const SetSelectionCreateContextRequest& request);

  Future<void> SetSelectionCreateContext(const std::string& context = {});

  struct GetSelectionCreateContextRequest {};

  struct GetSelectionCreateContextReply {
    uint16_t sequence{};
    std::string context{};
  };

  using GetSelectionCreateContextResponse =
      Response<GetSelectionCreateContextReply>;

  Future<GetSelectionCreateContextReply> GetSelectionCreateContext(
      const GetSelectionCreateContextRequest& request);

  Future<GetSelectionCreateContextReply> GetSelectionCreateContext();

  struct SetSelectionUseContextRequest {
    std::string context{};
  };

  using SetSelectionUseContextResponse = Response<void>;

  Future<void> SetSelectionUseContext(
      const SetSelectionUseContextRequest& request);

  Future<void> SetSelectionUseContext(const std::string& context = {});

  struct GetSelectionUseContextRequest {};

  struct GetSelectionUseContextReply {
    uint16_t sequence{};
    std::string context{};
  };

  using GetSelectionUseContextResponse = Response<GetSelectionUseContextReply>;

  Future<GetSelectionUseContextReply> GetSelectionUseContext(
      const GetSelectionUseContextRequest& request);

  Future<GetSelectionUseContextReply> GetSelectionUseContext();

  struct GetSelectionContextRequest {
    Atom selection{};
  };

  struct GetSelectionContextReply {
    uint16_t sequence{};
    std::string context{};
  };

  using GetSelectionContextResponse = Response<GetSelectionContextReply>;

  Future<GetSelectionContextReply> GetSelectionContext(
      const GetSelectionContextRequest& request);

  Future<GetSelectionContextReply> GetSelectionContext(
      const Atom& selection = {});

  struct GetSelectionDataContextRequest {
    Atom selection{};
  };

  struct GetSelectionDataContextReply {
    uint16_t sequence{};
    std::string context{};
  };

  using GetSelectionDataContextResponse =
      Response<GetSelectionDataContextReply>;

  Future<GetSelectionDataContextReply> GetSelectionDataContext(
      const GetSelectionDataContextRequest& request);

  Future<GetSelectionDataContextReply> GetSelectionDataContext(
      const Atom& selection = {});

  struct ListSelectionsRequest {};

  struct ListSelectionsReply {
    uint16_t sequence{};
    std::vector<ListItem> selections{};
  };

  using ListSelectionsResponse = Response<ListSelectionsReply>;

  Future<ListSelectionsReply> ListSelections(
      const ListSelectionsRequest& request);

  Future<ListSelectionsReply> ListSelections();

  struct GetClientContextRequest {
    uint32_t resource{};
  };

  struct GetClientContextReply {
    uint16_t sequence{};
    std::string context{};
  };

  using GetClientContextResponse = Response<GetClientContextReply>;

  Future<GetClientContextReply> GetClientContext(
      const GetClientContextRequest& request);

  Future<GetClientContextReply> GetClientContext(const uint32_t& resource = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

#endif  // UI_GFX_X_GENERATED_PROTOS_XSELINUX_H_
