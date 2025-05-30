// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_V1_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_V1_H_

#include <text-input-unstable-v1-client-protocol.h>

#include <cstdint>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/span_style.h"

namespace gfx {
class Rect;
class Range;
}  // namespace gfx

namespace ui {

class WaylandConnection;
class WaylandWindow;

// Client interface which handles wayland text input callbacks
class ZwpTextInputV1Client {
 public:
  virtual ~ZwpTextInputV1Client() = default;

  // Called when a new composing text (pre-edit) should be set around the
  // current cursor position. Any previously set composing text should
  // be removed.
  // Note that the preedit_cursor is byte-offset. It is the pre-edit cursor
  // position if the range is empty and selection otherwise.
  virtual void OnPreeditString(std::string_view text,
                               const std::vector<SpanStyle>& spans,
                               const gfx::Range& preedit_cursor) = 0;

  // Called when a complete input sequence has been entered.  The text to
  // commit could be either just a single character after a key press or the
  // result of some composing (pre-edit).
  virtual void OnCommitString(std::string_view text) = 0;

  // Called when the cursor position or selection should be modified. The new
  // cursor position is applied on the next OnCommitString. |index| and |anchor|
  // are measured in UTF-8 bytes.
  virtual void OnCursorPosition(int32_t index, int32_t anchor) = 0;

  // Called when client needs to delete all or part of the text surrounding
  // the cursor. |index| and |length| are expected to be a byte offset of |text|
  // passed via ZwpTextInputV1::SetSurroundingText.
  virtual void OnDeleteSurroundingText(int32_t index, uint32_t length) = 0;

  // Notify when a key event was sent. Key events should not be used
  // for normal text input operations, which should be done with
  // commit_string, delete_surrounding_text, etc.
  virtual void OnKeysym(uint32_t key,
                        uint32_t state,
                        uint32_t modifiers,
                        uint32_t time) = 0;

  // Called when the visibility state of the input panel changed.
  // There's no detailed spec of |state|, and no actual implementor except
  // components/exo is found in the world at this moment.
  // Thus, in ozone/wayland use the lowest bit as boolean
  // (visible=1/invisible=0), and ignore other bits for future compatibility.
  virtual void OnInputPanelState(uint32_t state) = 0;

  // Called when the modifiers map is updated.
  // Each element holds the XKB name represents a modifier, such as "Shift".
  // The position of the element represents the bit position of modifiers
  // on OnKeysym. E.g., if LSB of modifiers is set, modifiers_map[0] is
  // set, if (1 << 1) of modifiers is set, modifiers_map[1] is set, and so on.
  virtual void OnModifiersMap(std::vector<std::string> modifiers_map) = 0;
};

// A wrapper around different versions of wayland text input protocols.
// Wayland compositors support various different text input protocols which
// all from Chromium point of view provide the functionality needed by Chromium
// IME. This interface collects the functionality behind one wrapper API.
class ZwpTextInputV1 {
 public:
  virtual ~ZwpTextInputV1() = default;

  virtual void Reset() = 0;

  virtual void SetClient(ZwpTextInputV1Client* context) = 0;
  virtual void OnClientDestroyed(ZwpTextInputV1Client* context) = 0;
  virtual void Activate(WaylandWindow* window) = 0;
  virtual void Deactivate() = 0;

  virtual void ShowInputPanel() = 0;
  virtual void HideInputPanel() = 0;

  virtual void SetCursorRect(const gfx::Rect& rect) = 0;
  virtual void SetSurroundingText(const std::string& text,
                                  const gfx::Range& preedit_range,
                                  const gfx::Range& selection_range) = 0;
  virtual void SetContentType(TextInputType type,
                              uint32_t flags,
                              bool should_do_learning) = 0;
};

// Text input wrapper for text-input-unstable-v1
class ZwpTextInputV1Impl : public ZwpTextInputV1 {
 public:
  ZwpTextInputV1Impl(WaylandConnection* connection,
                     zwp_text_input_manager_v1* text_input_manager);
  ZwpTextInputV1Impl(const ZwpTextInputV1Impl&) = delete;
  ZwpTextInputV1Impl& operator=(const ZwpTextInputV1Impl&) = delete;
  ~ZwpTextInputV1Impl() override;

  void Reset() override;

  void SetClient(ZwpTextInputV1Client* context) override;
  void OnClientDestroyed(ZwpTextInputV1Client* context) override;
  void Activate(WaylandWindow* window) override;
  void Deactivate() override;

  void ShowInputPanel() override;
  void HideInputPanel() override;

  void SetCursorRect(const gfx::Rect& rect) override;
  void SetSurroundingText(const std::string& text,
                          const gfx::Range& preedit_range,
                          const gfx::Range& selection_range) override;
  void SetContentType(TextInputType type,
                      uint32_t flags,
                      bool should_do_learning) override;

 private:
  void ResetInputEventState();

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

  const raw_ptr<WaylandConnection> connection_;
  wl::Object<zwp_text_input_v1> obj_;
  raw_ptr<ZwpTextInputV1Client> client_;

  std::vector<SpanStyle> spans_;
  int32_t preedit_cursor_ = -1;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_V1_H_
