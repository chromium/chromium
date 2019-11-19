// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_V1_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_V1_H_

#include <text-input-unstable-v1-client-protocol.h>
#include <string>

#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper.h"

namespace gfx {
class Rect;
}

namespace ui {

class WaylandConnection;
class WaylandWindow;

class ZWPTextInputWrapperV1 : public ZWPTextInputWrapper {
 public:
  explicit ZWPTextInputWrapperV1(zwp_text_input_manager_v1* text_input_manager);
  ~ZWPTextInputWrapperV1() override;

  void Initialize(WaylandConnection* connection,
                  ZWPTextInputWrapperClient* client) override;

  void Reset() override;

  void Activate(WaylandWindow* window) override;
  void Deactivate() override;

  void ShowInputPanel() override;
  void HideInputPanel() override;

  void SetCursorRect(const gfx::Rect& rect) override;
  void SetSurroundingText(const base::string16& text,
                          const gfx::Range& selection_range) override;

 private:
  void ResetInputEventState();

  // zwp_text_input_v1_listener
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

  WaylandConnection* connection_ = nullptr;
  wl::Object<zwp_text_input_v1> obj_;
  ZWPTextInputWrapperClient* client_;

  int32_t preedit_cursor_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_V1_H_
