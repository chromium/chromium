// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/base_shell_dialog_win.h"

#include <algorithm>

#include "base/task/post_task.h"
#include "base/win/scoped_com_initializer.h"

namespace ui {

namespace {

// Creates a SingleThreadTaskRunner to run a shell dialog on. Each dialog
// requires its own dedicated single-threaded sequence otherwise in some
// situations where a singleton owns a single instance of this object we can
// have a situation where a modal dialog in one window blocks the appearance
// of a modal dialog in another.
scoped_refptr<base::SingleThreadTaskRunner> CreateDialogTaskRunner() {
  return CreateCOMSTATaskRunner(
      {base::ThreadPool(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
}

// Enables the window |owner|. Can only be run from the UI thread.
void SetOwnerEnabled(HWND owner, bool enabled) {
  if (IsWindow(owner))
    EnableWindow(owner, enabled);
}

}  // namespace

BaseShellDialogImpl::RunState::RunState() = default;
BaseShellDialogImpl::RunState::~RunState() = default;

// static
BaseShellDialogImpl::Owners BaseShellDialogImpl::owners_;
int BaseShellDialogImpl::instance_count_ = 0;

BaseShellDialogImpl::BaseShellDialogImpl() {
  ++instance_count_;
}

BaseShellDialogImpl::~BaseShellDialogImpl() {
  // All runs should be complete by the time this is called!
  if (--instance_count_ == 0)
    DCHECK(owners_.empty());
}

// static
void BaseShellDialogImpl::DisableOwner(HWND owner) {
  SetOwnerEnabled(owner, false);
}

std::unique_ptr<BaseShellDialogImpl::RunState> BaseShellDialogImpl::BeginRun(
    HWND owner) {
  // Cannot run a modal shell dialog if one is already running for this owner.
  DCHECK(!IsRunningDialogForOwner(owner));
  // The owner must be a top level window, otherwise we could end up with two
  // entries in our map for the same top level window.
  DCHECK(!owner || owner == GetAncestor(owner, GA_ROOT));
  auto run_state = std::make_unique<RunState>();
  run_state->dialog_task_runner = CreateDialogTaskRunner();
  run_state->owner = owner;
  if (owner) {
    owners_.insert(owner);
    DisableOwner(owner);
  }
  return run_state;
}

void BaseShellDialogImpl::EndRun(std::unique_ptr<RunState> run_state) {
  if (run_state->owner) {
    DCHECK(IsRunningDialogForOwner(run_state->owner));
    SetOwnerEnabled(run_state->owner, true);
    DCHECK(owners_.find(run_state->owner) != owners_.end());
    owners_.erase(run_state->owner);
  }
}

bool BaseShellDialogImpl::IsRunningDialogForOwner(HWND owner) const {
  return (owner && owners_.find(owner) != owners_.end());
}

}  // namespace ui
