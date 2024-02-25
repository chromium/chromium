// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/win/keyboard_hook_win_base.h"

#include "base/logging.h"

namespace ui {

KeyboardHookWinBase::KeyboardHookWinBase(
    std::optional<base::flat_set<DomCode>> dom_codes,
    KeyEventCallback callback,
    bool enable_hook_registration)
    : KeyboardHookBase(std::move(dom_codes), std::move(callback)),
      enable_hook_registration_(enable_hook_registration) {}

KeyboardHookWinBase::~KeyboardHookWinBase() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!enable_hook_registration_)
    return;

  if (!UnhookWindowsHookEx(hook_))
    DPLOG(ERROR) << "UnhookWindowsHookEx failed";
}

bool KeyboardHookWinBase::Register(HOOKPROC hook_proc) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If the hook was created for testing, |Register()| should not be called.
  DCHECK(enable_hook_registration_);

  DCHECK(!hook_);

  // Don't register hooks when there is a debugger to avoid painful user input
  // delays.
  if (IsDebuggerPresent())
    return false;

  // Per MSDN this Hook procedure will be called in the context of the thread
  // which installed it.
  hook_ = SetWindowsHookEx(WH_KEYBOARD_LL, hook_proc,
                           /*hMod=*/nullptr,
                           /*dwThreadId=*/0);
  DPLOG_IF(ERROR, !hook_) << "SetWindowsHookEx failed";

  return hook_ != nullptr;
}

// static
LRESULT CALLBACK
KeyboardHookWinBase::ProcessKeyEvent(KeyboardHookWinBase* instance,
                                     int code,
                                     WPARAM w_param,
                                     LPARAM l_param) {
  // If there is an error unhooking, this method could be called with a null
  // |instance_|.  Ensure we have a valid instance and that |code| is correct
  // before proceeding.
  if (!instance || code != HC_ACTION)
    return CallNextHookEx(nullptr, code, w_param, l_param);

  DCHECK_CALLED_ON_VALID_THREAD(instance->thread_checker_);

  KBDLLHOOKSTRUCT* ll_hooks = reinterpret_cast<KBDLLHOOKSTRUCT*>(l_param);

  // This vkey represents both a vkey and a location on the keyboard such as
  // VK_LCONTROL or VK_RCONTROL.
  DWORD vk = ll_hooks->vkCode;

  // Apply the extended flag prior to passing |scan_code| since |instance_| does
  // not have access to the low-level hook flags.
  DWORD scan_code = ll_hooks->scanCode;
  if (ll_hooks->flags & LLKHF_EXTENDED)
    scan_code |= 0xE000;

  if (instance->ProcessKeyEventMessage(w_param, vk, scan_code, ll_hooks->time))
    return 1;

  return CallNextHookEx(nullptr, code, w_param, l_param);
}

}  // namespace ui
