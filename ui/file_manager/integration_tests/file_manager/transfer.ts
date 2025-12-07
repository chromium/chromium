// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {type ElementObject} from '../prod/file_manager/shared_types.js';
import {ENTRIES, EntryType, getCaller, pending, repeatUntil, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_LOCAL_ENTRY_SET, OFFLINE_ENTRY_SET, SHARED_DRIVE_ENTRY_SET, SHARED_WITH_ME_ENTRY_SET} from './test_data.js';

// Do not use BASIC_DRIVE_ENTRY_SET because that list contains too many files
// which might cause the file list can't show all files (some are hidden until
// it scrolls), and end up with `waitForFiles` failing.
const DRIVE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.photos,
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument,
  ENTRIES.testSharedFile,
];

interface TransferLocationOptions {
  volumeName: string;
  breadcrumbsPath: string;
  isTeamDrive?: boolean;
  initialEntries: TestEntryInfo[];
}

/**
 * Info for the source or destination of a transfer.
 */
class TransferLocationInfo {
  /**
   * The volume type (e.g. downloads, drive, drive_recent,
   * drive_shared_with_me, drive_offline) or team drive name.
   */
  volumeName: string;

  breadcrumbsPath: string;

  isTeamDrive: boolean;

  /** Expected initial contents in the volume. */
  initialEntries: TestEntryInfo[];

  constructor(opts: TransferLocationOptions) {
    this.volumeName = opts.volumeName;
    this.breadcrumbsPath = opts.breadcrumbsPath;
    this.isTeamDrive = opts.isTeamDrive ?? false;
    this.initialEntries = opts.initialEntries;
  }
}

interface TransferOptions {
  fileToTransfer: TestEntryInfo;
  source: TransferLocationInfo;
  destination: TransferLocationInfo;
  expectedDialogText?: string;
  expectedDialogOkButtonText?: string;
  isMove?: boolean;
  expectFailure?: boolean;
}

/**
 * Info for the transfer operation.
 */
class TransferInfo {
  /** The file to copy or move. Must be in the source location. */
  fileToTransfer: TestEntryInfo;

  /** The source location. */
  source: TransferLocationInfo;

  /** The destination location. */
  destination: TransferLocationInfo;

  /**
   * The expected content of the transfer dialog, or undefined if no dialog is
   * expected.
   */
  expectedDialogText?: string;

  /**
   * The expected label of the “ok” button on the dialog, if the dialog is
   * expected.
   */
  expectedDialogOkButtonText?: string;

  /**
   * True if this transfer is for a move operation, false for a copy operation.
   */
  isMove: boolean;

  /**
   * Whether the test is expected to fail, i.e. transferring to a folder without
   * correct permissions.
   */
  expectFailure: boolean;

  constructor(opts: TransferOptions) {
    this.fileToTransfer = opts.fileToTransfer;
    this.source = opts.source;
    this.destination = opts.destination;
    this.expectedDialogText = opts.expectedDialogText;
    this.expectedDialogOkButtonText = opts.expectedDialogOkButtonText;
    this.isMove = opts.isMove ?? false;
    this.expectFailure = opts.expectFailure ?? false;
  }
}

/**
 * Test function to copy from the specified source to the specified destination.
 * @param transferInfo Options for the transfer.
 */
async function transferBetweenVolumes(transferInfo: TransferInfo) {
  let srcContents;
  if (transferInfo.source.isTeamDrive) {
    srcContents =
        TestEntryInfo.getExpectedRows(transferInfo.source.initialEntries.filter(
            entry => entry.type !== EntryType.SHARED_DRIVE &&
                entry.teamDriveName === transferInfo.source.volumeName));
  } else {
    srcContents =
        TestEntryInfo.getExpectedRows(transferInfo.source.initialEntries.filter(
            entry => entry.type !== EntryType.SHARED_DRIVE &&
                entry.teamDriveName === ''));
  }

  let dstContents;
  if (transferInfo.destination.isTeamDrive) {
    dstContents = TestEntryInfo.getExpectedRows(
        transferInfo.destination.initialEntries.filter(
            entry => entry.type !== EntryType.SHARED_DRIVE &&
                entry.teamDriveName === transferInfo.destination.volumeName));
  } else {
    dstContents = TestEntryInfo.getExpectedRows(
        transferInfo.destination.initialEntries.filter(
            entry => entry.type !== EntryType.SHARED_DRIVE &&
                entry.teamDriveName === ''));
  }

  const localFiles = BASIC_LOCAL_ENTRY_SET;
  const driveFiles = (transferInfo.source.isTeamDrive ||
                      transferInfo.destination.isTeamDrive) ?
      SHARED_DRIVE_ENTRY_SET :
      DRIVE_ENTRY_SET.concat([
        ENTRIES.sharedDirectory,
        ENTRIES.sharedDirectoryFile,
        ENTRIES.docxFile,
      ]);

  // Open files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, localFiles, driveFiles);

  // Expand Drive root if either src or dst is within Drive.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  if (transferInfo.source.isTeamDrive || transferInfo.destination.isTeamDrive) {
    const myDriveContent = TestEntryInfo.getExpectedRows(driveFiles.filter(
        e => e.teamDriveName === '' && e.computerName === ''));
    // Select + expand + wait for its content.

    await directoryTree.navigateToPath('/My Drive');
    await remoteCall.waitForFiles(appId, myDriveContent);
  }

  // Select the source folder.
  await directoryTree.navigateToPath(transferInfo.source.breadcrumbsPath);

  // Wait for the expected files to appear in the file list.
  await remoteCall.waitForFiles(appId, srcContents);

  // Focus the file list.
  await remoteCall.callRemoteTestUtil(
      'focus', appId, ['#file-list:not([hidden])']);

  // Select the source file.
  await remoteCall.waitUntilSelected(
      appId, transferInfo.fileToTransfer.nameText);

  // Copy the file.
  const transferCommand = transferInfo.isMove ? 'cut' : 'copy';
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'execCommand', appId, [transferCommand]));

  // Select the destination folder.
  await directoryTree.navigateToPath(transferInfo.destination.breadcrumbsPath);

  // Wait for the expected files to appear in the file list.
  await remoteCall.waitForFiles(
      appId, dstContents, {ignoreFileSize: true, ignoreLastModifiedTime: true});
  // Paste the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // If we're expecting a confirmation dialog, confirm that it is shown.
  if (transferInfo.expectedDialogText !== undefined) {
    const {innerText} = await remoteCall.waitForElement(
        appId, '.cr-dialog-container.shown .cr-dialog-text');
    chrome.test.assertEq(transferInfo.expectedDialogText, innerText);

    // Check that the dialog contains required buttons.
    const okButton = await remoteCall.waitForElement(
        appId, '.cr-dialog-container.shown .cr-dialog-ok');
    chrome.test.assertEq(
        transferInfo.expectedDialogOkButtonText, okButton.innerText);
    const cancelButton = await remoteCall.waitForElement(
        appId, '.cr-dialog-container.shown .cr-dialog-cancel');
    chrome.test.assertEq('Cancel', cancelButton.innerText);

    // Press OK button.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseClick', appId, ['button.cr-dialog-ok']));
  }

  // Wait for the file list to change, if the test is expected to pass.
  const dstContentsAfterPaste = dstContents.slice();
  const ignoreFileSize =
      transferInfo.source.volumeName === 'drive_shared_with_me' ||
      transferInfo.source.volumeName === 'drive_offline' ||
      transferInfo.destination.volumeName === 'drive_shared_with_me' ||
      transferInfo.destination.volumeName === 'drive_offline' ||
      transferInfo.destination.volumeName === 'my_files';

  // If we expected the transfer to succeed, add the pasted file to the list
  // of expected rows.
  if (!transferInfo.expectFailure &&
      transferInfo.source !== transferInfo.destination) {
    const pasteFile = transferInfo.fileToTransfer.getExpectedRow();
    // Check if we need to add (1) to the filename, in the case of a
    // duplicate file.
    for (let i = 0; i < dstContentsAfterPaste.length; i++) {
      if (dstContentsAfterPaste[i]![0] === pasteFile[0]) {
        // Replace the last '.' in filename with ' (1).'.
        // e.g. 'my.note.txt' -> 'my.note (1).txt'
        pasteFile[0] = pasteFile[0].replace(/\.(?=[^\.]+$)/, ' (1).');
        break;
      }
    }
    dstContentsAfterPaste.push(pasteFile);
  }

  // Check the last contents of file list.
  await remoteCall.waitForFiles(
      appId, dstContentsAfterPaste,
      {ignoreFileSize: ignoreFileSize, ignoreLastModifiedTime: true});

  return appId;
}

/** A list of transfer locations, for use with transferBetweenVolumes. */
const TRANSFER_LOCATIONS = {
  drive: new TransferLocationInfo({
    breadcrumbsPath: '/My Drive',
    volumeName: 'drive',
    initialEntries: DRIVE_ENTRY_SET.concat([
      ENTRIES.sharedDirectory,
      ENTRIES.docxFile,
    ]),
  }),

  driveWithTeamDriveEntries: new TransferLocationInfo({
    breadcrumbsPath: '/My Drive',
    volumeName: 'drive',
    initialEntries: SHARED_DRIVE_ENTRY_SET,
  }),

  driveSharedDirectory: new TransferLocationInfo({
    breadcrumbsPath: '/My Drive/Shared',
    volumeName: 'drive',
    initialEntries: [ENTRIES.sharedDirectoryFile],
  }),

  downloads: new TransferLocationInfo({
    breadcrumbsPath: '/My files/Downloads',
    volumeName: 'downloads',
    initialEntries: BASIC_LOCAL_ENTRY_SET,
  }),

  sharedWithMe: new TransferLocationInfo({
    breadcrumbsPath: '/Shared with me',
    volumeName: 'drive_shared_with_me',
    initialEntries: SHARED_WITH_ME_ENTRY_SET.concat([
      ENTRIES.sharedDirectory,
      ENTRIES.sharedDirectoryFile,
    ]),
  }),

  driveOffline: new TransferLocationInfo({
    breadcrumbsPath: '/Offline',
    volumeName: 'drive_offline',
    initialEntries: OFFLINE_ENTRY_SET,
  }),

  driveTeamDriveA: new TransferLocationInfo({
    breadcrumbsPath: '/Shared drives/Team Drive A',
    volumeName: 'Team Drive A',
    isTeamDrive: true,
    initialEntries: SHARED_DRIVE_ENTRY_SET,
  }),

  driveTeamDriveB: new TransferLocationInfo({
    breadcrumbsPath: '/Shared drives/Team Drive B',
    volumeName: 'Team Drive B',
    isTeamDrive: true,
    initialEntries: SHARED_DRIVE_ENTRY_SET,
  }),

  my_files: new TransferLocationInfo({
    breadcrumbsPath: '/My files',
    volumeName: 'my_files',
    initialEntries: [
      new TestEntryInfo({
        type: EntryType.DIRECTORY,
        targetPath: 'Play files',
        nameText: 'Play files',
        lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
        sizeText: '--',
        typeText: 'Folder',
      }),
      new TestEntryInfo({
        type: EntryType.DIRECTORY,
        targetPath: 'Downloads',
        nameText: 'Downloads',
        lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
        sizeText: '--',
        typeText: 'Folder',
      }),
      new TestEntryInfo({
        type: EntryType.DIRECTORY,
        targetPath: 'Linux files',
        nameText: 'Linux files',
        lastModifiedTime: '...',
        sizeText: '--',
        typeText: 'Folder',
      }),
    ],
  }),
};
Object.freeze(TRANSFER_LOCATIONS);

/**
 * Tests copying from Drive to Downloads.
 */
export async function transferFromDriveToDownloads() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.drive,
    destination: TRANSFER_LOCATIONS.downloads,
  }));
}

/**
 * Tests copying an office file from Drive to Downloads.
 */
export async function transferOfficeFileFromDriveToDownloads() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.docxFile,
    source: TRANSFER_LOCATIONS.drive,
    destination: TRANSFER_LOCATIONS.downloads,
  }));
}

/**
 * Tests moving files from MyFiles/Downloads to MyFiles crbug.com/925175.
 */
export async function transferFromDownloadsToMyFilesMove() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.downloads,
    destination: TRANSFER_LOCATIONS.my_files,
    isMove: true,
  }));
}

/**
 * Tests copying files from MyFiles/Downloads to MyFiles crbug.com/925175.
 */
export async function transferFromDownloadsToMyFiles() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.downloads,
    destination: TRANSFER_LOCATIONS.my_files,
    isMove: false,
  }));
}

/**
 * Tests copying from Downloads to Drive.
 */
export async function transferFromDownloadsToDrive() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.downloads,
    destination: TRANSFER_LOCATIONS.drive,
  }));
}

/**
 * Tests copying from Drive "Shared with me" to Downloads.
 */
export async function transferFromSharedWithMeToDownloads() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.testSharedFile,
    source: TRANSFER_LOCATIONS.sharedWithMe,
    destination: TRANSFER_LOCATIONS.downloads,
  }));
}

/**
 * Tests copying from Drive "Shared with me" to Drive.
 */
export async function transferFromSharedWithMeToDrive() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.testSharedDocument,
    source: TRANSFER_LOCATIONS.sharedWithMe,
    destination: TRANSFER_LOCATIONS.drive,
  }));
}


/**
 * Tests copying from Downloads to a shared folder on Drive.
 */
export async function transferFromDownloadsToSharedFolder() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.downloads,
    destination: TRANSFER_LOCATIONS.driveSharedDirectory,
    expectedDialogText:
        'Copying this item will share it with everyone who can see the ' +
        'shared folder \'Shared\'.',
    expectedDialogOkButtonText: 'Copy',
  }));
}

/**
 * Tests moving from Downloads to a shared folder on Drive.
 */
export async function transferFromDownloadsToSharedFolderMove() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.downloads,
    destination: TRANSFER_LOCATIONS.driveSharedDirectory,
    expectedDialogText:
        'Moving this item will share it with everyone who can see the ' +
        'shared folder \'Shared\'.',
    expectedDialogOkButtonText: 'Move',
    isMove: true,
  }));
}

/**
 * Tests copying from a shared folder on Drive to Downloads.
 */
export async function transferFromSharedFolderToDownloads() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.sharedDirectoryFile,
    source: TRANSFER_LOCATIONS.driveSharedDirectory,
    destination: TRANSFER_LOCATIONS.downloads,
  }));
}

/**
 * Tests copying from Drive offline to Downloads.
 */
export async function transferFromOfflineToDownloads() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.testSharedFile,
    source: TRANSFER_LOCATIONS.driveOffline,
    destination: TRANSFER_LOCATIONS.downloads,
  }));
}

/**
 * Tests copying from Drive offline to Drive.
 */
export async function transferFromOfflineToDrive() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.testDocument,
    source: TRANSFER_LOCATIONS.driveOffline,
    destination: TRANSFER_LOCATIONS.drive,
  }));
}

/**
 * Tests copying from a Team Drive to Drive.
 */
export async function transferFromTeamDriveToDrive() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.teamDriveAFile,
    source: TRANSFER_LOCATIONS.driveTeamDriveA,
    destination: TRANSFER_LOCATIONS.driveWithTeamDriveEntries,
  }));
}

/**
 * Tests copying from Drive to a Team Drive.
 */
export async function transferFromDriveToTeamDrive() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.driveWithTeamDriveEntries,
    destination: TRANSFER_LOCATIONS.driveTeamDriveA,
    expectedDialogText:
        'Members of \'Team Drive A\' will gain access to the copy of these ' +
        'items.',
    expectedDialogOkButtonText: 'Copy',
  }));
}

/**
 * Tests copying from a Team Drive to Downloads.
 */
export async function transferFromTeamDriveToDownloads() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.teamDriveAFile,
    source: TRANSFER_LOCATIONS.driveTeamDriveA,
    destination: TRANSFER_LOCATIONS.downloads,
  }));
}

/**
 * Tests that a hosted file cannot be transferred from a Team Drive to a local
 * drive (e.g. Downloads). Hosted documents only make sense in the context of
 * Drive.
 */
export async function transferHostedFileFromTeamDriveToDownloads() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.teamDriveAHostedFile,
    source: TRANSFER_LOCATIONS.driveTeamDriveA,
    destination: TRANSFER_LOCATIONS.downloads,
    expectFailure: true,
  }));
}

/**
 * Tests copying from Downloads to a Team Drive.
 */
export async function transferFromDownloadsToTeamDrive() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.downloads,
    destination: TRANSFER_LOCATIONS.driveTeamDriveA,
    expectedDialogText:
        'Members of \'Team Drive A\' will gain access to the copy of these ' +
        'items.',
    expectedDialogOkButtonText: 'Copy',
  }));
}

/**
 * Tests copying between Team Drives.
 */
export async function transferBetweenTeamDrives() {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.teamDriveBFile,
    source: TRANSFER_LOCATIONS.driveTeamDriveB,
    destination: TRANSFER_LOCATIONS.driveTeamDriveA,
    expectedDialogText:
        'Members of \'Team Drive A\' will gain access to the copy of these ' +
        'items.',
    expectedDialogOkButtonText: 'Copy',
  }));
}

/**
 * Tests that moving a file to its current location is a no-op.
 */
export async function transferFromDownloadsToDownloads() {
  const appId = await transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.downloads,
    destination: TRANSFER_LOCATIONS.downloads,
    isMove: true,
  }));

  // Check: No feedback panel items.
  const panelItems =
      await remoteCall.queryElements(appId, ['#progress-panel', '#panel']);

  chrome.test.assertEq(0, panelItems.length);
}

/**
 * Tests that the root html element .drag-drop-active class appears when drag
 * drop operations are active, and is removed when the operations complete.
 */
export async function transferDragDropActiveLeave() {
  const entries = [ENTRIES.hello, ENTRIES.photos];

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // The drag has to start in the file list column "name" text, otherwise it
  // starts a drag-selection instead of a drag operation.
  const source =
      `#file-list li[file-name="${ENTRIES.hello.nameText}"] .entry-name`;

  // Select the source file.
  await remoteCall.waitAndClickElement(appId, source);

  // Wait for the directory tree target.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByLabel('My files');

  // Check: the html element should not have drag-drop-active class.
  const htmlDragDropActive = ['html.drag-drop-active'];
  await remoteCall.waitForElementLost(appId, htmlDragDropActive);

  // Drag the source and hover it over the target.
  const finishDrop = await directoryTree.dragFilesToItemByLabel(
      source, 'My files', /* skipDrop= */ true);

  // Check: the html element should have drag-drop-active class.
  await remoteCall.waitForElementsCount(appId, htmlDragDropActive, 1);

  // Send a dragleave event to the target to end drag-drop operations.
  await finishDrop('#file-list', /* dragLeave= */ true);

  // Check: the html element should not have drag-drop-active class.
  await remoteCall.waitForElementLost(appId, htmlDragDropActive);
}

/**
 * Tests that the root html element .drag-drop-active class appears when drag
 * drop operations are active, and is removed when the operations complete.
 */
export async function transferDragDropActiveDrop() {
  const entries = [ENTRIES.hello, ENTRIES.photos];

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // Expand Downloads to display "photos" folder in the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.expandTreeItemByLabel('Downloads');

  // The drag has to start in the file list column "name" text, otherwise it
  // starts a drag-selection instead of a drag operation.
  const source =
      `#file-list li[file-name="${ENTRIES.hello.nameText}"] .entry-name`;

  // Select the source file.
  await remoteCall.waitAndClickElement(appId, source);

  // Wait for the directory tree target.
  await directoryTree.waitForItemByLabel('photos');

  // Check: the html element should not have drag-drop-active class.
  const htmlDragDropActive = ['html.drag-drop-active'];
  await remoteCall.waitForElementLost(appId, htmlDragDropActive);

  // Drag the source and hover it over the target.
  const finishDrop = await directoryTree.dragFilesToItemByLabel(
      source, 'My files', /* skipDrop= */ true);

  // Check: the html element should have drag-drop-active class.
  await remoteCall.waitForElementsCount(appId, htmlDragDropActive, 1);

  // Send a drop event to the target to end drag-drop operations.
  await finishDrop('#file-list', /* dragLeave= */ false);

  // Check: the html element should not have drag-drop-active class.
  await remoteCall.waitForElementLost(appId, htmlDragDropActive);
}

/**
 * Tests that dragging a file over a directory tree item that can accept the
 * drop changes the class of that tree item to 'accepts'.
 */
export async function transferDragDropTreeItemAccepts() {
  const entries = [ENTRIES.hello, ENTRIES.photos];

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // The drag has to start in the file list column "name" text, otherwise it
  // starts a drag-selection instead of a drag operation.
  const source =
      `#file-list li[file-name="${ENTRIES.photos.nameText}"] .entry-name`;

  // Select the source file.
  await remoteCall.waitAndClickElement(appId, source);

  // Wait for the directory tree target.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByLabel('My files');

  // Drag the source and hover it over the target.
  const finishDrop = await directoryTree.dragFilesToItemByLabel(
      source, 'My files', /* skipDrop= */ true);

  // Check: drag hovering should navigate the file list.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');

  // Check: the target should have accepts class and should not have denies
  // class.
  await directoryTree.waitForItemToAcceptDropByLabel('My files');

  // Send a dragleave event to the target to end drag-drop operations.
  await finishDrop('#file-list', /* dragLeave= */ true);

  // Check: the target should not have accepts class and should not have denies
  // class.
  await directoryTree.waitForItemToFinishDropByLabel('My files');
}

/**
 * Tests that dragging a file over a directory tree item that cannot accept
 * the drop changes the class of that tree item to 'denies'.
 */
export async function transferDragDropTreeItemDenies() {
  const entries = [ENTRIES.hello, ENTRIES.photos];

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // The drag has to start in the file list column "name" text, otherwise it
  // starts a drag-selection instead of a drag operation.
  const source =
      `#file-list li[file-name="${ENTRIES.hello.nameText}"] .entry-name`;

  // Select the source file.
  await remoteCall.waitAndClickElement(appId, source);

  // Wait for the directory tree target.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByLabel('Recent');

  // Drag the source and hover it over the target.
  const finishDrop = await directoryTree.dragFilesToItemByLabel(
      source, 'Recent', /* skipDrop= */ true);

  // Check: drag hovering should navigate the file list.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Recent');

  // Check: the target should have denies class and should not have accepts
  // class.
  await directoryTree.waitForItemToDenyDropByLabel('Recent');

  // Send a dragleave event to the target to end drag-drop operations.
  await finishDrop('#file-list', /* dragLeave= */ true);

  // Check: the target should not have denies class and should not have accepts
  // class.
  await directoryTree.waitForItemToFinishDropByLabel('Recent');
}

/**
 * Tests that dragging a file over an EntryList directory tree item (here a
 * partitioned USB drive) does not raise an error.
 */
export async function transferDragAndHoverTreeItemEntryList() {
  const entries = [ENTRIES.hello, ENTRIES.photos];

  // Mount a partitioned USB.
  await sendTestMessage({name: 'mountUsbWithPartitions'});

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // The drag has to start in the file list column "name" text, otherwise it
  // starts a drag-selection instead of a drag operation.
  const source =
      `#file-list li[file-name="${ENTRIES.hello.nameText}"] .entry-name`;

  // Wait for the directory tree target.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByLabel('Drive Label');

  // Drag the source and hover it over the target.
  await directoryTree.dragFilesToItemByLabel(
      source, 'Drive Label', /* skipDrop= */ true);

  // Check: drag hovering should navigate the file list.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Drive Label');
}

/**
 * Tests that dragging a file over a FakeEntry directory tree item (here a
 * USB drive) does not raise an error.
 */
export async function transferDragAndHoverTreeItemFakeEntry() {
  const entries = [ENTRIES.hello, ENTRIES.photos];

  // Mount a USB.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // The drag has to start in the file list column "name" text, otherwise it
  // starts a drag-selection instead of a drag operation.
  const source =
      `#file-list li[file-name="${ENTRIES.hello.nameText}"] .entry-name`;

  // Wait for the directory tree target.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  if (await remoteCall.isSinglePartitionFormat(appId)) {
    await directoryTree.waitForItemByLabel('FAKEUSB');
    await directoryTree.expandTreeItemByLabel('FAKEUSB');
  }
  await directoryTree.waitForItemByLabel('fake-usb');

  // Drag the source and hover it over the target.
  await directoryTree.dragFilesToItemByLabel(
      source, 'fake-usb', /* skipDrop= */ true);

  let navigationPath = '/fake-usb';
  if (await remoteCall.isSinglePartitionFormat(appId)) {
    navigationPath = '/FAKEUSB/fake-usb';
  }
  // Check: drag hovering should navigate the file list.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, navigationPath);
}

/**
 * Tests that dragging a file list item selects its file list row.
 */
export async function transferDragFileListItemSelects() {
  const entries = [ENTRIES.hello, ENTRIES.photos];

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // The drag has to start in the file list column "name" text, otherwise it
  // starts a drag-selection instead of a drag operation.
  const listItem = `#file-list li[file-name="${ENTRIES.hello.nameText}"]`;
  const source = listItem + ' .entry-name';

  // Wait for the source.
  await remoteCall.waitForElement(appId, source);

  // Wait for the target.
  const target = listItem + ' .detail-icon';
  await remoteCall.waitForElement(appId, target);

  // Check: the file list row should not be selected
  await remoteCall.waitForElement(appId, listItem + ':not([selected])');

  // Drag the source and hover it over the target.
  const skipDrop = true;
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil(
          'fakeDragAndDrop', appId, [source, target, skipDrop]),
      'fakeDragAndDrop failed');

  // Check: the file list row should be selected.
  await remoteCall.waitForElement(appId, listItem + '[selected]');
}

/**
 * Tests that dropping a file on a directory tree item (folder) copies the
 * file to that folder.
 */
export async function transferDragAndDrop() {
  const entries = [ENTRIES.hello, ENTRIES.photos];

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // Expand Downloads to display "photos" folder in the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.expandTreeItemByLabel('Downloads');

  // The drag has to start in the file list column "name" text, otherwise it
  // starts a drag-selection instead of a drag operation.
  const source =
      `#file-list li[file-name="${ENTRIES.hello.nameText}"] .entry-name`;

  // Wait for the source.
  await remoteCall.waitForElement(appId, source);

  // Wait for the directory tree target.
  await directoryTree.waitForItemByLabel('photos');

  // Drag the source and drop it on the target.
  await directoryTree.dragFilesToItemByLabel(
      source, 'photos', /* skipDrop= */ false);

  // Navigate the file list to the target.
  await directoryTree.selectItemByLabel('photos');

  // Wait for navigation to finish.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My files/Downloads/photos');

  // Check: the dropped file should appear in the file list.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.hello]),
      {ignoreLastModifiedTime: true});
}

/**
 * Tests that dropping a folder on a directory tree item (folder) copies the
 * folder and also updates the directory tree.
 */
export async function transferDragAndDropFolder() {
  // Note that directoryB is a child of directoryA.
  const entries = [ENTRIES.directoryA, ENTRIES.directoryB, ENTRIES.directoryD];

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // Expand Downloads to display folder "D" in the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.expandTreeItemByLabel('Downloads');

  // The drag has to start in the file list column "name" text, otherwise it
  // starts a drag-selection instead of a drag operation.
  const source =
      `#file-list li[file-name="${ENTRIES.directoryA.nameText}"] .entry-name`;

  // Wait for the source.
  await remoteCall.waitForElement(appId, source);

  // Wait for the directory tree target.
  await directoryTree.waitForItemByLabel(ENTRIES.directoryD.nameText);

  // Drag the source and drop it on the target.
  await directoryTree.dragFilesToItemByLabel(
      source, ENTRIES.directoryD.nameText, /* skipDrop= */ false);

  // Check: the dropped folder "A" should appear in the directory tree under
  // the target folder "D" and be expandable (as folder "A" contains "B").
  // signature, so it must be accessed with ['directoryA'].
  await directoryTree.expandTreeItemByLabel(ENTRIES.directoryA.nameText);
}

/*
 * Tests that dragging a file over a directory tree item (folder) navigates
 * the file list to that folder.
 */
export async function transferDragAndHover() {
  const entries = [ENTRIES.hello, ENTRIES.photos];

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // Expand Downloads to display "photos" folder in the directory tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.expandTreeItemByLabel('Downloads');

  // The drag has to start in the file list column "name" text, otherwise it
  // starts a drag-selection instead of a drag operation.
  const source =
      `#file-list li[file-name="${ENTRIES.hello.nameText}"] .entry-name`;

  // Wait for the directory tree target.
  await directoryTree.waitForItemByLabel('photos');

  // Drag the source and hover it over the target.
  await directoryTree.dragFilesToItemByLabel(
      source, 'photos', /* skipDrop= */ true);

  // Check: drag hovering should navigate the file list.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My files/Downloads/photos');
}

/**
 * Tests dropping a file originated from the browser.
 */
export async function transferDropBrowserFile() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Send drop event on current directory.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil(
          'fakeDropBrowserFile', appId,
          ['browserfile', 'content', 'text/plain', '#file-list']),
      'fakeDropBrowserFile failed');

  // File should be created.
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="browserfile"]');
}

/**
 * Tests that copying a deleted file shows an error.
 */
export async function transferDeletedFile() {
  const entry = ENTRIES.hello;

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Copy the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']));

  // Delete the file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'deleteFile', appId, [entry.nameText]));

  // Wait for completion of file deletion.
  await remoteCall.waitForElementLost(
      appId, `#file-list [file-name="${entry.nameText}"]`);

  // Paste the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // Check that the error appears in the feedback panel.
  let element: ElementObject|null = null;
  const caller = getCaller();
  await repeatUntil(async () => {
    element = await remoteCall.waitForElement(
        appId, ['#progress-panel', 'xf-panel-item']);
    const expectedMsg = `Whoops, ${entry.nameText} no longer exists.`;
    const actualMsg = element.attributes['primary-text'];

    if (actualMsg === expectedMsg) {
      return;
    }

    return pending(
        caller,
        `Expected feedback panel msg: "${expectedMsg}", got "${actualMsg}"`);
  });

  // Check that only one line of text is shown.
  chrome.test.assertFalse(!!element!.attributes['secondary-text']);
}

/**
 * Tests that transfer source/destination persists if app window is re-opened.
 */
export async function transferInfoIsRemembered() {
  const entry = ENTRIES.hello;

  // Open files app.
  let appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Copy the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']));

  // Tell the background page to never finish the file copy.
  await remoteCall.callRemoteTestUtil(
      'progressCenterNeverNotifyCompleted', appId, []);

  // Paste the file to begin a copy operation.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // The feedback panel should appear: record the feedback panel text.
  let panel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);
  const primaryText = panel.attributes['primary-text'];
  const secondaryText = panel.attributes['secondary-text'];

  // Open a Files app window again.
  appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Check the feedback panel text is remembered.
  panel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);
  chrome.test.assertEq(primaryText, panel.attributes['primary-text']);
  chrome.test.assertEq(secondaryText, panel.attributes['secondary-text']);
}

/**
 * Tests that destination text line shows name for USB targets.
 */
export async function transferToUsbHasDestinationText() {
  const entry = ENTRIES.hello;

  // Open files app.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByType('removable');

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Copy the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']));

  let navigationPath = '/fake-usb';
  if (await remoteCall.isSinglePartitionFormat(appId)) {
    navigationPath = '/FAKEUSB/fake-usb';
  }
  // Select USB volume.
  await directoryTree.navigateToPath(navigationPath);

  // Tell the background page to never finish the file copy.
  await remoteCall.callRemoteTestUtil(
      'progressCenterNeverNotifyCompleted', appId, []);

  // Paste the file to begin a copy operation.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // Check the feedback panel destination message contains the target device.
  const panel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);

  chrome.test.assertTrue(
      panel.attributes['primary-text']!.includes('to fake-usb'),
      'Feedback panel does not contain device name.');
}

/**
 * Tests that dismissing an error notification on the foreground
 * page is propagated to the background page.
 */
export async function transferDismissedErrorIsRemembered() {
  const entry = ENTRIES.hello;

  // Open Files app on Downloads.
  let appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Select a file to copy.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Copy the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']));

  // Force all file copy operations to trigger an error.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'forceErrorsOnFileOperations', appId, [true]));

  // Select Downloads.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByLabel('Downloads');

  // Paste the file to begin a copy operation.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // Check: an error feedback panel with failure status should appear.
  let errorPanel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);
  // If we've grabbed a reference to a progress panel, it will disappear
  // quickly and be replaced by the error panel, so loop and wait for it.
  while (errorPanel.attributes['indicator'] === 'progress') {
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'requestAnimationFrame', appId, []));
    errorPanel = await remoteCall.waitForElement(
        appId, ['#progress-panel', 'xf-panel-item']);
  }
  chrome.test.assertEq('failure', errorPanel.attributes['status']);

  // Press the dismiss button on the error feedback panel.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      [['#progress-panel', 'xf-panel-item', 'xf-button#secondary-action']]));

  // Open a Files app window again.
  appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Turn off the error generation for file operations.
  chrome.test.assertFalse(await remoteCall.callRemoteTestUtil(
      'forceErrorsOnFileOperations', appId, [false]));

  // Tell the background page to never finish the file copy.
  await remoteCall.callRemoteTestUtil(
      'progressCenterNeverNotifyCompleted', appId, []);

  // Paste the file to begin a copy operation.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // Check: the first feedback panel item should be a progress panel.
  // If the error persisted then we'd see a summary panel here.
  const progressPanel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);
  chrome.test.assertEq('progress', progressPanel.attributes['indicator']);
}

/**
 * Tests no remaining time displayed for not supported operations like format.
 */
export async function transferNotSupportedOperationHasNoRemainingTimeText() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Show a |format| progress panel.
  await remoteCall.callRemoteTestUtil('sendProgressItem', null, [
    'item-id-1',
    /* ProgressItemType.FORMAT */ 'format',
    /* ProgressItemState.PROGRESSING */ 'progressing',
    'Formatting',
  ]);

  // Check the progress panel is open.
  let panel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);

  // Check no remaining time shown for 'format' panel type.
  chrome.test.assertEq('', panel.attributes['secondary-text']);

  // Show a |format| error panel.
  await remoteCall.callRemoteTestUtil('sendProgressItem', null, [
    'item-id-2',
    /* ProgressItemType.FORMAT */ 'format',
    /* ProgressItemState.ERROR */ 'error',
    'Failed',
  ]);

  // Check the progress panel is open.
  panel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item#item-id-2']);

  // Check no remaining time shown for 'format' error panel type.
  chrome.test.assertEq('', panel.attributes['secondary-text']);
}

/**
 * Tests updating same panel keeps same message.
 * Use case: crbug/1137229
 */
export async function transferUpdateSamePanelItem() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Show a |format| error in feedback panel.
  await remoteCall.callRemoteTestUtil('sendProgressItem', null, [
    'item-id',
    /* ProgressItemType.FORMAT */ 'format',
    /* ProgressItemState.ERROR */ 'error',
    'Failed',
  ]);

  // Check the error panel is open.
  let panel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);

  // Dispatch another |format| feedback panel with the same id and panel type.
  await remoteCall.callRemoteTestUtil('sendProgressItem', null, [
    'item-id',
    /* ProgressItemType.FORMAT */ 'format',
    /* ProgressItemState.ERROR */ 'error',
    'Failed new message',
  ]);

  // Check the progress panel is open.
  panel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);

  // Check secondary text is still empty for the error panel.
  chrome.test.assertEq('', panel.attributes['secondary-text']);
}

/**
 * Tests prepraring message shown when the remaining time is zero.
 */
export async function transferShowPreparingMessageForZeroRemainingTime() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Show a |copy| progress in feedback panel.
  await remoteCall.callRemoteTestUtil('sendProgressItem', null, [
    'item-id',
    /* ProgressItemType.COPY */ 'copy',
    /* ProgressItemState.PROGRESSING */ 'progressing',
    'Copying File1.txt to Downloads',
    /* remainingTime*/ 0,
  ]);

  // Check the error panel is open.
  const panel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);

  // Check secondary text is preparing message.
  chrome.test.assertEq('Preparing', panel.attributes['secondary-text']);
}
