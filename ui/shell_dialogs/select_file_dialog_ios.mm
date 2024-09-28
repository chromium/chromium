// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog_ios.h"

#import <UIKit/UIDocumentPickerViewController.h>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "base/apple/foundation_util.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

@interface NativeFileDialog : NSObject <UIDocumentPickerDelegate> {
 @private
  base::WeakPtr<ui::SelectFileDialogImpl> _dialog;
  UIViewController* __weak _viewController;
  bool _allowMultipleFiles;
  UIDocumentPickerViewController* __strong _documentPickerController;
  NSArray<UTType*>* __strong _fileUTTypeLists;
  bool _allowsOtherFileTypes;
  bool _isDirectory;
}

- (instancetype)initWithDialog:(base::WeakPtr<ui::SelectFileDialogImpl>)dialog
                viewController:(UIViewController*)viewController
            allowMultipleFiles:(bool)allowMultipleFiles
               fileUTTypeLists:(NSArray<UTType*>*)fileUTTypeLists
          allowsOtherFileTypes:(bool)allowsOtherFileTypes
                   isDirectory:(bool)isDirectory;
- (void)dealloc;
- (void)showFilePickerMenu;
- (void)documentPicker:(UIDocumentPickerViewController*)controller
    didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls;
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller;
@end

@implementation NativeFileDialog

- (instancetype)initWithDialog:(base::WeakPtr<ui::SelectFileDialogImpl>)dialog
                viewController:(UIViewController*)viewController
            allowMultipleFiles:(bool)allowMultipleFiles
               fileUTTypeLists:(NSArray<UTType*>*)fileUTTypeLists
          allowsOtherFileTypes:(bool)allowsOtherFileTypes
                   isDirectory:(bool)isDirectory {
  if (!(self = [super init])) {
    return nil;
  }
  _dialog = dialog;
  _viewController = viewController;
  _allowMultipleFiles = allowMultipleFiles;
  _fileUTTypeLists = fileUTTypeLists;
  _allowsOtherFileTypes = allowsOtherFileTypes;
  _isDirectory = isDirectory;
  return self;
}

- (void)dealloc {
  _documentPickerController.delegate = nil;
}

- (void)showFilePickerMenu {
  NSArray* documentTypes = _isDirectory ? @[ UTTypeFolder ] : @[ UTTypeItem ];
  if (!_isDirectory && !_allowsOtherFileTypes) {
    documentTypes = _fileUTTypeLists;
  }
  _documentPickerController = [[UIDocumentPickerViewController alloc]
      initForOpeningContentTypes:documentTypes];
  _documentPickerController.allowsMultipleSelection = _allowMultipleFiles;

  _documentPickerController.delegate = self;

  UIViewController* currentViewController = _viewController;
  [currentViewController presentViewController:_documentPickerController
                                      animated:true
                                    completion:nil];
}

- (void)documentPicker:(UIDocumentPickerViewController*)controller
    didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
  if (!_dialog) {
    return;
  }
  std::vector<base::FilePath> paths;
  for (NSURL* url : urls) {
    if (!url.isFileURL) {
      continue;
    }
    NSString* path = url.path;
    paths.push_back(base::apple::NSStringToFilePath(path));
  }
  _dialog->FileWasSelected(_allowMultipleFiles, false, paths, 0);
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
  if (!_dialog) {
    return;
  }
  std::vector<base::FilePath> paths;
  _dialog->FileWasSelected(_allowMultipleFiles, true, paths, 0);
}

@end

namespace ui {

SelectFileDialogImpl::SelectFileDialogImpl(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : SelectFileDialog(listener, std::move(policy)) {}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow parent_window) const {
  return listener_;
}

void SelectFileDialogImpl::ListenerDestroyed() {
  listener_ = nullptr;
}

void SelectFileDialogImpl::FileWasSelected(
    bool is_multi,
    bool was_cancelled,
    const std::vector<base::FilePath>& files,
    int index) {
  if (!listener_) {
    return;
  }

  if (was_cancelled || files.empty()) {
    listener_->FileSelectionCanceled();
  } else {
    if (is_multi) {
      listener_->MultiFilesSelected(FilePathListToSelectedFileInfoList(files));
    } else {
      listener_->FileSelected(SelectedFileInfo(files[0]), index);
    }
  }
}

void SelectFileDialogImpl::SelectFileImpl(
    SelectFileDialog::Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow gfx_window,
    const GURL* caller) {
  has_multiple_file_type_choices_ =
      SelectFileDialog::SELECT_OPEN_MULTI_FILE == type;
  bool allows_other_file_types = false;
  bool directory = SelectFileDialog::SELECT_FOLDER == type ||
                   SelectFileDialog::SELECT_UPLOAD_FOLDER == type ||
                   SelectFileDialog::SELECT_EXISTING_FOLDER == type;
  NSMutableArray<UTType*>* file_uttype_lists = [NSMutableArray array];
  for (const auto& ext_list : file_types->extensions) {
    for (const base::FilePath::StringType& ext : ext_list) {
      UTType* uttype =
          [UTType typeWithFilenameExtension:base::SysUTF8ToNSString(ext)];
      if (!uttype) {
        continue;
      }

      if (![file_uttype_lists containsObject:uttype]) {
        [file_uttype_lists addObject:uttype];
      }
    }
  }
  if (file_types->include_all_files || file_types->extensions.empty()) {
    allows_other_file_types = true;
  }

  UIViewController* controller = gfx_window.Get().rootViewController;
  native_file_dialog_ =
      [[NativeFileDialog alloc] initWithDialog:weak_factory_.GetWeakPtr()
                                viewController:controller
                            allowMultipleFiles:has_multiple_file_type_choices_
                               fileUTTypeLists:file_uttype_lists
                          allowsOtherFileTypes:allows_other_file_types
                                   isDirectory:directory];
  [native_file_dialog_ showFilePickerMenu];
}

SelectFileDialogImpl::~SelectFileDialogImpl() {
  // Clear |weak_factory_| beforehand, to ensure that no callbacks will be made
  // when we cancel the NSSavePanels.
  weak_factory_.InvalidateWeakPtrs();
}

bool SelectFileDialogImpl::HasMultipleFileTypeChoicesImpl() {
  return has_multiple_file_type_choices_;
}

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy) {
  return new SelectFileDialogImpl(listener, std::move(policy));
}

}  // namespace ui
