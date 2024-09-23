// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/win/keyboard_hook_win_base.h"

#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/win/events_win_utils.h"

namespace ui {

namespace {

bool IsMediaKey(DWORD vk) {
  return vk == VK_MEDIA_NEXT_TRACK || vk == VK_MEDIA_PREV_TRACK ||
         vk == VK_MEDIA_PLAY_PAUSE || vk == VK_MEDIA_STOP;
}

class MediaKeyboardHookWinImpl : public KeyboardHookWinBase {
 public:
  MediaKeyboardHookWinImpl(KeyEventCallback callback,
                           bool enable_hook_registration);

  MediaKeyboardHookWinImpl(const MediaKeyboardHookWinImpl&) = delete;
  MediaKeyboardHookWinImpl& operator=(const MediaKeyboardHookWinImpl&) = delete;

  ~MediaKeyboardHookWinImpl() override;

  // KeyboardHookWinBase implementation.
  bool ProcessKeyEventMessage(WPARAM w_param,
                              DWORD vk,
                              DWORD scan_code,
                              DWORD time_stamp) override;

  bool Register();

 private:
  static LRESULT CALLBACK ProcessKeyEvent(int code,
                                          WPARAM w_param,
                                          LPARAM l_param);

  static MediaKeyboardHookWinImpl* instance_;

  // Tracks the last non-located key down seen in order to determine if the
  // current key event should be marked as a repeated key press.
  DWORD last_key_down_ = 0;
};

// static
MediaKeyboardHookWinImpl* MediaKeyboardHookWinImpl::instance_ = nullptr;

MediaKeyboardHookWinImpl::MediaKeyboardHookWinImpl(
    KeyEventCallback callback,
    bool enable_hook_registration)
    : KeyboardHookWinBase(
          std::optional<base::flat_set<DomCode>>(
              {DomCode::MEDIA_PLAY_PAUSE, DomCode::MEDIA_STOP,
               DomCode::MEDIA_TRACK_NEXT, DomCode::MEDIA_TRACK_PREVIOUS}),
          std::move(callback),
          enable_hook_registration) {}

MediaKeyboardHookWinImpl::~MediaKeyboardHookWinImpl() {
  if (!enable_hook_registration())
    return;

  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

bool MediaKeyboardHookWinImpl::Register() {
  // Only one instance of this class can be registered at a time.
  DCHECK(!instance_);
  instance_ = this;

  return KeyboardHookWinBase::Register(
      reinterpret_cast<HOOKPROC>(&MediaKeyboardHookWinImpl::ProcessKeyEvent));
}

// static
LRESULT CALLBACK MediaKeyboardHookWinImpl::ProcessKeyEvent(int code,
                                                           WPARAM w_param,
                                                           LPARAM l_param) {
  return KeyboardHookWinBase::ProcessKeyEvent(instance_, code, w_param,
                                              l_param);
}

bool MediaKeyboardHookWinImpl::ProcessKeyEventMessage(WPARAM w_param,
                                                      DWORD vk,
                                                      DWORD scan_code,
                                                      DWORD time_stamp) {
  if (!IsMediaKey(vk))
    return false;

  bool is_repeat = false;
  CHROME_MSG msg = {nullptr, static_cast<UINT>(w_param), vk,
                    GetLParamFromScanCode(scan_code), time_stamp};
  EventType event_type = EventTypeFromMSG(msg);
  if (event_type == EventType::kKeyPressed) {
    is_repeat = (last_key_down_ == vk);
    last_key_down_ = vk;
  } else {
    DCHECK_EQ(event_type, EventType::kKeyReleased);
    last_key_down_ = 0;
  }

  std::unique_ptr<KeyEvent> key_event =
      std::make_unique<KeyEvent>(KeyEventFromMSG(msg));
  if (is_repeat)
    key_event->SetFlags(key_event->flags() | EF_IS_REPEAT);
  ForwardCapturedKeyEvent(key_event.get());

  // If the event is handled, don't propagate to the OS.
  return key_event->handled();
}

}  // namespace

// static
std::unique_ptr<KeyboardHook> KeyboardHook::CreateMediaKeyboardHook(
    KeyEventCallback callback) {
  std::unique_ptr<MediaKeyboardHookWinImpl> keyboard_hook =
      std::make_unique<MediaKeyboardHookWinImpl>(
          std::move(callback),
          /*enable_hook_registration=*/true);

  if (!keyboard_hook->Register())
    return nullptr;

  return keyboard_hook;
}

std::unique_ptr<KeyboardHookWinBase>
KeyboardHookWinBase::CreateMediaKeyboardHookForTesting(
    KeyEventCallback callback) {
  return std::make_unique<MediaKeyboardHookWinImpl>(
      std::move(callback),
      /*enable_hook_registration=*/false);
}

}  // namespace ui
