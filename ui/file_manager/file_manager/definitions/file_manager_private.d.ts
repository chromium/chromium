// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.fileManagerPrivate API
 * Generated from: chrome/common/extensions/api/file_manager_private.idl
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/file_manager_private.idl -g ts_definitions` to
 * regenerate.
 */

import type {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {

    export namespace fileManagerPrivate {

      export enum VolumeType {
        DRIVE = 'drive',
        DOWNLOADS = 'downloads',
        REMOVABLE = 'removable',
        ARCHIVE = 'archive',
        PROVIDED = 'provided',
        MTP = 'mtp',
        MEDIA_VIEW = 'media_view',
        CROSTINI = 'crostini',
        ANDROID_FILES = 'android_files',
        DOCUMENTS_PROVIDER = 'documents_provider',
        TESTING = 'testing',
        SMB = 'smb',
        SYSTEM_INTERNAL = 'system_internal',
        GUEST_OS = 'guest_os',
      }

      export enum DeviceType {
        USB = 'usb',
        SD = 'sd',
        OPTICAL = 'optical',
        MOBILE = 'mobile',
        UNKNOWN = 'unknown',
      }

      export enum DeviceConnectionState {
        OFFLINE = 'OFFLINE',
        ONLINE = 'ONLINE',
      }

      export enum DriveConnectionStateType {
        OFFLINE = 'OFFLINE',
        METERED = 'METERED',
        ONLINE = 'ONLINE',
      }

      export enum DriveOfflineReason {
        NOT_READY = 'NOT_READY',
        NO_NETWORK = 'NO_NETWORK',
        NO_SERVICE = 'NO_SERVICE',
      }

      export enum MountContext {
        USER = 'user',
        AUTO = 'auto',
      }

      export enum MountCompletedEventType {
        MOUNT = 'mount',
        UNMOUNT = 'unmount',
      }

      export enum MountError {
        SUCCESS = 'success',
        IN_PROGRESS = 'in_progress',
        UNKNOWN_ERROR = 'unknown_error',
        INTERNAL_ERROR = 'internal_error',
        INVALID_ARGUMENT = 'invalid_argument',
        INVALID_PATH = 'invalid_path',
        PATH_ALREADY_MOUNTED = 'path_already_mounted',
        PATH_NOT_MOUNTED = 'path_not_mounted',
        DIRECTORY_CREATION_FAILED = 'directory_creation_failed',
        INVALID_MOUNT_OPTIONS = 'invalid_mount_options',
        INSUFFICIENT_PERMISSIONS = 'insufficient_permissions',
        MOUNT_PROGRAM_NOT_FOUND = 'mount_program_not_found',
        MOUNT_PROGRAM_FAILED = 'mount_program_failed',
        INVALID_DEVICE_PATH = 'invalid_device_path',
        UNKNOWN_FILESYSTEM = 'unknown_filesystem',
        UNSUPPORTED_FILESYSTEM = 'unsupported_filesystem',
        NEED_PASSWORD = 'need_password',
        CANCELLED = 'cancelled',
        BUSY = 'busy',
      }

      export enum FormatFileSystemType {
        VFAT = 'vfat',
        EXFAT = 'exfat',
        NTFS = 'ntfs',
      }

      export enum TransferState {
        IN_PROGRESS = 'in_progress',
        QUEUED = 'queued',
        COMPLETED = 'completed',
        FAILED = 'failed',
      }

      export enum InstallLinuxPackageStatus {
        STARTED = 'started',
        FAILED = 'failed',
        INSTALL_ALREADY_ACTIVE = 'install_already_active',
      }

      export enum FileWatchEventType {
        CHANGED = 'changed',
        ERROR = 'error',
      }

      export enum ChangeType {
        ADD_OR_UPDATE = 'add_or_update',
        DELETE = 'delete',
      }

      export enum SearchType {
        EXCLUDE_DIRECTORIES = 'EXCLUDE_DIRECTORIES',
        SHARED_WITH_ME = 'SHARED_WITH_ME',
        OFFLINE = 'OFFLINE',
        ALL = 'ALL',
      }

      export enum ZoomOperationType {
        IN = 'in',
        OUT = 'out',
        RESET = 'reset',
      }

      export enum InspectionType {
        NORMAL = 'normal',
        CONSOLE = 'console',
        ELEMENT = 'element',
        BACKGROUND = 'background',
      }

      export enum DeviceEventType {
        DISABLED = 'disabled',
        REMOVED = 'removed',
        HARD_UNPLUGGED = 'hard_unplugged',
        FORMAT_START = 'format_start',
        FORMAT_SUCCESS = 'format_success',
        FORMAT_FAIL = 'format_fail',
        RENAME_START = 'rename_start',
        RENAME_SUCCESS = 'rename_success',
        RENAME_FAIL = 'rename_fail',
        PARTITION_START = 'partition_start',
        PARTITION_SUCCESS = 'partition_success',
        PARTITION_FAIL = 'partition_fail',
      }

      export enum DriveSyncErrorType {
        DELETE_WITHOUT_PERMISSION = 'delete_without_permission',
        SERVICE_UNAVAILABLE = 'service_unavailable',
        NO_SERVER_SPACE = 'no_server_space',
        NO_SERVER_SPACE_ORGANIZATION = 'no_server_space_organization',
        NO_LOCAL_SPACE = 'no_local_space',
        NO_SHARED_DRIVE_SPACE = 'no_shared_drive_space',
        MISC = 'misc',
      }

      export enum DriveConfirmDialogType {
        ENABLE_DOCS_OFFLINE = 'enable_docs_offline',
      }

      export enum DriveDialogResult {
        NOT_DISPLAYED = 'not_displayed',
        ACCEPT = 'accept',
        REJECT = 'reject',
        DISMISS = 'dismiss',
      }

      export enum TaskResult {
        OPENED = 'opened',
        MESSAGE_SENT = 'message_sent',
        FAILED = 'failed',
        EMPTY = 'empty',
        FAILED_PLUGIN_VM_DIRECTORY_NOT_SHARED =
            'failed_plugin_vm_directory_not_shared',
      }

      export enum DriveShareType {
        CAN_EDIT = 'can_edit',
        CAN_COMMENT = 'can_comment',
        CAN_VIEW = 'can_view',
      }

      export enum EntryPropertyName {
        SIZE = 'size',
        MODIFICATION_TIME = 'modificationTime',
        MODIFICATION_BY_ME_TIME = 'modificationByMeTime',
        THUMBNAIL_URL = 'thumbnailUrl',
        CROPPED_THUMBNAIL_URL = 'croppedThumbnailUrl',
        IMAGE_WIDTH = 'imageWidth',
        IMAGE_HEIGHT = 'imageHeight',
        IMAGE_ROTATION = 'imageRotation',
        PINNED = 'pinned',
        PRESENT = 'present',
        HOSTED = 'hosted',
        AVAILABLE_OFFLINE = 'availableOffline',
        AVAILABLE_WHEN_METERED = 'availableWhenMetered',
        DIRTY = 'dirty',
        CUSTOM_ICON_URL = 'customIconUrl',
        CONTENT_MIME_TYPE = 'contentMimeType',
        SHARED_WITH_ME = 'sharedWithMe',
        SHARED = 'shared',
        STARRED = 'starred',
        EXTERNAL_FILE_URL = 'externalFileUrl',
        ALTERNATE_URL = 'alternateUrl',
        SHARE_URL = 'shareUrl',
        CAN_COPY = 'canCopy',
        CAN_DELETE = 'canDelete',
        CAN_RENAME = 'canRename',
        CAN_ADD_CHILDREN = 'canAddChildren',
        CAN_SHARE = 'canShare',
        CAN_PIN = 'canPin',
        IS_MACHINE_ROOT = 'isMachineRoot',
        IS_EXTERNAL_MEDIA = 'isExternalMedia',
        IS_ARBITRARY_SYNC_FOLDER = 'isArbitrarySyncFolder',
        SYNC_STATUS = 'syncStatus',
        PROGRESS = 'progress',
        SHORTCUT = 'shortcut',
        SYNC_COMPLETED_TIME = 'syncCompletedTime',
      }

      export enum Source {
        FILE = 'file',
        DEVICE = 'device',
        NETWORK = 'network',
        SYSTEM = 'system',
      }

      export enum SourceRestriction {
        ANY_SOURCE = 'any_source',
        NATIVE_SOURCE = 'native_source',
      }

      export enum FileCategory {
        ALL = 'all',
        AUDIO = 'audio',
        IMAGE = 'image',
        VIDEO = 'video',
        DOCUMENT = 'document',
      }

      export enum CrostiniEventType {
        ENABLE = 'enable',
        DISABLE = 'disable',
        SHARE = 'share',
        UNSHARE = 'unshare',
        DROP_FAILED_PLUGIN_VM_DIRECTORY_NOT_SHARED =
            'drop_failed_plugin_vm_directory_not_shared',
      }

      export enum ProviderSource {
        FILE = 'file',
        DEVICE = 'device',
        NETWORK = 'network',
      }

      export enum SharesheetLaunchSource {
        CONTEXT_MENU = 'context_menu',
        SHARESHEET_BUTTON = 'sharesheet_button',
        UNKNOWN = 'unknown',
      }

      export enum IoTaskState {
        QUEUED = 'queued',
        SCANNING = 'scanning',
        IN_PROGRESS = 'in_progress',
        PAUSED = 'paused',
        SUCCESS = 'success',
        ERROR = 'error',
        NEED_PASSWORD = 'need_password',
        CANCELLED = 'cancelled',
      }

      export enum IoTaskType {
        COPY = 'copy',
        DELETE = 'delete',
        EMPTY_TRASH = 'empty_trash',
        EXTRACT = 'extract',
        MOVE = 'move',
        RESTORE = 'restore',
        RESTORE_TO_DESTINATION = 'restore_to_destination',
        TRASH = 'trash',
        ZIP = 'zip',
      }

      export enum PolicyErrorType {
        DLP = 'dlp',
        ENTERPRISE_CONNECTORS = 'enterprise_connectors',
        DLP_WARNING_TIMEOUT = 'dlp_warning_timeout',
      }

      export enum PolicyDialogType {
        WARNING = 'warning',
        ERROR = 'error',
      }

      export enum RecentDateBucket {
        TODAY = 'today',
        YESTERDAY = 'yesterday',
        EARLIER_THIS_WEEK = 'earlier_this_week',
        EARLIER_THIS_MONTH = 'earlier_this_month',
        EARLIER_THIS_YEAR = 'earlier_this_year',
        OLDER = 'older',
      }

      export enum VmType {
        TERMINA = 'termina',
        PLUGIN_VM = 'plugin_vm',
        BOREALIS = 'borealis',
        BRUSCHETTA = 'bruschetta',
        ARCVM = 'arcvm',
      }

      export enum UserType {
        UNMANAGED = 'unmanaged',
        ORGANIZATION = 'organization',
      }

      export enum DlpLevel {
        REPORT = 'report',
        WARN = 'warn',
        BLOCK = 'block',
        ALLOW = 'allow',
      }

      export enum SyncStatus {
        NOT_FOUND = 'not_found',
        QUEUED = 'queued',
        IN_PROGRESS = 'in_progress',
        COMPLETED = 'completed',
        ERROR = 'error',
      }

      export enum PolicyDefaultHandlerStatus {
        DEFAULT_HANDLER_ASSIGNED_BY_POLICY =
            'default_handler_assigned_by_policy',
        INCORRECT_ASSIGNMENT = 'incorrect_assignment',
      }

      export enum BulkPinStage {
        STOPPED = 'stopped',
        PAUSED_OFFLINE = 'paused_offline',
        PAUSED_BATTERY_SAVER = 'paused_battery_saver',
        GETTING_FREE_SPACE = 'getting_free_space',
        LISTING_FILES = 'listing_files',
        SYNCING = 'syncing',
        SUCCESS = 'success',
        NOT_ENOUGH_SPACE = 'not_enough_space',
        CANNOT_GET_FREE_SPACE = 'cannot_get_free_space',
        CANNOT_LIST_FILES = 'cannot_list_files',
        CANNOT_ENABLE_DOCS_OFFLINE = 'cannot_enable_docs_offline',
      }

      export enum DefaultLocation {
        MY_FILES = 'my_files',
        GOOGLE_DRIVE = 'google_drive',
        ONEDRIVE = 'onedrive',
      }

      export enum CloudProvider {
        NOT_SPECIFIED = 'not_specified',
        GOOGLE_DRIVE = 'google_drive',
        ONEDRIVE = 'onedrive',
      }

      export interface FileTaskDescriptor {
        appId: string;
        taskType: string;
        actionId: string;
      }

      export interface FileTask {
        descriptor: FileTaskDescriptor;
        title: string;
        iconUrl?: string;
        isDefault?: boolean;
        isGenericFileHandler?: boolean;
        isDlpBlocked?: boolean;
      }

      export interface ResultingTasks {
        tasks: FileTask[];
        policyDefaultHandlerStatus?: PolicyDefaultHandlerStatus;
      }

      export interface EntryProperties {
        size?: number;
        modificationTime?: number;
        modificationByMeTime?: number;
        recentDateBucket?: RecentDateBucket;
        thumbnailUrl?: string;
        croppedThumbnailUrl?: string;
        imageWidth?: number;
        imageHeight?: number;
        imageRotation?: number;
        pinned?: boolean;
        present?: boolean;
        hosted?: boolean;
        availableOffline?: boolean;
        availableWhenMetered?: boolean;
        dirty?: boolean;
        customIconUrl?: string;
        contentMimeType?: string;
        sharedWithMe?: boolean;
        shared?: boolean;
        starred?: boolean;
        externalFileUrl?: string;
        alternateUrl?: string;
        shareUrl?: string;
        canCopy?: boolean;
        canDelete?: boolean;
        canRename?: boolean;
        canAddChildren?: boolean;
        canShare?: boolean;
        canPin?: boolean;
        isMachineRoot?: boolean;
        isExternalMedia?: boolean;
        isArbitrarySyncFolder?: boolean;
        syncStatus?: SyncStatus;
        progress?: number;
        syncCompletedTime?: number;
        shortcut?: boolean;
      }

      export interface MountPointSizeStats {
        totalSize: number;
        remainingSize: number;
      }

      export interface SearchDriveResponse {
        entries: Entry[];
        nextFeed: string;
      }

      export interface DriveQuotaMetadata {
        userType: UserType;
        usedBytes: number;
        totalBytes: number;
        organizationLimitExceeded: boolean;
        organizationName: string;
      }

      export interface ProfileInfo {
        profileId: string;
        displayName: string;
        isCurrentProfile: boolean;
      }

      export interface ProfilesResponse {
        profiles: ProfileInfo[];
        currentProfileId: string;
        displayedProfileId: string;
      }

      export interface IconSet {
        icon16x16Url?: string;
        icon32x32Url?: string;
      }

      export interface VolumeMetadata {
        volumeId: string;
        fileSystemId?: string;
        providerId?: string;
        source: Source;
        volumeLabel?: string;
        profile: ProfileInfo;
        sourcePath?: string;
        volumeType: VolumeType;
        deviceType?: DeviceType;
        devicePath?: string;
        isParentDevice?: boolean;
        isReadOnly: boolean;
        isReadOnlyRemovableDevice: boolean;
        hasMedia: boolean;
        configurable: boolean;
        watchable: boolean;
        mountCondition?: MountError;
        mountContext?: MountContext;
        diskFileSystemType?: string;
        iconSet: IconSet;
        driveLabel?: string;
        remoteMountPath?: string;
        hidden: boolean;
        vmType?: VmType;
      }

      export interface MountCompletedEvent {
        eventType: MountCompletedEventType;
        status: MountError;
        volumeMetadata: VolumeMetadata;
        shouldNotify: boolean;
      }

      export interface FileTransferStatus {
        fileUrl: string;
        transferState: TransferState;
        processed: number;
        total: number;
        numTotalJobs: number;
        showNotification: boolean;
        hideWhenZeroJobs: boolean;
      }

      export interface SyncState {
        fileUrl: string;
        syncStatus: SyncStatus;
        progress: number;
      }

      export interface DriveSyncErrorEvent {
        type: DriveSyncErrorType;
        fileUrl: string;
        sharedDrive?: string;
      }

      export interface DriveConfirmDialogEvent {
        type: DriveConfirmDialogType;
        fileUrl: string;
      }

      export interface FileChange {
        url: string;
        changes: ChangeType[];
      }

      export interface FileWatchEvent {
        eventType: FileWatchEventType;
        entry: Entry;
        changedFiles?: FileChange[];
      }

      export interface GetVolumeRootOptions {
        volumeId: string;
        writable?: boolean;
      }

      export interface Preferences {
        driveEnabled: boolean;
        driveSyncEnabledOnMeteredNetwork: boolean;
        searchSuggestEnabled: boolean;
        use24hourClock: boolean;
        timezone: string;
        arcEnabled: boolean;
        arcRemovableMediaAccessEnabled: boolean;
        folderShortcuts: string[];
        trashEnabled: boolean;
        officeFileMovedOneDrive: number;
        officeFileMovedGoogleDrive: number;
        driveFsBulkPinningAvailable: boolean;
        driveFsBulkPinningEnabled: boolean;
        localUserFilesAllowed: boolean;
        defaultLocation: DefaultLocation;
        skyVaultMigrationDestination: CloudProvider;
      }

      export interface PreferencesChange {
        driveSyncEnabledOnMeteredNetwork?: boolean;
        arcEnabled?: boolean;
        arcRemovableMediaAccessEnabled?: boolean;
        folderShortcuts?: string[];
        driveFsBulkPinningEnabled?: boolean;
      }

      export interface SearchParams {
        query: string;
        category?: FileCategory;
        modifiedTimestamp?: number;
        nextFeed: string;
      }

      export interface SearchMetadataParams {
        rootDir?: DirectoryEntry;
        query: string;
        types: SearchType;
        maxResults: number;
        modifiedTimestamp?: number;
        category?: FileCategory;
      }

      export interface DriveMetadataSearchResult {
        entry: Entry;
        highlightedBaseName: string;
        availableOffline?: boolean;
      }

      export interface DriveConnectionState {
        type: DriveConnectionStateType;
        reason?: DriveOfflineReason;
      }

      export interface DeviceEvent {
        type: DeviceEventType;
        devicePath: string;
        deviceLabel: string;
      }

      export interface Provider {
        providerId: string;
        iconSet: IconSet;
        name: string;
        configurable: boolean;
        watchable: boolean;
        multipleMounts: boolean;
        source: ProviderSource;
      }

      export interface FileSystemProviderAction {
        id: string;
        title?: string;
      }

      export interface LinuxPackageInfo {
        name: string;
        version: string;
        summary?: string;
        description?: string;
      }

      export interface CrostiniEvent {
        eventType: CrostiniEventType;
        vmName: string;
        containerName: string;
        entries: Entry[];
      }

      export interface CrostiniSharedPathResponse {
        entries: Entry[];
        firstForSession: boolean;
      }

      export interface AndroidApp {
        name: string;
        packageName: string;
        activityName: string;
        iconSet?: IconSet;
      }

      export interface StreamInfo {
        type: string;
        tags: {
          [key: string]: any,
        };
      }

      export interface AttachedImages {
        data: string;
        type: string;
      }

      export interface MediaMetadata {
        mimeType: string;
        height?: number;
        width?: number;
        duration?: number;
        rotation?: number;
        album?: string;
        artist?: string;
        comment?: string;
        copyright?: string;
        disc?: number;
        genre?: string;
        language?: string;
        title?: string;
        track?: number;
        rawTags: StreamInfo[];
        attachedImages: AttachedImages[];
      }

      export interface HoldingSpaceState {
        itemUrls: string[];
      }

      export interface OpenWindowParams {
        currentDirectoryURL?: string;
        selectionURL?: string;
      }

      export interface IoTaskParams {
        destinationFolder?: DirectoryEntry;
        password?: string;
        showNotification?: boolean;
      }

      export interface PolicyError {
        type: PolicyErrorType;
        policyFileCount: number;
        fileName: string;
        alwaysShowReview: boolean;
      }

      export interface ConflictPauseParams {
        conflictName?: string;
        conflictIsDirectory?: boolean;
        conflictMultiple?: boolean;
        conflictTargetUrl?: string;
      }

      export interface PolicyPauseParams {
        type: PolicyErrorType;
        policyFileCount: number;
        fileName: string;
        alwaysShowReview: boolean;
      }

      export interface PauseParams {
        conflictParams?: ConflictPauseParams;
        policyParams?: PolicyPauseParams;
      }

      export interface ConflictResumeParams {
        conflictResolve?: string;
        conflictApplyToAll?: boolean;
      }

      export interface PolicyResumeParams {
        type: PolicyErrorType;
      }

      export interface ResumeParams {
        conflictParams?: ConflictResumeParams;
        policyParams?: PolicyResumeParams;
      }

      export interface ProgressStatus {
        type: IoTaskType;
        state: IoTaskState;
        policyError?: PolicyError;
        sourceName: string;
        numRemainingItems: number;
        itemCount: number;
        destinationName: string;
        bytesTransferred: number;
        totalBytes: number;
        taskId: number;
        remainingSeconds: number;
        sourcesScanned: number;
        showNotification: boolean;
        errorName: string;
        pauseParams?: PauseParams;
        outputs?: Entry[];
        skippedEncryptedFiles: string[];
        destinationVolumeId: string;
      }

      export interface DlpMetadata {
        sourceUrl: string;
        isDlpRestricted: boolean;
        isRestrictedForDestination: boolean;
      }

      export interface DlpRestrictionDetails {
        level: DlpLevel;
        urls: string[];
        components: VolumeType[];
      }

      export interface DialogCallerInformation {
        url?: string;
        component?: VolumeType;
      }

      export interface MountableGuest {
        id: number;
        displayName: string;
        vmType: VmType;
      }

      export interface ParsedTrashInfoFile {
        restoreEntry: Entry;
        trashInfoFileName: string;
        deletionDate: number;
      }

      export interface BulkPinProgress {
        stage: BulkPinStage;
        freeSpaceBytes: number;
        requiredSpaceBytes: number;
        bytesToPin: number;
        pinnedBytes: number;
        filesToPin: number;
        listedFiles: number;
        remainingSeconds: number;
        shouldPin: boolean;
        emptiedQueue: boolean;
      }

      export interface MaterializedView {
        viewId: number;
        name: string;
      }

      export interface EntryData {
        entryUrl: string;
      }

      export function cancelDialog(): void;

      export function executeTask(
          descriptor: FileTaskDescriptor, entries: Entry[],
          callback: (result: TaskResult) => void): void;

      export function setDefaultTask(
          descriptor: FileTaskDescriptor, entries: Entry[], mimeTypes: string[],
          callback: () => void): void;

      export function getFileTasks(
          entries: Entry[], dlpSourceUrls: string[],
          callback: (resultingTasks: ResultingTasks) => void): void;

      export function getMimeType(url: string): Promise<string>;

      export function getContentMimeType(
          fileEntry: FileEntry, callback: (result: string) => void): void;

      export function getContentMetadata(
          fileEntry: FileEntry, mimeType: string, includeImages: boolean,
          callback: (result: MediaMetadata) => void): void;

      export function getStrings(callback: (result: {
                                   [key: string]: any,
                                 }) => void): void;

      export function addFileWatch(
          entry: Entry, callback: (success?: boolean) => void): void;

      export function removeFileWatch(
          entry: Entry, callback: (success?: boolean) => void): void;

      export function enableExternalFileScheme(): void;

      export function grantAccess(entryUrls: string[], callback: () => void):
          void;

      export function selectFiles(
          selectedPaths: string[], shouldReturnLocalPath: boolean,
          callback: () => void): void;

      export function selectFile(
          selectedPath: string, index: number, forOpening: boolean,
          shouldReturnLocalPath: boolean, callback: () => void): void;

      export function getEntryProperties(
          entries: Entry[], names: EntryPropertyName[],
          callback: (entryProperties: EntryProperties[]) => void): void;

      export function pinDriveFile(
          entry: Entry, pin: boolean, callback: () => void): void;

      export function resolveIsolatedEntries(
          entries: Entry[], callback: (entries: Entry[]) => void): void;

      export function addMount(
          fileUrl: string, password?: string,
          callback: (sourcePath: string) => void): void;

      export function cancelMounting(fileUrl: string, callback: () => void):
          void;

      export function removeMount(volumeId: string, callback: () => void): void;

      export function getVolumeMetadataList(
          callback: (volumeMetadataList: VolumeMetadata[]) => void): void;

      export function getDisallowedTransfers(
          entries: Entry[], destinationEntry: DirectoryEntry, isMove: boolean,
          callback: (disallowedEntries: Entry[]) => void): void;

      export function getDlpMetadata(
          entries: Entry[],
          callback: (dlpMetadata: DlpMetadata[]) => void): void;

      export function getDlpRestrictionDetails(
          sourceUrl: string,
          callback: (restrictionDetails: DlpRestrictionDetails[]) => void):
          void;

      export function getDlpBlockedComponents(
          sourceUrl: string,
          callback: (blockedComponents: VolumeType[]) => void): void;

      export function getDialogCaller(
          callback: (caller: DialogCallerInformation) => void): void;

      export function getSizeStats(
          volumeId: string,
          callback: (sizeStats?: MountPointSizeStats) => void): void;

      export function getDriveQuotaMetadata(
          entry: Entry,
          callback: (driveQuotaMetadata?: DriveQuotaMetadata) => void): void;

      export function formatVolume(
          volumeId: string, filesystem: FormatFileSystemType,
          volumeLabel: string): void;

      export function singlePartitionFormat(
          deviceStoragePath: string, filesystem: FormatFileSystemType,
          volumeLabel: string): void;

      export function renameVolume(volumeId: string, newName: string): void;

      export function getPreferences(callback: (result: Preferences) => void):
          void;

      export function setPreferences(changeInfo: PreferencesChange): void;

      export function searchDrive(
          searchParams: SearchParams,
          callback: (response: SearchDriveResponse) => void): void;

      export function searchDriveMetadata(
          searchParams: SearchMetadataParams,
          callback: (results: DriveMetadataSearchResult[]) => void): void;

      export function searchFiles(
          searchParams: SearchMetadataParams,
          callback: (entries: Entry[]) => void): void;

      export function getDeviceConnectionState(
          callback: (result: DeviceConnectionState) => void): void;

      export function getDriveConnectionState(
          callback: (result: DriveConnectionState) => void): void;

      export function validatePathNameLength(
          parentEntry: DirectoryEntry, name: string,
          callback: (result: boolean) => void): void;

      export function zoom(operation: ZoomOperationType): void;

      export function getProfiles(
          callback: (response: ProfilesResponse) => void): void;

      export function openInspector(type: InspectionType): void;

      export function openSettingsSubpage(subPage: string): void;

      export function getProviders(callback: (extensions: Provider[]) => void):
          void;

      export function addProvidedFileSystem(
          providerId: string, callback: () => void): void;

      export function configureVolume(volumeId: string, callback: () => void):
          void;

      export function getCustomActions(
          entries: Entry[],
          callback: (actions: FileSystemProviderAction[]) => void): void;

      export function executeCustomAction(
          entries: Entry[], actionId: string, callback: () => void): void;

      export function getDirectorySize(
          entry: DirectoryEntry, callback: (size: number) => void): void;

      export function getRecentFiles(
          restriction: SourceRestriction, query: string, cutoffDays: number,
          fileCategory: FileCategory, invalidateCache: boolean,
          callback: (entries: Entry[]) => void): void;

      export function getVolumeRoot(
          options: GetVolumeRootOptions,
          callback: (rootDir: DirectoryEntry) => void): void;

      export function mountCrostini(callback: () => void): void;

      export function sharePathsWithCrostini(
          vmName: string, entries: Entry[], persist: boolean,
          callback: () => void): void;

      export function unsharePathWithCrostini(
          vmName: string, entry: Entry, callback: () => void): void;

      export function getCrostiniSharedPaths(
          observeFirstForSession: boolean, vmName: string,
          callback: (response: CrostiniSharedPathResponse) => void): void;

      export function getLinuxPackageInfo(
          entry: Entry,
          callback: (linux_package_info: LinuxPackageInfo) => void): void;

      export function installLinuxPackage(
          entry: Entry,
          callback: (status: InstallLinuxPackageStatus) => void): void;

      export function importCrostiniImage(entry: Entry): void;

      export function getAndroidPickerApps(
          extensions: string[], callback: (apps: AndroidApp[]) => void): void;

      export function selectAndroidPickerApp(
          androidApp: AndroidApp, callback: () => void): void;

      export function sharesheetHasTargets(fileUrls: string[]):
          Promise<boolean>;

      export function invokeSharesheet(
          fileUrls: string[], launchSource: SharesheetLaunchSource,
          dlpSourceUrls: string[]): Promise<void>;

      export function toggleAddedToHoldingSpace(
          entries: Entry[], added: boolean, callback?: () => void): void;

      export function getHoldingSpaceState(
          callback: (state: HoldingSpaceState) => void): void;

      export function isTabletModeEnabled(callback: (result: boolean) => void):
          void;

      export function notifyDriveDialogResult(result: DriveDialogResult): void;

      export function openURL(url: string): void;

      export function openWindow(
          params: OpenWindowParams, callback: (result: boolean) => void): void;

      export function sendFeedback(): void;

      export function startIOTask(
          type: IoTaskType, entries: Entry[], params: IoTaskParams,
          callback?: (taskId: number) => void): void;

      export function cancelIOTask(taskId: number): void;

      export function resumeIOTask(taskId: number, params: ResumeParams): void;

      export function dismissIOTask(taskId: number, callback: () => void): void;

      export function showPolicyDialog(
          taskId: number, type: PolicyDialogType, callback: () => void): void;

      export function progressPausedTasks(callback: () => void): void;

      export function listMountableGuests(
          callback: (guest: MountableGuest[]) => void): void;

      export function mountGuest(id: number, callback: () => void): void;

      export function pollDriveHostedFilePinStates(): void;

      export function openManageSyncSettings(): void;

      export function parseTrashInfoFiles(
          entries: Entry[],
          callback: (files: ParsedTrashInfoFile[]) => void): void;

      export function getBulkPinProgress(
          callback: (progress: BulkPinProgress) => void): void;

      export function calculateBulkPinRequiredSpace(callback: () => void): void;

      export function getMaterializedViews(): Promise<MaterializedView[]>;

      export function readMaterializedView(viewId: number):
          Promise<EntryData[]>;

      export const onMountCompleted:
          ChromeEvent<(event: MountCompletedEvent) => void>;

      export const onFileTransfersUpdated:
          ChromeEvent<(event: FileTransferStatus) => void>;

      export const onPinTransfersUpdated:
          ChromeEvent<(event: FileTransferStatus) => void>;

      export const onIndividualFileTransfersUpdated:
          ChromeEvent<(event: SyncState[]) => void>;

      export const onDirectoryChanged:
          ChromeEvent<(event: FileWatchEvent) => void>;

      export const onPreferencesChanged: ChromeEvent<() => void>;

      export const onDeviceConnectionStatusChanged:
          ChromeEvent<(state: DeviceConnectionState) => void>;

      export const onDriveConnectionStatusChanged: ChromeEvent<() => void>;

      export const onDeviceChanged: ChromeEvent<(event: DeviceEvent) => void>;

      export const onDriveSyncError:
          ChromeEvent<(event: DriveSyncErrorEvent) => void>;

      export const onDriveConfirmDialog:
          ChromeEvent<(event: DriveConfirmDialogEvent) => void>;

      export const onAppsUpdated: ChromeEvent<() => void>;

      export const onCrostiniChanged:
          ChromeEvent<(event: CrostiniEvent) => void>;

      export const onTabletModeChanged: ChromeEvent<(enabled: boolean) => void>;

      export const onIOTaskProgressStatus:
          ChromeEvent<(status: ProgressStatus) => void>;

      export const onMountableGuestsChanged:
          ChromeEvent<(guests: MountableGuest[]) => void>;

      export const onBulkPinProgress:
          ChromeEvent<(progress: BulkPinProgress) => void>;

    }
  }
}
