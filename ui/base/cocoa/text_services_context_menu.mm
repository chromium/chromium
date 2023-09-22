// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/text_services_context_menu.h"

#import <AppKit/AppKit.h>

#include <utility>

#include "base/strings/sys_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

// Returns the TextDirection associated associated with the given BiDi
// |command_id|.
base::i18n::TextDirection GetTextDirectionFromCommandId(int command_id) {
  switch (command_id) {
    case ui::TextServicesContextMenu::kWritingDirectionDefault:
      return base::i18n::UNKNOWN_DIRECTION;
    case ui::TextServicesContextMenu::kWritingDirectionLtr:
      return base::i18n::LEFT_TO_RIGHT;
    case ui::TextServicesContextMenu::kWritingDirectionRtl:
      return base::i18n::RIGHT_TO_LEFT;
    default:
      NOTREACHED();
      return base::i18n::UNKNOWN_DIRECTION;
  }
}

}  // namespace

namespace ui {

TextServicesContextMenu::TextServicesContextMenu(Delegate* delegate)
    : speech_submenu_model_(this),
      bidi_submenu_model_(this),
      delegate_(delegate) {
  DCHECK(delegate);

  speech_submenu_model_.AddItemWithStringId(kSpeechStartSpeaking,
                                            IDS_SPEECH_START_SPEAKING_MAC);
  speech_submenu_model_.AddItemWithStringId(kSpeechStopSpeaking,
                                            IDS_SPEECH_STOP_SPEAKING_MAC);

  bidi_submenu_model_.AddCheckItemWithStringId(
      kWritingDirectionDefault, IDS_CONTENT_CONTEXT_WRITING_DIRECTION_DEFAULT);
  bidi_submenu_model_.AddCheckItemWithStringId(
      kWritingDirectionLtr, IDS_CONTENT_CONTEXT_WRITING_DIRECTION_LTR);
  bidi_submenu_model_.AddCheckItemWithStringId(
      kWritingDirectionRtl, IDS_CONTENT_CONTEXT_WRITING_DIRECTION_RTL);
}

// A note about the Speech submenu.
//
// All standard AppKit implementations of `-(IBAction)startSpeaking:(id)sender`
// and `-(IBAction)stopSpeaking:(id)sender` funnel into messages to
// `NSApplication`:

}  // namespace ui

@interface NSApplication (Speech)
- (void)speakString:(NSString*)string;
- (IBAction)stopSpeaking:(id)sender;
- (BOOL)isSpeaking;
@end

namespace ui {

// We do this as well, for two reasons:
//
// 1. Interoperability with the other parts of the system that use this same
//    speech synthesizer.
//
// 2. Working around a bug in `AVSpeechSynthesizer` which does not provide the
//    correct voice when a specific voice is chosen in the system accessibility
//    settings (see https://crbug.com/1484940#c9, FB13197951).

void TextServicesContextMenu::SpeakText(const std::u16string& text) {
  [NSApp speakString:base::SysUTF16ToNSString(text)];
}

void TextServicesContextMenu::StopSpeaking() {
  [NSApp stopSpeaking:nil];
}

bool TextServicesContextMenu::IsSpeaking() {
  return [NSApp isSpeaking];
}

void TextServicesContextMenu::AppendToContextMenu(SimpleMenuModel* model) {
  model->AddSeparator(NORMAL_SEPARATOR);
  model->AddSubMenuWithStringId(kSpeechMenu, IDS_SPEECH_MAC,
                                &speech_submenu_model_);
}

void TextServicesContextMenu::AppendEditableItems(SimpleMenuModel* model) {
  // MacOS provides a contextual menu to set writing direction for BiDi
  // languages. This functionality is exposed as a keyboard shortcut on
  // Windows and Linux.
  model->AddSubMenuWithStringId(kWritingDirectionMenu,
                                IDS_CONTENT_CONTEXT_WRITING_DIRECTION_MENU,
                                &bidi_submenu_model_);
}

bool TextServicesContextMenu::SupportsCommand(int command_id) const {
  switch (command_id) {
    case kWritingDirectionMenu:
    case kWritingDirectionDefault:
    case kWritingDirectionLtr:
    case kWritingDirectionRtl:
    case kSpeechMenu:
    case kSpeechStartSpeaking:
    case kSpeechStopSpeaking:
      return true;
  }

  return false;
}

bool TextServicesContextMenu::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case kWritingDirectionDefault:
    case kWritingDirectionLtr:
    case kWritingDirectionRtl:
      return delegate_->IsTextDirectionChecked(
          GetTextDirectionFromCommandId(command_id));
    case kSpeechStartSpeaking:
    case kSpeechStopSpeaking:
      return false;
  }

  NOTREACHED();
  return false;
}

bool TextServicesContextMenu::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case kSpeechMenu:
    case kWritingDirectionMenu:
      return true;
    case kWritingDirectionDefault:
    case kWritingDirectionLtr:
    case kWritingDirectionRtl:
      return delegate_->IsTextDirectionEnabled(
          GetTextDirectionFromCommandId(command_id));
    case kSpeechStartSpeaking:
      return true;
    case kSpeechStopSpeaking:
      return IsSpeaking();
  }

  NOTREACHED();
  return false;
}

void TextServicesContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case kWritingDirectionDefault:
    case kWritingDirectionLtr:
    case kWritingDirectionRtl:
      delegate_->UpdateTextDirection(GetTextDirectionFromCommandId(command_id));
      break;
    case kSpeechStartSpeaking:
      SpeakText(delegate_->GetSelectedText());
      break;
    case kSpeechStopSpeaking:
      StopSpeaking();
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace ui
