// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include "base/command_line.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"

#if defined(USE_OZONE)
#include "ui/base/clipboard/clipboard_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(USE_X11)
#include "ui/base/clipboard/clipboard_x11.h"
#endif

namespace ui {

// Clipboard factory method.
// TODO(crbug.com/1096425): Cleanup when non-Ozone path gets dropped.
Clipboard* Clipboard::Create() {
  // On Linux Desktop builds ozone usage depends on UseOzonePlatform feature.
  // For all the other Ozone builds, this is set to true.
  bool use_ozone_impl = features::IsUsingOzonePlatform();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Use platform-backed implementation iff --use-system-clipbboard is passed.
  use_ozone_impl &= base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseSystemClipboard);
#endif

#if defined(USE_X11)
  // Use X11 implementation unless UseOzonePlatform feature is enabled.
  if (!use_ozone_impl)
    return new ClipboardX11;
#endif

#if defined(USE_OZONE)
  if (use_ozone_impl && OzonePlatform::GetInstance()->GetPlatformClipboard())
    return new ClipboardOzone;
#endif

#if defined(USE_X11) && BUILDFLAG(IS_CHROMEOS_LACROS)
  NOTREACHED() << "System clipboard integration should be in place.";
#endif
  return new ClipboardNonBacked;
}

}  // namespace ui
