// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

[ExternalExtensionType="fileManagerPrivate.SearchType"]
typedef object FileManagerPrivateSearchType;

[ExternalExtensionType="fileManagerPrivate.FileCategory"]
typedef object FileManagerPrivateFileCategory;

[ExternalExtensionType="fileManagerPrivate.EntryProperties"]
typedef object FileManagerPrivateEntryProperties;

[ExternalExtensionType="fileSystemProvider.Action"]
typedef object FileSystemProviderAction;

[ExternalExtensionType="fileManagerPrivate.MediaMetadata"]
typedef object FileManagerPrivateMediaMetadata;

[ExternalExtensionType="fileManagerPrivate.TaskResult"]
typedef object FileManagerPrivateTaskResult;

[ExternalExtensionType="fileManagerPrivate.ResultingTasks"]
typedef object FileManagerPrivateResultingTasks;

[ExternalExtensionType="fileManagerPrivate.DlpMetadata"]
typedef object FileManagerPrivateDlpMetadata;

[ExternalExtensionType="fileManagerPrivate.DriveQuotaMetadata"]
typedef object FileManagerPrivateDriveQuotaMetadata;

[ExternalExtensionType="fileManagerPrivate.EntryPropertyName"]
typedef object FileManagerPrivateEntryPropertyName;

[ExternalExtensionType="fileManagerPrivate.FileTaskDescriptor"]
typedef object FileManagerPrivateFileTaskDescriptor;

[ExternalExtensionType="fileManagerPrivate.GetVolumeRootOptions"]
typedef object FileManagerPrivateGetVolumeRootOptions;

[ExternalExtensionType="fileManagerPrivate.SourceRestriction"]
typedef object FileManagerPrivateSourceRestriction;

[ExternalExtensionType="fileManagerPrivate.IoTaskType"]
typedef object FileManagerPrivateIoTaskType;

// Entry information that renderers need to create an Entry instance.
dictionary EntryDescription {
  required DOMString fileSystemName;
  required DOMString fileSystemRoot;
  required DOMString fileFullPath;
  required boolean fileIsDirectory;
};

dictionary IOTaskParams {
  DOMString destinationFolderUrl;
  DOMString password;
  boolean showNotification;
};

dictionary ParsedTrashInfoFile {
  required EntryDescription restoreEntry;
  required DOMString trashInfoFileName;
  required double deletionDate;
};

dictionary SearchFilesParams {
  DOMString rootUrl;
  required DOMString query;
  required FileManagerPrivateSearchType types;
  required long maxResults;
  required double modifiedTimestamp;
  required FileManagerPrivateFileCategory category;
};

dictionary CrostiniSharedPathResponse {
  required sequence<EntryDescription> entries;
  required boolean firstForSession;
};

// Internal, used by fileManagerPrivate's custom bindings.
[platforms=("chromeos"),
 implemented_in="chrome/browser/ash/extensions/file_manager/file_manager_private_api_functions.h"]
interface FileManagerPrivateInternal {
  // |PromiseValue|: entries
  [requiredCallback]
  static Promise<sequence<EntryDescription>> resolveIsolatedEntries(
      sequence<DOMString> urls);

  // |PromiseValue|: entryProperties
  [requiredCallback]
  static Promise<sequence<FileManagerPrivateEntryProperties>>
  getEntryProperties(sequence<DOMString> urls,
                     sequence<FileManagerPrivateEntryPropertyName> names);

  // |PromiseValue|: success
  [requiredCallback] static Promise<boolean?> addFileWatch(DOMString url);

  // |PromiseValue|: success
  [requiredCallback] static Promise<boolean?> removeFileWatch(DOMString url);

  // |PromiseValue|: actions
  [requiredCallback]
  static Promise<sequence<FileSystemProviderAction>> getCustomActions(
      sequence<DOMString> urls);

  [requiredCallback] static Promise<undefined> executeCustomAction(
      sequence<DOMString> urls,
      DOMString actionId);

  // |PromiseValue|: result
  [requiredCallback]
  static Promise<DOMString> getContentMimeType(DOMString blobUUID);

  // |PromiseValue|: result
  [requiredCallback]
  static Promise<FileManagerPrivateMediaMetadata> getContentMetadata(
      DOMString blobUUID,
      DOMString mimeType,
      boolean includeImages);

  [requiredCallback] static Promise<undefined> pinDriveFile(
      DOMString url,
      boolean pin);

  // |PromiseValue|: result
  [requiredCallback] static Promise<FileManagerPrivateTaskResult> executeTask(
      FileManagerPrivateFileTaskDescriptor descriptor,
      sequence<DOMString> urls);

  // |PromiseValue|: entries
  [requiredCallback] static Promise<sequence<EntryDescription>> searchFiles(
      SearchFilesParams searchParams);

  [requiredCallback] static Promise<undefined> setDefaultTask(
      FileManagerPrivateFileTaskDescriptor descriptor,
      sequence<DOMString> urls,
      sequence<DOMString> mimeTypes);

  // |PromiseValue|: resultingTasks
  [requiredCallback]
  static Promise<FileManagerPrivateResultingTasks> getFileTasks(
      sequence<DOMString> urls,
      sequence<DOMString> dlpSourceUrls);

  // |PromiseValue|: entries
  [requiredCallback]
  static Promise<sequence<EntryDescription>> getDisallowedTransfers(
      sequence<DOMString> entries,
      DOMString destinationEntry,
      boolean isMove);

  // |PromiseValue|: entries
  [requiredCallback]
  static Promise<sequence<FileManagerPrivateDlpMetadata>> getDlpMetadata(
      sequence<DOMString> entries);

  // |PromiseValue|: driveQuotaMetadata
  [requiredCallback]
  static Promise<FileManagerPrivateDriveQuotaMetadata?> getDriveQuotaMetadata(
      DOMString url);

  // |PromiseValue|: result
  [requiredCallback] static Promise<boolean> validatePathNameLength(
      DOMString parentUrl,
      DOMString name);

  // |PromiseValue|: size
  [requiredCallback] static Promise<double> getDirectorySize(DOMString url);

  // |PromiseValue|: rootDir
  [requiredCallback] static Promise<EntryDescription> getVolumeRoot(
      FileManagerPrivateGetVolumeRootOptions options);

  // |PromiseValue|: entries
  [requiredCallback] static Promise<sequence<EntryDescription>> getRecentFiles(
      FileManagerPrivateSourceRestriction restriction,
      DOMString query,
      long cutoff_days,
      FileManagerPrivateFileCategory file_category,
      boolean invalidate_cache);

  [requiredCallback] static Promise<undefined> sharePathsWithCrostini(
      DOMString vmName,
      sequence<DOMString> urls,
      boolean persist);

  [requiredCallback] static Promise<undefined> unsharePathWithCrostini(
      DOMString vmName,
      DOMString url);

  // |PromiseValue|: response
  [requiredCallback]
  static Promise<CrostiniSharedPathResponse> getCrostiniSharedPaths(
      boolean observeFirstForSession,
      DOMString vmName);

  static undefined importCrostiniImage(DOMString url);

  static Promise<undefined> toggleAddedToHoldingSpace(
      sequence<DOMString> urls,
      boolean add);

  // |PromiseValue|: taskId
  static Promise<long> startIOTask(
      FileManagerPrivateIoTaskType type,
      sequence<DOMString> urls,
      IOTaskParams params);

  // |PromiseValue|: files
  [requiredCallback]
  static Promise<sequence<ParsedTrashInfoFile>> parseTrashInfoFiles(
      sequence<DOMString> urls);
};

partial interface Browser {
  static attribute FileManagerPrivateInternal fileManagerPrivateInternal;
};
