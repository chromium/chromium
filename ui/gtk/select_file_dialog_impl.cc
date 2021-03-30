// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements common select dialog functionality between GTK and KDE.

#include "ui/gtk/select_file_dialog_impl.h"

#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/threading/thread_restrictions.h"

namespace {

enum UseKdeFileDialogStatus { UNKNOWN, NO_KDE, YES_KDE };

UseKdeFileDialogStatus use_kde_ = UNKNOWN;
std::string& KDialogVersion() {
  static base::NoDestructor<std::string> version;
  return *version;
}

}  // namespace

namespace gtk {

base::FilePath* SelectFileDialogImpl::last_saved_path_ = nullptr;
base::FilePath* SelectFileDialogImpl::last_opened_path_ = nullptr;

// static
ui::SelectFileDialog* SelectFileDialogImpl::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  if (use_kde_ == UNKNOWN) {
    // Start out assumimg we are not going to use KDE.
    use_kde_ = NO_KDE;

    // Check to see if KDE is the desktop environment.
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    base::nix::DesktopEnvironment desktop =
        base::nix::GetDesktopEnvironment(env.get());
    if (desktop == base::nix::DESKTOP_ENVIRONMENT_KDE3 ||
        desktop == base::nix::DESKTOP_ENVIRONMENT_KDE4 ||
        desktop == base::nix::DESKTOP_ENVIRONMENT_KDE5) {
      // Check to see if the user dislikes the KDE file dialog.
      if (!env->HasVar("NO_CHROME_KDE_FILE_DIALOG")) {
        // Check to see if the KDE dialog works.
        if (SelectFileDialogImpl::CheckKDEDialogWorksOnUIThread(
                KDialogVersion())) {
          use_kde_ = YES_KDE;
        }
      }
    }
  }

  if (use_kde_ == NO_KDE) {
    return SelectFileDialogImpl::NewSelectFileDialogImplGTK(listener,
                                                            std::move(policy));
  }

  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::nix::DesktopEnvironment desktop =
      base::nix::GetDesktopEnvironment(env.get());
  return SelectFileDialogImpl::NewSelectFileDialogImplKDE(
      listener, std::move(policy), desktop, KDialogVersion());
}

SelectFileDialogImpl::SelectFileDialogImpl(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : SelectFileDialog(listener, std::move(policy)),
      file_type_index_(0),
      type_(SELECT_NONE) {
  if (!last_saved_path_) {
    last_saved_path_ = new base::FilePath();
    last_opened_path_ = new base::FilePath();
  }
}

SelectFileDialogImpl::~SelectFileDialogImpl() = default;

void SelectFileDialogImpl::ListenerDestroyed() {
  listener_ = nullptr;
}

bool SelectFileDialogImpl::CallDirectoryExistsOnUIThread(
    const base::FilePath& path) {
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  return base::DirectoryExists(path);
}

}  // namespace gtk
