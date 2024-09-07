// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogType, type ElementObject} from '../prod/file_manager/shared_types.js';
import {ExecuteScriptError} from '../remote_call.js';
import {addEntries, ENTRIES, EntryType, getCaller, getHistogramCount, pending, repeatUntil, RootPath, sanitizeDate, sendTestMessage, TestEntryInfo, wait} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';
import {BASIC_ANDROID_ENTRY_SET, BASIC_FAKE_ENTRY_SET, BASIC_LOCAL_ENTRY_SET, MODIFIED_ENTRY_SET} from './test_data.js';

/** The tag used to create a safe environment to display the preview. */
const previewTag = 'iframe';

/** The JS code used to query the content window for preview. */
const contentWindowQuery = 'document.querySelector("iframe").contentWindow';

/** The name of the UMA emitted to track how Quick View is opened. */
const QuickViewUmaWayToOpenHistogramName = 'FileBrowser.QuickView.WayToOpen';

/**
 * The UMA's enumeration values (must be consistent with enums.xml, previously
 * histograms.xml).
 */
const QuickViewUmaWayToOpenHistogramValues = {
  CONTEXT_MENU: 0,
  SPACE_KEY: 1,
  SELECTION_MENU: 2,
};

/**
 * Check the background color for the content inside the quick view is one of
 * the 2 allowed colors
 */
async function checkBackgroundColor(backgroundColor: string): Promise<void> {
  const validBackground = [
    // Dark mode:
    'rgb(0, 0, 0)',
    // Light mode: the preview body backgroundColor should be transparent black.
    'rgba(0, 0, 0, 0)',
  ];
  // b/361293031: Accept either value, it was flaking between the values due to
  // issues outside the Files app.
  chrome.test.assertTrue(
      validBackground.includes(backgroundColor),
      `Unexepcted background color: ${backgroundColor}`);
}

/**
 * Waits for Quick View dialog to be open.
 *
 * @param appId Files app windowId.
 */
async function waitQuickViewOpen(appId: string) {
  const caller = getCaller();

  function checkQuickViewElementsDisplayBlock(elements: ElementObject[]) {
    const haveElements = Array.isArray(elements) && elements.length !== 0;
    if (!haveElements || elements[0]!.styles!['display'] !== 'block') {
      return pending(caller, 'Waiting for Quick View to open.');
    }
    return;
  }

  await repeatUntil(async () => {
    const elements = ['#quick-view', '#dialog[open]'];
    return checkQuickViewElementsDisplayBlock(
        await remoteCall.callRemoteTestUtil(
            'deepQueryAllElements', appId, [elements, ['display']]));
  });
}

/**
 * Waits for Quick View dialog to be closed.
 *
 * @param appId Files app windowId.
 */
async function waitQuickViewClose(appId: string) {
  const caller = getCaller();

  function checkQuickViewElementsDisplayNone(elements: ElementObject[]) {
    chrome.test.assertTrue(Array.isArray(elements));
    if (elements.length === 0 || elements[0]!.styles!['display'] !== 'none') {
      return pending(caller, 'Waiting for Quick View to close.');
    }

    return;
  }

  // Check: the Quick View dialog should not be shown.
  await repeatUntil(async () => {
    const elements = ['#quick-view', '#dialog:not([open])'];
    return checkQuickViewElementsDisplayNone(
        await remoteCall.callRemoteTestUtil(
            'deepQueryAllElements', appId, [elements, ['display']]));
  });
}

/**
 * Opens the Quick View dialog on a given file |name|. The file must be
 * present in the Files app file list.
 *
 * @param appId Files app windowId.
 * @param name File name.
 */
async function openQuickViewEx(appId: string, name: string) {
  // Select file |name| in the file list.
  await remoteCall.waitUntilSelected(appId, name);

  // Press the space key.
  const space = ['#file-list', ' ', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, space),
      'fakeKeyDown failed');

  // Check: the Quick View dialog should be shown.
  return waitQuickViewOpen(appId);
}

/**
 * Opens the Quick View dialog by right clicking on the file |name| and
 * using the "Get Info" command from the context menu.
 *
 * @param appId Files app windowId.
 * @param name File name.
 */
async function openQuickViewViaContextMenu(appId: string, name: string) {
  // Right-click the file in the file-list.
  const query = `#file-list [file-name="${name}"]`;
  await remoteCall.waitAndRightClick(appId, query);

  // Wait because WebUI Menu ignores the following click if it happens in
  // <200ms from the previous click.
  await wait(300);

  // Click the file-list context menu "Get info" command.
  const getInfoMenuItem = '#file-context-menu:not([hidden]) ' +
      ' [command="#get-info"]:not([hidden])';
  await remoteCall.waitAndClickElement(appId, getInfoMenuItem);

  // Check: the Quick View dialog should be shown.
  await waitQuickViewOpen(appId);
}

/**
 * Opens the Quick View dialog with given file |names|. The files must be
 * present and check-selected in the Files app file list.
 *
 * @param appId Files app windowId.
 * @param names File names.
 */
async function openQuickViewMultipleSelection(appId: string, names: string[]) {
  // Get the file-list rows that are check-selected (multi-selected).
  const selectedRows = await remoteCall.callRemoteTestUtil<ElementObject[]>(
      'deepQueryAllElements', appId, ['#file-list li[selected]']);

  // Check: the selection should contain the given file names.
  chrome.test.assertEq(names.length, selectedRows.length);
  for (let i = 0; i < names.length; i++) {
    chrome.test.assertTrue(
        selectedRows[i]?.attributes['file-name']?.includes(names[i]!)!);
  }

  // Open Quick View via its keyboard shortcut.
  const space = ['#file-list', ' ', false, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, space);

  // Check: the Quick View dialog should be shown.
  await waitQuickViewOpen(appId);
}

/**
 * Mount and select USB.
 *
 * @param appId Files app windowId.
 */
async function mountAndSelectUsb(appId: string) {
  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for the USB volume to mount and click to open the USB volume.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByType('removable');

  // Check: the USB files should appear in the file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
}

/**
 * Assuming that Quick View is currently open per openQuickView above, closes
 * the Quick View dialog.
 *
 * @param appId Files app windowId.
 */
async function closeQuickViewEx(appId: string) {
  // Click on Quick View to close it.
  const panelElements = ['#quick-view', '#contentPanel'];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [panelElements]),
      'fakeMouseClick failed');

  // Check: the Quick View dialog should not be shown.
  await waitQuickViewClose(appId);
}

/**
 * Assuming that Quick View is currently open per openQuickView above, return
 * the text shown in the QuickView Metadata Box field |name|. If the optional
 * |hidden| is 'hidden', the field |name| should not be visible.
 *
 * @param appId Files app windowId.
 * @param name QuickView Metadata Box field name.
 * @param hidden Whether the field name should be visible.
 *
 * @return text Text value in the field name.
 */
async function getQuickViewMetadataBoxField(
    appId: string, name: string, hidden: string = ''): Promise<string> {
  let filesMetadataBox = 'files-metadata-box';

  /**
   * <files-metadata-box> field rendering is async. The field name has been
   * rendered when its 'metadata' attribute indicates that.
   */
  switch (name) {
    case 'Size':
      filesMetadataBox += '[metadata~="size"]';
      break;
    case 'Date modified':
    case 'Type':
      filesMetadataBox += '[metadata~="mime"]';
      break;
    case 'File location':
      filesMetadataBox += '[metadata~="location"]';
      break;
    case 'Original location':
      filesMetadataBox += '[metadata~="originalLocation"]';
      break;
    default:
      filesMetadataBox += '[metadata~="meta"]';
      break;
  }

  /**
   * The <files-metadata-box> element resides in the #quick-view shadow DOM
   * as a child of the #dialog element.
   */
  const quickViewQuery = ['#quick-view', `#dialog[open] ${filesMetadataBox}`];

  /**
   * The <files-metadata-entry key="name"> element resides in the shadow DOM
   * of the <files-metadata-box>.
   */
  quickViewQuery.push(`files-metadata-entry[key="${name}"]`);

  /**
   * It has a #value div child in its shadow DOM containing the field value,
   * but if |hidden| was given, the field should not be visible.
   */
  if (hidden !== 'hidden') {
    quickViewQuery.push('#value > div:not([hidden])');
  } else {
    quickViewQuery.push('#box[hidden]');
  }

  const element = await remoteCall.waitForElement(appId, quickViewQuery);
  if (name === 'Date modified') {
    return sanitizeDate(element.text || '');
  } else {
    return element.text ?? '';
  }
}

/**
 * Executes a script in the context of a <preview-tag> element and returns its
 * output. Returns undefined when ExecuteScriptError is caught.
 *
 * @param appId App window Id.
 * @param query Query to the <preview-tag> element (this is
 *     ignored for SWA).
 * @param statement Javascript statement to be executed within the
 *     <preview-tag>.
 */
async function executeJsInPreviewTagAndCatchErrors<T>(
    appId: string, query: string[], statement: string): Promise<T|undefined> {
  try {
    return await remoteCall.executeJsInPreviewTag<T>(appId, query, statement);
  } catch (e) {
    if (e instanceof ExecuteScriptError) {
      return undefined;
    } else {
      throw (e);
    }
  }
}

/**
 * Tests opening Quick View on a local downloads file.
 */
export async function openQuickView() {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.hello.nameText);

  // Check the open button is shown.
  await remoteCall.waitForElement(
      appId, ['#quick-view', '#open-button:not([hidden])']);
}

/**
 * Tests opening Quick View on a local downloads file in an open file dialog.
 */
export async function openQuickViewDialog() {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], [],
      {type: DialogType.SELECT_OPEN_FILE});

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.hello.nameText);

  // Check the open button is not shown as we're in an open file dialog.
  await remoteCall.waitForElement(
      appId, ['#quick-view', '#open-button[hidden]']);
}

/**
 * Tests that Quick View opens via the context menu with a single selection.
 */
export async function openQuickViewViaContextMenuSingleSelection() {
  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select the file in the file list.
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.nameText);

  // Check: clicking the context menu "Get Info" should open Quick View.
  await openQuickViewViaContextMenu(appId, ENTRIES.hello.nameText);
}

/**
 * Tests that Quick View opens via the context menu when multiple files
 * are selected (file-list check-select mode).
 */
export async function openQuickViewViaContextMenuCheckSelections() {
  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Ctrl+A to select all files in the file-list.
  await remoteCall.fakeKeyDown(appId, '#file-list', 'a', true, false, false);

  // Check: clicking the context menu "Get Info" should open Quick View.
  await openQuickViewViaContextMenu(appId, ENTRIES.hello.nameText);
}

/**
 * Tests opening then closing Quick View on a local downloads file.
 */
export async function closeQuickView() {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.hello.nameText);

  // Close Quick View.
  await closeQuickViewEx(appId);
}

/**
 * Tests opening Quick View on a Drive file.
 */
export async function openQuickViewDrive() {
  // Open Files app on Drive containing ENTRIES.hello.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.hello]);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.hello.nameText);

  // Check: the correct mimeType should be displayed (see crbug.com/1067499
  // for details on mimeType differences between Drive and local filesystem).
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('text/plain', mimeType);

  // Check: the correct file location should be displayed in Drive.
  const location = await getQuickViewMetadataBoxField(appId, 'File location');
  chrome.test.assertEq('My Drive/hello.txt', location);
}

/**
 * Tests opening Quick View on a Smbfs file.
 */
export async function openQuickViewSmbfs() {
  // Open Files app on Downloads containing ENTRIES.photos.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Populate Smbfs with some files.
  await addEntries(['smbfs'], BASIC_LOCAL_ENTRY_SET);

  // Mount Smbfs volume.
  await sendTestMessage({name: 'mountSmbfs'});

  // Wait for the Smbfs volume to mount and click to open the Smbfs volume.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByType('smb');

  const files = TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.hello.nameText);
}

/**
 * Tests opening Quick View on a USB file.
 */
export async function openQuickViewUsb() {
  // Open Files app on Downloads containing ENTRIES.photos.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Open a USB file in Quick View.
  await mountAndSelectUsb(appId);
  await openQuickViewEx(appId, ENTRIES.hello.nameText);
}

/**
 * Tests opening Quick View on a removable partition.
 */
export async function openQuickViewRemovablePartitions() {
  // Open Files app on Downloads containing ENTRIES.photos.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Mount USB device containing partitions.
  await sendTestMessage({name: 'mountUsbWithPartitions'});

  // Wait for the USB root to be available.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemByLabel('Drive Label');
  await directoryTree.navigateToPath('/Drive Label');

  // Wait for 2 removable partitions to appear in the directory tree.
  await directoryTree.expandTreeItemByLabel('Drive Label');
  await directoryTree.waitForChildItemsCountByLabel('Drive Label', 2);

  // Click to open the first partition.
  await directoryTree.selectItemByType('removable');

  // Check: the USB files should appear in the file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.hello.nameText);
}

/**
 * Tests opening Quick View on an item that was Trashed shows original location
 * instead of the current file location.
 */
export async function openQuickViewTrash() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Delete item and wait for it to be removed (no dialog).
  await remoteCall.waitAndClickElement(appId, '#move-to-trash-button');
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Navigate to /Trash and ensure the file is shown.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/Trash');
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Open the file in Quick View.
  await openQuickViewEx(appId, 'hello.txt');

  // Check: the original location should be shown instead of the actual file
  // location.
  const location =
      await getQuickViewMetadataBoxField(appId, 'Original location');
  chrome.test.assertEq('My files/Downloads/hello.txt', location);
}

/**
 * Tests seeing dashes for an empty last_modified for DocumentsProvider.
 */
export async function openQuickViewLastModifiedMetaData() {
  const documentsProviderVolumeType = 'documents_provider';

  // Add files to the DocumentsProvider volume.
  await addEntries(['documents_provider'], MODIFIED_ENTRY_SET);

  // Open Files app.
  const appId = await remoteCall.openNewWindow(RootPath.DOWNLOADS);

  // Wait for the DocumentsProvider volume to mount and then click to open
  // DocumentsProvider Volume.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemToHaveChildrenByType(
      documentsProviderVolumeType, /* hasChildren= */ true);
  await directoryTree.selectItemByType(documentsProviderVolumeType);

  // Check: the DocumentsProvider files should appear in the file list.
  const files = TestEntryInfo.getExpectedRows(MODIFIED_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open a DocumentsProvider file in Quick View.
  await openQuickViewEx(appId, ENTRIES.hello.nameText);

  const lastValidModifiedText =
      await getQuickViewMetadataBoxField(appId, 'Date modified');
  chrome.test.assertEq(ENTRIES.hello.lastModifiedTime, lastValidModifiedText);

  await closeQuickViewEx(appId);

  // Open a DocumentsProvider file in Quick View.
  await openQuickViewEx(appId, ENTRIES.invalidLastModifiedDate.nameText);

  // Modified time should be displayed as "--" when it's absent.
  const lastInvalidModifiedText =
      await getQuickViewMetadataBoxField(appId, 'Date modified');
  chrome.test.assertEq('--', lastInvalidModifiedText);
}

/**
 * Tests opening Quick View on an MTP file.
 */
export async function openQuickViewMtp() {
  // Open Files app on Downloads containing ENTRIES.photos.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Mount a non-empty MTP volume.
  await sendTestMessage({name: 'mountFakeMtp'});

  // Wait for the MTP volume to mount and click to open the MTP volume.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByType('mtp');

  // Check: the MTP files should appear in the file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open an MTP file in Quick View.
  await openQuickViewEx(appId, ENTRIES.hello.nameText);
}

/**
 * Tests opening Quick View on a Crostini file.
 */
export async function openQuickViewCrostini() {
  // Open Files app on Downloads containing ENTRIES.photos.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Open a Crostini file in Quick View.
  await remoteCall.mountCrostini(appId);
  await openQuickViewEx(appId, ENTRIES.hello.nameText);
}

/**
 * Tests opening Quick View on a GuestOS file.
 */
export async function openQuickViewGuestOs() {
  // Open Files app on Downloads containing ENTRIES.photos.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Open a GuestOS file in Quick View.
  await remoteCall.mountGuestOs(appId, BASIC_LOCAL_ENTRY_SET);
  await openQuickViewEx(appId, ENTRIES.hello.nameText);
}

/**
 * Tests opening Quick View on an Android file.
 */
export async function openQuickViewAndroid() {
  // Open Files app on Android files.
  const appId = await remoteCall.openNewWindow(RootPath.ANDROID_FILES);

  // Add files to the Android files volume.
  const entrySet = BASIC_ANDROID_ENTRY_SET.concat([ENTRIES.documentsText]);
  await addEntries(['android_files'], entrySet);

  // Wait for the file list to appear.
  await remoteCall.waitForElement(appId, '#file-list');

  // Check: the basic Android file set should appear in the file list.
  let files = TestEntryInfo.getExpectedRows(BASIC_ANDROID_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Navigate to the Android files '/Documents' directory.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Play files/Documents');

  // Check: the 'android.txt' file should appear in the file list.
  files = [ENTRIES.documentsText.getExpectedRow()];
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open the Android file in Quick View.
  const documentsFileName = ENTRIES.documentsText.nameText;
  await openQuickViewEx(appId, documentsFileName);
}

/**
 * Tests opening Quick View on an Android file on GuestOS.
 */
export async function openQuickViewAndroidGuestOs() {
  // Open Files app on Android files.
  const appId = await remoteCall.openNewWindow(RootPath.ANDROID_FILES);

  // Add files to the Android files volume.
  const entrySet = BASIC_ANDROID_ENTRY_SET.concat([ENTRIES.documentsText]);
  await addEntries(['android_files'], entrySet);

  // Wait for the file list to appear.
  await remoteCall.waitForElement(appId, '#file-list');

  // Check: the basic Android file set should appear in the file list.
  let files = TestEntryInfo.getExpectedRows(BASIC_ANDROID_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Navigate to the Android files '/Documents' directory.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.navigateToPath('/My files/Play files/Documents');

  // Check: the 'android.txt' file should appear in the file list.
  files = [ENTRIES.documentsText.getExpectedRow()];
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open the Android file in Quick View.
  const documentsFileName = ENTRIES.documentsText.nameText;
  await openQuickViewEx(appId, documentsFileName);
}

/**
 * Tests opening Quick View on a DocumentsProvider root.
 */
export async function openQuickViewDocumentsProvider() {
  const DOCUMENTS_PROVIDER_VOLUME_TYPE = 'documents_provider';

  // Add files to the DocumentsProvider volume.
  await addEntries(['documents_provider'], BASIC_LOCAL_ENTRY_SET);

  // Open Files app.
  const appId = await remoteCall.openNewWindow(RootPath.DOWNLOADS);

  // Wait for the DocumentsProvider volume to mount.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemToHaveChildrenByType(
      DOCUMENTS_PROVIDER_VOLUME_TYPE, /* hasChildren= */ true);

  // Click to open the DocumentsProvider volume.
  await directoryTree.selectItemByType(DOCUMENTS_PROVIDER_VOLUME_TYPE);

  // Check: the DocumentsProvider files should appear in the file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open a DocumentsProvider file in Quick View.
  await openQuickViewEx(appId, ENTRIES.hello.nameText);

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Wait for the Quick View preview to load and display its content.
  const caller = getCaller();
  function checkPreviewTextLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || !elements[0]!.attributes['src']) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Wait until the preview displays the file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
                     appId, preview, getTextContent) as string[];
    // Check: the content of text file should be shown.
    if (!text || !text[0] || !text[0].includes('chocolate and chips')) {
      return pending(caller, `Waiting for ${previewTag} content.`);
    }
    return;
  });

  // Check: the correct size and date modified values should be displayed.
  const sizeText = await getQuickViewMetadataBoxField(appId, 'Size');
  chrome.test.assertEq(ENTRIES.hello.sizeText, sizeText);
  const lastModifiedText =
      await getQuickViewMetadataBoxField(appId, 'Date modified');
  chrome.test.assertEq(ENTRIES.hello.lastModifiedTime, lastModifiedText);
}

/**
 * Tests opening Quick View with a local text document identified as text from
 * file sniffing (the first word of the file is "From ", note trailing space).
 */
export async function openQuickViewSniffedText() {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Open Files app on Downloads containing ENTRIES.plainText.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.plainText], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.plainText.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || !elements[0]!.attributes['src']) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('text/plain', mimeType);
}

/**
 * Tests opening Quick View with a local text document whose MIME type cannot
 * be identified by MIME type sniffing.
 */
export async function openQuickViewTextFileWithUnknownMimeType() {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Open Files app on Downloads containing ENTRIES.hello.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.hello.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || !elements[0]!.attributes['src']) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the mimeType field should not be displayed.
  await getQuickViewMetadataBoxField(appId, 'Type', 'hidden');
}

/**
 * Tests opening Quick View with a text file containing some UTF-8 encoded
 * characters: crbug.com/1064855
 */
export async function openQuickViewUtf8Text() {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Open Files app on Downloads containing ENTRIES.utf8Text.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.utf8Text], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.utf8Text.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || !elements[0]!.attributes['src']) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Wait until the preview displays the file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
                     appId, preview, getTextContent) as string[];

    // Check: the content of ENTRIES.utf8Text should be shown.
    if (!text || !text[0] || !text[0].includes('їсти मुझे |∊☀✌✂♁ 🙂\n')) {
      return pending(caller, 'Waiting for preview content.');
    }

    return;
  });

  // Check: the correct file size should be displayed.
  const size = await getQuickViewMetadataBoxField(appId, 'Size');
  chrome.test.assertEq('191 bytes', size);
}

/**
 * Tests opening Quick View and scrolling its preview contents which contains a
 * tall text document.
 */
export async function openQuickViewScrollText() {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  function scrollQuickViewTextBy(y: number) {
    const doScrollBy = `${contentWindowQuery}.scrollBy(0,${y})`;
    return remoteCall.executeJsInPreviewTag(appId, preview, doScrollBy);
  }

  async function checkQuickViewTextScrollY(scrollY: {toString: () => string}) {
    if (!scrollY || Number(scrollY.toString()) <= 150) {
      await scrollQuickViewTextBy(100);
      return pending(caller, 'Waiting for Quick View to scroll.');
    }
    return;
  }

  // Open Files app on Downloads containing ENTRIES.tallText.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.tallText], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.tallText.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || !elements[0]!.attributes['src']) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Statement to get the Quick View preview scrollY.
  const getScrollY = `${contentWindowQuery}.scrollY`;

  // The initial preview scrollY should be 0.
  await repeatUntil(async () => {
    const scrollY =
        await executeJsInPreviewTagAndCatchErrors(appId, preview, getScrollY);
    if (String(scrollY) !== '0') {
      return pending(caller, 'Waiting for preview text to load.');
    }
    return;
  });

  // Scroll the preview and verify that it scrolled.
  await repeatUntil(async () => {
    const scrollY = await remoteCall.executeJsInPreviewTag<string>(
        appId, preview, getScrollY);
    return checkQuickViewTextScrollY(scrollY!);
  });
}

/**
 * Tests opening Quick View containing a PDF document.
 */
export async function openQuickViewPdf() {
  const caller = getCaller();

  /**
   * The PDF preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.content`];

  // Open Files app on Downloads containing ENTRIES.tallPdf.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.tallPdf], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.tallPdf.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewPdfLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || !elements[0]!.attributes['src']) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewPdfLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the preview embed type attribute.
  function checkPdfEmbedType(type: ElementObject[]) {
    const haveElements = Array.isArray(type) && type.length === 1;
    if (!haveElements || !type[0]!.toString().includes('pdf')) {
      return pending(caller, 'Waiting for plugin <embed> type.');
    }
    return type[0]!;
  }
  const type = await repeatUntil(async () => {
    const getType =
        contentWindowQuery + '.document.querySelector("embed").type';
    const type = await executeJsInPreviewTagAndCatchErrors(
                     appId, preview, getType) as ElementObject[];
    return checkPdfEmbedType(type);
  });

  // Check: the preview embed type should be PDF mime type.
  chrome.test.assertEq('application/pdf', type);

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('application/pdf', mimeType);
}

/**
 * Tests opening Quick View on a PDF document that opens a popup JS dialog.
 */
export async function openQuickViewPdfPopup() {
  const caller = getCaller();

  /**
   * The PDF preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.content`];

  // Open Files app on Downloads containing ENTRIES.popupPdf.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.popupPdf], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.popupPdf.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewPdfLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || !elements[0]!.attributes['src']) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewPdfLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the preview embed type attribute.
  function checkPdfEmbedType(type: unknown) {
    const haveElements = Array.isArray(type) && type.length === 1;
    if (!haveElements || !type[0].toString().includes('pdf')) {
      return pending(caller, 'Waiting for plugin <embed> type.');
    }
    return type[0];
  }
  const type = await repeatUntil(async () => {
    const getType =
        contentWindowQuery + '.document.querySelector("embed").type';
    const type =
        await executeJsInPreviewTagAndCatchErrors(appId, preview, getType);
    return checkPdfEmbedType(type);
  });

  // Check: the preview embed type should be PDF mime type.
  chrome.test.assertEq('application/pdf', type);

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('application/pdf', mimeType);
}

/**
 * Tests that Quick View does not display a PDF file preview when that is
 * disabled by system settings (preferences).
 */
export async function openQuickViewPdfPreviewsDisabled() {
  const caller = getCaller();

  /**
   * The #innerContentPanel resides in the #quick-view shadow DOM as a child
   * of the #dialog element, and contains the file preview result.
   */
  const contentPanel = ['#quick-view', '#dialog[open] #innerContentPanel'];

  // Disable PDF previews.
  await sendTestMessage({name: 'setPdfPreviewEnabled', enabled: false});

  // Open Files app on Downloads containing ENTRIES.tallPdf.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.tallPdf], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.tallPdf.nameText);

  // Wait for the innerContentPanel to load and display its content.
  function checkInnerContentPanel(elements: ElementObject[]) {
    const haveElements = Array.isArray(elements) && elements.length === 1;
    if (!haveElements || elements[0]!.styles!['display'] !== 'flex') {
      return pending(caller, 'Waiting for inner content panel to load.');
    }
    // Check: the PDF preview should not be shown.
    chrome.test.assertEq('No preview available', elements[0]!.text);
    return;
  }
  await repeatUntil(async () => {
    return checkInnerContentPanel(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [contentPanel, ['display']]));
  });

  // Check: the correct file mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('application/pdf', mimeType);
}

/**
 * Tests opening Quick View with a '.mhtml' filename extension.
 */
export async function openQuickViewMhtml() {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', 'files-safe-media[type="html"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.plainText.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.mHtml], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.mHtml.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || !elements[0]!.attributes['src']) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('text/plain', mimeType);

  // Check: the correct file location should be displayed.
  const location = await getQuickViewMetadataBoxField(appId, 'File location');
  chrome.test.assertEq('My files/Downloads/page.mhtml', location);
}

/**
 * Tests opening Quick View and scrolling its preview contents which contains a
 * tall html document.
 */
export async function openQuickViewScrollHtml() {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="html"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="html"]', previewTag];

  function scrollQuickViewHtmlBy(y: number) {
    const doScrollBy = `window.scrollBy(0,${y})`;
    return remoteCall.executeJsInPreviewTag(appId, preview, doScrollBy);
  }

  async function checkQuickViewHtmlScrollY(scrollY: {toString: () => any}) {
    if (!scrollY || Number(scrollY.toString()) <= 200) {
      await scrollQuickViewHtmlBy(100);
      return pending(caller, 'Waiting for Quick View to scroll.');
    }
    return;
  }

  // Open Files app on Downloads containing ENTRIES.tallHtml.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.tallHtml], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.tallHtml.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewHtmlLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewHtmlLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the Quick View preview scrollY.
  const getScrollY = 'window.scrollY';

  // The initial preview scrollY should be 0.
  await repeatUntil(async () => {
    const scrollY = await executeJsInPreviewTagAndCatchErrors<string>(
        appId, preview, getScrollY);
    if (String(scrollY) !== '0') {
      return pending(caller, `Waiting for preview text to load.`);
    }
    return;
  });

  // Scroll the preview and verify that it scrolled.
  await repeatUntil(async () => {
    const scrollY = await remoteCall.executeJsInPreviewTag<string>(
        appId, preview, getScrollY);
    return checkQuickViewHtmlScrollY(scrollY!);
  });

  // Check: the mimeType field should not be displayed.
  await getQuickViewMetadataBoxField(appId, 'Type', 'hidden');
}

/**
 * Tests opening Quick View on an html document to verify that the background
 * color of the <files-safe-media type="html"> that contains the preview is
 * solid white.
 */
export async function openQuickViewBackgroundColorHtml() {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="html"> shadow DOM,
   * which is a child of the #quick-view shadow DOM. This test only needs to
   * examine the <files-safe-media>'s iframe element.
   */
  const preview = ['#quick-view', 'files-safe-media[type="html"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.tallHtml.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.tallHtml], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.tallHtml.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewHtmlLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewHtmlLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the preview document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';
  const backgroundColor = await remoteCall.executeJsInPreviewTag<string[]>(
      appId, preview, getBackgroundStyle);
  chrome.test.assertTrue(!!backgroundColor);

  chrome.test.assertEq('rgba(0, 0, 0, 0)', backgroundColor[0]);
}

/**
 * Tests opening Quick View containing an audio file without album preview.
 */
export async function openQuickViewAudio() {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="audio"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="audio"]', previewTag];

  /**
   * The album artwork preview resides in the <files-safe-media> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const albumArtworkPreview = ['#quick-view', '#audio-artwork'];

  // Open Files app on Downloads containing ENTRIES.beautiful song.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.beautiful.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewAudioLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewAudioLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the audio artwork is not shown on the preview page.
  const albumArtworkElements = await remoteCall.callRemoteTestUtil(
      'deepQueryAllElements', appId, [albumArtworkPreview, ['display']]);
  const hasArtworkElements =
      Array.isArray(albumArtworkElements) && albumArtworkElements.length > 0;
  chrome.test.assertFalse(hasArtworkElements);

  // Get the preview document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';
  const backgroundColor = await remoteCall.executeJsInPreviewTag<string[]>(
      appId, preview, getBackgroundStyle);
  chrome.test.assertTrue(!!backgroundColor);
  await checkBackgroundColor(backgroundColor[0]!);

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('audio/ogg', mimeType);
}

/**
 * Tests opening Quick View containing an audio file on Drive.
 */
export async function openQuickViewAudioOnDrive() {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="audio"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="audio"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.beautiful song.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.beautiful]);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.beautiful.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewAudioLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewAudioLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the preview document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';
  const backgroundColor = await remoteCall.executeJsInPreviewTag<string[]>(
      appId, preview, getBackgroundStyle);
  chrome.test.assertTrue(!!backgroundColor);
  await checkBackgroundColor(backgroundColor[0]!);
}

/**
 * Tests opening Quick View containing an audio file that has an album art
 * image in its metadata.
 */
export async function openQuickViewAudioWithImageMetadata() {
  const caller = getCaller();

  // Define a test file containing audio file with metadata.
  const id3Audio = new TestEntryInfo({
    type: EntryType.FILE,
    sourceFileName: 'id3Audio.mp3',
    targetPath: 'id3Audio.mp3',
    mimeType: 'audio/mpeg',
    lastModifiedTime: 'December 25 2015, 11:16 PM',
    nameText: 'id3Audio.mp3',
    sizeText: '5KB',
    typeText: 'id3 encoded MP3 audio',
  });

  /**
   * The preview resides in the <files-safe-media> shadow DOM, which
   * is a child of the #quick-view shadow DOM.
   */
  const albumArtWebView = ['#quick-view', '#audio-artwork', previewTag];

  // Open Files app on Downloads containing the audio test file.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [id3Audio], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, id3Audio.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }

  // Wait until the preview has loaded the album image of the audio file.
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [albumArtWebView, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('audio/mpeg', mimeType);

  // Check: the audio album metadata should also be displayed.
  const album = await getQuickViewMetadataBoxField(appId, 'Album');
  chrome.test.assertEq(album, 'OK Computer');
}

/**
 * Tests opening Quick View containing an image with extension 'jpg'.
 */
export async function openQuickViewImageJpg() {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.smallJpeg.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.smallJpeg], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.smallJpeg.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the preview document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';
  const backgroundColor = await remoteCall.executeJsInPreviewTag<string[]>(
      appId, preview, getBackgroundStyle);
  chrome.test.assertTrue(!!backgroundColor);
  await checkBackgroundColor(backgroundColor[0]!);

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/jpeg', mimeType);
}

/**
 * Tests opening Quick View containing an image with extension 'jpeg'.
 */
export async function openQuickViewImageJpeg() {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.sampleJpeg.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.sampleJpeg], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.sampleJpeg.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the preview document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';
  const backgroundColor = await remoteCall.executeJsInPreviewTag<string[]>(
      appId, preview, getBackgroundStyle);
  chrome.test.assertTrue(!!backgroundColor);
  await checkBackgroundColor(backgroundColor[0]!);

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/jpeg', mimeType);
}

/**
 * Tests that opening Quick View on a JPEG image with EXIF displays the EXIF
 * information in the QuickView Metadata Box.
 */
export async function openQuickViewImageExif() {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.exifImage.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.exifImage], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.exifImage.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/jpeg', mimeType);

  // Check: the correct file modified time should be displayed.
  const time = await getQuickViewMetadataBoxField(appId, 'Date modified');
  chrome.test.assertEq('Jan 18, 2038, 1:02 AM', time);

  // Check: the correct image EXIF metadata should be displayed.
  const size = await getQuickViewMetadataBoxField(appId, 'Dimensions');
  chrome.test.assertEq('378 x 272', size);
  const model = await getQuickViewMetadataBoxField(appId, 'Device model');
  chrome.test.assertEq('FinePix S5000', model);
  const film = await getQuickViewMetadataBoxField(appId, 'Device settings');
  chrome.test.assertEq('f/2.8 0.004 5.7mm ISO200', film);
}

/**
 * Tests opening Quick View on an RAW image. The RAW image has EXIF and that
 * information should be displayed in the QuickView metadata box.
 */
export async function openQuickViewImageRaw() {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.rawImage.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.rawImage], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.rawImage.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/x-olympus-orf', mimeType);

  // Check: the RAW image EXIF metadata should be displayed.
  const size = await getQuickViewMetadataBoxField(appId, 'Dimensions');
  chrome.test.assertEq('4608 x 3456', size);
  const model = await getQuickViewMetadataBoxField(appId, 'Device model');
  chrome.test.assertEq('E-M1', model);
  const film = await getQuickViewMetadataBoxField(appId, 'Device settings');
  chrome.test.assertEq('f/8 0.002 12mm ISO200', film);
}

/**
 * Tests opening Quick View on an RAW .NEF image and that the dimensions
 * shown in the metadata box respect the image EXIF orientation.
 */
export async function openQuickViewImageRawWithOrientation() {
  const caller = getCaller();

  /**
   * The <files-safe-media type="image"> element is a shadow DOM child of
   * the #quick-view element, and has a shadow DOM child <webview> or <iframe>
   * for the preview.
   */
  const filesSafeMedia = ['#quick-view', 'files-safe-media[type="image"]'];

  // Open Files app on Downloads containing ENTRIES.rawNef.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.nefImage], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.nefImage.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    const preview = filesSafeMedia.concat([previewTag]);
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct image dimensions should be displayed.
  const size = await getQuickViewMetadataBoxField(appId, 'Dimensions');
  chrome.test.assertEq('1324 x 4028', size);

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/tiff', mimeType);

  // Get the fileSafeMedia element preview thumbnail image size.
  const element = await remoteCall.waitForElement(appId, filesSafeMedia);
  const image = new Image();
  let imageSize = '';
  image.onload = () => {
    imageSize = `${image.naturalWidth} x ${image.naturalHeight}`;
  };

  const sourceContent = JSON.parse(element.attributes['src']!);

  chrome.test.assertTrue(!!sourceContent.data);
  image.src = sourceContent.data as string;

  // Check: the preview thumbnail should have an orientated size.
  await repeatUntil(async () => {
    if (!image.complete || imageSize !== '120 x 160') {
      return pending(caller, 'Waiting for preview thumbnail size.');
    }

    return;
  });
}

/**
 * Tests opening Quick View with a VP8X format WEBP image.
 */
export async function openQuickViewImageWebp() {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.rawImage.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.webpImage], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.webpImage.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/webp', mimeType);

  // Check: the correct dimensions should be displayed.
  const size = await getQuickViewMetadataBoxField(appId, 'Dimensions');
  chrome.test.assertEq('400 x 175', size);
}

/**
 * Tests that opening Quick View on an image and clicking the image does not
 * focus the image. Instead, the user should still be able to cycle through
 * file list items in Quick View: crbug.com/1038835.
 */
export async function openQuickViewImageClick() {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Open Files app on Downloads containing two images.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.desktop, ENTRIES.image3], []);

  // Open the first image in Quick View.
  await openQuickViewEx(appId, ENTRIES.desktop.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  let mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/png', mimeType);

  // Click the image in Quick View to attempt to focus it.
  await remoteCall.waitAndClickElement(appId, preview);

  // Press the down arrow key to attempt to select the next file.
  const downArrow = ['#quick-view', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downArrow));

  // Wait for the Quick View preview to load and display its content.
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct mimeType should be displayed.
  mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/jpeg', mimeType);

  // Check: Quick View should be able to close.
  await closeQuickViewEx(appId);
}

/**
 * Tests that opening a broken image in Quick View displays the "no-preview
 * available" generic icon and has a [load-error] attribute.
 */
export async function openQuickViewBrokenImage() {
  const caller = getCaller();

  /**
   * The [generic-thumbnail] element resides in the #quick-view shadow DOM
   * as a child of .no-preview-container which is a sibling of the
   * files-safe-media[type="image"] element.
   */
  const genericThumbnail = [
    '#quick-view',
    'files-safe-media[type="image"][hidden] + .no-preview-container > [generic-thumbnail="image"]',
  ];

  // Open Files app on Downloads containing ENTRIES.brokenJpeg.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.brokenJpeg], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.brokenJpeg.nameText);

  // Check: the quick view element should have a 'load-error' attribute.
  await remoteCall.waitForElement(appId, '#quick-view[load-error]');

  // Wait for the generic thumbnail to load and display its content.
  function checkForGenericThumbnail(elements: ElementObject[]) {
    const haveElements = Array.isArray(elements) && elements.length === 1;
    if (!haveElements || elements[0]!.styles!['display'] !== 'block') {
      return pending(caller, 'Waiting for generic thumbnail to load.');
    }
    return;
  }

  // Check: the generic thumbnail icon should be displayed.
  await repeatUntil(async () => {
    return checkForGenericThumbnail(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [genericThumbnail, ['display']]));
  });
}

/**
 * Tests opening Quick View containing a video.
 */
export async function openQuickViewVideo() {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="video"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="video"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.webm video.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.webm], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.webm.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewVideoLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewVideoLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the preview document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';
  const backgroundColor = await remoteCall.executeJsInPreviewTag<string[]>(
      appId, preview, getBackgroundStyle);
  chrome.test.assertTrue(!!backgroundColor);
  await checkBackgroundColor(backgroundColor[0]!);

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('video/webm', mimeType);

  // Close Quick View.
  await closeQuickViewEx(appId);

  // Check: closing Quick View should remove the video <files-safe-media>
  // preview element, so it stops playing the video. crbug.com/970192
  await remoteCall.waitForElementLost(appId, preview);
}

/**
 * Tests opening Quick View containing a video on Drive.
 */
export async function openQuickViewVideoOnDrive() {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="video"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="video"]', previewTag];

  // Open Files app on Downloads containing ENTRIES.webm video.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.webm]);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.webm.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewVideoLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewVideoLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Get the preview document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';
  const backgroundColor = await remoteCall.executeJsInPreviewTag<string[]>(
      appId, preview, getBackgroundStyle);
  chrome.test.assertTrue(!!backgroundColor);
  await checkBackgroundColor(backgroundColor[0]!);

  // Check: the correct mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('video/webm', mimeType);

  // Close Quick View.
  await closeQuickViewEx(appId);

  // Check: closing Quick View should remove the video <files-safe-media>
  // preview element, so it stops playing the video. crbug.com/970192
  await remoteCall.waitForElementLost(appId, preview);
}

/**
 * Tests opening Quick View with multiple files and using the up/down arrow
 * keys to select and view their content.
 */
export async function openQuickViewKeyboardUpDownChangesView() {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Open Files app on Downloads containing two text files.
  const files = [ENTRIES.hello, ENTRIES.tallText];
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, files, []);

  // Open the last file in Quick View.
  await openQuickViewEx(appId, ENTRIES.tallText.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || !elements[0]!.attributes['src']) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Press the down arrow key to select the next file.
  const downArrow = ['#quick-view', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
                     appId, preview, getTextContent) as string[];
    if (!text || !text[0] || !text[0].includes('This is a sample file')) {
      return pending(caller, 'Waiting for preview content.');
    }
    return;
  });

  // Press the up arrow key to select the previous file.
  const upArrow = ['#quick-view', 'ArrowUp', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, upArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
                     appId, preview, getTextContent) as string[];
    if (!text || !text[0] || !text[0].includes('42 tall text')) {
      return pending(caller, 'Waiting for preview content.');
    }
    return;
  });
}

/**
 * Tests opening Quick View with multiple files and using the left/right arrow
 * keys to select and view their content.
 */
export async function openQuickViewKeyboardLeftRightChangesView() {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Open Files app on Downloads containing two text files.
  const files = [ENTRIES.hello, ENTRIES.tallText];
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, files, []);

  // Open the last file in Quick View.
  await openQuickViewEx(appId, ENTRIES.tallText.nameText);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || !elements[0]!.attributes['src']) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Press the right arrow key to select the next file item.
  const rightArrow = ['#quick-view', 'ArrowRight', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, rightArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
                     appId, preview, getTextContent) as string[];
    if (!text || !text[0] || !text[0].includes('This is a sample file')) {
      return pending(caller, 'Waiting for preview content.');
    }
    return;
  });

  // Press the left arrow key to select the previous file item.
  const leftArrow = ['#quick-view', 'ArrowLeft', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, leftArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
                     appId, preview, getTextContent) as string[];
    if (!text || !text[0] || !text[0].includes('42 tall text')) {
      return pending(caller, 'Waiting for preview content.');
    }
    return;
  });
}

/**
 * Tests that the metadatabox can be toggled opened/closed by pressing the
 * Enter key on the Quick View toolbar info button.
 */
export async function openQuickViewToggleInfoButtonKeyboard() {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.hello.nameText);

  // Check: the metadatabox should be open.
  const metaShown = ['#quick-view', '#contentPanel[metadata-box-active]'];
  await remoteCall.waitForElement(appId, metaShown);

  // The toolbar info button query differs in files-ng.
  const quickView = await remoteCall.waitForElement(appId, ['#quick-view']);
  let infoButton = ['#quick-view', '#metadata-button'];
  if (quickView.attributes['files-ng'] !== undefined) {
    infoButton = ['#quick-view', '#info-button'];
  }

  // Press Enter key on the info button.
  const key = [infoButton, 'Enter', false, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key);

  // Check: the metadatabox should close.
  await remoteCall.waitForElementLost(appId, metaShown);

  // Press Enter key on the info button.
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key);

  // Check: the metadatabox should open.
  await remoteCall.waitForElement(appId, metaShown);
}

/**
 * Tests that the metadatabox can be toggled opened/closed by clicking the
 * the Quick View toolbar info button.
 */
export async function openQuickViewToggleInfoButtonClick() {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.hello.nameText);

  // Check: the metadatabox should be open.
  const metaShown = ['#quick-view', '#contentPanel[metadata-box-active]'];
  await remoteCall.waitForElement(appId, metaShown);

  // The toolbar info button query differs in files-ng.
  const quickView = await remoteCall.waitForElement(appId, ['#quick-view']);
  let infoButton = ['#quick-view', '#metadata-button'];
  if (quickView.attributes['files-ng'] !== undefined) {
    infoButton = ['#quick-view', '#info-button'];
  }

  // Click the info button.
  await remoteCall.waitAndClickElement(appId, infoButton);

  // Check: the metadatabox should close.
  await remoteCall.waitForElementLost(appId, metaShown);

  // Click the info button.
  await remoteCall.waitAndClickElement(appId, infoButton);

  // Check: the metadatabox should open.
  await remoteCall.waitForElement(appId, metaShown);
}

/**
 * Tests that Quick View opens with multiple files selected.
 */
export async function openQuickViewWithMultipleFiles() {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Add item 3 to the check-selection, ENTRIES.desktop.
  const downKey = ['#file-list', 'ArrowDown', false, false, false];
  for (let i = 0; i < 3; i++) {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downKey),
        'ArrowDown failed');
  }
  const ctrlSpace = ['#file-list', ' ', true, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Add item 5 to the check-selection, ENTRIES.hello.
  const ctrlDown = ['#file-list', 'ArrowDown', true, false, false];
  for (let i = 0; i < 2; i++) {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
        'Ctrl+ArrowDown failed');
  }
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Open Quick View with the check-selected files.
  await openQuickViewMultipleSelection(appId, ['Desktop', 'hello']);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }

  // Check: ENTRIES.desktop should be displayed in the preview.
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Check: the correct file mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('image/png', mimeType);
}

/**
 * Tests that Quick View displays text files when multiple files are
 * selected.
 */
export async function openQuickViewWithMultipleFilesText() {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  const files = [ENTRIES.tallText, ENTRIES.hello, ENTRIES.smallJpeg];
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, files, []);

  // Add item 1 to the check-selection, ENTRIES.smallJpeg.
  const downKey = ['#file-list', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downKey),
      'ArrowDown failed');
  const ctrlSpace = ['#file-list', ' ', true, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Add item 3 to the check-selection, ENTRIES.hello.
  const ctrlDown = ['#file-list', 'ArrowDown', true, false, false];
  for (let i = 0; i < 2; i++) {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
        'Ctrl+ArrowDown failed');
  }
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Open Quick View with the check-selected files.
  await openQuickViewMultipleSelection(appId, ['small', 'hello']);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }

  // Check: the image file should be displayed in the content panel.
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const textView = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Press the down arrow key to select the next file.
  const downArrow = ['#quick-view', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downArrow));

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || !elements[0]!.attributes['src']) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }

  // Check: the text file should be displayed in the content panel.
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [textView, ['display']]));
  });

  // Check: the open button should be displayed.
  await remoteCall.waitForElement(
      appId, ['#quick-view', '#open-button:not([hidden])']);
}

/**
 * Tests that Quick View displays pdf files when multiple files are
 * selected.
 */
export async function openQuickViewWithMultipleFilesPdf() {
  const caller = getCaller();

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const preview = ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  const files = [ENTRIES.tallPdf, ENTRIES.desktop, ENTRIES.smallJpeg];
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, files, []);

  // Add item 1 to the check-selection, ENTRIES.smallJpeg.
  const downKey = ['#file-list', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downKey),
      'ArrowDown failed');
  const ctrlSpace = ['#file-list', ' ', true, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Add item 3 to the check-selection, ENTRIES.tallPdf.
  const ctrlDown = ['#file-list', 'ArrowDown', true, false, false];
  for (let i = 0; i < 2; i++) {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
        'Ctrl+ArrowDown failed');
  }
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Open Quick View with the check-selected files.
  await openQuickViewMultipleSelection(appId, ['small', 'tall']);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }

  // Check: the image file should be displayed in the content panel.
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  /**
   * The PDF preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const pdfView = ['#quick-view', `#dialog[open] ${previewTag}.content`];

  // Press the down arrow key to select the next file.
  const downArrow = ['#quick-view', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downArrow));

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewPdfLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || !elements[0]!.attributes['src']) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }

  // Check: the pdf file should be displayed in the content panel.
  await repeatUntil(async () => {
    return checkPreviewPdfLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [pdfView, ['display']]));
  });

  // Check: the open button should be displayed.
  await remoteCall.waitForElement(
      appId, ['#quick-view', '#open-button:not([hidden])']);
}

/**
 * Tests that the content panel changes when using the up/down arrow keys
 * when multiple files are selected.
 */
export async function openQuickViewWithMultipleFilesKeyboardUpDown() {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Open Files app on Downloads containing three text files.
  const files = [ENTRIES.hello, ENTRIES.tallText, ENTRIES.plainText];
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, files, []);

  // Add item 1 to the check-selection, ENTRIES.tallText.
  const downKey = ['#file-list', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downKey),
      'ArrowDown failed');
  const ctrlSpace = ['#file-list', ' ', true, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Add item 3 to the check-selection, ENTRIES.hello.
  const ctrlDown = ['#file-list', 'ArrowDown', true, false, false];
  for (let i = 0; i < 2; i++) {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
        'Ctrl+ArrowDown failed');
  }
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Open Quick View with the check-selected files.
  await openQuickViewMultipleSelection(appId, ['tall', 'hello']);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || !elements[0]!.attributes['src']) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Press the down arrow key to select the next file.
  const downArrow = ['#quick-view', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
                     appId, preview, getTextContent) as string[];
    // Check: the content of ENTRIES.hello should be shown.
    if (!text || !text[0] || !text[0].includes('This is a sample file')) {
      return pending(caller, 'Waiting for preview content.');
    }
    return;
  });

  // Press the up arrow key to select the previous file.
  const upArrow = ['#quick-view', 'ArrowUp', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, upArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
                     appId, preview, getTextContent) as string[];
    // Check: the content of ENTRIES.tallText should be shown.
    if (!text || !text[0] || !text[0].includes('42 tall text')) {
      return pending(caller, 'Waiting for preview content.');
    }
    return;
  });
}

/**
 * Tests that the content panel changes when using the left/right arrow keys
 * when multiple files are selected.
 */
export async function openQuickViewWithMultipleFilesKeyboardLeftRight() {
  const caller = getCaller();

  /**
   * The text preview resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const preview = ['#quick-view', `#dialog[open] ${previewTag}.text-content`];

  // Open Files app on Downloads containing three text files.
  const files = [ENTRIES.hello, ENTRIES.tallText, ENTRIES.plainText];
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS, files, []);

  // Add item 1 to the check-selection, ENTRIES.tallText.
  const downKey = ['#file-list', 'ArrowDown', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downKey),
      'ArrowDown failed');
  const ctrlSpace = ['#file-list', ' ', true, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Add item 3 to the check-selection, ENTRIES.hello.
  const ctrlDown = ['#file-list', 'ArrowDown', true, false, false];
  for (let i = 0; i < 2; i++) {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
        'Ctrl+ArrowDown failed');
  }
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Ctrl+Space failed');

  // Open Quick View with the check-selected files.
  await openQuickViewMultipleSelection(appId, ['tall', 'hello']);

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewTextLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || !elements[0]!.attributes['src']) {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [preview, ['display']]));
  });

  // Press the right arrow key to select the next file item.
  const rightArrow = ['#quick-view', 'ArrowRight', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, rightArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors(
                     appId, preview, getTextContent) as string[];
    // Check: the content of ENTRIES.hello should be shown.
    if (!text || !text[0] || !text[0].includes('This is a sample file')) {
      return pending(caller, 'Waiting for preview content.');
    }
    return;
  });

  // Press the left arrow key to select the previous file item.
  const leftArrow = ['#quick-view', 'ArrowLeft', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, leftArrow));

  // Wait until the preview displays that file's content.
  await repeatUntil(async () => {
    const getTextContent = contentWindowQuery + '.document.body.textContent';
    const text = await executeJsInPreviewTagAndCatchErrors<string>(
        appId, preview, getTextContent);
    // Check: the content of ENTRIES.tallText should be shown.
    if (!text || !text[0] || !text[0].includes('42 tall text')) {
      return pending(caller, 'Waiting for preview content.');
    }
    return;
  });
}

/**
 * Tests opening Quick View and closing with Escape key returns focus to file
 * list.
 */
export async function openQuickViewAndEscape() {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.hello.nameText);

  // Hit Escape key to close Quick View.
  const panelElements = ['#quick-view', '#contentPanel'];
  const key = [panelElements, 'Escape', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown Escape failed');

  // Check: the Quick View element should not be shown.
  await waitQuickViewClose(appId);

  // Check: the file list should gain the focus.
  const element = await remoteCall.waitForElement(appId, '#file-list:focus');
  chrome.test.assertEq(
      'file-list', element.attributes['id'], '#file-list should be focused');
}

/**
 * Test opening Quick View when Directory Tree is focused it should display if
 * there is only 1 file/folder selected in the file list.
 */
export async function openQuickViewFromDirectoryTree() {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Focus Directory Tree.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.focusTree();

  // Ctrl+A to select the only file.
  const ctrlA = [directoryTree.rootSelector, 'a', true, false, false] as const;
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  // Use selection menu button to open Quick View.
  await remoteCall.simulateUiClick(
      appId, '#selection-menu-button:not([hidden])');

  // Wait because WebUI Menu ignores the following click if it happens in
  // <200ms from the previous click.
  await wait(300);

  // Click the Menu item to show the Quick View.
  const getInfoMenuItem = '#file-context-menu:not([hidden]) ' +
      ' [command="#get-info"]:not([hidden])';
  await remoteCall.simulateUiClick(appId, getInfoMenuItem);

  // Check: the Quick View dialog should be shown.
  const caller = getCaller();
  await repeatUntil(async () => {
    const query = ['#quick-view', '#dialog[open]'];
    const elements = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [query, ['display']]);
    const haveElements = Array.isArray(elements) && elements.length !== 0;
    if (!haveElements || elements[0]!.styles!['display'] !== 'block') {
      return pending(caller, 'Waiting for Quick View to open.');
    }
    return true;
  });
}

/**
 * Tests the tab-index focus order when sending tab keys when an image file is
 * shown in Quick View.
 */
export async function openQuickViewTabIndexImage() {
  // Prepare a list of tab-index focus queries.
  const tabQueries = [
    {'query': ['#quick-view', `[aria-label="Back"]:focus`]},
    {'query': ['#quick-view', `[aria-label="Open"]:focus`]},
    {'query': ['#quick-view', `[aria-label="Delete"]:focus`]},
    {'query': ['#quick-view', `[aria-label="File info"]:focus`]},
  ];

  // Open Files app on Downloads containing ENTRIES.smallJpeg.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.smallJpeg], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.smallJpeg.nameText);

  for (const query of tabQueries) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result =
        await sendTestMessage({name: 'dispatchTabKey', shift: false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: the queried element should gain the focus.
    await remoteCall.waitForElement(appId, query.query);
  }
}

/**
 * Tests the tab-index focus order when sending tab keys when a text file is
 * shown in Quick View.
 */
export async function openQuickViewTabIndexText() {
  // Prepare a list of tab-index focus queries.
  const tabQueries = [
    {'query': ['#quick-view', `[aria-label="Back"]:focus`]},
    {'query': ['#quick-view', `[aria-label="Open"]:focus`]},
    {'query': ['#quick-view', `[aria-label="Delete"]:focus`]},
    {'query': ['#quick-view', `[aria-label="File info"]:focus`]},
    {'query': ['#quick-view']},  // Tab past the content panel.
    {'query': ['#quick-view', `[aria-label="Back"]:focus`]},
  ];

  // Open Files app on Downloads containing ENTRIES.tallText.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.tallText], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.tallText.nameText);

  for (const query of tabQueries) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result =
        await sendTestMessage({name: 'dispatchTabKey', shift: false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: the queried element should gain the focus.
    await remoteCall.waitForElement(appId, query.query);
  }
}

/**
 * Tests the tab-index focus order when sending tab keys when an HTML file is
 * shown in Quick View.
 */
export async function openQuickViewTabIndexHtml() {
  // Prepare a list of tab-index focus queries.
  const tabQueries = [
    {'query': ['#quick-view', `[aria-label="Back"]:focus`]},
    {'query': ['#quick-view', `[aria-label="Open"]:focus`]},
    {'query': ['#quick-view', `[aria-label="Delete"]:focus`]},
    {'query': ['#quick-view', `[aria-label="File info"]:focus`]},
  ];

  // Open Files app on Downloads containing ENTRIES.tallHtml.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.tallHtml], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.tallHtml.nameText);

  for (const query of tabQueries) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result =
        await sendTestMessage({name: 'dispatchTabKey', shift: false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: the queried element should gain the focus.
    await remoteCall.waitForElement(appId, query.query);
  }
}

/**
 * Tests the tab-index focus order when sending tab keys when an audio file
 * is shown in Quick View.
 */
export async function openQuickViewTabIndexAudio() {
  // Open Files app on Downloads containing ENTRIES.beautiful song.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.beautiful.nameText);

  // Prepare a list of tab-index focus queries.
  const tabQueries = [
    {'query': ['#quick-view', `[aria-label="Back"]:focus`]},
    {'query': ['#quick-view', `[aria-label="Open"]:focus`]},
    {'query': ['#quick-view', `[aria-label="Delete"]:focus`]},
    {'query': ['#quick-view', `[aria-label="File info"]:focus`]},
  ];

  for (const query of tabQueries) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result =
        await sendTestMessage({name: 'dispatchTabKey', shift: false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: the queried element should gain the focus.
    await remoteCall.waitForElement(appId, query.query);
  }

  // Send tab keys until Back gains the focus again.
  while (true) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result =
        await sendTestMessage({name: 'dispatchTabKey', shift: false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: back should eventually get the focus again.
    const activeElement =
        await remoteCall.callRemoteTestUtil<ElementObject|null>(
            'deepGetActiveElement', appId, []);
    if (activeElement && activeElement.attributes['aria-label'] === 'Back') {
      break;
    }
  }
}

/**
 * Tests the tab-index focus order when sending tab keys when a video file is
 * shown in Quick View.
 */
export async function openQuickViewTabIndexVideo() {
  // Open Files app on Downloads containing ENTRIES.webm video.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.webm], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.webm.nameText);

  // Prepare a list of tab-index focus queries.
  const tabQueries = [
    {'query': ['#quick-view', `[aria-label="Back"]:focus`]},
    {'query': ['#quick-view', `[aria-label="Open"]:focus`]},
    {'query': ['#quick-view', `[aria-label="Delete"]:focus`]},
    {'query': ['#quick-view', `[aria-label="File info"]:focus`]},
  ];

  for (const query of tabQueries) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result =
        await sendTestMessage({name: 'dispatchTabKey', shift: false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: the queried element should gain the focus.
    await remoteCall.waitForElement(appId, query.query);
  }

  // Send tab keys until Back gains the focus again.
  while (true) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result =
        await sendTestMessage({name: 'dispatchTabKey', shift: false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: back should eventually get the focus again.
    const activeElement =
        await remoteCall.callRemoteTestUtil<ElementObject|null>(
            'deepGetActiveElement', appId, []);
    if (activeElement && activeElement.attributes['aria-label'] === 'Back') {
      break;
    }
  }
}

/**
 * Tests that the tab-index focus stays within the delete confirm dialog.
 */
export async function openQuickViewTabIndexDeleteDialog() {
  // Open Files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Open a USB file in Quick View. USB delete never uses trash and always
  // shows the delete dialog.
  await mountAndSelectUsb(appId);
  await openQuickViewEx(appId, ENTRIES.hello.nameText);

  // Open the Quick View delete confirm dialog.
  const deleteKey = ['#quick-view', 'Delete', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, deleteKey),
      'Pressing Delete failed.');

  // Check: the Quick View delete confirm dialog should open.
  await remoteCall.waitForElement(
      appId,  // The <cr-dialog> is a child of the Quick View shadow DOM.
      ['#quick-view', '.cr-dialog-container.shown .cr-dialog-cancel:focus']);

  // Prepare a list of tab-index focus queries.
  const tabQueries = [
    {'query': ['#quick-view', '.cr-dialog-ok:not([hidden])']},
    {'query': ['#quick-view', '.cr-dialog-cancel:not([hidden])']},
  ];

  for (const query of tabQueries) {
    // Make the browser dispatch a tab key event to FilesApp.
    const result =
        await sendTestMessage({name: 'dispatchTabKey', shift: false});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    // Note: Allow 500ms between key events to filter out the focus
    // traversal problems noted in crbug.com/907380#c10.
    await wait(500);

    // Check: the queried element should gain the focus.
    await remoteCall.waitForElement(appId, query.query);
  }
}

/**
 * Tests deleting an item from Quick View when in single select mode, and
 * that Quick View closes when there are no more items to view.
 */
export async function openQuickViewAndDeleteSingleSelection() {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.hello.nameText);

  // Press delete key.
  const deleteKey = ['#quick-view', 'Delete', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, deleteKey),
      'Pressing Delete failed.');

  // Check: |hello.txt| should have been deleted.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Check: the Quick View dialog should close.
  await waitQuickViewClose(appId);
}

/**
 * Tests deleting an item from Quick View while in check-selection mode.
 * Deletes the item at the bottom of the file list, and checks that
 * the item below the item deleted is shown in Quick View after the item's
 * deletion.
 */
export async function openQuickViewAndDeleteCheckSelection() {
  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  const caller = getCaller();

  // Ctrl+A to select all files in the file-list.
  const ctrlA = ['#file-list', 'a', true, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlA),
      'Ctrl+A failed');

  // Open Quick View via its keyboard shortcut.
  const space = ['#file-list', ' ', false, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, space);

  // Check: the Quick View dialog should be shown.
  await waitQuickViewOpen(appId);

  // Press the up arrow to go to the last file in the selection.
  const quickViewArrowUp = ['#quick-view', 'ArrowUp', false, false, false];
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, quickViewArrowUp));

  // Press delete key.
  const deleteKey = ['#quick-view', 'Delete', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, deleteKey),
      'Pressing Delete failed.');

  // Check: |hello.txt| should have been deleted.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Check: Quick View should display the entry below |hello.txt|,
  // which is |world.ogv|.
  function checkPreviewVideoLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }

  const videoWebView =
      ['#quick-view', 'files-safe-media[type="video"]', previewTag];
  await repeatUntil(async () => {
    return checkPreviewVideoLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [videoWebView, ['display']]));
  });

  // Check: the mimeType of |world.ogv| should be 'video/ogg'.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('video/ogg', mimeType);
}

/**
 * Tests that deleting all items in a check-selection closes the Quick View.
 */
export async function openQuickViewDeleteEntireCheckSelection() {
  const caller = getCaller();

  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Check-select Beautiful Song.ogg and My Desktop Background.png.
  const ctrlDown = ['#file-list', 'ArrowDown', true, false, false];
  const ctrlSpace = ['#file-list', ' ', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
      'Pressing Ctrl+Down failed.');

  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
      'Pressing Ctrl+Down failed.');

  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Pressing Ctrl+Space failed.');

  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
      'Pressing Ctrl+Down failed.');

  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
      'Pressing Ctrl+Space failed.');

  // Open Quick View on the check-selected files.
  await openQuickViewMultipleSelection(appId, ['Beautiful', 'Desktop']);

  /**
   * The preview resides in the <files-safe-media type="audio"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const audioWebView =
      ['#quick-view', 'files-safe-media[type="audio"]', previewTag];

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewAudioLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewAudioLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [audioWebView, ['display']]));
  });

  // Press delete.
  const deleteKey = ['#quick-view', 'Delete', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, deleteKey),
      'Pressing Delete failed.');

  // Check: |Beautiful Song.ogg| should have been deleted.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="Beautiful Song.ogg"]');

  /**
   * The preview resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const imageWebView =
      ['#quick-view', 'files-safe-media[type="image"]', previewTag];

  // Wait for the Quick View preview to load and display its content.
  function checkPreviewImageLoaded(elements: ElementObject[]) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements) {
      haveElements = elements[0]!.styles!['display']!.includes('block');
    }
    if (!haveElements || elements[0]!.attributes['loaded'] !== '') {
      return pending(caller, `Waiting for ${previewTag} to load.`);
    }
    return;
  }
  await repeatUntil(async () => {
    return checkPreviewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [imageWebView, ['display']]));
  });

  // Press delete.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, deleteKey),
      'Pressing Delete failed.');

  // Check: |My Desktop Background.png| should have been deleted.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="My Desktop Background.png"]');

  // Check: the Quick View dialog should close.
  await waitQuickViewClose(appId);
}

/**
 * Tests that an item can be deleted using the Quick View delete button.
 */
export async function openQuickViewClickDeleteButton() {
  // Open Files app on Downloads containing ENTRIES.hello.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.hello.nameText);

  // Click the Quick View delete button.
  const quickViewDeleteButton = ['#quick-view', '#delete-button:not([hidden])'];
  await remoteCall.waitAndClickElement(appId, quickViewDeleteButton);

  // Check: |hello.txt| should have been deleted.
  await remoteCall.waitForElementLost(
      appId, '#file-list [file-name="hello.txt"]');

  // Check: the Quick View dialog should close.
  await waitQuickViewClose(appId);
}

/**
 * Tests that the delete button is not shown if the file displayed in Quick
 * View cannot be deleted.
 */
export async function openQuickViewDeleteButtonNotShown() {
  // Open Files app on My Files
  const appId = await remoteCall.openNewWindow('');

  // Wait for the file list to appear.
  await remoteCall.waitForElement(appId, '#file-list');

  // Check: My Files should contain the expected entries.
  const expectedRows = [
    ['Play files', '--', 'Folder'],
    ['Downloads', '--', 'Folder'],
    ['Linux files', '--', 'Folder'],
  ];
  await remoteCall.waitForFiles(
      appId, expectedRows, {ignoreLastModifiedTime: true});

  // Open Play files in Quick View, which cannot be deleted.
  await openQuickViewEx(appId, 'Play files');

  // Check: the delete button should not be shown.
  const quickViewDeleteButton = ['#quick-view', '#delete-button[hidden]'];
  await remoteCall.waitForElement(appId, quickViewDeleteButton);
}

/**
 * Tests that the correct WayToOpen UMA histogram is recorded when opening
 * a single file via Quick View using "Get Info" from the context menu.
 */
export async function openQuickViewUmaViaContextMenu() {
  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Record the UMA value's bucket count before we use the menu option.
  const contextMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  // Open Quick View via the entry context menu.
  await openQuickViewViaContextMenu(appId, ENTRIES.hello.nameText);

  // Check: the context menu histogram should increment by 1.
  const contextMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  chrome.test.assertEq(
      contextMenuUMAValueAfterOpening, contextMenuUMAValueBeforeOpening + 1);
  chrome.test.assertEq(
      selectionMenuUMAValueAfterOpening, selectionMenuUMAValueBeforeOpening);
}

/**
 * Tests that the correct WayToOpen UMA histogram is recorded when using
 * Quick View in check-select mode using "Get Info" from the context
 * menu.
 */
export async function openQuickViewUmaForCheckSelectViaContextMenu() {
  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Record the UMA value's bucket count before we use the menu option.
  const contextMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  // Ctrl+A to select all files in the file-list.
  const ctrlA = ['#file-list', 'a', true, false, false] as const;
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  // Open Quick View using the context menu.
  await openQuickViewViaContextMenu(appId, ENTRIES.hello.nameText);

  // Check: the context menu histogram should increment by 1.
  const contextMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  chrome.test.assertEq(
      contextMenuUMAValueAfterOpening, contextMenuUMAValueBeforeOpening + 1);
  chrome.test.assertEq(
      selectionMenuUMAValueAfterOpening, selectionMenuUMAValueBeforeOpening);
}

/**
 * Tests that the correct WayToOpen UMA histogram is recorded when using
 * Quick View in check-select mode using "Get Info" from the Selection
 * menu.
 */
export async function openQuickViewUmaViaSelectionMenu() {
  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Ctrl+A to select all files in the file-list.
  const ctrlA = ['#file-list', 'a', true, false, false] as const;
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  const caller = getCaller();

  // Wait until the selection menu is visible.
  function checkElementsDisplayVisible(elements: ElementObject[]) {
    chrome.test.assertTrue(Array.isArray(elements));
    if (elements.length === 0 || elements[0]!.styles!['display'] === 'none') {
      return pending(caller, 'Waiting for Selection Menu to be visible.');
    }
    return;
  }

  await repeatUntil(async () => {
    const elements = ['#selection-menu-button'];
    return checkElementsDisplayVisible(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [elements, ['display']]));
  });

  // Record the UMA value's bucket count before we use the menu option.
  const contextMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  // Click the Selection Menu button. Using fakeMouseClick causes
  // the focus to switch from file-list such that crbug.com/1046997
  // cannot be tested, use simulateUiClick() instead.
  await remoteCall.simulateUiClick(
      appId, '#selection-menu-button:not([hidden])');

  // Wait because WebUI Menu ignores the following click if it happens in
  // <200ms from the previous click.
  await wait(300);

  // Click the file-list context menu "Get info" command.
  await remoteCall.simulateUiClick(
      appId,
      '#file-context-menu:not([hidden]) [command="#get-info"]:not([hidden])');

  // Check: the Quick View dialog should be shown.
  await repeatUntil(async () => {
    const query = ['#quick-view', '#dialog[open]'];
    const elements = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [query, ['display']]);
    const haveElements = Array.isArray(elements) && elements.length !== 0;
    if (!haveElements || elements[0]!.styles!['display'] !== 'block') {
      return pending(caller, 'Waiting for Quick View to open.');
    }
    return true;
  });

  // Check: the context menu histogram should increment by 1.
  const contextMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  chrome.test.assertEq(
      contextMenuUMAValueAfterOpening, contextMenuUMAValueBeforeOpening);
  chrome.test.assertEq(
      selectionMenuUMAValueAfterOpening,
      selectionMenuUMAValueBeforeOpening + 1);
}

/**
 * Tests that the correct WayToOpen UMA histogram is recorded when using
 * Quick View in check-select mode using "Get Info" from the context
 * menu opened via keyboard tabbing (not mouse).
 */
export async function openQuickViewUmaViaSelectionMenuKeyboard() {
  const caller = getCaller();

  // Open Files app on Downloads containing BASIC_LOCAL_ENTRY_SET.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Ctrl+A to select all files in the file-list.
  const ctrlA = ['#file-list', 'a', true, false, false] as const;
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  // Wait until the selection menu is visible.
  function checkElementsDisplayVisible(elements: ElementObject[]) {
    chrome.test.assertTrue(Array.isArray(elements));
    if (elements.length === 0 || elements[0]!.styles!['display'] === 'none') {
      return pending(caller, 'Waiting for Selection Menu to be visible.');
    }
    return;
  }

  await repeatUntil(async () => {
    const elements = ['#selection-menu-button'];
    return checkElementsDisplayVisible(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [elements, ['display']]));
  });

  // Record the UMA value's bucket count before we use the menu option.
  const contextMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueBeforeOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  // Tab to the Selection Menu button.
  await repeatUntil(async () => {
    const result = await sendTestMessage({name: 'dispatchTabKey'});
    chrome.test.assertEq(
        'tabKeyDispatched', result, 'Tab key dispatch failure');

    const element = await remoteCall.callRemoteTestUtil<ElementObject|null>(
        'getActiveElement', appId, []);

    if (element && element.attributes['id'] === 'selection-menu-button') {
      return true;
    }
    return pending(
        caller, 'Waiting for selection-menu-button to become active');
  });

  // Key down to the "Get Info" command.
  await repeatUntil(async () => {
    const keyDown =
        ['#selection-menu-button', 'ArrowDown', false, false, false];
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, keyDown));

    const element = await remoteCall.callRemoteTestUtil<ElementObject|null>(
        'getActiveElement', appId, []);

    if (element && element.attributes['command'] === '#get-info') {
      return true;
    }
    return pending(caller, 'Waiting for get-info command to become active');
  });

  // Select the "Get Info" command using the Enter key.
  const keyEnter = ['#selection-menu-button', 'Enter', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, keyEnter));

  // Check: the Quick View dialog should be shown.
  await repeatUntil(async () => {
    const query = ['#quick-view', '#dialog[open]'];
    const elements = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [query, ['display']]);
    const haveElements = Array.isArray(elements) && elements.length !== 0;
    if (!haveElements || elements[0]!.styles!['display'] !== 'block') {
      return pending(caller, 'Waiting for Quick View to open.');
    }
    return true;
  });

  // Check: the context menu histogram should increment by 1.
  const contextMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.CONTEXT_MENU);

  const selectionMenuUMAValueAfterOpening = await getHistogramCount(
      QuickViewUmaWayToOpenHistogramName,
      QuickViewUmaWayToOpenHistogramValues.SELECTION_MENU);

  chrome.test.assertEq(
      contextMenuUMAValueAfterOpening, contextMenuUMAValueBeforeOpening);
  chrome.test.assertEq(
      selectionMenuUMAValueAfterOpening,
      selectionMenuUMAValueBeforeOpening + 1);
}

/**
 * Tests that Quick View does not display a CSE file preview.
 */
export async function openQuickViewEncryptedFile() {
  const caller = getCaller();

  /**
   * The #innerContentPanel resides in the #quick-view shadow DOM as a child
   * of the #dialog element, and contains the file preview result.
   */
  const contentPanel = ['#quick-view', '#dialog[open] #innerContentPanel'];

  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.testCSEFile]);

  // Open the file in Quick View.
  await openQuickViewEx(appId, ENTRIES.testCSEFile.nameText);

  // Wait for the innerContentPanel to load and display its content.
  function checkInnerContentPanel(elements: ElementObject[]) {
    const haveElements = Array.isArray(elements) && elements.length === 1;
    if (!haveElements || elements[0]!.styles!['display'] !== 'flex') {
      return pending(caller, 'Waiting for inner content panel to load.');
    }
    // Check: the preview should not be shown.
    chrome.test.assertEq('No preview available', elements[0]!.innerText);
    return;
  }
  await repeatUntil(async () => {
    return checkInnerContentPanel(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [contentPanel, ['display']]));
  });

  // Check: the correct file mimeType should be displayed.
  const mimeType = await getQuickViewMetadataBoxField(appId, 'Type');
  chrome.test.assertEq('Encrypted text/plain', mimeType);
}
