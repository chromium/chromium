// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/select_file_dialog_linux_gtk.h"

#include <glib/gi18n.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "ui/aura/window_observer.h"
#include "ui/base/glib/scoped_gobject.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_ui.h"
#include "ui/gtk/gtk_ui_platform.h"
#include "ui/gtk/gtk_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_linux.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#include "url/gurl.h"

namespace gtk {

namespace {

// TODO(https://crbug.com/981309): These getters will be unnecessary after
// migrating to GtkFileChooserNative.
const char* GettextPackage() {
  static base::NoDestructor<std::string> gettext_package(
      "gtk" + base::NumberToString(GtkVersion().components()[0]) + "0");
  return gettext_package->c_str();
}

const char* GtkGettext(const char* str) {
  return g_dgettext(GettextPackage(), str);
}

const char* GetCancelLabel() {
  if (!GtkCheckVersion(4))
    return "gtk-cancel";  // In GTK3, this is GTK_STOCK_CANCEL.
  static const char* cancel = GtkGettext("_Cancel");
  return cancel;
}

const char* GetOpenLabel() {
  if (!GtkCheckVersion(4))
    return "gtk-open";  // In GTK3, this is GTK_STOCK_OPEN.
  static const char* open = GtkGettext("_Open");
  return open;
}

const char* GetSaveLabel() {
  if (!GtkCheckVersion(4))
    return "gtk-save";  // In GTK3, this is GTK_STOCK_SAVE.
  static const char* save = GtkGettext("_Save");
  return save;
}

// Runs DesktopWindowTreeHostLinux::EnableEventListening() when the file-picker
// is closed.
void OnFilePickerDestroy(base::OnceClosure* callback_raw) {
  std::unique_ptr<base::OnceClosure> callback = base::WrapUnique(callback_raw);
  std::move(*callback).Run();
}

void GtkFileChooserSetFilename(GtkFileChooser* dialog,
                               const base::FilePath& path) {
  if (GtkCheckVersion(4)) {
    auto file = TakeGObject(g_file_new_for_path(path.value().c_str()));
    gtk_file_chooser_set_file(dialog, file, nullptr);
  } else {
    gtk_file_chooser_set_filename(dialog, path.value().c_str());
  }
}

int GtkDialogSelectedFilterIndex(GtkWidget* dialog) {
  GtkFileFilter* selected_filter =
      gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(dialog));
  int idx = -1;
  if (GtkCheckVersion(4)) {
    auto filters =
        TakeGObject(gtk_file_chooser_get_filters(GTK_FILE_CHOOSER(dialog)));
    int size = g_list_model_get_n_items(filters);
    for (; idx < size; ++idx) {
      if (g_list_model_get_item(filters, idx) == selected_filter)
        break;
    }
  } else {
    GSList* filters = gtk_file_chooser_list_filters(GTK_FILE_CHOOSER(dialog));
    idx = g_slist_index(filters, selected_filter);
    g_slist_free(filters);
  }
  return idx;
}

std::string GtkFileChooserGetFilename(GtkWidget* dialog) {
  const char* filename = nullptr;
  struct GFreeDeleter {
    void operator()(gchar* ptr) const { g_free(ptr); }
  };
  std::unique_ptr<gchar, GFreeDeleter> gchar_filename;
  if (GtkCheckVersion(4)) {
    if (auto file =
            TakeGObject(gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog)))) {
      filename = g_file_peek_path(file);
    }
  } else {
    gchar_filename = std::unique_ptr<gchar, GFreeDeleter>(
        gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)));
    filename = gchar_filename.get();
  }
  return filename ? std::string(filename) : std::string();
}

std::vector<base::FilePath> GtkFileChooserGetFilenames(GtkWidget* dialog) {
  std::vector<base::FilePath> filenames_fp;
  if (GtkCheckVersion(4)) {
    auto files = Gtk4FileChooserGetFiles(GTK_FILE_CHOOSER(dialog));
    auto size = g_list_model_get_n_items(files);
    for (unsigned int i = 0; i < size; ++i) {
      auto file = TakeGObject(G_FILE(g_list_model_get_object(files, i)));
      filenames_fp.emplace_back(g_file_peek_path(file));
    }
  } else {
    GSList* filenames =
        gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
    if (!filenames)
      return {};
    for (GSList* iter = filenames; iter != nullptr; iter = g_slist_next(iter)) {
      base::FilePath path(static_cast<char*>(iter->data));
      g_free(iter->data);
      filenames_fp.push_back(path);
    }
    g_slist_free(filenames);
  }
  return filenames_fp;
}

}  // namespace

// The size of the preview we display for selected image files. We set height
// larger than width because generally there is more free space vertically
// than horizontally (setting the preview image will always expand the width of
// the dialog, but usually not the height). The image's aspect ratio will always
// be preserved.  Only used on GTK3.
static const int kPreviewWidth = 256;
static const int kPreviewHeight = 512;

SelectFileDialogLinuxGtk::SelectFileDialogLinuxGtk(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : SelectFileDialogLinux(listener, std::move(policy)) {}

SelectFileDialogLinuxGtk::~SelectFileDialogLinuxGtk() {
  for (auto* window : parents_)
    window->RemoveObserver(this);
  auto dialogs = dialogs_;
  for (auto dialog_signal : dialogs) {
    auto* dialog = dialog_signal.first;
    auto signal_id = dialog_signal.second;
    g_signal_handler_disconnect(dialog, signal_id);
    GtkWindowDestroy(dialog);
    OnFileChooserDestroy(dialog);
  }
}

bool SelectFileDialogLinuxGtk::IsRunning(
    gfx::NativeWindow parent_window) const {
  return parents_.find(parent_window) != parents_.end();
}

bool SelectFileDialogLinuxGtk::HasMultipleFileTypeChoicesImpl() {
  return file_types().extensions.size() > 1;
}

void SelectFileDialogLinuxGtk::OnWindowDestroying(aura::Window* window) {
  // Remove the |parent| property associated with the |dialog|.
  for (auto dialog_signal : dialogs_) {
    auto* dialog = dialog_signal.first;
    aura::Window* parent = GetAuraTransientParent(dialog);
    if (parent == window)
      ClearAuraTransientParent(dialog, parent);
  }

  std::set<aura::Window*>::iterator iter = parents_.find(window);
  if (iter != parents_.end()) {
    (*iter)->RemoveObserver(this);
    parents_.erase(iter);
  }
}

// We ignore |default_extension|.
void SelectFileDialogLinuxGtk::SelectFileImpl(
    Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    void* params,
    const GURL* caller) {
  set_type(type);
  if (owning_window) {
    owning_window->AddObserver(this);
    parents_.insert(owning_window);
  }

  std::string title_string = base::UTF16ToUTF8(title);

  set_file_type_index(file_type_index);
  if (file_types)
    set_file_types(*file_types);

  GtkWidget* dialog = nullptr;
  switch (type) {
    case SELECT_FOLDER:
    case SELECT_UPLOAD_FOLDER:
    case SELECT_EXISTING_FOLDER:
      dialog = CreateSelectFolderDialog(type, title_string, default_path,
                                        owning_window);
      break;
    case SELECT_OPEN_FILE:
      dialog = CreateFileOpenDialog(title_string, default_path, owning_window);
      break;
    case SELECT_OPEN_MULTI_FILE:
      dialog =
          CreateMultiFileOpenDialog(title_string, default_path, owning_window);
      break;
    case SELECT_SAVEAS_FILE:
      dialog = CreateSaveAsDialog(title_string, default_path, owning_window);
      break;
    case SELECT_NONE:
      NOTREACHED();
      return;
  }
  if (GtkCheckVersion(4)) {
    gtk_window_set_hide_on_close(GTK_WINDOW(dialog), true);
  } else {
    g_signal_connect(dialog, "delete-event",
                     G_CALLBACK(gtk_widget_hide_on_delete), nullptr);
  }

  dialogs_[dialog] = g_signal_connect(
      dialog, "destroy", G_CALLBACK(OnFileChooserDestroyThunk), this);

  if (!GtkCheckVersion(4)) {
    preview_ = gtk_image_new();
    g_signal_connect(dialog, "update-preview", G_CALLBACK(OnUpdatePreviewThunk),
                     this);
    gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(dialog), preview_);
  }

  params_map_[dialog] = params;

  // Disable input events handling in the host window to make this dialog modal.
  if (owning_window) {
    views::DesktopWindowTreeHostLinux* host =
        static_cast<views::DesktopWindowTreeHostLinux*>(
            owning_window->GetHost());
    if (host) {
      // In some circumstances (e.g. dialog from flash plugin) the mouse has
      // been captured and by turning off event listening, it is never
      // released. So we manually ensure there is no current capture.
      host->ReleaseCapture();
      std::unique_ptr<base::OnceClosure> callback =
          std::make_unique<base::OnceClosure>(host->DisableEventListening());
      // OnFilePickerDestroy() is called when |dialog| destroyed, which allows
      // to invoke the callback function to re-enable event handling on the
      // owning window.
      g_object_set_data_full(
          G_OBJECT(dialog), "callback", callback.release(),
          reinterpret_cast<GDestroyNotify>(OnFilePickerDestroy));
      gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    }
  }

  if (!GtkCheckVersion(4))
    gtk_widget_show_all(dialog);
  gtk::GtkUi::GetPlatform()->ShowGtkWindow(GTK_WINDOW(dialog));
}

void SelectFileDialogLinuxGtk::AddFilters(GtkFileChooser* chooser) {
  for (size_t i = 0; i < file_types().extensions.size(); ++i) {
    GtkFileFilter* filter = nullptr;
    std::set<std::string> fallback_labels;

    for (const auto& current_extension : file_types().extensions[i]) {
      if (!current_extension.empty()) {
        if (!filter)
          filter = gtk_file_filter_new();
        for (std::string pattern :
             {"*." + base::ToLowerASCII(current_extension),
              "*." + base::ToUpperASCII(current_extension)}) {
          gtk_file_filter_add_pattern(filter, pattern.c_str());
          fallback_labels.insert(pattern);
        }
      }
    }
    // We didn't find any non-empty extensions to filter on.
    if (!filter)
      continue;

    // The description vector may be blank, in which case we are supposed to
    // use some sort of default description based on the filter.
    if (i < file_types().extension_description_overrides.size()) {
      gtk_file_filter_set_name(
          filter,
          base::UTF16ToUTF8(file_types().extension_description_overrides[i])
              .c_str());
    } else {
      // There is no system default filter description so we use
      // the extensions themselves if the description is blank.
      std::vector<std::string> fallback_labels_vector(fallback_labels.begin(),
                                                      fallback_labels.end());
      std::string fallback_label =
          base::JoinString(fallback_labels_vector, ",");
      gtk_file_filter_set_name(filter, fallback_label.c_str());
    }

    gtk_file_chooser_add_filter(chooser, filter);
    if (i == file_type_index() - 1)
      gtk_file_chooser_set_filter(chooser, filter);
  }

  // Add the *.* filter, but only if we have added other filters (otherwise it
  // is implied).
  if (file_types().include_all_files && !file_types().extensions.empty()) {
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_filter_set_name(
        filter, l10n_util::GetStringUTF8(IDS_SAVEAS_ALL_FILES).c_str());
    gtk_file_chooser_add_filter(chooser, filter);
  }
}

void SelectFileDialogLinuxGtk::FileSelected(GtkWidget* dialog,
                                            const base::FilePath& path) {
  if (type() == SELECT_SAVEAS_FILE) {
    set_last_saved_path(path.DirName());
  } else if (type() == SELECT_OPEN_FILE || type() == SELECT_FOLDER ||
             type() == SELECT_UPLOAD_FOLDER ||
             type() == SELECT_EXISTING_FOLDER) {
    set_last_opened_path(path.DirName());
  } else {
    NOTREACHED();
  }

  if (listener_) {
    listener_->FileSelected(path, GtkDialogSelectedFilterIndex(dialog) + 1,
                            PopParamsForDialog(dialog));
  }
  GtkWindowDestroy(dialog);
}

void SelectFileDialogLinuxGtk::MultiFilesSelected(
    GtkWidget* dialog,
    const std::vector<base::FilePath>& files) {
  set_last_opened_path(files[0].DirName());

  if (listener_)
    listener_->MultiFilesSelected(files, PopParamsForDialog(dialog));
  GtkWindowDestroy(dialog);
}

void SelectFileDialogLinuxGtk::FileNotSelected(GtkWidget* dialog) {
  void* params = PopParamsForDialog(dialog);
  if (listener_)
    listener_->FileSelectionCanceled(params);
  GtkWindowDestroy(dialog);
}

GtkWidget* SelectFileDialogLinuxGtk::CreateFileOpenHelper(
    const std::string& title,
    const base::FilePath& default_path,
    gfx::NativeWindow parent) {
  GtkWidget* dialog = GtkFileChooserDialogNew(
      title.c_str(), nullptr, GTK_FILE_CHOOSER_ACTION_OPEN, GetCancelLabel(),
      GTK_RESPONSE_CANCEL, GetOpenLabel(), GTK_RESPONSE_ACCEPT);
  SetGtkTransientForAura(dialog, parent);
  AddFilters(GTK_FILE_CHOOSER(dialog));

  if (!default_path.empty()) {
    if (CallDirectoryExistsOnUIThread(default_path)) {
      GtkFileChooserSetCurrentFolder(GTK_FILE_CHOOSER(dialog), default_path);
    } else {
      // If the file doesn't exist, this will just switch to the correct
      // directory. That's good enough.
      GtkFileChooserSetFilename(GTK_FILE_CHOOSER(dialog), default_path);
    }
  } else if (!last_opened_path()->empty()) {
    GtkFileChooserSetCurrentFolder(GTK_FILE_CHOOSER(dialog),
                                   *last_opened_path());
  }
  return dialog;
}

GtkWidget* SelectFileDialogLinuxGtk::CreateSelectFolderDialog(
    Type type,
    const std::string& title,
    const base::FilePath& default_path,
    gfx::NativeWindow parent) {
  std::string title_string = title;
  if (title_string.empty()) {
    title_string =
        (type == SELECT_UPLOAD_FOLDER)
            ? l10n_util::GetStringUTF8(IDS_SELECT_UPLOAD_FOLDER_DIALOG_TITLE)
            : l10n_util::GetStringUTF8(IDS_SELECT_FOLDER_DIALOG_TITLE);
  }
  std::string accept_button_label =
      (type == SELECT_UPLOAD_FOLDER)
          ? l10n_util::GetStringUTF8(
                IDS_SELECT_UPLOAD_FOLDER_DIALOG_UPLOAD_BUTTON)
          : GetOpenLabel();

  GtkWidget* dialog = GtkFileChooserDialogNew(
      title_string.c_str(), nullptr, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      GetCancelLabel(), GTK_RESPONSE_CANCEL, accept_button_label.c_str(),
      GTK_RESPONSE_ACCEPT);
  SetGtkTransientForAura(dialog, parent);
  GtkFileChooser* chooser = GTK_FILE_CHOOSER(dialog);
  if (type == SELECT_UPLOAD_FOLDER || type == SELECT_EXISTING_FOLDER)
    gtk_file_chooser_set_create_folders(chooser, FALSE);
  if (!default_path.empty())
    GtkFileChooserSetCurrentFolder(chooser, default_path);
  else if (!last_opened_path()->empty())
    GtkFileChooserSetCurrentFolder(chooser, *last_opened_path());
  GtkFileFilter* only_folders = gtk_file_filter_new();
  gtk_file_filter_set_name(
      only_folders,
      l10n_util::GetStringUTF8(IDS_SELECT_FOLDER_DIALOG_TITLE).c_str());
  gtk_file_filter_add_mime_type(only_folders, "application/x-directory");
  gtk_file_filter_add_mime_type(only_folders, "inode/directory");
  gtk_file_filter_add_mime_type(only_folders, "text/directory");
  gtk_file_chooser_add_filter(chooser, only_folders);
  gtk_file_chooser_set_select_multiple(chooser, FALSE);
  g_signal_connect(dialog, "response",
                   G_CALLBACK(OnSelectSingleFolderDialogResponseThunk), this);
  return dialog;
}

GtkWidget* SelectFileDialogLinuxGtk::CreateFileOpenDialog(
    const std::string& title,
    const base::FilePath& default_path,
    gfx::NativeWindow parent) {
  std::string title_string =
      !title.empty() ? title
                     : l10n_util::GetStringUTF8(IDS_OPEN_FILE_DIALOG_TITLE);

  GtkWidget* dialog = CreateFileOpenHelper(title_string, default_path, parent);
  gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), FALSE);
  g_signal_connect(dialog, "response",
                   G_CALLBACK(OnSelectSingleFileDialogResponseThunk), this);
  return dialog;
}

GtkWidget* SelectFileDialogLinuxGtk::CreateMultiFileOpenDialog(
    const std::string& title,
    const base::FilePath& default_path,
    gfx::NativeWindow parent) {
  std::string title_string =
      !title.empty() ? title
                     : l10n_util::GetStringUTF8(IDS_OPEN_FILES_DIALOG_TITLE);

  GtkWidget* dialog = CreateFileOpenHelper(title_string, default_path, parent);
  gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
  g_signal_connect(dialog, "response",
                   G_CALLBACK(OnSelectMultiFileDialogResponseThunk), this);
  return dialog;
}

GtkWidget* SelectFileDialogLinuxGtk::CreateSaveAsDialog(
    const std::string& title,
    const base::FilePath& default_path,
    gfx::NativeWindow parent) {
  std::string title_string =
      !title.empty() ? title
                     : l10n_util::GetStringUTF8(IDS_SAVE_AS_DIALOG_TITLE);

  GtkWidget* dialog = GtkFileChooserDialogNew(
      title_string.c_str(), nullptr, GTK_FILE_CHOOSER_ACTION_SAVE,
      GetCancelLabel(), GTK_RESPONSE_CANCEL, GetSaveLabel(),
      GTK_RESPONSE_ACCEPT);
  SetGtkTransientForAura(dialog, parent);

  AddFilters(GTK_FILE_CHOOSER(dialog));
  if (!default_path.empty()) {
    if (CallDirectoryExistsOnUIThread(default_path)) {
      // If this is an existing directory, navigate to that directory, with no
      // filename.
      GtkFileChooserSetCurrentFolder(GTK_FILE_CHOOSER(dialog), default_path);
    } else {
      // The default path does not exist, or is an existing file. We use
      // set_current_folder() followed by set_current_name(), as per the
      // recommendation of the GTK docs.
      GtkFileChooserSetCurrentFolder(GTK_FILE_CHOOSER(dialog),
                                     default_path.DirName());
      gtk_file_chooser_set_current_name(
          GTK_FILE_CHOOSER(dialog), default_path.BaseName().value().c_str());
    }
  } else if (!last_saved_path()->empty()) {
    GtkFileChooserSetCurrentFolder(GTK_FILE_CHOOSER(dialog),
                                   *last_saved_path());
  }
  gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), FALSE);
  // Overwrite confirmation is always enabled in GTK4.
  if (!GtkCheckVersion(4)) {
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),
                                                   TRUE);
  }
  g_signal_connect(dialog, "response",
                   G_CALLBACK(OnSelectSingleFileDialogResponseThunk), this);
  return dialog;
}

void* SelectFileDialogLinuxGtk::PopParamsForDialog(GtkWidget* dialog) {
  std::map<GtkWidget*, void*>::iterator iter = params_map_.find(dialog);
  DCHECK(iter != params_map_.end());
  void* params = iter->second;
  params_map_.erase(iter);
  return params;
}

bool SelectFileDialogLinuxGtk::IsCancelResponse(gint response_id) {
  bool is_cancel = response_id == GTK_RESPONSE_CANCEL ||
                   response_id == GTK_RESPONSE_DELETE_EVENT;
  if (is_cancel)
    return true;

  DCHECK(response_id == GTK_RESPONSE_ACCEPT);
  return false;
}

void SelectFileDialogLinuxGtk::SelectSingleFileHelper(GtkWidget* dialog,
                                                      gint response_id,
                                                      bool allow_folder) {
  if (IsCancelResponse(response_id)) {
    FileNotSelected(dialog);
    return;
  }

  auto filename = GtkFileChooserGetFilename(dialog);
  if (filename.empty()) {
    FileNotSelected(dialog);
    return;
  }
  base::FilePath path(filename);

  if (allow_folder) {
    FileSelected(dialog, path);
    return;
  }

  if (CallDirectoryExistsOnUIThread(path))
    FileNotSelected(dialog);
  else
    FileSelected(dialog, path);
}

void SelectFileDialogLinuxGtk::OnSelectSingleFileDialogResponse(
    GtkWidget* dialog,
    int response_id) {
  SelectSingleFileHelper(dialog, response_id, false);
}

void SelectFileDialogLinuxGtk::OnSelectSingleFolderDialogResponse(
    GtkWidget* dialog,
    int response_id) {
  SelectSingleFileHelper(dialog, response_id, true);
}

void SelectFileDialogLinuxGtk::OnSelectMultiFileDialogResponse(
    GtkWidget* dialog,
    int response_id) {
  if (IsCancelResponse(response_id)) {
    FileNotSelected(dialog);
    return;
  }

  auto filenames = GtkFileChooserGetFilenames(dialog);
  filenames.erase(std::remove_if(filenames.begin(), filenames.end(),
                                 [this](const base::FilePath& path) {
                                   return CallDirectoryExistsOnUIThread(path);
                                 }),
                  filenames.end());
  if (filenames.empty()) {
    FileNotSelected(dialog);
    return;
  }
  MultiFilesSelected(dialog, filenames);
}

void SelectFileDialogLinuxGtk::OnFileChooserDestroy(GtkWidget* dialog) {
  dialogs_.erase(dialog);

  // |parent| can be nullptr when closing the host window
  // while opening the file-picker.
  aura::Window* parent = GetAuraTransientParent(dialog);
  if (!parent)
    return;
  ClearAuraTransientParent(dialog, parent);
  std::set<aura::Window*>::iterator iter = parents_.find(parent);
  if (iter != parents_.end()) {
    (*iter)->RemoveObserver(this);
    parents_.erase(iter);
  } else {
    NOTREACHED();
  }
}

void SelectFileDialogLinuxGtk::OnUpdatePreview(GtkWidget* chooser) {
  DCHECK(!GtkCheckVersion(4));
  gchar* filename =
      gtk_file_chooser_get_preview_filename(GTK_FILE_CHOOSER(chooser));
  if (!filename) {
    gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(chooser),
                                               FALSE);
    return;
  }

  // Don't attempt to open anything which isn't a regular file. If a named pipe,
  // this may hang. See https://crbug.com/534754.
  struct stat stat_buf;
  if (stat(filename, &stat_buf) != 0 || !S_ISREG(stat_buf.st_mode)) {
    g_free(filename);
    gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(chooser),
                                               FALSE);
    return;
  }

  // This will preserve the image's aspect ratio.
  GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_size(filename, kPreviewWidth,
                                                       kPreviewHeight, nullptr);
  g_free(filename);
  if (pixbuf) {
    gtk_image_set_from_pixbuf(GTK_IMAGE(preview_), pixbuf);
    g_object_unref(pixbuf);
  }
  gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(chooser),
                                             pixbuf ? TRUE : FALSE);
}

}  // namespace gtk
