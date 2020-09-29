// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_DATA_ENDPOINT_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_DATA_ENDPOINT_H_

#include "base/optional.h"
#include "base/stl_util.h"
#include "url/origin.h"

namespace ui {

// EndpointType can represent either the source of the clipboard data or the
// destination trying to read the clipboard data.
// Whenever a new format is supported, a new enum should be added.
enum class EndpointType {
#if defined(OS_CHROMEOS) || (OS_LINUX) || (OS_FUCHSIA)
  kGuestOs = 0,  // Guest OS: PluginVM, Crostini.
#endif           // defined(OS_CHROMEOS) || (OS_LINUX) || (OS_FUCHSIA)
#if defined(OS_CHROMEOS)
  kArc = 1,               // ARC.
#endif                    // defined(OS_CHROMEOS)
  kUrl = 2,               // Website URL e.g. www.example.com.
  kClipboardHistory = 3,  // Clipboard History UI has privileged access to any
                          // clipboard data.
  kMaxValue = kClipboardHistory
};

// ClipboardDataEndpoint can represent:
// - The source of the data in the clipboard.
// - The destination trying to access the clipboard data.
class COMPONENT_EXPORT(UI_BASE_CLIPBOARD) ClipboardDataEndpoint {
 public:
  explicit ClipboardDataEndpoint(const url::Origin& origin);
  // This constructor shouldn't be used if |type| == EndpointType::kUrl.
  explicit ClipboardDataEndpoint(EndpointType type);

  ClipboardDataEndpoint(const ClipboardDataEndpoint& other);
  ClipboardDataEndpoint(ClipboardDataEndpoint&& other);

  ClipboardDataEndpoint& operator=(const ClipboardDataEndpoint& other) = delete;
  ClipboardDataEndpoint& operator=(ClipboardDataEndpoint&& other) = delete;

  bool operator==(const ClipboardDataEndpoint& other) const;

  ~ClipboardDataEndpoint();

  bool IsUrlType() const { return type_ == EndpointType::kUrl; }

  const url::Origin* origin() const { return base::OptionalOrNullptr(origin_); }

  EndpointType type() const { return type_; }

 private:
  // This variable should always have a value representing the object type.
  const EndpointType type_;
  // The url::Origin of the data endpoint. It always has a value if |type_| ==
  // EndpointType::kUrl, otherwise it's empty.
  const base::Optional<url::Origin> origin_;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_DATA_ENDPOINT_H_
