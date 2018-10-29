// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog_mac.h"

#include <CoreServices/CoreServices.h>
#include <stddef.h>

#include <vector>

#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#import "ui/base/cocoa/nib_loading.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

const int kFileTypePopupTag = 1234;

CFStringRef CreateUTIFromExtension(const base::FilePath::StringType& ext) {
  base::ScopedCFTypeRef<CFStringRef> ext_cf(base::SysUTF8ToCFStringRef(ext));
  return UTTypeCreatePreferredIdentifierForTag(
      kUTTagClassFilenameExtension, ext_cf.get(), NULL);
}

NSString* GetDescriptionFromExtension(const base::FilePath::StringType& ext) {
  base::ScopedCFTypeRef<CFStringRef> uti(CreateUTIFromExtension(ext));
  base::ScopedCFTypeRef<CFStringRef> description(
      UTTypeCopyDescription(uti.get()));

  if (description && CFStringGetLength(description))
    return [[base::mac::CFToNSCast(description.get()) retain] autorelease];

  // In case no description is found, create a description based on the
  // unknown extension type (i.e. if the extension is .qqq, the we create
  // a description "QQQ File (.qqq)").
  base::string16 ext_name = base::UTF8ToUTF16(ext);
  return l10n_util::GetNSStringF(IDS_APP_SAVEAS_EXTENSION_FORMAT,
                                 base::i18n::ToUpper(ext_name), ext_name);
}

}  // namespace

// A bridge class to act as the modal delegate to the save/open sheet and send
// the results to the C++ class.
@interface SelectFileDialogBridge : NSObject<NSOpenSavePanelDelegate> {
 @private
  ui::SelectFileDialogImpl* selectFileDialogImpl_;  // WEAK; owns us
}

- (id)initWithSelectFileDialogImpl:(ui::SelectFileDialogImpl*)s;
- (void)selectFileDialogImplWillBeDestroyed;
- (void)endedPanel:(NSSavePanel*)panel
         didCancel:(bool)did_cancel
              type:(ui::SelectFileDialog::Type)type
      parentWindow:(NSWindow*)parentWindow;

// NSSavePanel delegate method
- (BOOL)panel:(id)sender shouldEnableURL:(NSURL *)url;

@end

// Target for NSPopupButton control in file dialog's accessory view.
@interface ExtensionDropdownHandler : NSObject {
 @private
  // The file dialog to which this target object corresponds. Weak reference
  // since the dialog_ will stay alive longer than this object.
  NSSavePanel* dialog_;

  // An array whose each item corresponds to an array of different extensions in
  // an extension group.
  base::scoped_nsobject<NSArray> fileTypeLists_;
}

- (id)initWithDialog:(NSSavePanel*)dialog fileTypeLists:(NSArray*)fileTypeLists;

- (void)popupAction:(id)sender;
@end

namespace ui {

SelectFileDialogImpl::SelectFileDialogImpl(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : SelectFileDialog(listener, std::move(policy)),
      bridge_(
          [[SelectFileDialogBridge alloc] initWithSelectFileDialogImpl:this]) {}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow parent_window) const {
  return parents_.find(parent_window.GetNativeNSWindow()) != parents_.end();
}

void SelectFileDialogImpl::ListenerDestroyed() {
  listener_ = NULL;
}

void SelectFileDialogImpl::FileWasSelected(
    NSSavePanel* dialog,
    NSWindow* parent_window,
    bool was_cancelled,
    bool is_multi,
    const std::vector<base::FilePath>& files,
    int index) {
  parents_.erase(parent_window);

  auto it = dialog_data_map_.find(dialog);
  DCHECK(it != dialog_data_map_.end());
  void* params = it->second.params;
  dialog_data_map_.erase(it);

  [dialog setDelegate:nil];

  if (!listener_)
    return;

  if (was_cancelled || files.empty()) {
    listener_->FileSelectionCanceled(params);
  } else {
    if (is_multi) {
      listener_->MultiFilesSelected(files, params);
    } else {
      listener_->FileSelected(files[0], index, params);
    }
  }
}

void SelectFileDialogImpl::SelectFileImpl(
    Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_native_window,
    void* params) {
  DCHECK(type == SELECT_FOLDER || type == SELECT_UPLOAD_FOLDER ||
         type == SELECT_EXISTING_FOLDER || type == SELECT_OPEN_FILE ||
         type == SELECT_OPEN_MULTI_FILE || type == SELECT_SAVEAS_FILE);
  NSWindow* owning_window = owning_native_window.GetNativeNSWindow();
  parents_.insert(owning_window);

  // Note: we need to retain the dialog as owning_window can be null.
  // (See http://crbug.com/29213 .)
  NSSavePanel* dialog;
  if (type == SELECT_SAVEAS_FILE)
    dialog = [[NSSavePanel savePanel] retain];
  else
    dialog = [[NSOpenPanel openPanel] retain];

  if (!title.empty())
    [dialog setMessage:base::SysUTF16ToNSString(title)];

  NSString* default_dir = nil;
  NSString* default_filename = nil;
  if (!default_path.empty()) {
    // The file dialog is going to do a ton of stats anyway. Not much
    // point in eliminating this one.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (base::DirectoryExists(default_path)) {
      default_dir = base::SysUTF8ToNSString(default_path.value());
    } else {
      default_dir = base::SysUTF8ToNSString(default_path.DirName().value());
      default_filename =
          base::SysUTF8ToNSString(default_path.BaseName().value());
    }
  }

  base::scoped_nsobject<ExtensionDropdownHandler> handler;
  if (type != SELECT_FOLDER && type != SELECT_UPLOAD_FOLDER &&
      type != SELECT_EXISTING_FOLDER) {
    if (file_types) {
      handler = SelectFileDialogImpl::SetAccessoryView(
          dialog, file_types, file_type_index, default_extension);
    } else {
      // If no type info is specified, anything goes.
      [dialog setAllowsOtherFileTypes:YES];
    }
  }

  auto inserted = dialog_data_map_.insert(
      std::make_pair(dialog, DialogData(params, handler)));
  DCHECK(inserted.second);  // Dialog should never exist already.

  hasMultipleFileTypeChoices_ =
      file_types ? file_types->extensions.size() > 1 : true;

  if (type == SELECT_SAVEAS_FILE) {
    // When file extensions are hidden and removing the extension from
    // the default filename gives one which still has an extension
    // that OS X recognizes, it will get confused and think the user
    // is trying to override the default extension. This happens with
    // filenames like "foo.tar.gz" or "ball.of.tar.png". Work around
    // this by never hiding extensions in that case.
    base::FilePath::StringType penultimate_extension =
        default_path.RemoveFinalExtension().FinalExtension();
    if (!penultimate_extension.empty() &&
        penultimate_extension.length() <= 5U) {
      [dialog setExtensionHidden:NO];
    } else {
      [dialog setCanSelectHiddenExtension:YES];
    }
  } else {
    NSOpenPanel* open_dialog = base::mac::ObjCCastStrict<NSOpenPanel>(dialog);

    if (type == SELECT_OPEN_MULTI_FILE)
      [open_dialog setAllowsMultipleSelection:YES];
    else
      [open_dialog setAllowsMultipleSelection:NO];

    if (type == SELECT_FOLDER || type == SELECT_UPLOAD_FOLDER ||
        type == SELECT_EXISTING_FOLDER) {
      [open_dialog setCanChooseFiles:NO];
      [open_dialog setCanChooseDirectories:YES];

      if (type == SELECT_FOLDER)
        [open_dialog setCanCreateDirectories:YES];
      else
        [open_dialog setCanCreateDirectories:NO];

      NSString *prompt = (type == SELECT_UPLOAD_FOLDER)
          ? l10n_util::GetNSString(IDS_SELECT_UPLOAD_FOLDER_BUTTON_TITLE)
          : l10n_util::GetNSString(IDS_SELECT_FOLDER_BUTTON_TITLE);
      [open_dialog setPrompt:prompt];
    } else {
      [open_dialog setCanChooseFiles:YES];
      [open_dialog setCanChooseDirectories:NO];
    }

    [open_dialog setDelegate:bridge_.get()];
  }
  if (default_dir)
    [dialog setDirectoryURL:[NSURL fileURLWithPath:default_dir]];
  if (default_filename)
    [dialog setNameFieldStringValue:default_filename];

  // Ensure the bridge (rather than |this|) is retained by the block.
  SelectFileDialogBridge* bridge = bridge_.get();
  [dialog beginSheetModalForWindow:owning_window
                 completionHandler:^(NSInteger result) {
                   [bridge endedPanel:dialog
                            didCancel:result != NSFileHandlingPanelOKButton
                                 type:type
                         parentWindow:owning_window];

                   // Balance the setDelegate above. Note this should usually
                   // have been done already in FileWasSelected().
                   [dialog setDelegate:nil];

                   // Balance the retain at the start of SelectFileImpl().
                   [dialog release];
                 }];
}

SelectFileDialogImpl::DialogData::DialogData(
    void* params_,
    base::scoped_nsobject<ExtensionDropdownHandler> handler)
    : params(params_), extension_dropdown_handler(handler) {}

SelectFileDialogImpl::DialogData::DialogData(const DialogData& other) = default;

SelectFileDialogImpl::DialogData::~DialogData() {}

SelectFileDialogImpl::~SelectFileDialogImpl() {
  // Walk through the open dialogs and close them all.  Use a temporary vector
  // to hold the pointers, since we can't delete from the map as we're iterating
  // through it.
  std::vector<NSSavePanel*> panels;
  for (const auto& value : dialog_data_map_)
    panels.push_back(value.first);

  for (const auto& panel : panels)
    [panel cancel:panel];

  // Running |cancel| on all the panels should have run all the completion
  // handlers, but retaining references to C++ objects inside an NSObject can
  // result in subtle problems. Ensure the reference to |this| is cleared.
  DCHECK(dialog_data_map_.empty());
  [bridge_ selectFileDialogImplWillBeDestroyed];
}

// static
base::scoped_nsobject<ExtensionDropdownHandler>
SelectFileDialogImpl::SetAccessoryView(
    NSSavePanel* dialog,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension) {
  DCHECK(file_types);
  NSView* accessory_view = ui::GetViewFromNib(@"SaveAccessoryView");
  if (!accessory_view)
    return base::scoped_nsobject<ExtensionDropdownHandler>();
  [dialog setAccessoryView:accessory_view];

  NSPopUpButton* popup = [accessory_view viewWithTag:kFileTypePopupTag];
  DCHECK(popup);

  // Create an array with each item corresponding to an array of different
  // extensions in an extension group.
  NSMutableArray* file_type_lists = [NSMutableArray array];
  int default_extension_index = -1;
  for (size_t i = 0; i < file_types->extensions.size(); ++i) {
    const std::vector<base::FilePath::StringType>& ext_list =
        file_types->extensions[i];

    // Generate type description for the extension group.
    NSString* type_description = nil;
    if (i < file_types->extension_description_overrides.size() &&
        !file_types->extension_description_overrides[i].empty()) {
      type_description = base::SysUTF16ToNSString(
          file_types->extension_description_overrides[i]);
    } else {
      // No description given for a list of extensions; pick the first one
      // from the list (arbitrarily) and use its description.
      DCHECK(!ext_list.empty());
      type_description = GetDescriptionFromExtension(ext_list[0]);
    }
    DCHECK_NE(0u, [type_description length]);
    [popup addItemWithTitle:type_description];

    // Populate file_type_lists.
    // Set to store different extensions in the current extension group.
    NSMutableSet* file_type_set = [NSMutableSet set];
    for (const base::FilePath::StringType& ext : ext_list) {
      if (ext == default_extension)
        default_extension_index = i;

      // Crash reports suggest that CreateUTIFromExtension may return nil. Hence
      // we nil check before adding to |file_type_set|. See crbug.com/630101 and
      // rdar://27490414.
      base::ScopedCFTypeRef<CFStringRef> uti(CreateUTIFromExtension(ext));
      if (uti)
        [file_type_set addObject:base::mac::CFToNSCast(uti.get())];

      // Always allow the extension itself, in case the UTI doesn't map
      // back to the original extension correctly. This occurs with dynamic
      // UTIs on 10.7 and 10.8.
      // See http://crbug.com/148840, http://openradar.me/12316273
      base::ScopedCFTypeRef<CFStringRef> ext_cf(
          base::SysUTF8ToCFStringRef(ext));
      [file_type_set addObject:base::mac::CFToNSCast(ext_cf.get())];
    }
    [file_type_lists addObject:[file_type_set allObjects]];
  }

  if (file_types->include_all_files || file_types->extensions.empty()) {
    [popup addItemWithTitle:l10n_util::GetNSString(IDS_APP_SAVEAS_ALL_FILES)];
    [dialog setAllowsOtherFileTypes:YES];
  }

  base::scoped_nsobject<ExtensionDropdownHandler> handler(
      [[ExtensionDropdownHandler alloc] initWithDialog:dialog
                                         fileTypeLists:file_type_lists]);

  // This establishes a weak reference to handler. Hence we persist it as part
  // of dialog_data_map_.
  [popup setTarget:handler];
  [popup setAction:@selector(popupAction:)];

  // file_type_index uses 1 based indexing.
  if (file_type_index) {
    DCHECK_LE(static_cast<size_t>(file_type_index),
              file_types->extensions.size());
    DCHECK_GE(file_type_index, 1);
    [popup selectItemAtIndex:file_type_index - 1];
    [handler popupAction:popup];
  } else if (!default_extension.empty() && default_extension_index != -1) {
    [popup selectItemAtIndex:default_extension_index];
    [dialog
        setAllowedFileTypes:@[ base::SysUTF8ToNSString(default_extension) ]];
  } else {
    // Select the first item.
    [popup selectItemAtIndex:0];
    [handler popupAction:popup];
  }

  return handler;
}

bool SelectFileDialogImpl::HasMultipleFileTypeChoicesImpl() {
  return hasMultipleFileTypeChoices_;
}

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy) {
  return new SelectFileDialogImpl(listener, std::move(policy));
}

}  // namespace ui

@implementation SelectFileDialogBridge

- (id)initWithSelectFileDialogImpl:(ui::SelectFileDialogImpl*)s {
  if ((self = [super init])) {
    selectFileDialogImpl_ = s;
  }
  return self;
}

- (void)selectFileDialogImplWillBeDestroyed {
  selectFileDialogImpl_ = nullptr;
}

- (void)endedPanel:(NSSavePanel*)panel
         didCancel:(bool)did_cancel
              type:(ui::SelectFileDialog::Type)type
      parentWindow:(NSWindow*)parentWindow {
  if (!selectFileDialogImpl_)
    return;

  int index = 0;
  std::vector<base::FilePath> paths;
  if (!did_cancel) {
    if (type == ui::SelectFileDialog::SELECT_SAVEAS_FILE) {
      if ([[panel URL] isFileURL]) {
        paths.push_back(base::mac::NSStringToFilePath([[panel URL] path]));
      }

      NSView* accessoryView = [panel accessoryView];
      if (accessoryView) {
        NSPopUpButton* popup = [accessoryView viewWithTag:kFileTypePopupTag];
        if (popup) {
          // File type indexes are 1-based.
          index = [popup indexOfSelectedItem] + 1;
        }
      } else {
        index = 1;
      }
    } else {
      CHECK([panel isKindOfClass:[NSOpenPanel class]]);
      NSArray* urls = [static_cast<NSOpenPanel*>(panel) URLs];
      for (NSURL* url in urls)
        if ([url isFileURL])
          paths.push_back(base::FilePath(base::SysNSStringToUTF8([url path])));
    }
  }

  bool isMulti = type == ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE;
  selectFileDialogImpl_->FileWasSelected(panel,
                                         parentWindow,
                                         did_cancel,
                                         isMulti,
                                         paths,
                                         index);
}

- (BOOL)panel:(id)sender shouldEnableURL:(NSURL *)url {
  return [url isFileURL];
}

- (BOOL)panel:(id)sender validateURL:(NSURL*)url error:(NSError**)outError {
  // Refuse to accept users closing the dialog with a key repeat, since the key
  // may have been first pressed while the user was looking at insecure content.
  // See https://crbug.com/637098.
  if ([[NSApp currentEvent] type] == NSKeyDown &&
      [[NSApp currentEvent] isARepeat]) {
    return NO;
  }

  return YES;
}

@end

@implementation ExtensionDropdownHandler

- (id)initWithDialog:(NSSavePanel*)dialog
       fileTypeLists:(NSArray*)fileTypeLists {
  if ((self = [super init])) {
    dialog_ = dialog;
    fileTypeLists_.reset([fileTypeLists retain]);
  }
  return self;
}

- (void)popupAction:(id)sender {
  NSUInteger index = [sender indexOfSelectedItem];
  if (index < [fileTypeLists_ count]) {
    // For save dialogs, this causes the first item in the allowedFileTypes
    // array to be used as the extension for the save panel.
    [dialog_ setAllowedFileTypes:[fileTypeLists_ objectAtIndex:index]];
  } else {
    // The user selected "All files" option.
    [dialog_ setAllowedFileTypes:nil];
  }
}

@end
