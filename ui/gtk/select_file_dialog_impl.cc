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
#include "base/notreached.h"
#include "base/threading/thread_restrictions.h"
#include "ui/gtk/select_file_dialog_impl_portal.h"

namespace {

enum FileDialogChoice { kUnknown, kGtk, kKde, kPortal };

FileDialogChoice dialog_choice_ = kUnknown;

std::string& KDialogVersion() {
  static base::NoDestructor<std::string> version;
  return *version;
}

}  // namespace

namespace gtk {

base::FilePath* SelectFileDialogImpl::last_saved_path_ = nullptr;
base::FilePath* SelectFileDialogImpl::last_opened_path_ = nullptr;

// static
void SelectFileDialogImpl::Initialize() {
  SelectFileDialogImplPortal::StartAvailabilityTestInBackground();
}

// static
void SelectFileDialogImpl::Shutdown() {
  SelectFileDialogImplPortal::DestroyPortalConnection();
}

// static
ui::SelectFileDialog* SelectFileDialogImpl::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  if (dialog_choice_ == kUnknown) {
    // Start out assumimg we are going to use GTK.
    dialog_choice_ = kGtk;

    // Check to see if the portal is available.
    if (SelectFileDialogImplPortal::IsPortalAvailable()) {
      dialog_choice_ = kPortal;
    } else {
      // Make sure to kill the portal connection.
      SelectFileDialogImplPortal::DestroyPortalConnection();

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
            dialog_choice_ = kKde;
          }
        }
      }
    }
  }

  switch (dialog_choice_) {
    case kGtk:
      return SelectFileDialogImpl::NewSelectFileDialogImplGTK(
          listener, std::move(policy));
    case kPortal:
      return SelectFileDialogImpl::NewSelectFileDialogImplPortal(
          listener, std::move(policy));
    case kKde: {
      std::unique_ptr<base::Environment> env(base::Environment::Create());
      base::nix::DesktopEnvironment desktop =
          base::nix::GetDesktopEnvironment(env.get());
      return SelectFileDialogImpl::NewSelectFileDialogImplKDE(
          listener, std::move(policy), desktop, KDialogVersion());
    }
    case kUnknown:
      NOTREACHED();
      return nullptr;
  }
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
