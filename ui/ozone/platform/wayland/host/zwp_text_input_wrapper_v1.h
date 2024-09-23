// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_V1_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_V1_H_

#include <cstdint>
#include <string>
#include <vector>

#include <text-input-extension-unstable-v1-client-protocol.h>
#include <text-input-unstable-v1-client-protocol.h>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper.h"

namespace gfx {
class Rect;
}

namespace ui {

class WaylandConnection;
class WaylandWindow;

// Text input wrapper for text-input-unstable-v1
class ZWPTextInputWrapperV1 : public ZWPTextInputWrapper {
 public:
  ZWPTextInputWrapperV1(WaylandConnection* connection,
                        ZWPTextInputWrapperClient* client,
                        zwp_text_input_manager_v1* text_input_manager,
                        zcr_text_input_extension_v1* text_input_extension);
  ZWPTextInputWrapperV1(const ZWPTextInputWrapperV1&) = delete;
  ZWPTextInputWrapperV1& operator=(const ZWPTextInputWrapperV1&) = delete;
  ~ZWPTextInputWrapperV1() override;

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
  void ResetInputEventState();
  void TryScheduleFinalizeVirtualKeyboardChanges();
  void FinalizeVirtualKeyboardChanges();
  bool SupportsFinalizeVirtualKeyboardChanges();

  // zwp_text_input_v1_listener callbacks:
  static void OnEnter(void* data,
                      struct zwp_text_input_v1* text_input,
                      struct wl_surface* surface);
  static void OnLeave(void* data, struct zwp_text_input_v1* text_input);
  static void OnModifiersMap(void* data,
                             struct zwp_text_input_v1* text_input,
                             struct wl_array* map);
  static void OnInputPanelState(void* data,
                                struct zwp_text_input_v1* text_input,
                                uint32_t state);
  static void OnPreeditString(void* data,
                              struct zwp_text_input_v1* text_input,
                              uint32_t serial,
                              const char* text,
                              const char* commit);
  static void OnPreeditStyling(void* data,
                               struct zwp_text_input_v1* text_input,
                               uint32_t index,
                               uint32_t length,
                               uint32_t style);
  static void OnPreeditCursor(void* data,
                              struct zwp_text_input_v1* text_input,
                              int32_t index);
  static void OnCommitString(void* data,
                             struct zwp_text_input_v1* text_input,
                             uint32_t serial,
                             const char* text);
  static void OnCursorPosition(void* data,
                               struct zwp_text_input_v1* text_input,
                               int32_t index,
                               int32_t anchor);
  static void OnDeleteSurroundingText(void* data,
                                      struct zwp_text_input_v1* text_input,
                                      int32_t index,
                                      uint32_t length);
  static void OnKeysym(void* data,
                       struct zwp_text_input_v1* text_input,
                       uint32_t serial,
                       uint32_t time,
                       uint32_t key,
                       uint32_t state,
                       uint32_t modifiers);
  static void OnLanguage(void* data,
                         struct zwp_text_input_v1* text_input,
                         uint32_t serial,
                         const char* language);
  static void OnTextDirection(void* data,
                              struct zwp_text_input_v1* text_input,
                              uint32_t serial,
                              uint32_t direction);

  // zcr_extended_text_input_v1_listener callbacks:
  static void OnSetPreeditRegion(
      void* data,
      struct zcr_extended_text_input_v1* extended_text_input,
      int32_t index,
      uint32_t length);
  static void OnClearGrammarFragments(
      void* data,
      struct zcr_extended_text_input_v1* extended_text_input,
      uint32_t start,
      uint32_t end);
  static void OnAddGrammarFragment(
      void* data,
      struct zcr_extended_text_input_v1* extended_text_input,
      uint32_t start,
      uint32_t end,
      const char* suggestion);
  static void OnSetAutocorrectRange(
      void* data,
      struct zcr_extended_text_input_v1* extended_text_input,
      uint32_t start,
      uint32_t end);
  static void OnSetVirtualKeyboardOccludedBounds(
      void* data,
      struct zcr_extended_text_input_v1* extended_text_input,
      int32_t x,
      int32_t y,
      int32_t width,
      int32_t height);
  static void OnConfirmPreedit(
      void* data,
      struct zcr_extended_text_input_v1* extended_text_input,
      uint32_t selection_behavior);
  static void OnInsertImage(
      void* data,
      struct zcr_extended_text_input_v1* extended_text_input,
      const char* src);
  static void OnInsertImageWithLargeURL(
      void* data,
      struct zcr_extended_text_input_v1* extended_text_input,
      const char* mime_type,
      const char* charset,
      const int32_t raw_fd,
      const uint32_t size);

  const raw_ptr<WaylandConnection> connection_;
  wl::Object<zwp_text_input_v1> obj_;
  wl::Object<zcr_extended_text_input_v1> extended_obj_;
  const raw_ptr<ZWPTextInputWrapperClient> client_;

  std::vector<ZWPTextInputWrapperClient::SpanStyle> spans_;
  int32_t preedit_cursor_ = -1;

  // Timer for sending the finalize_virtual_keyboard_changes request.
  base::OneShotTimer send_vk_finalize_timer_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_V1_H_
