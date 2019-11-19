// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_LINUX_INPUT_METHOD_AURALINUX_H_
#define UI_BASE_IME_LINUX_INPUT_METHOD_AURALINUX_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/linux/linux_input_method_context.h"

namespace ui {

// A ui::InputMethod implementation for Aura on Linux platforms. The
// implementation details are separated to ui::LinuxInputMethodContext
// interface.
class COMPONENT_EXPORT(UI_BASE_IME_LINUX) InputMethodAuraLinux
    : public InputMethodBase,
      public LinuxInputMethodContextDelegate {
 public:
  explicit InputMethodAuraLinux(internal::InputMethodDelegate* delegate);
  ~InputMethodAuraLinux() override;

  LinuxInputMethodContext* GetContextForTesting(bool is_simple);

  // Overriden from InputMethod.
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) override;
  void OnTextInputTypeChanged(const TextInputClient* client) override;
  void OnCaretBoundsChanged(const TextInputClient* client) override;
  void CancelComposition(const TextInputClient* client) override;
  bool IsCandidatePopupOpen() const override;

  // Overriden from ui::LinuxInputMethodContextDelegate
  void OnCommit(const base::string16& text) override;
  void OnDeleteSurroundingText(int32_t index, uint32_t length) override;
  void OnPreeditChanged(const CompositionText& composition_text) override;
  void OnPreeditEnd() override;
  void OnPreeditStart() override {}

 protected:
  // Overridden from InputMethodBase.
  void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                 TextInputClient* focused) override;
  void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                TextInputClient* focused) override;
  void ConfirmCompositionText(bool reset_engine, bool keep_selection) override;

 private:
  bool HasInputMethodResult();
  bool NeedInsertChar() const;
  ui::EventDispatchDetails SendFakeProcessKeyEvent(ui::KeyEvent* event) const
      WARN_UNUSED_RESULT;
  void UpdateContextFocusState();
  void ResetContext();
  bool IgnoringNonKeyInput() const;

  // Processes the key event after the event is processed by the system IME or
  // the extension.
  ui::EventDispatchDetails ProcessKeyEventDone(ui::KeyEvent* event,
                                               bool filtered,
                                               bool is_handled)
      WARN_UNUSED_RESULT;

  // Callback function for IMEEngineHandlerInterface::ProcessKeyEvent().
  // It recovers the context when the event is being passed to the extension and
  // call ProcessKeyEventDone() for the following processing. This is necessary
  // as this method is async. The environment may be changed by other generated
  // key events by the time the callback is run.
  void ProcessKeyEventByEngineDone(ui::KeyEvent* event,
                                   bool filtered,
                                   bool composition_changed,
                                   ui::CompositionText* composition,
                                   base::string16* result_text,
                                   bool is_handled);

  std::unique_ptr<LinuxInputMethodContext> context_;
  std::unique_ptr<LinuxInputMethodContext> context_simple_;

  base::string16 result_text_;

  ui::CompositionText composition_;

  // The current text input type used to indicates if |context_| and
  // |context_simple_| are focused or not.
  TextInputType text_input_type_;

  // Indicates if currently in sync mode when handling a key event.
  // This is used in OnXXX callbacks from GTK IM module.
  bool is_sync_mode_;

  // Indicates if the composition text is changed or deleted.
  bool composition_changed_;

  // Ignore commit/preedit-changed/preedit-end signals if this time is still in
  // the future.
  base::TimeTicks suppress_non_key_input_until_ = base::TimeTicks::UnixEpoch();

  // Used for making callbacks.
  base::WeakPtrFactory<InputMethodAuraLinux> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InputMethodAuraLinux);
};

}  // namespace ui

#endif  // UI_BASE_IME_LINUX_INPUT_METHOD_AURALINUX_H_
