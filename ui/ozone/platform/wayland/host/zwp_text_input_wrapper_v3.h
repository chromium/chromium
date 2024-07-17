// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_V3_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_V3_H_

#include <text-input-unstable-v3-client-protocol.h>

#include <cstdint>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper.h"

namespace ui {

class WaylandConnection;
class WaylandWindow;

// Text input wrapper for text-input-unstable-v3
class ZWPTextInputWrapperV3 : public ZWPTextInputWrapper {
 public:
  ZWPTextInputWrapperV3(WaylandConnection* connection,
                        ZWPTextInputWrapperClient* client,
                        zwp_text_input_manager_v3* text_input_manager);
  ZWPTextInputWrapperV3(const ZWPTextInputWrapperV3&) = delete;
  ZWPTextInputWrapperV3& operator=(const ZWPTextInputWrapperV3&) = delete;
  ~ZWPTextInputWrapperV3() override;

  void Reset() override;

  void Activate(WaylandWindow* window,
                ui::TextInputClient::FocusReason reason) override;
  void Deactivate() override;

  void ShowInputPanel() override;
  void HideInputPanel() override;

  void SetCursorRect(const gfx::Rect& rect) override;
  void SetSurroundingText(const std::string& text,
                          const gfx::Range& preedit_range,
                          const gfx::Range& selection_range) override;
  bool HasAdvancedSurroundingTextSupport() const override;
  void SetSurroundingTextOffsetUtf16(uint32_t offset_utf16) override;
  void SetContentType(TextInputType type,
                      TextInputMode mode,
                      uint32_t flags,
                      bool should_do_learning,
                      bool can_compose_inline) override;
  void SetGrammarFragmentAtCursor(const ui::GrammarFragment& fragment) override;
  void SetAutocorrectInfo(const gfx::Range& autocorrect_range,
                          const gfx::Rect& autocorrect_bounds) override;

 private:
  // zwp_text_input_v3_listener
  static void OnEnter(void* data,
                      struct zwp_text_input_v3* text_input,
                      struct wl_surface* surface);
  static void OnLeave(void* data,
                      struct zwp_text_input_v3* text_input,
                      struct wl_surface* surface);
  static void OnPreeditString(void* data,
                              struct zwp_text_input_v3* text_input,
                              const char* text,
                              int32_t cursor_begin,
                              int32_t cursor_end);
  static void OnCommitString(void* data,
                             struct zwp_text_input_v3* text_input,
                             const char* text);
  static void OnDeleteSurroundingText(void* data,
                                      struct zwp_text_input_v3* text_input,
                                      uint32_t before_length,
                                      uint32_t after_length);
  static void OnDone(void* data,
                     struct zwp_text_input_v3* text_input,
                     uint32_t serial);

  const raw_ptr<WaylandConnection> connection_;
  wl::Object<zwp_text_input_v3> obj_;
  const raw_ptr<ZWPTextInputWrapperClient> client_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_V3_H_
