// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_DLP_CONTROLLER_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_DLP_CONTROLLER_H_

#include "base/component_export.h"
#include "ui/base/clipboard/clipboard_data_endpoint.h"

namespace ui {

// The Clipboard Data Leak Prevention controller is used to control clipboard
// read operations. It should allow/disallow clipboard data read given the
// source of the data and the destination trying to access the data and a set of
// rules controlling these source and destination.
class COMPONENT_EXPORT(UI_BASE_CLIPBOARD) ClipboardDlpController {
 public:
  // Returns a pointer to the existing instance of the class.
  static ClipboardDlpController* Get();

  // Deletes the existing instance of the class if it's already created.
  // Indicates that restricting clipboard content is no longer required.
  static void DeleteInstance();

  virtual bool IsDataReadAllowed(
      const ClipboardDataEndpoint* const data_src,
      const ClipboardDataEndpoint* const data_dst) const = 0;

 protected:
  ClipboardDlpController();
  virtual ~ClipboardDlpController();

 private:
  // A singleton of ClipboardDlpController. Equals nullptr when there's not any
  // clipboard restrictions required.
  static ClipboardDlpController* g_clipboard_dlp_controller_;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_DLP_CONTROLLER_H_
