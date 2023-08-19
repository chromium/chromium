// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/secure_password_input.h"

#import <Carbon/Carbon.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/check_op.h"

namespace {

// Used to protect from out-of-order calls to enabling/disabling functions.
int g_password_input_counter = 0;

// SetPasswordInputEnabled() is copied from
// enableSecureTextInput() and disableSecureTextInput() functions in
// third_party/WebKit/WebCore/platform/SecureTextInput.cpp
//
// The following technote describes proper EnableSecureEventInput() usage:
// https://developer.apple.com/library/archive/technotes/tn2150/_index.html
void SetPasswordInputEnabled(bool enabled) {
  if (enabled) {
    EnableSecureEventInput();

    CFArrayRef inputSources = TISCreateASCIICapableInputSourceList();
    TSMSetDocumentProperty(/*docID=*/nullptr,
                           kTSMDocumentEnabledInputSourcesPropertyTag,
                           sizeof(CFArrayRef), &inputSources);
    CFRelease(inputSources);
  } else {
    TSMRemoveDocumentProperty(/*docID=*/nullptr,
                              kTSMDocumentEnabledInputSourcesPropertyTag);

    DisableSecureEventInput();
  }
}

}  // namespace

namespace ui {

ScopedPasswordInputEnabler::ScopedPasswordInputEnabler() {
  if (!g_password_input_counter) {
    SetPasswordInputEnabled(true);
  }
  ++g_password_input_counter;
}

ScopedPasswordInputEnabler::~ScopedPasswordInputEnabler() {
  --g_password_input_counter;
  DCHECK_LE(0, g_password_input_counter);
  if (!g_password_input_counter) {
    SetPasswordInputEnabled(false);
  }
}

// static
bool ScopedPasswordInputEnabler::IsPasswordInputEnabled() {
  return g_password_input_counter > 0;
}

}  // namespace ui
