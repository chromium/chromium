// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LINUX_PORTAL_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LINUX_PORTAL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/sequenced_task_runner.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "ui/shell_dialogs/select_file_dialog_linux.h"

namespace ui {

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

  // Starts running a test to check for the presence of the file chooser portal
  // on the D-Bus task runner. This should only be called once, preferably
  // around program start.
  static void StartAvailabilityTestInBackground();

  // Checks if the file chooser portal is available. Blocks if the availability
  // test from above has not yet completed (which should generally not happen).
  static bool IsPortalAvailable();

  // Destroys the connection to the bus.
  static void DestroyPortalConnection();

 protected:
  ~SelectFileDialogLinuxPortal() override;

  // BaseShellDialog implementation:
  bool IsRunning(gfx::NativeWindow parent_window) const override;

  // SelectFileDialog implementation.
  // |params| is user data we pass back via the Listener interface.
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override;

  bool HasMultipleFileTypeChoicesImpl() override;

 private:
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
    absl::optional<PortalFilter> default_filter;
  };

  // A wrapper over some shared contextual information that needs to be passed
  // around between various handler functions. This is ref-counted due to some
  // of the locations its used in having slightly unclear or error-prone
  // lifetimes.
  struct DialogInfo : base::RefCountedThreadSafe<DialogInfo> {
    DialogInfo();

    // The response object handle that the portal will send a signal to upon the
    // dialog's completion.
    raw_ptr<dbus::ObjectProxy, DanglingUntriaged> response_handle = nullptr;
    absl::optional<gfx::AcceleratedWidget> parent;
    Type type;
    // The task runner the SelectFileImpl method was called on.
    scoped_refptr<base::SequencedTaskRunner> main_task_runner;
    // The untyped params to pass to the listener.
    raw_ptr<void> listener_params = nullptr;

   private:
    friend class base::RefCountedThreadSafe<DialogInfo>;

    ~DialogInfo();
  };

  static scoped_refptr<dbus::Bus>* AcquireBusStorageOnBusThread();
  static dbus::Bus* AcquireBusOnBusThread();

  static void DestroyBusOnBusThread();

  static void CheckPortalAvailabilityOnBusThread();

  static bool IsPortalRunningOnBusThread(dbus::ObjectProxy* dbus_proxy);
  static bool IsPortalActivatableOnBusThread(dbus::ObjectProxy* dbus_proxy);

  // Returns a flag, written by the D-Bus thread and read by the UI thread,
  // indicating whether or not the availability test has completed.
  static base::AtomicFlag* GetAvailabilityTestCompletionFlag();

  PortalFilterSet BuildFilterSet();

  void SelectFileImplWithParentHandle(
      scoped_refptr<DialogInfo> info,
      std::u16string title,
      base::FilePath default_path,
      PortalFilterSet filter_set,
      base::FilePath::StringType default_extension,
      std::string parent_handle);

  void SelectFileImplOnBusThread(scoped_refptr<DialogInfo> info,
                                 std::u16string title,
                                 base::FilePath default_path,
                                 PortalFilterSet filter_set,
                                 base::FilePath::StringType default_extension,
                                 std::string parent_handle);

  void AppendOptions(dbus::MessageWriter* writer,
                     Type type,
                     const std::string& response_handle_token,
                     const base::FilePath& default_path,
                     const PortalFilterSet& filter_set);
  void AppendFiltersOption(dbus::MessageWriter* writer,
                           const std::vector<PortalFilter>& filters);
  void AppendFilterStruct(dbus::MessageWriter* writer,
                          const PortalFilter& filter);

  // Sets up listeners for the response handle's signals.
  void ConnectToHandle(scoped_refptr<DialogInfo> info);

  // Completes an open call, notifying the listener with the given paths, and
  // marks the dialog as closed.
  void CompleteOpen(scoped_refptr<DialogInfo> info,
                    std::vector<base::FilePath> paths,
                    std::string current_filter);
  // Completes an open call, notifying the listener with a cancellation, and
  // marks the dialog as closed.
  void CancelOpen(scoped_refptr<DialogInfo> info);

  void CompleteOpenOnMainThread(scoped_refptr<DialogInfo> info,
                                std::vector<base::FilePath> paths,
                                std::string current_filter);
  void CancelOpenOnMainThread(scoped_refptr<DialogInfo> info);

  // Removes the DialogInfo parent. Must be called on the UI task runner.
  void UnparentOnMainThread(DialogInfo* info);

  void OnCallResponse(dbus::Bus* bus,
                      scoped_refptr<DialogInfo> info,
                      dbus::Response* response,
                      dbus::ErrorResponse* error_response);

  void OnResponseSignalConnected(scoped_refptr<DialogInfo> info,
                                 const std::string& interface,
                                 const std::string& signal,
                                 bool connected);

  void OnResponseSignalEmitted(scoped_refptr<DialogInfo> info,
                               dbus::Signal* signal);

  bool CheckResponseCode(dbus::MessageReader* reader);
  bool ReadResponseResults(dbus::MessageReader* reader,
                           std::vector<std::string>* uris,
                           std::string* current_filter);
  std::vector<base::FilePath> ConvertUrisToPaths(
      const std::vector<std::string>& uris);

  std::set<gfx::AcceleratedWidget> parents_;

  // Written by the D-Bus thread and read by the UI thread.
  static bool is_portal_available_;

  // Used by the D-Bus thread to generate unique handle tokens.
  static int handle_token_counter_;

  std::vector<PortalFilter> filters_;
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LINUX_PORTAL_H_
