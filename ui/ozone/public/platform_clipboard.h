// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_CLIPBOARD_H_
#define UI_OZONE_PUBLIC_PLATFORM_CLIPBOARD_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted_memory.h"
#include "ui/base/clipboard/clipboard_buffer.h"

namespace ui {

// Allows Chrome controls and windows to exchange data with each other and other
// applications, e.g., to copy and paste.
//
// In environments that have multiple clipboards (like Linux X11 or OS X, see
// ui::ClipboardBuffer), the implementation should provide a separate data
// buffer for each system clipboard.  When data is requested or offered, the
// caller specifies which buffer to use by providing the |buffer| parameter.
class COMPONENT_EXPORT(OZONE_BASE) PlatformClipboard {
 public:
  virtual ~PlatformClipboard() {}

  // DataMap is a map from "mime type" to associated data, whereas
  // the data can be organized differently for each mime type.
  using Data = scoped_refptr<base::RefCountedBytes>;
  using DataMap = std::unordered_map<std::string, Data>;

  // Offers a given clipboard data 'data_map' to the host system clipboard.
  //
  // It is common that host clipboard implementations simply get offered
  // the set of mime types available for the data being shared. In such cases,
  // the actual clipboard data is only 'transferred' to the consuming
  // application asynchronously, upon an explicit request for data given a
  // specific mime type. This is the case of Wayland compositors and MacOS
  // (NSPasteboard), for example.
  //
  // The invoker assumes the Ozone implementation will not free |DataMap|
  // before |OfferDataClosure| is called.
  //
  // OfferDataClosure should be invoked when the host clipboard implementation
  // acknowledges that the "offer to clipboard" operation is performed.
  using OfferDataClosure = base::OnceCallback<void()>;
  virtual void OfferClipboardData(ClipboardBuffer buffer,
                                  const DataMap& data_map,
                                  OfferDataClosure callback) = 0;

  // Reads data from host system clipboard given mime type. The resulting data
  // is returned asynchronously through |callback|, whereas nullptr is returned
  // if the request fails for some reason (i.e: no data available, etc).
  using RequestDataClosure = base::OnceCallback<void(const Data& result)>;
  virtual void RequestClipboardData(ClipboardBuffer buffer,
                                    const std::string& mime_type,
                                    RequestDataClosure callback) = 0;

  // Gets the mime types of the data available for clipboard operations
  // in the host system clipboard.
  //
  // GetMimeTypesClosure is invoked when the mime types available for clipboard
  // operations are known.
  using GetMimeTypesClosure =
      base::OnceCallback<void(const std::vector<std::string>&)>;
  virtual void GetAvailableMimeTypes(ClipboardBuffer buffer,
                                     GetMimeTypesClosure callback) = 0;

  // Returns true if the current application writing data to the host clipboard
  // data is this one; false otherwise.
  //
  // It can be relevant to know this information in case the client wants to
  // caches the clipboard data, and wants to know if it is possible to use
  // the cached data in order to reply faster to read-clipboard operations.
  virtual bool IsSelectionOwner(ClipboardBuffer buffer) = 0;

  // ClipboardDataChangedCallback is used to notify the PlatformClipboard client
  // that the clipboard content for a given |buffer| has changed, so that it can
  // take the necessary actions, e.g: to update clipboard's sequence number,
  // notify observers, etc.
  using ClipboardDataChangedCallback =
      base::RepeatingCallback<void(ClipboardBuffer buffer)>;
  virtual void SetClipboardDataChangedCallback(
      ClipboardDataChangedCallback callback) = 0;

  // Returns whether the kSelection buffer is available.
  virtual bool IsSelectionBufferAvailable() const = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_CLIPBOARD_H_
