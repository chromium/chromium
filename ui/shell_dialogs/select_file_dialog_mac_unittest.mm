// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#import "ui/shell_dialogs/select_file_dialog_mac.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/foundation_util.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/mac/mac_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/remote_cocoa/app_shim/select_file_dialog_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/shell_dialogs/select_file_policy.h"

#define EXPECT_EQ_BOOL(a, b) \
  EXPECT_EQ(static_cast<bool>(a), static_cast<bool>(b))

namespace {
const int kFileTypePopupTag = 1234;

// Returns a vector containing extension descriptions for a given popup.
std::vector<std::u16string> GetExtensionDescriptionList(NSPopUpButton* popup) {
  std::vector<std::u16string> extension_descriptions;
  for (NSString* description in popup.itemTitles) {
    extension_descriptions.push_back(base::SysNSStringToUTF16(description));
  }
  return extension_descriptions;
}

// Returns the NSPopupButton associated with the given `panel`.
NSPopUpButton* GetPopup(NSSavePanel* panel) {
  return [panel.accessoryView viewWithTag:kFileTypePopupTag];
}

}  // namespace

namespace ui::test {

// Helper test base to initialize SelectFileDialogImpl.
class SelectFileDialogMacTest : public PlatformTest,
                                public SelectFileDialog::Listener {
 public:
  SelectFileDialogMacTest()
      : dialog_(new SelectFileDialogImpl(this, nullptr)) {}
  SelectFileDialogMacTest(const SelectFileDialogMacTest&) = delete;
  SelectFileDialogMacTest& operator=(const SelectFileDialogMacTest&) = delete;

 protected:
  base::test::TaskEnvironment task_environment_ = base::test::TaskEnvironment(
      base::test::TaskEnvironment::MainThreadType::UI);

  struct FileDialogArguments {
    SelectFileDialog::Type type = SelectFileDialog::SELECT_SAVEAS_FILE;
    std::u16string title;
    base::FilePath default_path;
    raw_ptr<SelectFileDialog::FileTypeInfo> file_types = nullptr;
    int file_type_index = 0;
    base::FilePath::StringType default_extension;
  };

  // Helper method to create a dialog with the given `args`. Returns the created
  // NSSavePanel.
  NSSavePanel* SelectFileWithParams(FileDialogArguments args) {
    NSWindow* parent_window =
        [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                    styleMask:NSWindowStyleMaskTitled
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    parent_window.releasedWhenClosed = NO;
    parent_windows_.push_back(parent_window);

    dialog_->SelectFile(args.type, args.title, args.default_path,
                        args.file_types, args.file_type_index,
                        args.default_extension, parent_window, nullptr);

    // At this point, the Mojo IPC to show the dialog is queued up. Spin the
    // message loop to get the Mojo IPC to happen.
    base::RunLoop().RunUntilIdle();

    // Now there is an actual panel that exists.
    NSSavePanel* panel = remote_cocoa::SelectFileDialogBridge::
        GetLastCreatedNativePanelForTesting();
    DCHECK(panel);

    // Pump the message loop until the panel reports that it's visible.
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    id<NSObject> observer = [NSNotificationCenter.defaultCenter
        addObserverForName:NSWindowDidUpdateNotification
                    object:panel
                     queue:nil
                usingBlock:^(NSNotification* note) {
                  if (panel.visible) {
                    quit_closure.Run();
                  }
                }];
    run_loop.Run();
    [NSNotificationCenter.defaultCenter removeObserver:observer];

    return panel;
  }

  // Returns the number of panels currently active.
  size_t GetActivePanelCount() const {
    return dialog_->dialog_data_list_.size();
  }

  // Sets a callback to be called when a dialog is closed.
  void SetDialogClosedCallback(base::RepeatingClosure callback) {
    dialog_->dialog_closed_callback_for_testing_ = callback;
  }

  void ResetDialog() {
    dialog_ = new SelectFileDialogImpl(this, nullptr);

    // Spin the run loop to get any pending Mojo IPC sent.
    base::RunLoop().RunUntilIdle();
  }

 private:
  scoped_refptr<SelectFileDialogImpl> dialog_;

  std::vector<NSWindow*> parent_windows_;
};

class SelectFileDialogMacOpenAndSaveTest
    : public SelectFileDialogMacTest,
      public ::testing::WithParamInterface<SelectFileDialog::Type> {};

INSTANTIATE_TEST_SUITE_P(All,
                         SelectFileDialogMacOpenAndSaveTest,
                         ::testing::Values(SelectFileDialog::SELECT_SAVEAS_FILE,
                                           SelectFileDialog::SELECT_OPEN_FILE));

// Verify that the extension popup has the correct description and changing the
// popup item changes the allowed file types.
TEST_F(SelectFileDialogMacTest, ExtensionPopup) {
  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = {{"html", "htm"}, {"jpeg", "jpg"}};
  file_type_info.extension_description_overrides = {u"Webpage", u"Image"};
  file_type_info.include_all_files = false;

  FileDialogArguments args;
  args.file_types = &file_type_info;

  NSSavePanel* panel = SelectFileWithParams(args);
  NSPopUpButton* popup = GetPopup(panel);
  ASSERT_TRUE(popup);

  // Check that the dropdown list created has the correct description.
  const std::vector<std::u16string> extension_descriptions =
      GetExtensionDescriptionList(popup);
  EXPECT_EQ(file_type_info.extension_description_overrides,
            extension_descriptions);

  // Ensure other file types are not allowed.
  EXPECT_FALSE(panel.allowsOtherFileTypes);

  // Check that the first item was selected, since a file_type_index of 0 was
  // passed and no default extension was provided.
  EXPECT_EQ(0, popup.indexOfSelectedItem);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  NSArray<NSString*>* file_types = panel.allowedFileTypes;
#pragma clang diagnostic pop

  EXPECT_TRUE([file_types containsObject:@"htm"]);
  EXPECT_TRUE([file_types containsObject:@"html"]);
  // Extensions should appear in order of input.
  EXPECT_LT([file_types indexOfObject:@"html"],
            [file_types indexOfObject:@"htm"]);
  EXPECT_FALSE([file_types containsObject:@"jpg"]);

  // Select the second item.
  [popup.menu performActionForItemAtIndex:1];
  EXPECT_EQ(1, popup.indexOfSelectedItem);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  file_types = panel.allowedFileTypes;
#pragma clang diagnostic pop

  EXPECT_TRUE([file_types containsObject:@"jpg"]);
  EXPECT_TRUE([file_types containsObject:@"jpeg"]);
  // Extensions should appear in order of input.
  EXPECT_LT([file_types indexOfObject:@"jpeg"],
            [file_types indexOfObject:@"jpg"]);
  EXPECT_FALSE([file_types containsObject:@"html"]);
}

// Verify file_type_info.include_all_files argument is respected.
TEST_P(SelectFileDialogMacOpenAndSaveTest, IncludeAllFiles) {
  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = {{"html", "htm"}, {"jpeg", "jpg"}};
  file_type_info.extension_description_overrides = {u"Webpage", u"Image"};
  file_type_info.include_all_files = true;

  FileDialogArguments args;
  args.type = GetParam();
  args.file_types = &file_type_info;

  NSSavePanel* panel = SelectFileWithParams(args);
  NSPopUpButton* popup = GetPopup(panel);
  ASSERT_TRUE(popup);

  // Ensure other file types are allowed.
  EXPECT_TRUE(panel.allowsOtherFileTypes);

  // Check that the dropdown list created has the correct description.
  const std::vector<std::u16string> extension_descriptions =
      GetExtensionDescriptionList(popup);

  // Save dialogs don't have "all files".
  if (args.type == SelectFileDialog::SELECT_SAVEAS_FILE) {
    ASSERT_EQ(2lu, extension_descriptions.size());
    EXPECT_EQ(u"Webpage", extension_descriptions[0]);
    EXPECT_EQ(u"Image", extension_descriptions[1]);
  } else {
    ASSERT_EQ(3lu, extension_descriptions.size());
    EXPECT_EQ(u"Webpage", extension_descriptions[0]);
    EXPECT_EQ(u"Image", extension_descriptions[1]);
    EXPECT_EQ(u"All Files", extension_descriptions[2]);

    // Note that no further testing on the popup can be done. Open dialogs are
    // out-of-process starting in macOS 10.15, so once it's been run and closed,
    // the accessory view controls no longer work.
  }
}

// Verify that file_type_index and default_extension arguments cause the
// appropriate extension group to be initially selected.
TEST_F(SelectFileDialogMacTest, InitialSelection) {
  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = {{"html", "htm"}, {"jpeg", "jpg"}};
  file_type_info.extension_description_overrides = {u"Webpage", u"Image"};

  FileDialogArguments args;
  args.file_types = &file_type_info;
  args.file_type_index = 2;
  args.default_extension = "jpg";

  NSSavePanel* panel = SelectFileWithParams(args);
  NSPopUpButton* popup = GetPopup(panel);
  ASSERT_TRUE(popup);

  // Verify that the `file_type_index` causes the second item to be initially
  // selected.
  EXPECT_EQ(1, popup.indexOfSelectedItem);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  NSArray<NSString*>* file_types = panel.allowedFileTypes;
#pragma clang diagnostic pop

  EXPECT_TRUE([file_types containsObject:@"jpg"]);
  EXPECT_TRUE([file_types containsObject:@"jpeg"]);
  EXPECT_FALSE([file_types containsObject:@"html"]);

  ResetDialog();
  args.file_type_index = 0;
  args.default_extension = "pdf";
  panel = SelectFileWithParams(args);
  popup = GetPopup(panel);
  ASSERT_TRUE(popup);

  // Verify that the first item was selected, since the default extension passed
  // was not present in the extension list.
  EXPECT_EQ(0, popup.indexOfSelectedItem);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  file_types = panel.allowedFileTypes;
#pragma clang diagnostic pop

  EXPECT_TRUE([file_types containsObject:@"html"]);
  EXPECT_TRUE([file_types containsObject:@"htm"]);
  EXPECT_FALSE([file_types containsObject:@"pdf"]);
  EXPECT_FALSE([file_types containsObject:@"jpeg"]);

  ResetDialog();
  args.file_type_index = 0;
  args.default_extension = "jpg";
  panel = SelectFileWithParams(args);
  popup = GetPopup(panel);
  ASSERT_TRUE(popup);

  // Verify that the extension group corresponding to the default extension is
  // initially selected.
  EXPECT_EQ(1, popup.indexOfSelectedItem);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  file_types = panel.allowedFileTypes;
#pragma clang diagnostic pop

  EXPECT_TRUE([file_types containsObject:@"jpg"]);
  EXPECT_TRUE([file_types containsObject:@"jpeg"]);
  EXPECT_FALSE([file_types containsObject:@"html"]);
}

// Verify that an appropriate extension description is shown even if an empty
// extension description is passed for a given extension group.
TEST_F(SelectFileDialogMacTest, EmptyDescription) {
  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = {{"pdf"}, {"jpg"}, {"qqq"}};
  file_type_info.extension_description_overrides = {u"", u"Image", u""};

  FileDialogArguments args;
  args.file_types = &file_type_info;
  NSSavePanel* panel = SelectFileWithParams(args);
  NSPopUpButton* popup = GetPopup(panel);
  ASSERT_TRUE(popup);

  const std::vector<std::u16string> extension_descriptions =
      GetExtensionDescriptionList(popup);
  ASSERT_EQ(3lu, extension_descriptions.size());

  // Verify that the correct system description is produced for known file types
  // like pdf if no extension description is provided by the client. Modern
  // versions of macOS have settled on "PDF document" as the official
  // description.
  EXPECT_EQ(u"PDF document", extension_descriptions[0]);

  // Verify that if an override description is provided, it is used.
  EXPECT_EQ(u"Image", extension_descriptions[1]);

  // Verify the description for unknown file types if no extension description
  // is provided by the client.
  EXPECT_EQ(u"QQQ File (.qqq)", extension_descriptions[2]);
}

// Verify that passing an empty extension list in file_type_info causes no
// extension dropdown to display.
TEST_P(SelectFileDialogMacOpenAndSaveTest, EmptyExtension) {
  SelectFileDialog::FileTypeInfo file_type_info;

  FileDialogArguments args;
  args.type = GetParam();
  args.file_types = &file_type_info;

  NSSavePanel* panel = SelectFileWithParams(args);
  EXPECT_FALSE(panel.accessoryView);

  // Ensure other file types are allowed.
  EXPECT_TRUE(panel.allowsOtherFileTypes);
}

// Verify that passing a null file_types value causes no extension dropdown to
// display.
TEST_F(SelectFileDialogMacTest, FileTypesNull) {
  NSSavePanel* panel = SelectFileWithParams({});
  EXPECT_TRUE(panel.allowsOtherFileTypes);
  EXPECT_FALSE(panel.accessoryView);

  // Ensure other file types are allowed.
  EXPECT_TRUE(panel.allowsOtherFileTypes);
}

// Verify that appropriate properties are set on the NSSavePanel for different
// dialog types.
TEST_F(SelectFileDialogMacTest, SelectionType) {
  SelectFileDialog::FileTypeInfo file_type_info;
  FileDialogArguments args;
  args.file_types = &file_type_info;

  enum {
    HAS_ACCESSORY_VIEW = 1,
    PICK_FILES = 2,
    PICK_DIRS = 4,
    CREATE_DIRS = 8,
    MULTIPLE_SELECTION = 16,
  };

  struct SelectionTypeTestCase {
    SelectFileDialog::Type type;
    unsigned options;
    std::string prompt;
  } test_cases[] = {
      {SelectFileDialog::SELECT_FOLDER, PICK_DIRS | CREATE_DIRS, "Select"},
      {SelectFileDialog::SELECT_UPLOAD_FOLDER, PICK_DIRS, "Upload"},
      {SelectFileDialog::SELECT_EXISTING_FOLDER, PICK_DIRS, "Select"},
      {SelectFileDialog::SELECT_SAVEAS_FILE, CREATE_DIRS, "Save"},
      {SelectFileDialog::SELECT_OPEN_FILE, PICK_FILES, "Open"},
      {SelectFileDialog::SELECT_OPEN_MULTI_FILE,
       PICK_FILES | MULTIPLE_SELECTION, "Open"},
  };

  for (size_t i = 0; i < std::size(test_cases); i++) {
    SCOPED_TRACE(
        base::StringPrintf("i=%lu file_dialog_type=%d", i, test_cases[i].type));
    args.type = test_cases[i].type;
    ResetDialog();
    NSSavePanel* panel = SelectFileWithParams(args);

    EXPECT_EQ_BOOL(test_cases[i].options & HAS_ACCESSORY_VIEW,
                   panel.accessoryView);
    EXPECT_EQ_BOOL(test_cases[i].options & CREATE_DIRS,
                   panel.canCreateDirectories);
    EXPECT_EQ(test_cases[i].prompt, base::SysNSStringToUTF8([panel prompt]));

    if (args.type != SelectFileDialog::SELECT_SAVEAS_FILE) {
      NSOpenPanel* open_panel = base::apple::ObjCCast<NSOpenPanel>(panel);
      // Verify that for types other than save file dialogs, an NSOpenPanel is
      // created.
      ASSERT_TRUE(open_panel);
      EXPECT_EQ_BOOL(test_cases[i].options & PICK_FILES,
                     open_panel.canChooseFiles);
      EXPECT_EQ_BOOL(test_cases[i].options & PICK_DIRS,
                     open_panel.canChooseDirectories);
      EXPECT_EQ_BOOL(test_cases[i].options & MULTIPLE_SELECTION,
                     open_panel.allowsMultipleSelection);
    }
  }
}

// Verify that the correct message is set on the NSSavePanel.
TEST_F(SelectFileDialogMacTest, DialogMessage) {
  const std::string test_title = "test title";
  FileDialogArguments args;
  args.title = base::ASCIIToUTF16(test_title);
  NSSavePanel* panel = SelectFileWithParams(args);
  EXPECT_EQ(test_title, base::SysNSStringToUTF8(panel.message));
}

// Verify that multiple file dialogs are correctly handled.
TEST_F(SelectFileDialogMacTest, MultipleDialogs) {
  FileDialogArguments args;
  NSSavePanel* panel1 = SelectFileWithParams(args);
  NSSavePanel* panel2 = SelectFileWithParams(args);
  ASSERT_EQ(2lu, GetActivePanelCount());

  // Verify closing the panel decreases the panel count.
  base::RunLoop run_loop1;
  SetDialogClosedCallback(run_loop1.QuitClosure());
  [panel1 cancel:nil];
  run_loop1.Run();
  ASSERT_EQ(1lu, GetActivePanelCount());

  // In 10.15, file picker dialogs are remote, and the restriction of apps not
  // being allowed to OK their own file requests has been extended from just
  // sandboxed apps to all apps. Since the dialogs can't be OKed, at least close
  // them all.
  base::RunLoop run_loop2;
  SetDialogClosedCallback(run_loop2.QuitClosure());
  [panel2 cancel:nil];
  run_loop2.Run();
  EXPECT_EQ(0lu, GetActivePanelCount());

  SetDialogClosedCallback({});
}

// Verify that the default_path argument is respected.
TEST_F(SelectFileDialogMacTest, DefaultPath) {
  FileDialogArguments args;
  args.default_path = base::GetHomeDir().AppendASCII("test.txt");

  NSSavePanel* panel = SelectFileWithParams(args);

  panel.extensionHidden = NO;

  EXPECT_EQ(args.default_path.DirName(),
            base::apple::NSStringToFilePath(panel.directoryURL.path));
  EXPECT_EQ(args.default_path.BaseName(),
            base::apple::NSStringToFilePath(panel.nameFieldStringValue));
}

// Verify that the file dialog does not hide extension for filenames with
// multiple extensions.
TEST_F(SelectFileDialogMacTest, MultipleExtension) {
  const std::string fake_path_normal = "/fake_directory/filename.tar";
  const std::string fake_path_multiple = "/fake_directory/filename.tar.gz";
  const std::string fake_path_long = "/fake_directory/example.com-123.json";
  FileDialogArguments args;

  args.default_path = base::FilePath(FILE_PATH_LITERAL(fake_path_normal));
  NSSavePanel* panel = SelectFileWithParams(args);
  EXPECT_TRUE(panel.canSelectHiddenExtension);
  EXPECT_TRUE(panel.extensionHidden);

  ResetDialog();
  args.default_path = base::FilePath(FILE_PATH_LITERAL(fake_path_multiple));
  panel = SelectFileWithParams(args);
  EXPECT_FALSE(panel.canSelectHiddenExtension);
  EXPECT_FALSE(panel.extensionHidden);

  ResetDialog();
  args.default_path = base::FilePath(FILE_PATH_LITERAL(fake_path_long));
  panel = SelectFileWithParams(args);
  EXPECT_FALSE(panel.canSelectHiddenExtension);
  EXPECT_FALSE(panel.extensionHidden);
}

// Verify that the file dialog does not hide extension when the
// `keep_extension_visible` flag is set to true.
TEST_F(SelectFileDialogMacTest, KeepExtensionVisible) {
  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = {{"html", "htm"}, {"jpeg", "jpg"}};
  file_type_info.keep_extension_visible = true;

  FileDialogArguments args;
  args.file_types = &file_type_info;

  NSSavePanel* panel = SelectFileWithParams(args);
  EXPECT_FALSE(panel.canSelectHiddenExtension);
  EXPECT_FALSE(panel.extensionHidden);
}

TEST_F(SelectFileDialogMacTest, DontCrashWithBogusExtension) {
  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = {{"bogus type", "j.pg"}};

  FileDialogArguments args;
  args.file_types = &file_type_info;

  NSSavePanel* panel = SelectFileWithParams(args);
  // If execution gets this far, there was no crash.
  EXPECT_TRUE(panel);
}

}  // namespace ui::test
