// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MOJOM_CLIPBOARD_BLINK_MOJOM_TRAITS_H_
#define UI_BASE_MOJOM_CLIPBOARD_BLINK_MOJOM_TRAITS_H_

#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-shared.h"
#include "ui/base/clipboard/clipboard_buffer.h"

namespace mojo {

template <>
struct EnumTraits<blink::mojom::ClipboardBuffer, ui::ClipboardBuffer> {
  static blink::mojom::ClipboardBuffer ToMojom(ui::ClipboardBuffer buffer) {
    // We only convert ui::ClipboardBuffer to blink::mojom::ClipboardBuffer
    // in tests, and they use ui::ClipboardBuffer::kCopyPaste.
    DCHECK(buffer == ui::ClipboardBuffer::kCopyPaste);
    return blink::mojom::ClipboardBuffer::kStandard;
  }

  static ui::ClipboardBuffer FromMojom(blink::mojom::ClipboardBuffer buffer) {
    switch (buffer) {
      case blink::mojom::ClipboardBuffer::kStandard:
        return ui::ClipboardBuffer::kCopyPaste;
      case blink::mojom::ClipboardBuffer::kSelection:
        return ui::ClipboardBuffer::kSelection;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // UI_BASE_MOJOM_CLIPBOARD_BLINK_MOJOM_TRAITS_H_
