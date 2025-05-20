// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_V3_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_V3_H_

#include <text-input-unstable-v3-client-protocol.h>

#include <cstdint>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

struct SpanStyle;
class WaylandConnection;

class ZwpTextInputV3Client {
 public:
  virtual ~ZwpTextInputV3Client() = default;

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
};

class ZwpTextInputV3 {
 public:
  virtual ~ZwpTextInputV3() = default;

  virtual void SetClient(ZwpTextInputV3Client* context) = 0;
  virtual void OnClientDestroyed(ZwpTextInputV3Client* context) = 0;
  virtual void Enable() = 0;
  virtual void Disable() = 0;
  virtual void Reset() = 0;
  virtual void SetCursorRect(const gfx::Rect& rect) = 0;
  virtual void SetSurroundingText(const std::string& text,
                                  const gfx::Range& preedit_range,
                                  const gfx::Range& selection_range) = 0;
  virtual void SetContentType(TextInputType type,
                              uint32_t flags,
                              bool should_do_learning) = 0;
};

// Represents a zwp_text_input_v3 object.
class ZwpTextInputV3Impl : public ZwpTextInputV3 {
 public:
  ZwpTextInputV3Impl(WaylandConnection* connection,
                     zwp_text_input_manager_v3* text_input_manager);
  ZwpTextInputV3Impl(const ZwpTextInputV3Impl&) = delete;
  ZwpTextInputV3Impl& operator=(const ZwpTextInputV3Impl&) = delete;
  ~ZwpTextInputV3Impl() override;

  void SetClient(ZwpTextInputV3Client* context) override;
  void OnClientDestroyed(ZwpTextInputV3Client* context) override;
  void Enable() override;
  void Disable() override;
  void Reset() override;

  void SetCursorRect(const gfx::Rect& rect) override;
  void SetSurroundingText(const std::string& text,
                          const gfx::Range& preedit_range,
                          const gfx::Range& selection_range) override;
  void SetContentType(TextInputType type,
                      uint32_t flags,
                      bool should_do_learning) override;

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

  // Text input state received from IME, to be applied on done event.
  struct InputEvents {
    InputEvents();
    ~InputEvents();
    std::optional<PreeditData> preedit;
    std::optional<std::string> commit;
    uint32_t last_done_serial = 0;
  };

  // Data sent to IME.
  struct ImeData {
    ImeData();
    ~ImeData();
    void Reset();
    std::unique_ptr<gfx::Rect> cursor_rect;
    std::unique_ptr<ContentType> content_type;
    std::unique_ptr<SetSurroundingTextData> surrounding_text;
    // Only used when committed.
    uint32_t commit_count = 0;
  };

  bool DoneSerialEqualsCommitCount();

  bool SendCursorRect();
  bool SendContentType();
  bool SendSurroundingText();

  void SendPendingImeData();

  void ResetPendingImeData();
  void ResetCommittedImeData();
  void ResetInputEventsState();

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
  raw_ptr<ZwpTextInputV3Client> client_;

  // Input events state that will be applied in done event.
  InputEvents pending_input_events_;

  // Pending data to be sent to IME.
  ImeData pending_ime_data_;

  // Data that was last sent to IME and committed.
  ImeData committed_ime_data_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_TEXT_INPUT_V3_H_
