// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_FILE_SELECT_HELPER_H_
#define WEBLAYER_BROWSER_FILE_SELECT_HELPER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {
class FileSelectListener;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace ui {
struct SelectedFileInfo;
}

namespace weblayer {

// This class handles file-selection requests coming from renderer processes.
// It implements both the initialisation and listener functions for
// file-selection dialogs.
//
// Since FileSelectHelper listens to observations of a widget, it needs to live
// on and be destroyed on the UI thread. References to FileSelectHelper may be
// passed on to other threads.
class FileSelectHelper : public base::RefCountedThreadSafe<
                             FileSelectHelper,
                             content::BrowserThread::DeleteOnUIThread>,
                         public ui::SelectFileDialog::Listener {
 public:
  FileSelectHelper(const FileSelectHelper&) = delete;
  FileSelectHelper& operator=(const FileSelectHelper&) = delete;

  // Show the file chooser dialog.
  static void RunFileChooser(
      content::RenderFrameHost* render_frame_host,
      scoped_refptr<content::FileSelectListener> listener,
      const blink::mojom::FileChooserParams& params);

 private:
  friend class base::RefCountedThreadSafe<FileSelectHelper>;
  friend class base::DeleteHelper<FileSelectHelper>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;

  FileSelectHelper();
  ~FileSelectHelper() override;

  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      blink::mojom::FileChooserParamsPtr params);

  // Cleans up and releases this instance. This must be called after the last
  // callback is received from the file chooser dialog.
  void RunFileChooserEnd();

  // SelectFileDialog::Listener overrides.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectedWithExtraInfo(const ui::SelectedFileInfo& file,
                                 int index,
                                 void* params) override;
  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override;
  void MultiFilesSelectedWithExtraInfo(
      const std::vector<ui::SelectedFileInfo>& files,
      void* params) override;
  void FileSelectionCanceled(void* params) override;

  // This method is called after the user has chosen the file(s) in the UI in
  // order to process and filter the list before returning the final result to
  // the caller.
  void ConvertToFileChooserFileInfoList(
      const std::vector<ui::SelectedFileInfo>& files);

  // A weak pointer to the WebContents of the RenderFrameHost, for life checks.
  base::WeakPtr<content::WebContents> web_contents_;

  // |listener_| receives the result of the FileSelectHelper.
  scoped_refptr<content::FileSelectListener> listener_;

  // Dialog box used for choosing files to upload from file form fields.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // The type of file dialog last shown.
  ui::SelectFileDialog::Type dialog_type_ =
      ui::SelectFileDialog::SELECT_OPEN_FILE;

  // The mode of file dialog last shown.
  blink::mojom::FileChooserParams::Mode dialog_mode_ =
      blink::mojom::FileChooserParams::Mode::kOpen;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_FILE_SELECT_HELPER_H_
