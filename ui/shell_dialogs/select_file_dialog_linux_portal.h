// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LINUX_PORTAL_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LINUX_PORTAL_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/dbus/properties/types.h"
#include "components/dbus/xdg/request.h"
#include "dbus/bus.h"
#include "ui/shell_dialogs/select_file_dialog_linux.h"

namespace ui {

using OnSelectFileExecutedCallback =
    base::OnceCallback<void(std::vector<base::FilePath> paths,
                            std::string current_filter)>;
using OnSelectFileCanceledCallback = base::OnceCallback<void()>;

// Implementation of SelectFileDialog that has the XDG file chooser portal show
// a platform-dependent file selection dialog. This acts as a modal dialog.
class SelectFileDialogLinuxPortal : public SelectFileDialogLinux {
 public:
  SelectFileDialogLinuxPortal(Listener* listener,
                              std::unique_ptr<ui::SelectFilePolicy> policy);

  SelectFileDialogLinuxPortal(const SelectFileDialogLinuxPortal& other) =
      delete;
  SelectFileDialogLinuxPortal& operator=(
      const SelectFileDialogLinuxPortal& other) = delete;

  // Starts running a test to check for the presence of the file chooser portal.
  // Must be called on the UI thread. This should only be called once,
  // preferably around program start.
  static void StartAvailabilityTestInBackground();

  // Checks if the file chooser portal is available. Logs a warning if the
  // availability test has not yet completed.
  static bool IsPortalAvailable();

 protected:
  ~SelectFileDialogLinuxPortal() override;

  // BaseShellDialog:
  bool IsRunning(gfx::NativeWindow parent_window) const override;

  // SelectFileDialog:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override;
  bool HasMultipleFileTypeChoicesImpl() override;

 private:
  // Glob-style patterns are indicated by 0, MIME types by 1. Patterns are
  // case-sensitive.
  using DbusFilterPattern = DbusStruct<DbusUint32, DbusString>;
  using DbusFilterPatterns = DbusArray<DbusFilterPattern>;
  // The first string is a user-visible name for the filter.
  using DbusFilter = DbusStruct<DbusString, DbusFilterPatterns>;
  using DbusFilters = DbusArray<DbusFilter>;

  // A named set of patterns used as a dialog filter.
  struct PortalFilter {
    PortalFilter();
    PortalFilter(const PortalFilter& other);
    PortalFilter(PortalFilter&& other);
    ~PortalFilter();

    PortalFilter& operator=(const PortalFilter& other) = default;
    PortalFilter& operator=(PortalFilter&& other) = default;

    std::string name;
    std::vector<std::string> patterns;
  };

  // A set of PortalFilters, potentially with a default.
  struct PortalFilterSet {
    PortalFilterSet();
    PortalFilterSet(const PortalFilterSet& other);
    PortalFilterSet(PortalFilterSet&& other);
    ~PortalFilterSet();

    PortalFilterSet& operator=(const PortalFilterSet& other) = default;
    PortalFilterSet& operator=(PortalFilterSet&& other) = default;

    std::vector<PortalFilter> filters;
    std::optional<PortalFilter> default_filter;
  };

  PortalFilterSet BuildFilterSet();

  void SelectFileImplWithParentHandle(
      std::u16string title,
      base::FilePath default_path,
      PortalFilterSet filter_set,
      base::FilePath::StringType default_extension,
      std::string parent_handle);

  void SelectFileImplOnMainThread(std::u16string title,
                                  base::FilePath default_path,
                                  const bool default_path_exists,
                                  PortalFilterSet filter_set,
                                  base::FilePath::StringType default_extension,
                                  std::string parent_handle);

  DbusDictionary BuildOptionsDictionary(const base::FilePath& default_path,
                                        bool default_path_exists,
                                        const PortalFilterSet& filter_set);

  DbusFilter MakeFilterStruct(const PortalFilter& filter);

  void MakeFileChooserRequest(const std::string& method,
                              const std::string& title,
                              DbusDictionary options,
                              std::string parent_handle);

  void OnFileChooserResponse(
      base::expected<DbusDictionary, dbus_xdg::ResponseError> results);

  void CompleteOpen(std::vector<base::FilePath> paths,
                    std::string current_filter);

  void CancelOpen();

  void DialogCreatedOnInvoker();
  void CompleteOpenOnInvoker(std::vector<base::FilePath> paths,
                             std::string current_filter);
  void CancelOpenOnInvoker();

  // Removes the DialogInfo parent.
  void UnparentOnInvoker();

  Type type_ = SELECT_NONE;

  // The task runner the SelectFileImpl method was called on.
  scoped_refptr<base::SequencedTaskRunner> invoker_task_runner_;

  // This should be used by the invoker task runner.
  base::WeakPtr<aura::WindowTreeHost> host_;

  std::vector<PortalFilter> filters_;

  // The Request representing an in-flight file chooser call, if any.
  // We keep this alive until the response arrives or until the dialog
  // is destroyed, whichever comes first.
  std::unique_ptr<dbus_xdg::Request> file_chooser_request_;

  // Event handling on the parent window is disabled while the dialog is active
  // to make the dialog modal.  This closure should be run when the dialog is
  // closed to reenable event handling.
  base::OnceClosure reenable_window_event_handling_;
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LINUX_PORTAL_H_
