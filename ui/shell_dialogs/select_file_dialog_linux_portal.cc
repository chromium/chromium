// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/shell_dialogs/select_file_dialog_linux_portal.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/linux/linux_ui_delegate.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace ui {

namespace {

constexpr char kDBusMethodNameHasOwner[] = "NameHasOwner";
constexpr char kDBusMethodListActivatableNames[] = "ListActivatableNames";
constexpr char kMethodStartServiceByName[] = "StartServiceByName";

constexpr char kXdgPortalService[] = "org.freedesktop.portal.Desktop";
constexpr char kXdgPortalObject[] = "/org/freedesktop/portal/desktop";

constexpr int kXdgPortalRequiredVersion = 3;

constexpr char kXdgPortalRequestInterfaceName[] =
    "org.freedesktop.portal.Request";
constexpr char kXdgPortalResponseSignal[] = "Response";

constexpr char kFileChooserInterfaceName[] =
    "org.freedesktop.portal.FileChooser";

constexpr char kFileChooserMethodOpenFile[] = "OpenFile";
constexpr char kFileChooserMethodSaveFile[] = "SaveFile";

constexpr char kFileChooserOptionHandleToken[] = "handle_token";
constexpr char kFileChooserOptionAcceptLabel[] = "accept_label";
constexpr char kFileChooserOptionMultiple[] = "multiple";
constexpr char kFileChooserOptionDirectory[] = "directory";
constexpr char kFileChooserOptionFilters[] = "filters";
constexpr char kFileChooserOptionCurrentFilter[] = "current_filter";
constexpr char kFileChooserOptionCurrentFolder[] = "current_folder";
constexpr char kFileChooserOptionCurrentName[] = "current_name";
constexpr char kFileChooserOptionModal[] = "modal";

constexpr int kFileChooserFilterKindGlob = 0;

constexpr char kFileUriPrefix[] = "file://";

// Time to wait for the notification service to start, in milliseconds.
constexpr base::TimeDelta kStartServiceTimeout = base::Seconds(1);

struct FileChooserProperties : dbus::PropertySet {
  dbus::Property<uint32_t> version;

  explicit FileChooserProperties(dbus::ObjectProxy* object_proxy)
      : dbus::PropertySet(object_proxy, kFileChooserInterfaceName, {}) {
    RegisterProperty("version", &version);
  }

  ~FileChooserProperties() override = default;
};

void AppendStringOption(dbus::MessageWriter* writer,
                        const std::string& name,
                        const std::string& value) {
  dbus::MessageWriter option_writer(nullptr);
  writer->OpenDictEntry(&option_writer);

  option_writer.AppendString(name);
  option_writer.AppendVariantOfString(value);

  writer->CloseContainer(&option_writer);
}

void AppendByteStringOption(dbus::MessageWriter* writer,
                            const std::string& name,
                            const std::string& value) {
  dbus::MessageWriter option_writer(nullptr);
  writer->OpenDictEntry(&option_writer);

  option_writer.AppendString(name);

  dbus::MessageWriter value_writer(nullptr);
  option_writer.OpenVariant("ay", &value_writer);

  value_writer.AppendArrayOfBytes(
      base::make_span(reinterpret_cast<const std::uint8_t*>(value.c_str()),
                      // size + 1 will include the null terminator.
                      value.size() + 1));

  option_writer.CloseContainer(&value_writer);
  writer->CloseContainer(&option_writer);
}

void AppendBoolOption(dbus::MessageWriter* writer,
                      const std::string& name,
                      bool value) {
  dbus::MessageWriter option_writer(nullptr);
  writer->OpenDictEntry(&option_writer);

  option_writer.AppendString(name);
  option_writer.AppendVariantOfBool(value);

  writer->CloseContainer(&option_writer);
}

scoped_refptr<dbus::Bus>* AcquireBusStorageOnBusThread() {
  static base::NoDestructor<scoped_refptr<dbus::Bus>> bus(nullptr);
  if (!*bus) {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SESSION;
    options.connection_type = dbus::Bus::PRIVATE;
    options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();

    *bus = base::MakeRefCounted<dbus::Bus>(options);
  }

  return bus.get();
}

dbus::Bus* AcquireBusOnBusThread() {
  return AcquireBusStorageOnBusThread()->get();
}

void DestroyBusOnBusThread() {
  scoped_refptr<dbus::Bus>* bus_storage = AcquireBusStorageOnBusThread();
  (*bus_storage)->ShutdownAndBlock();

  // If the connection is restarted later on, we need to make sure the entire
  // bus is newly created. Otherwise, references to an old, invalid task runner
  // may persist.
  bus_storage->reset();
}

}  // namespace

SelectFileDialogLinuxPortal::SelectFileDialogLinuxPortal(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : SelectFileDialogLinux(listener, std::move(policy)) {}

SelectFileDialogLinuxPortal::~SelectFileDialogLinuxPortal() {
  UnparentOnMainThread();
  // `info_` may have weak pointers which must be invalidated on the dbus
  // thread. Pass our reference to that thread so weak pointers get invalidated
  // on the correct sequence.
  dbus_thread_linux::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce([](scoped_refptr<DialogInfo> info) {}, std::move(info_)));
}

// static
void SelectFileDialogLinuxPortal::StartAvailabilityTestInBackground() {
  if (GetAvailabilityTestCompletionFlag()->IsSet())
    return;

  dbus_thread_linux::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SelectFileDialogLinuxPortal::CheckPortalAvailabilityOnBusThread));
}

// static
bool SelectFileDialogLinuxPortal::IsPortalAvailable() {
  if (!GetAvailabilityTestCompletionFlag()->IsSet())
    LOG(WARNING) << "Portal availability checked before test was complete";

  return is_portal_available_;
}

// static
void SelectFileDialogLinuxPortal::DestroyPortalConnection() {
  dbus_thread_linux::GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&DestroyBusOnBusThread));
}

bool SelectFileDialogLinuxPortal::IsRunning(
    gfx::NativeWindow parent_window) const {
  return parent_window && host_ && host_.get() == parent_window->GetHost();
}

void SelectFileDialogLinuxPortal::SelectFileImpl(
    Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    const GURL* caller) {
  info_ = base::MakeRefCounted<DialogInfo>(
      base::BindOnce(&SelectFileDialogLinuxPortal::DialogCreatedOnMainThread,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&SelectFileDialogLinuxPortal::CompleteOpenOnMainThread,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&SelectFileDialogLinuxPortal::CancelOpenOnMainThread,
                     weak_factory_.GetWeakPtr()));
  info_->type = type;
  info_->main_task_runner = base::SequencedTaskRunner::GetCurrentDefault();

  if (owning_window) {
    if (auto* root = owning_window->GetRootWindow()) {
      if (auto* host = root->GetNativeWindowProperty(
              views::DesktopWindowTreeHostLinux::kWindowKey)) {
        host_ = static_cast<aura::WindowTreeHost*>(host)->GetWeakPtr();
      }
    }
  }

  if (file_types)
    set_file_types(*file_types);

  set_file_type_index(file_type_index);

  PortalFilterSet filter_set = BuildFilterSet();

  // Keep a copy of the filters so the index of the chosen one can be identified
  // and returned to listeners later.
  filters_ = filter_set.filters;

  if (host_) {
    auto* delegate = ui::LinuxUiDelegate::GetInstance();
    if (delegate &&
        delegate->ExportWindowHandle(
            host_->GetAcceleratedWidget(),
            base::BindOnce(
                &SelectFileDialogLinuxPortal::SelectFileImplWithParentHandle,
                this, title, default_path, filter_set, default_extension))) {
      // Return early to skip the fallback below.
      return;
    } else {
      LOG(WARNING) << "Failed to export window handle for portal select dialog";
    }
  }

  // No parent, so just use a blank parent handle.
  SelectFileImplWithParentHandle(title, default_path, filter_set,
                                 default_extension, "");
}

bool SelectFileDialogLinuxPortal::HasMultipleFileTypeChoicesImpl() {
  return file_types().extensions.size() > 1;
}

// static
void SelectFileDialogLinuxPortal::CheckPortalAvailabilityOnBusThread() {
  DCHECK(dbus_thread_linux::GetTaskRunner()->RunsTasksInCurrentSequence());
  base::AtomicFlag* availability_test_complete =
      GetAvailabilityTestCompletionFlag();
  if (availability_test_complete->IsSet())
    return;

  dbus::Bus* bus = AcquireBusOnBusThread();

  dbus::ObjectProxy* dbus_proxy =
      bus->GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  if (IsPortalRunningOnBusThread(dbus_proxy) ||
      IsPortalActivatableOnBusThread(dbus_proxy)) {
    dbus::ObjectPath portal_path(kXdgPortalObject);
    dbus::ObjectProxy* portal =
        bus->GetObjectProxy(kXdgPortalService, portal_path);

    FileChooserProperties properties(portal);
    if (!properties.GetAndBlock(&properties.version)) {
      LOG(ERROR) << "Failed to read portal version property";
    } else if (properties.version.value() >= kXdgPortalRequiredVersion) {
      is_portal_available_ = true;
    }
  }

  VLOG(1) << "File chooser portal available: "
          << (is_portal_available_ ? "yes" : "no");
  availability_test_complete->Set();
}

// static
bool SelectFileDialogLinuxPortal::IsPortalRunningOnBusThread(
    dbus::ObjectProxy* dbus_proxy) {
  dbus::MethodCall method_call(DBUS_INTERFACE_DBUS, kDBusMethodNameHasOwner);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kXdgPortalService);

  std::unique_ptr<dbus::Response> response =
      dbus_proxy
          ->CallMethodAndBlock(&method_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr);
  if (!response)
    return false;

  dbus::MessageReader reader(response.get());
  bool owned = false;
  if (!reader.PopBool(&owned)) {
    LOG(ERROR) << "Failed to read response";
    return false;
  }

  return owned;
}

// static
bool SelectFileDialogLinuxPortal::IsPortalActivatableOnBusThread(
    dbus::ObjectProxy* dbus_proxy) {
  dbus::MethodCall method_call(DBUS_INTERFACE_DBUS,
                               kDBusMethodListActivatableNames);

  std::unique_ptr<dbus::Response> response =
      dbus_proxy
          ->CallMethodAndBlock(&method_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr);
  if (!response)
    return false;

  dbus::MessageReader reader(response.get());
  std::vector<std::string> names;
  if (!reader.PopArrayOfStrings(&names)) {
    LOG(ERROR) << "Failed to read response";
    return false;
  }

  if (base::Contains(names, kXdgPortalService)) {
    dbus::MethodCall start_service_call(DBUS_INTERFACE_DBUS,
                                        kMethodStartServiceByName);
    dbus::MessageWriter start_service_writer(&start_service_call);
    start_service_writer.AppendString(kXdgPortalService);
    start_service_writer.AppendUint32(/*flags=*/0);
    auto start_service_response =
        dbus_proxy
            ->CallMethodAndBlock(&start_service_call,
                                 kStartServiceTimeout.InMilliseconds())
            .value_or(nullptr);
    if (!start_service_response)
      return false;
    dbus::MessageReader start_service_reader(start_service_response.get());
    uint32_t start_service_reply = 0;
    if (start_service_reader.PopUint32(&start_service_reply) &&
        (start_service_reply == DBUS_START_REPLY_SUCCESS ||
         start_service_reply == DBUS_START_REPLY_ALREADY_RUNNING)) {
      return true;
    }
  }
  return false;
}

SelectFileDialogLinuxPortal::PortalFilter::PortalFilter() = default;
SelectFileDialogLinuxPortal::PortalFilter::PortalFilter(
    const PortalFilter& other) = default;
SelectFileDialogLinuxPortal::PortalFilter::PortalFilter(PortalFilter&& other) =
    default;
SelectFileDialogLinuxPortal::PortalFilter::~PortalFilter() = default;

SelectFileDialogLinuxPortal::PortalFilterSet::PortalFilterSet() = default;
SelectFileDialogLinuxPortal::PortalFilterSet::PortalFilterSet(
    const PortalFilterSet& other) = default;
SelectFileDialogLinuxPortal::PortalFilterSet::PortalFilterSet(
    PortalFilterSet&& other) = default;
SelectFileDialogLinuxPortal::PortalFilterSet::~PortalFilterSet() = default;

SelectFileDialogLinuxPortal::DialogInfo::DialogInfo(
    base::OnceClosure created_callback,
    OnSelectFileExecutedCallback selected_callback,
    OnSelectFileCanceledCallback canceled_callback)
    : created_callback_(std::move(created_callback)),
      selected_callback_(std::move(selected_callback)),
      canceled_callback_(std::move(canceled_callback)) {}
SelectFileDialogLinuxPortal::DialogInfo::~DialogInfo() = default;

// static
base::AtomicFlag*
SelectFileDialogLinuxPortal::GetAvailabilityTestCompletionFlag() {
  static base::NoDestructor<base::AtomicFlag> flag;
  return flag.get();
}

SelectFileDialogLinuxPortal::PortalFilterSet
SelectFileDialogLinuxPortal::BuildFilterSet() {
  PortalFilterSet filter_set;

  for (size_t i = 0; i < file_types().extensions.size(); ++i) {
    PortalFilter filter;

    for (const std::string& extension : file_types().extensions[i]) {
      if (extension.empty())
        continue;

      filter.patterns.push_back("*." + base::ToLowerASCII(extension));
      auto upper = "*." + base::ToUpperASCII(extension);
      if (upper != filter.patterns.back())
        filter.patterns.push_back(std::move(upper));
    }

    if (filter.patterns.empty())
      continue;

    // If there is no matching description, use a default description based on
    // the filter.
    if (i < file_types().extension_description_overrides.size()) {
      filter.name =
          base::UTF16ToUTF8(file_types().extension_description_overrides[i]);
    }
    if (filter.name.empty()) {
      std::vector<std::string> patterns_vector(filter.patterns.begin(),
                                               filter.patterns.end());
      filter.name = base::JoinString(patterns_vector, ",");
    }

    // The -1 is required to match against the right filter because
    // |file_type_index_| is 1-indexed.
    if (i == file_type_index() - 1)
      filter_set.default_filter = filter;

    filter_set.filters.push_back(std::move(filter));
  }

  if (file_types().include_all_files && !filter_set.filters.empty()) {
    // Add the *.* filter, but only if we have added other filters (otherwise it
    // is implied).
    PortalFilter filter;
    filter.name = l10n_util::GetStringUTF8(IDS_SAVEAS_ALL_FILES);
    filter.patterns.push_back("*.*");

    filter_set.filters.push_back(std::move(filter));
  }

  return filter_set;
}

void SelectFileDialogLinuxPortal::SelectFileImplWithParentHandle(
    std::u16string title,
    base::FilePath default_path,
    PortalFilterSet filter_set,
    base::FilePath::StringType default_extension,
    std::string parent_handle) {
  bool default_path_exists = CallDirectoryExistsOnUIThread(default_path);
  dbus_thread_linux::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SelectFileDialogLinuxPortal::DialogInfo::SelectFileImplOnBusThread,
          info_, std::move(title), std::move(default_path), default_path_exists,
          std::move(filter_set), std::move(default_extension),
          std::move(parent_handle)));
}

void SelectFileDialogLinuxPortal::DialogInfo::SelectFileImplOnBusThread(
    std::u16string title,
    base::FilePath default_path,
    const bool default_path_exists,
    PortalFilterSet filter_set,
    base::FilePath::StringType default_extension,
    std::string parent_handle) {
  DCHECK(dbus_thread_linux::GetTaskRunner()->RunsTasksInCurrentSequence());
  dbus::Bus* bus = AcquireBusOnBusThread();
  if (!bus->Connect())
    LOG(ERROR) << "Could not connect to bus for XDG portal";

  std::string method;
  switch (type) {
    case SELECT_FOLDER:
    case SELECT_UPLOAD_FOLDER:
    case SELECT_EXISTING_FOLDER:
    case SELECT_OPEN_FILE:
    case SELECT_OPEN_MULTI_FILE:
      method = kFileChooserMethodOpenFile;
      break;
    case SELECT_SAVEAS_FILE:
      method = kFileChooserMethodSaveFile;
      break;
    case SELECT_NONE:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  dbus::MethodCall method_call(kFileChooserInterfaceName, method);
  dbus::MessageWriter writer(&method_call);

  writer.AppendString(parent_handle);

  if (!title.empty()) {
    writer.AppendString(base::UTF16ToUTF8(title));
  } else {
    int message_id = 0;
    if (type == SELECT_SAVEAS_FILE) {
      message_id = IDS_SAVEAS_ALL_FILES;
    } else if (type == SELECT_OPEN_MULTI_FILE) {
      message_id = IDS_OPEN_FILES_DIALOG_TITLE;
    } else {
      message_id = IDS_OPEN_FILE_DIALOG_TITLE;
    }
    writer.AppendString(l10n_util::GetStringUTF8(message_id));
  }

  std::string response_handle_token =
      base::StringPrintf("handle_%d", handle_token_counter_++);

  AppendOptions(&writer, response_handle_token, default_path,
                default_path_exists, filter_set);

  dbus::ObjectPath expected_handle_path(base::nix::XdgDesktopPortalRequestPath(
      bus->GetConnectionName(), response_handle_token));

  response_handle_ =
      bus->GetObjectProxy(kXdgPortalService, expected_handle_path);
  ConnectToHandle();

  dbus::ObjectPath portal_path(kXdgPortalObject);
  dbus::ObjectProxy* portal =
      bus->GetObjectProxy(kXdgPortalService, portal_path);
  portal->CallMethodWithErrorResponse(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&DialogInfo::OnCallResponse, weak_factory_.GetWeakPtr(),
                     base::Unretained(bus)));
}

void SelectFileDialogLinuxPortal::DialogInfo::AppendOptions(
    dbus::MessageWriter* writer,
    const std::string& response_handle_token,
    const base::FilePath& default_path,
    const bool default_path_exists,
    const SelectFileDialogLinuxPortal::PortalFilterSet& filter_set) {
  dbus::MessageWriter options_writer(nullptr);
  writer->OpenArray("{sv}", &options_writer);

  AppendStringOption(&options_writer, kFileChooserOptionHandleToken,
                     response_handle_token);

  if (type == SelectFileDialog::Type::SELECT_UPLOAD_FOLDER) {
    AppendStringOption(&options_writer, kFileChooserOptionAcceptLabel,
                       l10n_util::GetStringUTF8(
                           IDS_SELECT_UPLOAD_FOLDER_DIALOG_UPLOAD_BUTTON));
  }

  if (type == SelectFileDialog::Type::SELECT_FOLDER ||
      type == SelectFileDialog::Type::SELECT_UPLOAD_FOLDER ||
      type == SelectFileDialog::Type::SELECT_EXISTING_FOLDER) {
    AppendBoolOption(&options_writer, kFileChooserOptionDirectory, true);
  } else if (type == SelectFileDialog::Type::SELECT_OPEN_MULTI_FILE) {
    AppendBoolOption(&options_writer, kFileChooserOptionMultiple, true);
  }

  if (!default_path.empty()) {
    if (default_path_exists) {
      // If this is an existing directory, navigate to that directory, with no
      // filename.
      AppendByteStringOption(&options_writer, kFileChooserOptionCurrentFolder,
                             default_path.value());
    } else {
      // The default path does not exist, or is an existing file. We use
      // current_folder followed by current_name, as per the recommendation of
      // the GTK docs and the pattern followed by SelectFileDialogLinuxGtk.
      AppendByteStringOption(&options_writer, kFileChooserOptionCurrentFolder,
                             default_path.DirName().value());

      // current_folder is supported by xdg-desktop-portal but current_name
      // is not - only try to set this when invoking a save file dialog.
      if (type == SelectFileDialog::Type::SELECT_SAVEAS_FILE) {
        AppendStringOption(&options_writer, kFileChooserOptionCurrentName,
                           default_path.BaseName().value());
      }
    }
  }

  AppendFiltersOption(&options_writer, filter_set.filters);
  if (filter_set.default_filter) {
    dbus::MessageWriter option_writer(nullptr);
    options_writer.OpenDictEntry(&option_writer);

    option_writer.AppendString(kFileChooserOptionCurrentFilter);

    dbus::MessageWriter value_writer(nullptr);
    option_writer.OpenVariant("(sa(us))", &value_writer);

    AppendFilterStruct(&value_writer, *filter_set.default_filter);

    option_writer.CloseContainer(&value_writer);
    options_writer.CloseContainer(&option_writer);
  }

  AppendBoolOption(&options_writer, kFileChooserOptionModal, true);

  writer->CloseContainer(&options_writer);
}

void SelectFileDialogLinuxPortal::DialogInfo::AppendFiltersOption(
    dbus::MessageWriter* writer,
    const std::vector<PortalFilter>& filters) {
  dbus::MessageWriter option_writer(nullptr);
  writer->OpenDictEntry(&option_writer);

  option_writer.AppendString(kFileChooserOptionFilters);

  dbus::MessageWriter variant_writer(nullptr);
  option_writer.OpenVariant("a(sa(us))", &variant_writer);

  dbus::MessageWriter filters_writer(nullptr);
  variant_writer.OpenArray("(sa(us))", &filters_writer);

  for (const PortalFilter& filter : filters) {
    AppendFilterStruct(&filters_writer, filter);
  }

  variant_writer.CloseContainer(&filters_writer);
  option_writer.CloseContainer(&variant_writer);
  writer->CloseContainer(&option_writer);
}

void SelectFileDialogLinuxPortal::DialogInfo::AppendFilterStruct(
    dbus::MessageWriter* writer,
    const PortalFilter& filter) {
  dbus::MessageWriter filter_writer(nullptr);
  writer->OpenStruct(&filter_writer);

  filter_writer.AppendString(filter.name);

  dbus::MessageWriter patterns_writer(nullptr);
  filter_writer.OpenArray("(us)", &patterns_writer);

  for (const std::string& pattern : filter.patterns) {
    dbus::MessageWriter pattern_writer(nullptr);
    patterns_writer.OpenStruct(&pattern_writer);

    pattern_writer.AppendUint32(kFileChooserFilterKindGlob);
    pattern_writer.AppendString(pattern);

    patterns_writer.CloseContainer(&pattern_writer);
  }

  filter_writer.CloseContainer(&patterns_writer);
  writer->CloseContainer(&filter_writer);
}

void SelectFileDialogLinuxPortal::DialogInfo::ConnectToHandle() {
  DCHECK(dbus_thread_linux::GetTaskRunner()->RunsTasksInCurrentSequence());
  response_handle_->ConnectToSignal(
      kXdgPortalRequestInterfaceName, kXdgPortalResponseSignal,
      base::BindRepeating(&DialogInfo::OnResponseSignalEmitted,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(&DialogInfo::OnResponseSignalConnected,
                     weak_factory_.GetWeakPtr()));
}

void SelectFileDialogLinuxPortal::DialogInfo::CompleteOpen(
    std::vector<base::FilePath> paths,
    std::string current_filter) {
  DCHECK(dbus_thread_linux::GetTaskRunner()->RunsTasksInCurrentSequence());
  response_handle_->Detach();
  main_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(selected_callback_), std::move(paths),
                                std::move(current_filter)));
}

void SelectFileDialogLinuxPortal::DialogInfo::CancelOpen() {
  response_handle_->Detach();
  main_task_runner->PostTask(FROM_HERE, std::move(canceled_callback_));
}

void SelectFileDialogLinuxPortal::DialogCreatedOnMainThread() {
  if (!host_) {
    return;
  }
  host_->ReleaseCapture();
  reenable_window_event_handling_ =
      static_cast<views::DesktopWindowTreeHostLinux*>(host_.get())
          ->DisableEventListening();
}

void SelectFileDialogLinuxPortal::CompleteOpenOnMainThread(
    std::vector<base::FilePath> paths,
    std::string current_filter) {
  UnparentOnMainThread();

  if (listener_) {
    if (info_->type == SELECT_OPEN_MULTI_FILE) {
      listener_->MultiFilesSelected(FilePathListToSelectedFileInfoList(paths));
    } else if (paths.size() > 1) {
      LOG(ERROR) << "Got >1 file URI from a single-file chooser";
    } else {
      int index = 1;
      for (size_t i = 0; i < filters_.size(); ++i) {
        if (filters_[i].name == current_filter) {
          index = 1 + i;
          break;
        }
      }
      listener_->FileSelected(SelectedFileInfo(paths[0]), index);
    }
  }
}

void SelectFileDialogLinuxPortal::CancelOpenOnMainThread() {
  UnparentOnMainThread();

  if (listener_)
    listener_->FileSelectionCanceled();
}

void SelectFileDialogLinuxPortal::UnparentOnMainThread() {
  if (reenable_window_event_handling_) {
    std::move(reenable_window_event_handling_).Run();
  }
  host_ = nullptr;
}

void SelectFileDialogLinuxPortal::DialogInfo::OnCallResponse(
    dbus::Bus* bus,
    dbus::Response* response,
    dbus::ErrorResponse* error_response) {
  DCHECK(dbus_thread_linux::GetTaskRunner()->RunsTasksInCurrentSequence());
  if (response) {
    dbus::MessageReader reader(response);
    dbus::ObjectPath actual_handle_path;
    if (!reader.PopObjectPath(&actual_handle_path)) {
      LOG(ERROR) << "Invalid portal response";
    } else {
      if (response_handle_->object_path() != actual_handle_path) {
        VLOG(1) << "Re-attaching response handle to "
                << actual_handle_path.value();

        response_handle_->Detach();
        response_handle_ =
            bus->GetObjectProxy(kXdgPortalService, actual_handle_path);
        ConnectToHandle();
      }

      // Return before the operation is cancelled.
      return;
    }
  } else if (error_response) {
    std::string error_name = error_response->GetErrorName();
    std::string error_message;
    dbus::MessageReader reader(error_response);
    reader.PopString(&error_message);

    LOG(ERROR) << "Portal returned error: " << error_name << ": "
               << error_message;
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  // All error paths end up here.
  CancelOpen();
}

void SelectFileDialogLinuxPortal::DialogInfo::OnResponseSignalConnected(
    const std::string& interface,
    const std::string& signal,
    bool connected) {
  DCHECK(dbus_thread_linux::GetTaskRunner()->RunsTasksInCurrentSequence());
  if (!connected) {
    LOG(ERROR) << "Could not connect to Response signal";
    CancelOpen();
  } else if (created_callback_) {
    main_task_runner->PostTask(FROM_HERE, std::move(created_callback_));
  }
}

void SelectFileDialogLinuxPortal::DialogInfo::OnResponseSignalEmitted(
    dbus::Signal* signal) {
  DCHECK(dbus_thread_linux::GetTaskRunner()->RunsTasksInCurrentSequence());
  dbus::MessageReader reader(signal);

  std::vector<std::string> uris;
  std::string current_filter;
  if (!CheckResponseCode(&reader) ||
      !ReadResponseResults(&reader, &uris, &current_filter)) {
    CancelOpen();
    return;
  }

  std::vector<base::FilePath> paths = ConvertUrisToPaths(uris);
  if (!paths.empty())
    CompleteOpen(std::move(paths), std::move(current_filter));
  else
    CancelOpen();
}

bool SelectFileDialogLinuxPortal::DialogInfo::CheckResponseCode(
    dbus::MessageReader* reader) {
  std::uint32_t response = 0;
  if (!reader->PopUint32(&response)) {
    LOG(ERROR) << "Failed to read response ID";
    return false;
  } else if (response != 0) {
    return false;
  }

  return true;
}

bool SelectFileDialogLinuxPortal::DialogInfo::ReadResponseResults(
    dbus::MessageReader* reader,
    std::vector<std::string>* uris,
    std::string* current_filter) {
  dbus::MessageReader results_reader(nullptr);
  if (!reader->PopArray(&results_reader)) {
    LOG(ERROR) << "Failed to read file chooser variant";
    return false;
  }

  while (results_reader.HasMoreData()) {
    dbus::MessageReader entry_reader(nullptr);
    std::string key;
    if (!results_reader.PopDictEntry(&entry_reader) ||
        !entry_reader.PopString(&key)) {
      LOG(ERROR) << "Failed to read response entry";
      return false;
    }

    if (key == "uris") {
      dbus::MessageReader uris_reader(nullptr);
      if (!entry_reader.PopVariant(&uris_reader) ||
          !uris_reader.PopArrayOfStrings(uris)) {
        LOG(ERROR) << "Failed to read <uris> response entry value";
        return false;
      }
    }
    if (key == "current_filter") {
      dbus::MessageReader current_filter_reader(nullptr);
      dbus::MessageReader current_filter_struct_reader(nullptr);
      if (!entry_reader.PopVariant(&current_filter_reader) ||
          !current_filter_reader.PopStruct(&current_filter_struct_reader) ||
          !current_filter_struct_reader.PopString(current_filter)) {
        LOG(ERROR) << "Failed to read <current_filter> response entry value";
      }
    }
  }

  return true;
}

std::vector<base::FilePath>
SelectFileDialogLinuxPortal::DialogInfo::ConvertUrisToPaths(
    const std::vector<std::string>& uris) {
  std::vector<base::FilePath> paths;
  for (const std::string& uri : uris) {
    if (!base::StartsWith(uri, kFileUriPrefix, base::CompareCase::SENSITIVE)) {
      LOG(WARNING) << "Ignoring unknown file chooser URI: " << uri;
      continue;
    }

    std::string_view encoded_path(uri);
    encoded_path.remove_prefix(strlen(kFileUriPrefix));

    url::RawCanonOutputT<char16_t> decoded_path;
    url::DecodeURLEscapeSequences(
        encoded_path, url::DecodeURLMode::kUTF8OrIsomorphic, &decoded_path);
    paths.emplace_back(base::UTF16ToUTF8(decoded_path.view()));
  }

  return paths;
}

bool SelectFileDialogLinuxPortal::is_portal_available_ = false;
int SelectFileDialogLinuxPortal::handle_token_counter_ = 0;

}  // namespace ui
