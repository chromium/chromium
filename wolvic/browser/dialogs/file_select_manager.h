// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_DIALOGS_FILE_SELECT_MANAGER_H_
#define WOLVIC_BROWSER_DIALOGS_FILE_SELECT_MANAGER_H_

#include <memory>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/file_select_listener.h"

namespace content {
class FileSelectListener;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace wolvic {

// This class handles file-selection requests coming from renderer processes.
// It implements both the initialisation and listener functions for
// file-selection dialogs on Java.
class FileSelectManager : public base::RefCountedThreadSafe<FileSelectManager,
                          content::BrowserThread::DeleteOnUIThread> {
 public:
  FileSelectManager(const FileSelectManager&) = delete;
  FileSelectManager& operator=(const FileSelectManager&) = delete;

  // Show the file chooser dialog.
  static void RunFileChooser(
      content::RenderFrameHost* render_frame_host,
      scoped_refptr<content::FileSelectListener> listener,
      const blink::mojom::FileChooserParams& params);

  void OnFileSelected(
      JNIEnv* env, const base::android::JavaParamRef<jstring>& filepath);
  void OnMultipleFilesSelected(
      JNIEnv* env, const base::android::JavaParamRef<jobjectArray>& filepaths);
  void OnFileSelectionCanceled(JNIEnv* env);

 private:
  friend class base::RefCountedThreadSafe<FileSelectManager>;
  friend class base::DeleteHelper<FileSelectManager>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;

  FileSelectManager();
  ~FileSelectManager();

  void RunSelectFile(content::RenderFrameHost* render_frame_host,
                     scoped_refptr<content::FileSelectListener> listener,
                     const blink::mojom::FileChooserParams& params);

  // Cleans up and releases this instance. This must be called after the last
  // callback is received from the file chooser dialog.
  void RunSelectFileEnd();

  // This method is called after the user has chosen the file(s) in the UI in
  // order to process and filter the list before returning the final result to
  // the caller.
  void ConvertToFileChooserFileInfoList(
      const std::vector<base::FilePath>& files);

  // A weak pointer to the WebContents of the RenderFrameHost, for life checks.
  base::WeakPtr<content::WebContents> web_contents_;

  // |listener_| receives the result of the FileSelectManager.
  scoped_refptr<content::FileSelectListener> listener_;

  base::android::ScopedJavaGlobalRef<jobject> java_impl_;

  // The mode of file dialog last shown.
  blink::mojom::FileChooserParams::Mode dialog_mode_ =
      blink::mojom::FileChooserParams::Mode::kOpen;
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_DIALOGS_FILE_SELECT_MANAGER_H_
