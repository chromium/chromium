// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/file_select_helper.h"

#include <string>

#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if defined(OS_ANDROID)
#include "ui/android/view_android.h"
#else
#include "ui/aura/window.h"
#endif

namespace weblayer {
using blink::mojom::FileChooserFileInfo;
using blink::mojom::FileChooserFileInfoPtr;
using blink::mojom::FileChooserParams;
using blink::mojom::FileChooserParamsPtr;

// static
void FileSelectHelper::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<content::FileSelectListener> listener,
    const FileChooserParams& params) {
  // TODO: Should we handle text/json+contacts accept type?

  // FileSelectHelper will keep itself alive until it sends the result
  // message.
  scoped_refptr<FileSelectHelper> file_select_helper(new FileSelectHelper());
  file_select_helper->RunFileChooser(render_frame_host, std::move(listener),
                                     params.Clone());
}

FileSelectHelper::FileSelectHelper() = default;

FileSelectHelper::~FileSelectHelper() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_)
    select_file_dialog_->ListenerDestroyed();
}

void FileSelectHelper::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<content::FileSelectListener> listener,
    FileChooserParamsPtr params) {
  DCHECK(!web_contents());
  DCHECK(listener);
  DCHECK(!listener_);

  listener_ = std::move(listener);
  Observe(content::WebContents::FromRenderFrameHost(render_frame_host));

  select_file_dialog_ = ui::SelectFileDialog::Create(this, nullptr);

  dialog_mode_ = params->mode;
  switch (params->mode) {
    case FileChooserParams::Mode::kOpen:
      dialog_type_ = ui::SelectFileDialog::SELECT_OPEN_FILE;
      break;
    case FileChooserParams::Mode::kOpenMultiple:
      dialog_type_ = ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE;
      break;
    case FileChooserParams::Mode::kUploadFolder:
      // For now we don't support inputs with webkitdirectory in weblayer.
      dialog_type_ = ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE;
      break;
    case FileChooserParams::Mode::kSave:
      dialog_type_ = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
      break;
    default:
      // Prevent warning.
      dialog_type_ = ui::SelectFileDialog::SELECT_OPEN_FILE;
      NOTREACHED();
  }

  gfx::NativeWindow owning_window;
#if defined(OS_ANDROID)
  owning_window = web_contents()->GetNativeView()->GetWindowAndroid();
#else
  owning_window = web_contents()->GetNativeView()->GetToplevelWindow();
#endif

#if defined(OS_ANDROID)
  // Android needs the original MIME types and an additional capture value.
  std::pair<std::vector<base::string16>, bool> accept_types =
      std::make_pair(params->accept_types, params->use_media_capture);
#endif

  // Many of these params are not used in the Android SelectFileDialog
  // implementation, so we can safely pass empty values.
  select_file_dialog_->SelectFile(dialog_type_, base::string16(),
                                  base::FilePath(), nullptr, 0,
                                  base::FilePath::StringType(), owning_window,
#if defined(OS_ANDROID)
                                  &accept_types);
#else
                                  nullptr);
#endif

  // Because this class returns notifications to the RenderViewHost, it is
  // difficult for callers to know how long to keep a reference to this
  // instance. We AddRef() here to keep the instance alive after we return
  // to the caller, until the last callback is received from the file dialog.
  // At that point, we must call RunFileChooserEnd().
  AddRef();
}

void FileSelectHelper::RunFileChooserEnd() {
  if (listener_)
    listener_->FileSelectionCanceled();
  Release();
}

void FileSelectHelper::FileSelected(const base::FilePath& path,
                                    int index,
                                    void* params) {
  FileSelectedWithExtraInfo(ui::SelectedFileInfo(path, path), index, params);
}

void FileSelectHelper::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file,
    int index,
    void* params) {
  ConvertToFileChooserFileInfoList({file});
}

void FileSelectHelper::MultiFilesSelected(
    const std::vector<base::FilePath>& files,
    void* params) {
  std::vector<ui::SelectedFileInfo> selected_files =
      ui::FilePathListToSelectedFileInfoList(files);

  MultiFilesSelectedWithExtraInfo(selected_files, params);
}

void FileSelectHelper::MultiFilesSelectedWithExtraInfo(
    const std::vector<ui::SelectedFileInfo>& files,
    void* params) {
  ConvertToFileChooserFileInfoList(files);
}

void FileSelectHelper::FileSelectionCanceled(void* params) {
  RunFileChooserEnd();
}

void FileSelectHelper::ConvertToFileChooserFileInfoList(
    const std::vector<ui::SelectedFileInfo>& files) {
  if (AbortIfWebContentsDestroyed())
    return;

  std::vector<FileChooserFileInfoPtr> chooser_files;
  for (const auto& file : files) {
    chooser_files.push_back(
        FileChooserFileInfo::NewNativeFile(blink::mojom::NativeFileInfo::New(
            file.local_path,
            base::FilePath(file.display_name).AsUTF16Unsafe())));
  }

  listener_->FileSelected(std::move(chooser_files), base::FilePath(),
                          dialog_mode_);
  listener_ = nullptr;

  // No members should be accessed from here on.
  RunFileChooserEnd();
}

bool FileSelectHelper::AbortIfWebContentsDestroyed() {
  if (web_contents() == nullptr) {
    RunFileChooserEnd();
    return true;
  }

  return false;
}

}  // namespace weblayer
