// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/clipboard_ozone.h"
#include "ui/base/ui_base_switches.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

Clipboard* Clipboard::Create() {
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS builds (both on-device and linux-chromeos), always use
  // ClipboardNonBacked so that Ash clipboard features (e.g. ClipboardHistory)
  // function properly. On linux-chromeos with --use-system-clipboard,
  // ClipboardNonBacked bridges to Ozone's PlatformClipboard (e.g.
  // X11ClipboardOzone).
  auto* clipboard = new ClipboardNonBacked;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseSystemClipboard) &&
      OzonePlatform::IsInitialized()) {
    if (auto* platform_clipboard =
            OzonePlatform::GetInstance()->GetPlatformClipboard()) {
      clipboard->SetPlatformClipboard(platform_clipboard);
    }
  }
  return clipboard;
#else
  // On Linux Desktop, Ozone's Clipboard impl is always used.
  if (OzonePlatform::GetInstance()->GetPlatformClipboard()) {
    return new ClipboardOzone;
  }
  return new ClipboardNonBacked;
#endif
}

}  // namespace ui
