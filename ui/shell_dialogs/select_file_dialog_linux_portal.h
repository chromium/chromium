// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LINUX_PORTAL_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LINUX_PORTAL_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/sequenced_task_runner.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
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

  // A wrapper over some shared contextual information that needs to be passed
  // around between SelectFileDialogLinuxPortal and the Portal via D-Bus.
  // This is ref-counted due to sharing between the 2 sequences: dbus sequence
  // and main sequence.
  // Usage: SelectFileDialogLinuxPortal instantiates DialogInfo with the 2
  // callbacks, sets the public members and then call SelectFileImplOnDbus().
  // DialogInfo notifies the end result via one of the callbacks.
  class DialogInfo : public base::RefCountedThreadSafe<DialogInfo> {
   public:
    DialogInfo(base::OnceClosure created_callback,
               OnSelectFileExecutedCallback selected_callback,
               OnSelectFileCanceledCallback canceled_callback);

    // Sets up listeners for the response handle's signals.
    void SelectFileImplOnBusThread(std::u16string title,
                                   base::FilePath default_path,
                                   const bool default_path_exists,
                                   PortalFilterSet filter_set,
                                   base::FilePath::StringType default_extension,
                                   std::string parent_handle);
    Type type;
    // The task runner the SelectFileImpl method was called on.
    scoped_refptr<base::SequencedTaskRunner> main_task_runner;

   private:
    friend class base::RefCountedThreadSafe<DialogInfo>;
    ~DialogInfo();

    // Should run on D-Bus thread.
    void ConnectToHandle();
    void OnCallResponse(dbus::Bus* bus,
                        dbus::Response* response,
                        dbus::ErrorResponse* error_response);
    void OnResponseSignalEmitted(dbus::Signal* signal);
    bool CheckResponseCode(dbus::MessageReader* reader);
    bool ReadResponseResults(dbus::MessageReader* reader,
                             std::vector<std::string>* uris,
                             std::string* current_filter);
    void OnResponseSignalConnected(const std::string& interface,
                                   const std::string& signal,
                                   bool connected);
    void AppendFiltersOption(dbus::MessageWriter* writer,
                             const std::vector<PortalFilter>& filters);
    void AppendOptions(dbus::MessageWriter* writer,
                       const std::string& response_handle_token,
                       const base::FilePath& default_path,
                       const bool derfault_path_exists,
                       const PortalFilterSet& filter_set);
    void AppendFilterStruct(dbus::MessageWriter* writer,
                            const PortalFilter& filter);
    std::vector<base::FilePath> ConvertUrisToPaths(
        const std::vector<std::string>& uris);

    // Completes an open call, notifying the listener with the given paths, and
    // marks the dialog as closed.
    void CompleteOpen(std::vector<base::FilePath> paths,
                      std::string current_filter);
    // Completes an open call, notifying the listener with a cancellation, and
    // marks the dialog as closed.
    void CancelOpen();

    // These callbacks should run on main thread.
    // It will point to SelectFileDialogPortal::DialogCreatedOnMainThread.
    base::OnceClosure created_callback_;
    // It will point to SelectFileDialogPortal::CompleteOpenOnMainThread.
    OnSelectFileExecutedCallback selected_callback_;
    // It will point to SelectFileDialogPortal::CancelOpenOnMainThread.
    OnSelectFileCanceledCallback canceled_callback_;

    // The response object handle that the portal will send a signal to upon the
    // dialog's completion.
    raw_ptr<dbus::ObjectProxy, DanglingUntriaged> response_handle_ = nullptr;

    // `response_handle_` owns callbacks with methods bound to `this`.  To
    // prevent leaking, the callbacks are bound with weak references to `this`.
    base::WeakPtrFactory<DialogInfo> weak_factory_{this};
  };

  // D-Bus configuration and initialization.
  static void CheckPortalAvailabilityOnBusThread();
  static bool IsPortalRunningOnBusThread(dbus::ObjectProxy* dbus_proxy);
  static bool IsPortalActivatableOnBusThread(dbus::ObjectProxy* dbus_proxy);

  // Returns a flag, written by the D-Bus thread and read by the UI thread,
  // indicating whether or not the availability test has completed.
  static base::AtomicFlag* GetAvailabilityTestCompletionFlag();

  PortalFilterSet BuildFilterSet();

  void SelectFileImplWithParentHandle(
      std::u16string title,
      base::FilePath default_path,
      PortalFilterSet filter_set,
      base::FilePath::StringType default_extension,
      std::string parent_handle);

  void DialogCreatedOnMainThread();
  void CompleteOpenOnMainThread(std::vector<base::FilePath> paths,
                                std::string current_filter);
  void CancelOpenOnMainThread();

  // Removes the DialogInfo parent. Must be called on the UI task runner.
  void UnparentOnMainThread();

  // This should be used in the main thread.
  base::WeakPtr<aura::WindowTreeHost> host_;

  // Data shared across main thread and D-Bus thread.
  scoped_refptr<DialogInfo> info_;

  // Written by the D-Bus thread and read by the UI thread.
  static bool is_portal_available_;

  // Used by the D-Bus thread to generate unique handle tokens.
  static int handle_token_counter_;

  std::vector<PortalFilter> filters_;

  // Event handling on the parent window is disabled while the dialog is active
  // to make the dialog modal.  This closure should be run when the dialog is
  // closed to reenable event handling.
  base::OnceClosure reenable_window_event_handling_;

  // `DialogInfo` keeps callbacks to methods bound to `this`, and `this` keeps
  // a strong reference to `DialogInfo`.  To prevent a reference cycle, the
  // `DialogInfo` is instead passed callbacks that weakly reference `this`.
  base::WeakPtrFactory<SelectFileDialogLinuxPortal> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LINUX_PORTAL_H_
