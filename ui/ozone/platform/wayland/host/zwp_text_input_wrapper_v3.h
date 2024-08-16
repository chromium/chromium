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
  struct ContentType {
    constexpr ContentType() = default;
    constexpr ContentType(uint32_t content_hint, uint32_t content_purpose)
        : content_hint(content_hint), content_purpose(content_purpose) {}
    bool operator==(const ContentType& other) const = default;
    uint32_t content_hint = ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE;
    uint32_t content_purpose = ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
  };
  struct SetSurroundingTextData {
    constexpr SetSurroundingTextData() = default;
    constexpr SetSurroundingTextData(std::string text,
                                     int32_t cursor,
                                     int32_t anchor)
        : text(std::move(text)), cursor(cursor), anchor(anchor) {}
    bool operator==(const SetSurroundingTextData&) const = default;
    std::string text;
    int32_t cursor = 0;
    int32_t anchor = 0;
  };
  struct PreeditData {
    constexpr PreeditData() = default;
    constexpr PreeditData(std::string text,
                          int32_t cursor_begin,
                          int32_t cursor_end)
        : text(std::move(text)),
          cursor_begin(cursor_begin),
          cursor_end(cursor_end) {}
    std::string text;
    int32_t cursor_begin = 0;
    int32_t cursor_end = 0;
  };

  void SendCursorRect(const gfx::Rect& rect);
  void SendContentType(const ContentType& content_type);
  void SendSurroundingText(const SetSurroundingTextData& data);
  void ApplyPendingSetRequests();
  void ResetPendingSetRequests();
  void ResetLastSentValues();
  void ResetPendingInputEvents();
  void Commit();

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
  uint32_t commit_count_ = 0;
  uint32_t last_done_serial_ = 0;

  // Pending input events that will be applied in done event.
  std::optional<PreeditData> pending_preedit_;
  std::optional<std::string> pending_commit_;

  // Pending set requests to be sent to compositor
  std::optional<gfx::Rect> pending_set_cursor_rect_;
  std::optional<ContentType> pending_set_content_type_;
  std::optional<SetSurroundingTextData> pending_set_surrounding_text_;

  // last sent values
  std::optional<gfx::Rect> last_sent_cursor_rect_;
  std::optional<ContentType> last_sent_content_type_;
  std::optional<SetSurroundingTextData> last_sent_surrounding_text_data_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_WRAPPER_V3_H_
