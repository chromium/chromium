// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/fake_event_rewriter_ash_delegate.h"

#include "base/check_op.h"

namespace ui::test {

FakeEventRewriterAshDelegate::FakeEventRewriterAshDelegate() = default;
FakeEventRewriterAshDelegate::~FakeEventRewriterAshDelegate() = default;

void FakeEventRewriterAshDelegate::SetModifierRemapping(
    std::string_view pref_name,
    ui::mojom::ModifierKey value) {
  DCHECK_NE(ui::mojom::ModifierKey::kIsoLevel5ShiftMod3, value);
  modifier_remapping_.insert_or_assign(std::string(pref_name), value);
}

bool FakeEventRewriterAshDelegate::RewriteModifierKeys() {
  return !suppress_modifier_key_rewrites_;
}

void FakeEventRewriterAshDelegate::SuppressModifierKeyRewrites(
    bool should_suppress) {
  suppress_modifier_key_rewrites_ = should_suppress;
}

bool FakeEventRewriterAshDelegate::RewriteMetaTopRowKeyComboEvents(
    int device_id) const {
  return true;
}

void FakeEventRewriterAshDelegate::SuppressMetaTopRowKeyComboRewrites(
    bool should_suppress) {}

std::optional<mojom::ModifierKey>
FakeEventRewriterAshDelegate::GetKeyboardRemappedModifierValue(
    int device_id,
    ui::mojom::ModifierKey modifier_key,
    const std::string& pref_name) const {
  auto it = modifier_remapping_.find(pref_name);
  if (it == modifier_remapping_.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool FakeEventRewriterAshDelegate::TopRowKeysAreFunctionKeys(
    int device_id) const {
  return false;
}

bool FakeEventRewriterAshDelegate::IsExtensionCommandRegistered(
    KeyboardCode key_code,
    int flags) const {
  return false;
}

bool FakeEventRewriterAshDelegate::IsSearchKeyAcceleratorReserved() const {
  return false;
}

bool FakeEventRewriterAshDelegate::NotifyDeprecatedRightClickRewrite() {
  return false;
}

bool FakeEventRewriterAshDelegate::NotifyDeprecatedSixPackKeyRewrite(
    KeyboardCode key_code) {
  return false;
}

void FakeEventRewriterAshDelegate::RecordEventRemappedToRightClick(
    bool alt_based_right_click) {}

void FakeEventRewriterAshDelegate::RecordSixPackEventRewrite(
    KeyboardCode key_code,
    bool alt_based) {}

std::optional<ui::mojom::SimulateRightClickModifier>
FakeEventRewriterAshDelegate::GetRemapRightClickModifier(int device_id) {
  return std::nullopt;
}

std::optional<ui::mojom::SixPackShortcutModifier>
FakeEventRewriterAshDelegate::GetShortcutModifierForSixPackKey(
    int device_id,
    ui::KeyboardCode key_code) {
  return std::nullopt;
}

void FakeEventRewriterAshDelegate::NotifyRightClickRewriteBlockedBySetting(
    ui::mojom::SimulateRightClickModifier blocked_modifier,
    ui::mojom::SimulateRightClickModifier active_modifier) {}

void FakeEventRewriterAshDelegate::NotifySixPackRewriteBlockedBySetting(
    ui::KeyboardCode key_code,
    ui::mojom::SixPackShortcutModifier blocked_modifier,
    ui::mojom::SixPackShortcutModifier active_modifier,
    int device_id) {}

void FakeEventRewriterAshDelegate::NotifySixPackRewriteBlockedByFnKey(
    ui::KeyboardCode key_code,
    ui::mojom::SixPackShortcutModifier modifier) {}

void FakeEventRewriterAshDelegate::NotifyTopRowRewriteBlockedByFnKey() {}

std::optional<ui::mojom::ExtendedFkeysModifier>
FakeEventRewriterAshDelegate::GetExtendedFkeySetting(
    int device_id,
    ui::KeyboardCode key_code) {
  return std::nullopt;
}

}  // namespace ui::test
