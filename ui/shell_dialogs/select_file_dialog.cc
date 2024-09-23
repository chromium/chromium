// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog.h"

#include <stddef.h>
#include <algorithm>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/task/single_thread_task_runner.h"
#include "base/third_party/icu/icu_utf.h"
#include "build/build_config.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

namespace {

// Optional dialog factory. Leaked.
ui::SelectFileDialogFactory* dialog_factory_ = nullptr;

void TruncateStringToSize(base::FilePath::StringType* string, size_t size) {
  if (string->size() <= size)
    return;
#if BUILDFLAG(IS_WIN)
  const auto* c_str = base::as_u16cstr(string->c_str());
  for (size_t i = 0; i < string->size(); ++i) {
    base_icu::UChar32 codepoint;
    size_t original_i = i;
    if (!base::ReadUnicodeCharacter(c_str, size, &i, &codepoint) || i >= size) {
      string->resize(original_i);
      return;
    }
  }
#else
  base::TruncateUTF8ToByteSize(*string, size, string);
#endif
}

}  // namespace

namespace ui {

void SelectFileDialog::Listener::MultiFilesSelected(
    const std::vector<SelectedFileInfo>& files) {
  NOTREACHED();
}

SelectFileDialog::FileTypeInfo::FileTypeInfo() = default;

SelectFileDialog::FileTypeInfo::FileTypeInfo(const FileTypeInfo& other) =
    default;

SelectFileDialog::FileTypeInfo::FileTypeInfo(FileExtensionList in_extensions)
    : extensions({std::move(in_extensions)}) {}

SelectFileDialog::FileTypeInfo::FileTypeInfo(
    std::vector<FileExtensionList> in_extensions)
    : extensions(std::move(in_extensions)) {}

SelectFileDialog::FileTypeInfo::FileTypeInfo(
    std::vector<FileExtensionList> in_extensions,
    std::vector<std::u16string> in_descriptions)
    : extensions(std::move(in_extensions)),
      extension_description_overrides(std::move(in_descriptions)) {
  CHECK(extension_description_overrides.empty() ||
        extension_description_overrides.size() == extensions.size());
}

SelectFileDialog::FileTypeInfo::~FileTypeInfo() = default;

// static
void SelectFileDialog::SetFactory(
    std::unique_ptr<ui::SelectFileDialogFactory> factory) {
  delete dialog_factory_;
  dialog_factory_ = factory.release();
}

// static
scoped_refptr<SelectFileDialog> SelectFileDialog::Create(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  if (dialog_factory_)
    return dialog_factory_->Create(listener, std::move(policy));
  return CreateSelectFileDialog(listener, std::move(policy));
}

base::FilePath SelectFileDialog::GetShortenedFilePath(
    const base::FilePath& path) {
  const size_t kMaxNameLength = 255;
  if (path.BaseName().value().length() <= kMaxNameLength)
    return path;
  base::FilePath filename = path.BaseName();
  base::FilePath::StringType extension = filename.FinalExtension();
  filename = filename.RemoveFinalExtension();
  base::FilePath::StringType file_string = filename.value();
  // 1 for . plus 12 for longest known extension.
  size_t max_extension_length = 13;
  if (file_string.length() < kMaxNameLength) {
    max_extension_length =
        std::max(max_extension_length, kMaxNameLength - file_string.length());
  }
  // Take the first max_extension_length characters (this will be the
  // leading '.' plus the next max_extension_length - 1).
  TruncateStringToSize(&extension, max_extension_length);
  TruncateStringToSize(&file_string, kMaxNameLength - extension.length());
  return path.DirName().Append(file_string).AddExtension(extension);
}

#if BUILDFLAG(IS_ANDROID)
// These are overridden by Android's SelectFileDialog subclass.
void SelectFileDialog::SetAcceptTypes(std::vector<std::u16string> types) {}
void SelectFileDialog::SetUseMediaCapture(bool use_media_capture) {}
void SelectFileDialog::SetOpenWritable(bool open_writable) {}
#endif

void SelectFileDialog::SelectFile(
    Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    const GURL* caller) {
  DCHECK(listener_);

  if (select_file_policy_.get() &&
      !select_file_policy_->CanOpenSelectFileDialog()) {
    select_file_policy_->SelectFileDenied();

    // Inform the listener that no file was selected.
    // Post a task rather than calling FileSelectionCanceled directly to ensure
    // that the listener is called asynchronously.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&SelectFileDialog::CancelFileSelection, this));
    return;
  }

  base::FilePath path = GetShortenedFilePath(default_path);

  // Call the platform specific implementation of the file selection dialog.
  SelectFileImpl(type, title, path, file_types, file_type_index,
                 default_extension, owning_window, caller);
}

bool SelectFileDialog::HasMultipleFileTypeChoices() {
  return HasMultipleFileTypeChoicesImpl();
}

SelectFileDialog::SelectFileDialog(Listener* listener,
                                   std::unique_ptr<ui::SelectFilePolicy> policy)
    : listener_(listener), select_file_policy_(std::move(policy)) {
  DCHECK(listener_);
}

SelectFileDialog::~SelectFileDialog() {}

void SelectFileDialog::CancelFileSelection() {
  if (listener_)
    listener_->FileSelectionCanceled();
}

}  // namespace ui
