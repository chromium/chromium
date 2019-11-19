// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/media_keys_listener.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hidsystem/ev_keymap.h>

#include "base/containers/flat_set.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/system_media_controls_media_keys_listener.h"

namespace ui {

namespace {

// The media keys subtype. No official docs found, but widely known.
// http://lists.apple.com/archives/cocoa-dev/2007/Aug/msg00499.html
const int kSystemDefinedEventMediaKeysSubtype = 8;

KeyboardCode MediaKeyCodeToKeyboardCode(int key_code) {
  switch (key_code) {
    case NX_KEYTYPE_PLAY:
      return VKEY_MEDIA_PLAY_PAUSE;
    case NX_KEYTYPE_PREVIOUS:
    case NX_KEYTYPE_REWIND:
      return VKEY_MEDIA_PREV_TRACK;
    case NX_KEYTYPE_NEXT:
    case NX_KEYTYPE_FAST:
      return VKEY_MEDIA_NEXT_TRACK;
  }
  return VKEY_UNKNOWN;
}

class MediaKeysListenerImpl : public MediaKeysListener {
 public:
  MediaKeysListenerImpl(MediaKeysListener::Delegate* delegate, Scope scope);

  ~MediaKeysListenerImpl() override;

  // MediaKeysListener:
  bool StartWatchingMediaKey(KeyboardCode key_code) override;
  void StopWatchingMediaKey(KeyboardCode key_code) override;
  void SetIsMediaPlaying(bool is_playing) override {}

 private:
  // Callback on media key event.
  void OnMediaKeyEvent(KeyboardCode key_code);

  // The callback for when an event tap happens.
  static CGEventRef EventTapCallback(CGEventTapProxy proxy,
                                     CGEventType type,
                                     CGEventRef event,
                                     void* refcon);

  // Internal methods to create or remove the event tap.
  void StartEventTapIfNecessary();
  void StopEventTapIfNecessary();

  MediaKeysListener::Delegate* delegate_;
  const Scope scope_;
  // Event tap for intercepting mac media keys.
  CFMachPortRef event_tap_ = nullptr;
  CFRunLoopSourceRef event_tap_source_ = nullptr;
  base::flat_set<KeyboardCode> key_codes_;

  DISALLOW_COPY_AND_ASSIGN(MediaKeysListenerImpl);
};

MediaKeysListenerImpl::MediaKeysListenerImpl(
    MediaKeysListener::Delegate* delegate,
    Scope scope)
    : delegate_(delegate), scope_(scope) {
  CHECK_NE(delegate_, nullptr);
}

MediaKeysListenerImpl::~MediaKeysListenerImpl() {
  StopEventTapIfNecessary();
}

bool MediaKeysListenerImpl::StartWatchingMediaKey(KeyboardCode key_code) {
  key_codes_.insert(key_code);
  StartEventTapIfNecessary();
  return true;
}

void MediaKeysListenerImpl::StopWatchingMediaKey(KeyboardCode key_code) {
  key_codes_.erase(key_code);

  if (key_codes_.empty())
    StopEventTapIfNecessary();
}

void MediaKeysListenerImpl::StartEventTapIfNecessary() {
  // Make sure there's no existing event tap.
  if (event_tap_) {
    return;
  }
  DCHECK_EQ(event_tap_, nullptr);
  DCHECK_EQ(event_tap_source_, nullptr);

  // Add an event tap to intercept the system defined media key events.
  event_tap_ = CGEventTapCreate(
      kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
      CGEventMaskBit(NX_SYSDEFINED), EventTapCallback, this);
  if (event_tap_ == nullptr) {
    LOG(ERROR) << "Error: failed to create event tap.";
    return;
  }

  event_tap_source_ =
      CFMachPortCreateRunLoopSource(kCFAllocatorSystemDefault, event_tap_, 0);
  if (event_tap_source_ == nullptr) {
    LOG(ERROR) << "Error: failed to create new run loop source.";
    return;
  }

  CFRunLoopAddSource(CFRunLoopGetCurrent(), event_tap_source_,
                     kCFRunLoopCommonModes);
}

void MediaKeysListenerImpl::StopEventTapIfNecessary() {
  if (!event_tap_) {
    return;
  }
  CFRunLoopRemoveSource(CFRunLoopGetCurrent(), event_tap_source_,
                        kCFRunLoopCommonModes);
  // Ensure both event tap and source are initialized.
  DCHECK_NE(event_tap_, nullptr);
  DCHECK_NE(event_tap_source_, nullptr);

  // Invalidate the event tap.
  CFMachPortInvalidate(event_tap_);
  CFRelease(event_tap_);
  event_tap_ = nullptr;

  // Release the event tap source.
  CFRelease(event_tap_source_);
  event_tap_source_ = nullptr;
}

void MediaKeysListenerImpl::OnMediaKeyEvent(KeyboardCode key_code) {
  // Create an accelerator corresponding to the keyCode.
  const Accelerator accelerator(key_code, 0);
  return delegate_->OnMediaKeysAccelerator(accelerator);
}

// Processed events should propagate if they aren't handled by any listeners.
// For events that don't matter, this handler should return as quickly as
// possible.
// Returning event causes the event to propagate to other applications.
// Returning nullptr prevents the event from propagating.
// static
CGEventRef MediaKeysListenerImpl::EventTapCallback(CGEventTapProxy proxy,
                                                   CGEventType type,
                                                   CGEventRef event,
                                                   void* refcon) {
  MediaKeysListenerImpl* shortcut_listener =
      static_cast<MediaKeysListenerImpl*>(refcon);

  const bool is_active = [NSApp isActive];

  if (shortcut_listener->scope_ == Scope::kFocused && !is_active) {
    return event;
  }

  // Handle the timeout case by re-enabling the tap.
  if (type == kCGEventTapDisabledByTimeout) {
    CGEventTapEnable(shortcut_listener->event_tap_, true);
    return event;
  }

  // Convert the CGEvent to an NSEvent for access to the data1 field.
  NSEvent* ns_event = [NSEvent eventWithCGEvent:event];
  if (ns_event == nil) {
    return event;
  }

  // Ignore events that are not system defined media keys.
  if (type != NX_SYSDEFINED || [ns_event type] != NSSystemDefined ||
      [ns_event subtype] != kSystemDefinedEventMediaKeysSubtype) {
    return event;
  }

  NSInteger data1 = [ns_event data1];
  // Ignore media keys that aren't previous, next and play/pause.
  // Magical constants are from http://weblog.rogueamoeba.com/2007/09/29/
  int key_code = (data1 & 0xFFFF0000) >> 16;
  if (key_code != NX_KEYTYPE_PLAY && key_code != NX_KEYTYPE_NEXT &&
      key_code != NX_KEYTYPE_PREVIOUS && key_code != NX_KEYTYPE_FAST &&
      key_code != NX_KEYTYPE_REWIND) {
    return event;
  }

  int key_flags = data1 & 0x0000FFFF;
  bool is_key_pressed = ((key_flags & 0xFF00) >> 8) == 0xA;

  // If the key wasn't pressed (eg. was released), ignore this event.
  if (!is_key_pressed)
    return event;

  // If we don't care about the given key, ignore this event.
  const KeyboardCode ui_key_code = MediaKeyCodeToKeyboardCode(key_code);
  if (!shortcut_listener->key_codes_.contains(ui_key_code))
    return event;

  // Now we have a media key that we care about. Send it to the caller.
  shortcut_listener->OnMediaKeyEvent(ui_key_code);

  // Prevent event from proagating to other apps.
  return nullptr;
}

}  // namespace

std::unique_ptr<MediaKeysListener> MediaKeysListener::Create(
    MediaKeysListener::Delegate* delegate,
    MediaKeysListener::Scope scope) {
  // For Mac OS 10.12.2 or later, we want to use MPRemoteCommandCenter for
  // getting media keys globally if there is a RemoteCommandCenterDelegate
  // available.
  if (scope == Scope::kGlobal) {
    auto listener =
        std::make_unique<SystemMediaControlsMediaKeysListener>(delegate);
    if (listener->Initialize())
      return listener;
  }

  return std::make_unique<MediaKeysListenerImpl>(delegate, scope);
}

}  // namespace ui
