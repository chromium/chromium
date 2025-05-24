// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "ui/shell_dialogs/select_file_dialog_linux_portal.h"

#include <string_view>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/check_for_service_and_start.h"
#include "components/dbus/xdg/request.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/linux/linux_ui_delegate.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace ui {

namespace {

constexpr char kXdgPortalService[] = "org.freedesktop.portal.Desktop";
constexpr char kXdgPortalObject[] = "/org/freedesktop/portal/desktop";

constexpr int kXdgPortalRequiredVersion = 3;

constexpr char kFileChooserInterfaceName[] =
    "org.freedesktop.portal.FileChooser";

constexpr char kFileChooserMethodOpenFile[] = "OpenFile";
constexpr char kFileChooserMethodSaveFile[] = "SaveFile";

constexpr char kFileChooserOptionAcceptLabel[] = "accept_label";
constexpr char kFileChooserOptionMultiple[] = "multiple";
constexpr char kFileChooserOptionDirectory[] = "directory";
constexpr char kFileChooserOptionFilters[] = "filters";
constexpr char kFileChooserOptionCurrentFilter[] = "current_filter";
constexpr char kFileChooserOptionCurrentFolder[] = "current_folder";
constexpr char kFileChooserOptionCurrentName[] = "current_name";
constexpr char kFileChooserOptionModal[] = "modal";

constexpr uint32_t kFileChooserFilterKindGlob = 0;

constexpr char kFileUriPrefix[] = "file://";

enum class ServiceAvailability {
  kNotStarted,
  kInProgress,
  kNotAvailable,
  kAvailable,
};

ServiceAvailability g_service_availability = ServiceAvailability::kNotStarted;

scoped_refptr<base::SequencedTaskRunner>& GetMainTaskRunner() {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      main_task_runner;
  return *main_task_runner;
}

void OnGetPropertyReply(dbus::Response* response) {
  if (!response) {
    g_service_availability = ServiceAvailability::kNotAvailable;
    return;
  }

  uint32_t version = 0;
  dbus::MessageReader reader(response);
  if (!reader.PopVariantOfUint32(&version)) {
    g_service_availability = ServiceAvailability::kNotAvailable;
    return;
  }

  g_service_availability = version >= kXdgPortalRequiredVersion
                               ? ServiceAvailability::kAvailable
                               : ServiceAvailability::kNotAvailable;
}

void OnServiceStarted(std::optional<bool> service_started) {
  if (!service_started.value_or(false)) {
    g_service_availability = ServiceAvailability::kNotAvailable;
    return;
  }

  dbus::ObjectProxy* portal =
      dbus_thread_linux::GetSharedSessionBus()->GetObjectProxy(
          kXdgPortalService, dbus::ObjectPath(kXdgPortalObject));

  dbus::MethodCall get_property_call(DBUS_INTERFACE_PROPERTIES, "Get");
  dbus::MessageWriter get_property_writer(&get_property_call);
  get_property_writer.AppendString(kFileChooserInterfaceName);
  get_property_writer.AppendString("version");
  portal->CallMethod(&get_property_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::BindOnce(&OnGetPropertyReply));
}

DbusByteArray PathToByteArray(const base::FilePath& path) {
  return DbusByteArray(base::MakeRefCounted<base::RefCountedBytes>(
      base::as_bytes(base::span_with_nul_from_cstring_view(
          base::cstring_view(path.value())))));
}

std::vector<base::FilePath> ConvertUrisToPaths(
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

}  // namespace

SelectFileDialogLinuxPortal::SelectFileDialogLinuxPortal(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : SelectFileDialogLinux(listener, std::move(policy)) {}

SelectFileDialogLinuxPortal::~SelectFileDialogLinuxPortal() {
  if (reenable_window_event_handling_) {
    invoker_task_runner_->PostTask(FROM_HERE,
                                   std::move(reenable_window_event_handling_));
  }
}

// static
void SelectFileDialogLinuxPortal::StartAvailabilityTestInBackground() {
  if (g_service_availability != ServiceAvailability::kNotStarted) {
    return;
  }
  g_service_availability = ServiceAvailability::kInProgress;

  GetMainTaskRunner() = base::SequencedTaskRunner::GetCurrentDefault();

  dbus_utils::CheckForServiceAndStart(dbus_thread_linux::GetSharedSessionBus(),
                                      kXdgPortalService,
                                      base::BindOnce(&OnServiceStarted));
}

// static
bool SelectFileDialogLinuxPortal::IsPortalAvailable() {
  if (g_service_availability == ServiceAvailability::kInProgress) {
    LOG(WARNING) << "Portal availability checked before test was complete";
  }

  return g_service_availability == ServiceAvailability::kAvailable;
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
  type_ = type;
  invoker_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();

  if (owning_window) {
    if (auto* root = owning_window->GetRootWindow()) {
      if (auto* host = root->GetNativeWindowProperty(
              views::DesktopWindowTreeHostLinux::kWindowKey)) {
        host_ = static_cast<aura::WindowTreeHost*>(host)->GetWeakPtr();
      }
    }
  }

  if (file_types) {
    set_file_types(*file_types);
  }

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

SelectFileDialogLinuxPortal::PortalFilterSet
SelectFileDialogLinuxPortal::BuildFilterSet() {
  PortalFilterSet filter_set;

  for (size_t i = 0; i < file_types().extensions.size(); ++i) {
    PortalFilter filter;

    std::vector<std::string> original_patterns;

    for (const std::string& extension : file_types().extensions[i]) {
      if (extension.empty()) {
        continue;
      }

      // We want to allow ASCII case-insensitive matches for the extension on
      // a per-character basis, since that's what
      // https://html.spec.whatwg.org/multipage/input.html#attr-input-accept
      // suggests.  For example, we should accept file.txt, file.TXT, or
      // file.tXt.  To do this, we expand characters with ASCII case
      // equivalents to be represented by [aA], as documented in
      // https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.FileChooser.html
      std::string pattern("*.");
      for (char c : extension) {
        char lower = base::ToLowerASCII(c);
        char upper = base::ToUpperASCII(c);
        if (upper != lower) {
          pattern.append({'[', lower, upper, ']'});
        } else {
          pattern.append({c});
        }
      }
      filter.patterns.push_back(pattern);

      // Save the original form for use as a fallback description.
      original_patterns.push_back("*." + extension);
    }

    if (filter.patterns.empty()) {
      continue;
    }

    // If there is no matching description, use a default description based on
    // the filter.
    if (i < file_types().extension_description_overrides.size()) {
      filter.name =
          base::UTF16ToUTF8(file_types().extension_description_overrides[i]);
    }
    if (filter.name.empty()) {
      filter.name = base::JoinString(original_patterns, ",");
    }

    // The -1 is required to match against the right filter because
    // |file_type_index_| is 1-indexed.
    if (i == file_type_index() - 1) {
      filter_set.default_filter = filter;
    }

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
  GetMainTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SelectFileDialogLinuxPortal::SelectFileImplOnMainThread,
                     this, std::move(title), std::move(default_path),
                     default_path_exists, std::move(filter_set),
                     std::move(default_extension), std::move(parent_handle)));
}

void SelectFileDialogLinuxPortal::SelectFileImplOnMainThread(
    std::u16string title,
    base::FilePath default_path,
    const bool default_path_exists,
    PortalFilterSet filter_set,
    base::FilePath::StringType default_extension,
    std::string parent_handle) {
  CHECK(GetMainTaskRunner()->RunsTasksInCurrentSequence());

  std::string method;
  switch (type_) {
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
      NOTREACHED();
  }

  std::string utf8_title;
  if (!title.empty()) {
    utf8_title = base::UTF16ToUTF8(title);
  } else {
    int message_id = 0;
    if (type_ == SELECT_SAVEAS_FILE) {
      message_id = IDS_SAVEAS_ALL_FILES;
    } else if (type_ == SELECT_OPEN_MULTI_FILE) {
      message_id = IDS_OPEN_FILES_DIALOG_TITLE;
    } else {
      message_id = IDS_OPEN_FILE_DIALOG_TITLE;
    }
    utf8_title = l10n_util::GetStringUTF8(message_id);
  }

  MakeFileChooserRequest(
      method, utf8_title,
      BuildOptionsDictionary(default_path, default_path_exists, filter_set),
      std::move(parent_handle));
}

DbusDictionary SelectFileDialogLinuxPortal::BuildOptionsDictionary(
    const base::FilePath& default_path,
    bool default_path_exists,
    const PortalFilterSet& filter_set) {
  DbusDictionary dict;

  switch (type_) {
    case SelectFileDialog::SELECT_UPLOAD_FOLDER:
      dict.PutAs(kFileChooserOptionAcceptLabel,
                 DbusString(l10n_util::GetStringUTF8(
                     IDS_SELECT_UPLOAD_FOLDER_DIALOG_UPLOAD_BUTTON)));
      [[fallthrough]];
    case SelectFileDialog::SELECT_FOLDER:
    case SelectFileDialog::Type::SELECT_EXISTING_FOLDER:
      dict.PutAs(kFileChooserOptionDirectory, DbusBoolean(true));
      break;
    case SelectFileDialog::SELECT_OPEN_MULTI_FILE:
      dict.PutAs(kFileChooserOptionMultiple, DbusBoolean(true));
      break;
    default:
      break;
  }

  if (!default_path.empty() && base::IsStringUTF8(default_path.value())) {
    if (default_path_exists) {
      // If this is an existing directory, navigate to that directory, with no
      // filename.
      dict.PutAs(kFileChooserOptionCurrentFolder,
                 PathToByteArray(default_path));
    } else {
      // The default path does not exist, or is an existing file. We use
      // current_folder followed by current_name, as per the recommendation of
      // the GTK docs and the pattern followed by SelectFileDialogLinuxGtk.
      dict.PutAs(kFileChooserOptionCurrentFolder,
                 PathToByteArray(default_path.DirName()));

      // current_folder is supported by xdg-desktop-portal but current_name
      // is not - only try to set this when invoking a save file dialog.
      if (type_ == SELECT_SAVEAS_FILE) {
        dict.PutAs(kFileChooserOptionCurrentName,
                   DbusString(default_path.BaseName().value()));
      }
    }
  }

  if (!filter_set.filters.empty()) {
    DbusFilters filters_array;
    for (const auto& filter : filter_set.filters) {
      filters_array.value().push_back(MakeFilterStruct(filter));
    }
    dict.PutAs(kFileChooserOptionFilters, std::move(filters_array));

    if (filter_set.default_filter) {
      dict.PutAs(kFileChooserOptionCurrentFilter,
                 MakeFilterStruct(*filter_set.default_filter));
    }
  }

  dict.PutAs(kFileChooserOptionModal, DbusBoolean(true));

  return dict;
}

SelectFileDialogLinuxPortal::DbusFilter
SelectFileDialogLinuxPortal::MakeFilterStruct(const PortalFilter& filter) {
  DbusFilterPatterns patterns;
  for (const std::string& pattern_str : filter.patterns) {
    patterns.value().push_back(MakeDbusStruct(
        DbusUint32(kFileChooserFilterKindGlob), DbusString(pattern_str)));
  }
  return MakeDbusStruct(DbusString(filter.name), std::move(patterns));
}

void SelectFileDialogLinuxPortal::MakeFileChooserRequest(
    const std::string& method,
    const std::string& title,
    DbusDictionary options,
    std::string parent_handle) {
  CHECK(GetMainTaskRunner()->RunsTasksInCurrentSequence());
  scoped_refptr<dbus::Bus> bus = dbus_thread_linux::GetSharedSessionBus();
  dbus::ObjectProxy* portal = bus->GetObjectProxy(
      kXdgPortalService, dbus::ObjectPath(kXdgPortalObject));

  // Unretained is safe since we own the Request object.
  auto callback =
      base::BindOnce(&SelectFileDialogLinuxPortal::OnFileChooserResponse,
                     base::Unretained(this));
  file_chooser_request_ = std::make_unique<dbus_xdg::Request>(
      bus, portal, kFileChooserInterfaceName, method,
      MakeDbusParameters(DbusString(std::move(parent_handle)),
                         DbusString(title)),
      std::move(options), std::move(callback));
  invoker_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SelectFileDialogLinuxPortal::DialogCreatedOnInvoker,
                     this));
}

void SelectFileDialogLinuxPortal::OnFileChooserResponse(
    base::expected<DbusDictionary, dbus_xdg::ResponseError> results) {
  CHECK(GetMainTaskRunner()->RunsTasksInCurrentSequence());

  file_chooser_request_.reset();

  if (!results.has_value()) {
    CancelOpen();
    return;
  }

  std::vector<std::string> uris;
  auto* wrapped_uris = results->GetAs<DbusArray<DbusString>>("uris");
  if (wrapped_uris) {
    for (auto& element : wrapped_uris->value()) {
      uris.push_back(element.value());
    }
  }

  if (uris.empty()) {
    CancelOpen();
    return;
  }

  std::vector<base::FilePath> paths = ConvertUrisToPaths(uris);
  if (paths.empty()) {
    CancelOpen();
    return;
  }

  std::string current_filter_name;
  auto* current_filter = results->GetAs<DbusFilter>("current_filter");
  if (current_filter) {
    current_filter_name = std::get<0>(current_filter->value()).value();
  }

  CompleteOpen(std::move(paths), std::move(current_filter_name));
}

void SelectFileDialogLinuxPortal::CompleteOpen(
    std::vector<base::FilePath> paths,
    std::string current_filter) {
  dbus_thread_linux::GetSharedSessionBus()->AssertOnOriginThread();
  invoker_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SelectFileDialogLinuxPortal::CompleteOpenOnInvoker, this,
                     std::move(paths), std::move(current_filter)));
}

void SelectFileDialogLinuxPortal::CancelOpen() {
  dbus_thread_linux::GetSharedSessionBus()->AssertOnOriginThread();
  invoker_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SelectFileDialogLinuxPortal::CancelOpenOnInvoker, this));
}

void SelectFileDialogLinuxPortal::DialogCreatedOnInvoker() {
  CHECK(invoker_task_runner_->RunsTasksInCurrentSequence());
  if (!host_) {
    return;
  }
  host_->ReleaseCapture();
  reenable_window_event_handling_ =
      static_cast<views::DesktopWindowTreeHostLinux*>(host_.get())
          ->DisableEventListening();
}

void SelectFileDialogLinuxPortal::CompleteOpenOnInvoker(
    std::vector<base::FilePath> paths,
    std::string current_filter) {
  CHECK(invoker_task_runner_->RunsTasksInCurrentSequence());
  UnparentOnInvoker();

  if (listener_) {
    if (type_ == SELECT_OPEN_MULTI_FILE) {
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

void SelectFileDialogLinuxPortal::CancelOpenOnInvoker() {
  CHECK(invoker_task_runner_->RunsTasksInCurrentSequence());
  UnparentOnInvoker();

  if (listener_) {
    listener_->FileSelectionCanceled();
  }
}

void SelectFileDialogLinuxPortal::UnparentOnInvoker() {
  if (reenable_window_event_handling_) {
    std::move(reenable_window_event_handling_).Run();
  }
  host_ = nullptr;
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

}  // namespace ui
