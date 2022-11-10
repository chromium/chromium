// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/emoji/emoji_panel_helper.h"

#include "chromeos/crosapi/mojom/emoji_picker.mojom.h"
#include "chromeos/lacros/lacros_service.h"

namespace ui {

bool IsEmojiPanelSupported() {
  return chromeos::LacrosService::Get()
      ->IsAvailable<crosapi::mojom::EmojiPicker>();
}

void ShowEmojiPanel() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::EmojiPicker>())
    lacros_service->GetRemote<crosapi::mojom::EmojiPicker>()->ShowEmojiPicker();
}

}  // namespace ui