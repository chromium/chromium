// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_FAKE_EVENT_REWRITER_ASH_DELEGATE_H_
#define UI_EVENTS_ASH_FAKE_EVENT_REWRITER_ASH_DELEGATE_H_

#include <map>
#include <string>
#include <string_view>

#include "ui/events/ash/event_rewriter_ash.h"

namespace ui::test {

// Fake implementation of EventRewriterAsh::Delegate to be used by tests.
class FakeEventRewriterAshDelegate : public ui::EventRewriterAsh::Delegate {
 public:
  FakeEventRewriterAshDelegate();
  FakeEventRewriterAshDelegate(const FakeEventRewriterAshDelegate&) = delete;
  FakeEventRewriterAshDelegate& operator=(const FakeEventRewriterAshDelegate&) =
      delete;
  ~FakeEventRewriterAshDelegate() override;

  // Configures the modifier remapping as specified.
  void SetModifierRemapping(std::string_view pref_name,
                            ui::mojom::ModifierKey value);

  // ui::EventRewriterAsh::Delegate:
  bool RewriteModifierKeys() override;
  void SuppressModifierKeyRewrites(bool should_suppress) override;
  bool RewriteMetaTopRowKeyComboEvents(int device_id) const override;
  void SuppressMetaTopRowKeyComboRewrites(bool should_suppress) override;
  std::optional<ui::mojom::ModifierKey> GetKeyboardRemappedModifierValue(
      int device_id,
      ui::mojom::ModifierKey modifier_key,
      const std::string& pref_name) const override;
  bool TopRowKeysAreFunctionKeys(int device_id) const override;
  bool IsExtensionCommandRegistered(KeyboardCode key_code,
                                    int flags) const override;
  bool IsSearchKeyAcceleratorReserved() const override;
  bool NotifyDeprecatedRightClickRewrite() override;
  bool NotifyDeprecatedSixPackKeyRewrite(KeyboardCode key_code) override;
  void RecordEventRemappedToRightClick(bool alt_based_right_click) override;
  void RecordSixPackEventRewrite(KeyboardCode key_code,
                                 bool alt_based) override;
  std::optional<ui::mojom::SimulateRightClickModifier>
  GetRemapRightClickModifier(int device_id) override;
  std::optional<ui::mojom::SixPackShortcutModifier>
  GetShortcutModifierForSixPackKey(int device_id,
                                   ui::KeyboardCode key_code) override;
  void NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier blocked_modifier,
      ui::mojom::SimulateRightClickModifier active_modifier) override;
  void NotifySixPackRewriteBlockedBySetting(
      ui::KeyboardCode key_code,
      ui::mojom::SixPackShortcutModifier blocked_modifier,
      ui::mojom::SixPackShortcutModifier active_modifier,
      int device_id) override;
  void NotifySixPackRewriteBlockedByFnKey(
      ui::KeyboardCode key_code,
      ui::mojom::SixPackShortcutModifier modifier) override;
  void NotifyTopRowRewriteBlockedByFnKey() override;

  std::optional<ui::mojom::ExtendedFkeysModifier> GetExtendedFkeySetting(
      int device_id,
      ui::KeyboardCode key_code) override;

 private:
  bool suppress_modifier_key_rewrites_ = false;
  std::map<std::string, ui::mojom::ModifierKey> modifier_remapping_;
};

}  // namespace ui::test

#endif  // UI_EVENTS_ASH_FAKE_EVENT_REWRITER_ASH_DELEGATE_H_
