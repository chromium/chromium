// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/text_services_context_menu.h"

#include <utility>

#include <ApplicationServices/ApplicationServices.h>
#include <CoreAudio/CoreAudio.h>

#include "base/mac/mac_logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

// The speech channel used for speaking. This is shared to check if a speech
// channel is currently speaking.
SpeechChannel g_speech_channel;

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

void TextServicesContextMenu::SpeakText(const std::u16string& text) {
  if (IsSpeaking())
    StopSpeaking();

  if (!g_speech_channel) {
    OSErr result = NewSpeechChannel(nullptr, &g_speech_channel);
    OSSTATUS_DCHECK(result == noErr, result);
  }

  SpeakCFString(g_speech_channel, base::SysUTF16ToCFStringRef(text), nullptr);
}

void TextServicesContextMenu::StopSpeaking() {
  DCHECK(g_speech_channel);
  StopSpeechAt(g_speech_channel, kImmediate);
  DisposeSpeechChannel(g_speech_channel);
  g_speech_channel = nullptr;
}

bool TextServicesContextMenu::IsSpeaking() {
  return SpeechBusy();
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
