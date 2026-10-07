// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxContextAddedMethod} from '//resources/cr_components/search/constants.js';
import {assertNotReachedCase} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {FuseboxAction, SuggestInventory} from '//resources/mojo/components/omnibox/browser/fusebox_action.mojom-webui.js';
import {TabAttachmentSource} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {DriveUploadError, SearchContextAttachment, SelectedFileInfo, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {ContextUploadErrorType, ContextUploadStatus, InputType, ModelMode, ToolMode} from './composebox_query.mojom-webui.js';
import type {InputState} from './composebox_query.mojom-webui.js';

// LINT.IfChange(FileValidationError)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
export enum ComposeboxFileValidationError {
  NONE = 0,
  TOO_MANY_FILES = 1,
  FILE_EMPTY = 2,
  FILE_SIZE_TOO_LARGE = 3,
  MAX_VALUE = FILE_SIZE_TOO_LARGE,
}

// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/enums.xml:FileValidationError)

// LINT.IfChange(ContextType)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
export enum ContextType {
  TAB = 0,
  FILE = 1,
  IMAGE = 2,
  IMAGE_GEN = 3,
  DEEP_RESEARCH = 4,
  CANVAS = 5,
  AUTO_MODEL = 6,
  THINKING_MODEL = 7,
  REGULAR_MODEL = 8,
  PRO_NO_GEN_UI_MODEL = 9,
  UNKNOWN = 10,
  DRIVE = 11,
  MAX_VALUE = DRIVE,
}

// LINT.ThenChange(//components/omnibox/common/omnibox_metrics_utils.h:ContextType,
// //tools/metrics/histograms/metadata/contextual_search/enums.xml:ContextType)

// These values are sorted by precedence. The error with the highest value
// will be the one shown to the user if multiple errors apply.
export enum ProcessFilesError {
  NONE = 0,
  INVALID_TYPE = 1,
  FILE_TOO_LARGE = 2,
  FILE_EMPTY = 3,
  MAX_FILES_EXCEEDED = 4,
  MAX_IMAGES_EXCEEDED = 5,
  MAX_PDFS_EXCEEDED = 6,
  FILE_UPLOAD_NOT_ALLOWED = 7,
}

export enum TabSuggestionsState {
  NOT_STARTED = 0,
  LOADING = 1,
  LOADED = 2,
}

export const FILE_VALIDATION_ERRORS_MAP =
    new Map<ContextUploadErrorType, string>([
      [
        ContextUploadErrorType.kBrowserProcessingError,
        'composeboxFileUploadFailed',
      ],
      [
        ContextUploadErrorType.kImageProcessingError,
        'composeFileTypesAllowedError',
      ],
      [
        ContextUploadErrorType.kServerSizeLimitExceeded,
        'composeboxFileUploadInvalidTooLarge',
      ],
      [
        ContextUploadErrorType.kUnknown,
        'composeboxFileUploadValidationFailed',
      ],
      [
        ContextUploadErrorType.kBrowserProcessingFileTooLargeError,
        'composeboxFileUploadInvalidTooLarge',
      ],
      [
        ContextUploadErrorType.kBrowserProcessingFileEmptyError,
        'composeboxFileUploadInvalidEmptySize',
      ],
      [
        ContextUploadErrorType.kBrowserProcessingMaxFilesExceededError,
        'maxFilesReachedError',
      ],
      [
        ContextUploadErrorType.kBrowserProcessingUnsupportedFileTypeError,
        'composeFileTypesAllowedError',
      ],
      [
        ContextUploadErrorType.kBrowserProcessingFileUploadNotAllowedError,
        'composeboxFileUploadNotAllowed',
      ],
      [
        ContextUploadErrorType.kBrowserProcessingMaxImagesExceededError,
        'maxImagesReachedError',
      ],
      [
        ContextUploadErrorType.kBrowserProcessingMaxPdfsExceededError,
        'maxPdfsReachedError',
      ],
    ]);

export class ComposeboxFile {
  uuid: UnguessableToken;
  name: string;
  objectUrl: string|null;
  dataUrl: string|null;
  type: string;
  inputType: InputType;
  status: ContextUploadStatus;
  url: Url|null;
  tabId: number|null;
  isDeletable: boolean;
  iconName: string|null;
  supportsUnimodal: boolean;
  thumbnailUrl?: string|null;
  iconUrl?: Url|null;
  origin?: TabAttachmentSource;
  delayUpload?: boolean;
  // True if this is a placeholder ("ghost") entry created from an upload
  // status update for a token the frontend does not know about yet. Its
  // metadata, including `inputType`, is not yet known and must not be relied
  // on until the entry is replaced by a fully hydrated one.
  isGhost?: boolean;

  constructor(
      uuid: UnguessableToken, name: string, type: string, inputType: InputType,
      options?: Partial<ComposeboxFile>) {
    this.uuid = uuid;
    this.name = name;
    this.type = type;
    this.inputType = inputType;
    this.objectUrl = options?.objectUrl ?? null;
    this.dataUrl = options?.dataUrl ?? null;
    this.status = options?.status ?? ContextUploadStatus.kNotUploaded;
    this.url = options?.url ?? null;
    this.tabId = options?.tabId ?? null;
    this.isDeletable = options?.isDeletable ?? true;
    this.iconName = options?.iconName ?? null;
    this.supportsUnimodal = options?.supportsUnimodal ?? false;
    this.thumbnailUrl = options?.thumbnailUrl ?? null;
    this.iconUrl = options?.iconUrl ?? null;
    this.origin = options?.origin;
    this.delayUpload = options?.delayUpload ?? false;
    this.isGhost = options?.isGhost ?? false;
  }

  static createFromFile(
      uuid: UnguessableToken, file: File|{name: string, type: string},
      status: ContextUploadStatus = ContextUploadStatus.kNotUploaded,
      options?: Partial<ComposeboxFile>): ComposeboxFile {
    const inputType = file.type.includes('image') ? InputType.kLensImage :
                                                    InputType.kLensFile;
    return new ComposeboxFile(uuid, file.name, file.type, inputType, {
      status,
      ...options,
    });
  }

  static createFromTab(
      uuid: UnguessableToken, tabId: number, title: string, url: Url,
      options?: Partial<ComposeboxFile>): ComposeboxFile {
    return new ComposeboxFile(uuid, title, 'tab', InputType.kBrowserTab, {
      status: ContextUploadStatus.kUploadSuccessful,
      tabId,
      url,
      ...options,
    });
  }

  static createFromInjectedInput(
      uuid: UnguessableToken, dataUrl: string, name: string = 'Pasted Image',
      iconName: string|null = null): ComposeboxFile {
    return new ComposeboxFile(
        uuid, name, 'injectedinput', InputType.kLensImage, {
          dataUrl,
          objectUrl: dataUrl,
          status: ContextUploadStatus.kUploadSuccessful,
          iconName,
        });
  }
}

export interface ComposeboxState {
  text: string;
  files: ContextualUpload[];
  error?: DriveUploadError;
  mode: ToolMode;
  model: ModelMode;
  suggestInventory?: SuggestInventory;
  // <if expr="not is_android">
  smartTabSharingActive: boolean;
  // </if>
}

export interface FileUpload {
  file: File;
}

export interface DriveUpload {
  token: UnguessableToken;
  mimeType: string;
  fileName: string;
  thumbnailUrl: string|null;
  iconUrl: Url|null;
}

export function isAutoAddedOrigin(origin?: TabAttachmentSource): boolean {
  return origin === TabAttachmentSource.kAutoAdded ||
      origin === TabAttachmentSource.kCurrentTabChip;
}

export function hasOnlyAutoAddedTabs(
    files: Map<UnguessableToken, ComposeboxFile>): boolean {
  if (files.size === 0) {
    return false;
  }
  for (const file of files.values()) {
    if (file.inputType !== InputType.kBrowserTab ||
        !isAutoAddedOrigin(file.origin)) {
      return false;
    }
  }
  return true;
}

export interface ComposeboxInputModelParams {
  files?: Map<UnguessableToken, ComposeboxFile>;
  attachedContext?: Map<UnguessableToken, ComposeboxFile>;
  smartTabSharingActive?: boolean;
  tabFaviconChipsToCoinsEnabled?: boolean;
  input?: string;
  selectedMatchIndex?: number;
  hasResult?: boolean;
  activeTool?: ToolMode;
}

export class ComposeboxInputModel {
  readonly attachedContext: Map<UnguessableToken, ComposeboxFile>;
  readonly smartTabSharingActive: boolean;
  readonly tabFaviconChipsToCoinsEnabled: boolean;
  readonly input: string;
  readonly selectedMatchIndex: number;
  readonly hasResult: boolean;
  readonly activeTool?: ToolMode;

  get files(): Map<UnguessableToken, ComposeboxFile> {
    return this.attachedContext;
  }

  constructor(params?: ComposeboxInputModelParams) {
    this.attachedContext =
        params?.attachedContext ?? params?.files ?? new Map();
    this.smartTabSharingActive = params?.smartTabSharingActive ?? false;
    this.tabFaviconChipsToCoinsEnabled =
        params?.tabFaviconChipsToCoinsEnabled ?? false;
    this.input = params?.input ?? '';
    this.selectedMatchIndex = params?.selectedMatchIndex ?? -1;
    this.hasResult = params?.hasResult ?? false;
    this.activeTool = params?.activeTool;
  }

  hasTabs(): boolean {
    return (this.tabFaviconChipsToCoinsEnabled &&
            Array.from(this.attachedContext.values()).some(f => !!f.url)) ||
        this.smartTabSharingActive;
  }

  hasNonTabFiles(): boolean {
    return Array.from(this.attachedContext.values()).some(f => !f.url);
  }

  getNonTabFileNum(): number {
    return Array.from(this.attachedContext.values())
        .filter(file => file.inputType !== InputType.kBrowserTab)
        .length;
  }

  getSharedTabs(): TabInfo[] {
    return Array.from(this.attachedContext.values())
        .filter(file => !!file.url)
        .map(file => ({
               tabId: file.tabId!,
               title: file.name,
               url: file.url!,
               // Attached files carry no loading state; embedders fill it in
               // from the latest tab suggestions.
               isLoading: false,
             } as TabInfo));
  }

  hasFiles(): boolean {
    return this.attachedContext.size > 0;
  }

  hasMultipleFiles(): boolean {
    return this.attachedContext.size > 1 || this.smartTabSharingActive;
  }

  hasOnlyAutoAddedTabs(): boolean {
    return hasOnlyAutoAddedTabs(this.attachedContext);
  }

  hasUnimodalFile(): boolean {
    return Array.from(this.attachedContext.values())
        .some(file => file.supportsUnimodal);
  }

  hasValidQuery(): boolean {
    if (this.hasUnimodalFile()) {
      return true;
    }
    if (this.selectedMatchIndex >= 0 && this.hasResult) {
      return true;
    }
    return this.input.trim().length > 0;
  }

  hasContent(ignoreAutoAddedTabs: boolean = false): boolean {
    const hasFiles = this.hasFiles() &&
        !(ignoreAutoAddedTabs && this.hasOnlyAutoAddedTabs());
    return (this.activeTool !== undefined &&
            this.activeTool !== ToolMode.kUnspecified) ||
        this.input.trim().length > 0 || hasFiles;
  }

  canSubmit(): boolean {
    return this.hasValidQuery() || this.hasFiles();
  }
}

export function hasOnlyAutoAddedTabAttachments(
    attachments: SearchContextAttachment[]): boolean {
  if (attachments.length === 0) {
    return false;
  }
  for (const attachment of attachments) {
    if (!attachment.tabAttachment) {
      return false;
    }
    if (!isAutoAddedOrigin(attachment.tabAttachment.source)) {
      return false;
    }
  }
  return true;
}

export interface TabUpload {
  tabId: number;
  url: Url;
  title: string;
  delayUpload: boolean;
  origin: TabAttachmentSource;
}

export interface BrowserFileUpload {
  token: UnguessableToken;
  fileInfo: SelectedFileInfo;
}

export type ContextualUpload =
    TabUpload|FileUpload|DriveUpload|BrowserFileUpload;

// Represents an embedder-agnostic request to execute a FuseboxAction in a
// Composebox instance
export interface ComposeboxFuseboxActionRequest {
  suggestion: string;
  files: ContextualUpload[];
  fuseboxAction?: FuseboxAction;
}

export enum GlifAnimationState {
  INELIGIBLE = 'ineligible',
  SPINNER_ONLY = 'spinner-only',
  STARTED = 'started',
  FINISHED = 'finished',
}

// LINT.IfChange(ContextualSearchInputStateDeletionType)
export enum ContextualSearchInputStateDeletionType {
  FILE = 0,
  TAB = 1,
  TOOL = 2,
  MAX_VALUE = 2,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/enums.xml:ContextualSearchInputStateDeletionType)

export function recordEnumerationValue(
    metricName: string, value: number, enumSize: number) {
  const metricsService = chrome.histograms || chrome.metricsPrivate;
  if (metricsService) {
    metricsService.recordEnumerationValue(metricName, value, enumSize);
  }
}

export function recordUserAction(metricName: string) {
  const metricsService = chrome.histograms || chrome.metricsPrivate;
  if (metricsService) {
    metricsService.recordUserAction(metricName);
  }
}

export function recordBoolean(metricName: string, value: boolean) {
  const metricsService = chrome.histograms || chrome.metricsPrivate;
  if (metricsService) {
    metricsService.recordBoolean(metricName, value);
  }
}

// TODO(crbug.com/468329884): Consider making this a new contextual entry
// source so the realbox and composebox don't both get logged as NTP.
export function recordContextAdditionMethod(
    additionMethod: ComposeboxContextAddedMethod, composeboxSource: string) {
  recordEnumerationValue(
      'ContextualSearch.ContextAdded.ContextAddedMethod.' + composeboxSource,
      additionMethod, ComposeboxContextAddedMethod.MAX_VALUE + 1);
}

// LINT.IfChange(getContextTypeString)
function getContextTypeString(type: ContextType): string {
  switch (type) {
    case ContextType.TAB:
      return 'Tab';
    case ContextType.FILE:
      return 'File';
    case ContextType.IMAGE:
      return 'Image';
    case ContextType.IMAGE_GEN:
      return 'ImageGen';
    case ContextType.DEEP_RESEARCH:
      return 'DeepResearch';
    case ContextType.DRIVE:
      return 'Drive';
    case ContextType.CANVAS:
      return 'Canvas';
    case ContextType.AUTO_MODEL:
      return 'AutoModel';
    case ContextType.THINKING_MODEL:
      return 'ThinkingModel';
    case ContextType.REGULAR_MODEL:
      return 'RegularModel';
    case ContextType.PRO_NO_GEN_UI_MODEL:
      return 'ProNoGenUiModel';
    case ContextType.UNKNOWN:
      return 'Unknown';
    default:
      return 'Unknown';
  }
}
// LINT.ThenChange(//components/omnibox/common/omnibox_metrics_utils.cc:GetContextTypeString)

export function recordContextualElementClickedMetric(
    composeboxSource: string, popupType: string, contextType: ContextType) {
  const metricName = `${composeboxSource}.AimEntrypoint.${
      popupType}.ContextualElement.Clicked`;
  recordEnumerationValue(metricName, contextType, ContextType.MAX_VALUE + 1);
  recordUserAction(`${metricName}.${getContextTypeString(contextType)}`);
}

export function recordContextualElementShownMetric(
    composeboxSource: string, popupType: string, contextType: ContextType) {
  const metricName =
      `${composeboxSource}.AimEntrypoint.${popupType}.ContextualElement.Shown`;
  recordEnumerationValue(metricName, contextType, ContextType.MAX_VALUE + 1);
}

export function recordToolModeSelection(
    mode: ToolMode, composeboxSource: string, popupType: string) {
  let contextType = ContextType.UNKNOWN;
  switch (mode) {
    case ToolMode.kImageGen:
      contextType = ContextType.IMAGE_GEN;
      break;
    case ToolMode.kDeepSearch:
      contextType = ContextType.DEEP_RESEARCH;
      break;
    case ToolMode.kCanvas:
      contextType = ContextType.CANVAS;
      break;
    default:
      break;
  }
  recordContextualElementClickedMetric(
      composeboxSource, popupType, contextType);
}

export function recordToolModeShown(
    mode: ToolMode, composeboxSource: string, popupType: string) {
  let contextType = ContextType.UNKNOWN;
  switch (mode) {
    case ToolMode.kImageGen:
      contextType = ContextType.IMAGE_GEN;
      break;
    case ToolMode.kDeepSearch:
      contextType = ContextType.DEEP_RESEARCH;
      break;
    case ToolMode.kCanvas:
      contextType = ContextType.CANVAS;
      break;
    default:
      break;
  }
  recordContextualElementShownMetric(composeboxSource, popupType, contextType);
}

export function recordModelModeSelection(
    model: ModelMode, composeboxSource: string, popupType: string) {
  let contextType = ContextType.UNKNOWN;
  switch (model) {
    case ModelMode.kGeminiRegular:
      contextType = ContextType.REGULAR_MODEL;
      break;
    case ModelMode.kGeminiPro:
      contextType = ContextType.THINKING_MODEL;
      break;
    case ModelMode.kGeminiProAutoroute:
      contextType = ContextType.AUTO_MODEL;
      break;
    case ModelMode.kGeminiProNoGenUi:
      contextType = ContextType.PRO_NO_GEN_UI_MODEL;
      break;
    default:
      break;
  }
  recordContextualElementClickedMetric(
      composeboxSource, popupType, contextType);
}

export function recordModelModeShown(
    model: ModelMode, composeboxSource: string, popupType: string) {
  let contextType = ContextType.UNKNOWN;
  switch (model) {
    case ModelMode.kGeminiRegular:
      contextType = ContextType.REGULAR_MODEL;
      break;
    case ModelMode.kGeminiPro:
      contextType = ContextType.THINKING_MODEL;
      break;
    case ModelMode.kGeminiProAutoroute:
      contextType = ContextType.AUTO_MODEL;
      break;
    case ModelMode.kGeminiProNoGenUi:
      contextType = ContextType.PRO_NO_GEN_UI_MODEL;
      break;
    default:
      break;
  }
  recordContextualElementShownMetric(composeboxSource, popupType, contextType);
}

export function recordInputTypeShown(
    type: InputType, composeboxSource: string, popupType: string) {
  let contextType = ContextType.UNKNOWN;
  switch (type) {
    case InputType.kLensFile:
      contextType = ContextType.FILE;
      break;
    case InputType.kLensImage:
      contextType = ContextType.IMAGE;
      break;
    case InputType.kBrowserTab:
      contextType = ContextType.TAB;
      break;
    case InputType.kDrive:
      contextType = ContextType.DRIVE;
      break;
    default:
      break;
  }
  recordContextualElementShownMetric(composeboxSource, popupType, contextType);
}

export function hasAllowedInputs(
    inputState: InputState|null, usePecApi: boolean): boolean {
  if (!usePecApi) {
    return true;
  }
  return !!inputState &&
      (inputState.allowedModels.length > 0 ||
       inputState.allowedTools.length > 0 ||
       inputState.allowedInputTypes.length > 0);
}

/**
 * Helper to retrieve a boolean from loadTimeData with a fallback if the
 * value hasn't been set.
 */
// TODO(b/474406096): As part of componentization, the use of
// `loadTimeData.valueExists` in this file should be removed and the
// per-embedder behavior migrated into the relevant embedder. If a feature does
// still need to exist in the base component, it should become a property
// instead.
export function getLoadTimeBoolean(id: string, defaultValue: boolean): boolean {
  return loadTimeData.valueExists(id) ? loadTimeData.getBoolean(id) :
                                        defaultValue;
}

export function isContextUploadStatusTerminal(status: ContextUploadStatus):
    boolean {
  switch (status) {
    case ContextUploadStatus.kUploadSuccessful:
    case ContextUploadStatus.kUploadFailed:
    case ContextUploadStatus.kValidationFailed:
    case ContextUploadStatus.kUploadExpired:
    case ContextUploadStatus.kUploadReplaced:
      return true;
    case ContextUploadStatus.kNotUploaded:
    case ContextUploadStatus.kProcessing:
    case ContextUploadStatus.kUploadStarted:
    case ContextUploadStatus.kProcessingSuggestSignalsReady:
      return false;
    default:
      assertNotReachedCase(status, 'Unknown enum value');
  }
}

export function mapUploadErrorToProcessFilesError(errorType: ContextUploadErrorType):
    ProcessFilesError {
  switch (errorType) {
    case ContextUploadErrorType.kBrowserProcessingFileTooLargeError:
      return ProcessFilesError.FILE_TOO_LARGE;
    case ContextUploadErrorType.kBrowserProcessingFileEmptyError:
      return ProcessFilesError.FILE_EMPTY;
    case ContextUploadErrorType.kBrowserProcessingMaxFilesExceededError:
      return ProcessFilesError.MAX_FILES_EXCEEDED;
    case ContextUploadErrorType.kBrowserProcessingUnsupportedFileTypeError:
      return ProcessFilesError.INVALID_TYPE;
    case ContextUploadErrorType.kBrowserProcessingFileUploadNotAllowedError:
      return ProcessFilesError.FILE_UPLOAD_NOT_ALLOWED;
    case ContextUploadErrorType.kBrowserProcessingMaxImagesExceededError:
      return ProcessFilesError.MAX_IMAGES_EXCEEDED;
    case ContextUploadErrorType.kBrowserProcessingMaxPdfsExceededError:
      return ProcessFilesError.MAX_PDFS_EXCEEDED;
    default:
      return ProcessFilesError.NONE;
  }
}

/**
 * Returns whether a given tab ID represents a valid browser tab.
 */
export function isValidTabId(tabId: number|undefined|null): boolean {
  return typeof tabId === 'number' && tabId > 0;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
export enum SmartTabSharingSurface {
  OMNIBOX_COMPOSEBOX = 0,
  CONTEXTUAL_SEARCHBOX = 1,
  MAX_VALUE = CONTEXTUAL_SEARCHBOX,
}

// LINT.IfChange(TabPickerSurface)

// Histogram name suffix identifying which tab picker implementation recorded
// the sample.
export enum TabPickerSurface {
  // The tab picker page served by the Contextual Tasks component extension
  // (tab_picker.html, a web-accessible resource the AIM web page embeds).
  CONTEXTUAL_TASKS_EXTENSION = 'ContextualTasksExtension',
  // The tab picker flyout embedded in the in-WebUI composebox action menu in
  // Contextual Tasks.
  CONTEXTUAL_TASKS_WEB_UI = 'ContextualTasksWebUi',
  // The tab picker flyout embedded in the in-WebUI composebox action menu on
  // the New Tab Page.
  NEW_TAB_PAGE = 'NewTabPage',
  // The tab picker flyout embedded in the in-WebUI composebox action menu in
  // the Omnibox.
  OMNIBOX = 'Omnibox',
}

// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/histograms.xml:TabPickerSurface)

export function mapMetricsSourceToTabPickerSurface(metricsSource: string):
    TabPickerSurface|null {
  switch (metricsSource) {
    case 'ContextualTasks':
      return TabPickerSurface.CONTEXTUAL_TASKS_WEB_UI;
    case 'NewTabPage':
      return TabPickerSurface.NEW_TAB_PAGE;
    case 'Omnibox':
    case 'OmniboxEverywhere':
      return TabPickerSurface.OMNIBOX;
    default:
      return null;
  }
}

// Largest position the tab picker position histogram records explicitly; higher
// positions land in the overflow bucket. This is a histogram bound, not a limit
// on the suggestion list -- that is driven by the
// NtpComposeboxContextMenuMaxTabSuggestions Finch param, which currently
// defaults to 3. The bound is deliberately generous so that raising the param
// does not require changing the histogram.
const TAB_PICKER_POSITION_HISTOGRAM_MAX = 20;

/**
 * Records which tab the user attached from the tab picker, along two
 * independent axes.
 *
 * These are deliberately separate histograms rather than one enum, because the
 * two properties are orthogonal and frequently co-occur:
 *  - `isActiveTab` comes from TabInfo.showInCurrentTabChip, which the browser
 *    computes by comparing the tab's URL against the active tab's URL.
 *  - `index` is the tab's position in the suggestion list, which the browser
 *    sorts by descending recency. Index 0 is the most recently active eligible
 *    tab, and is the entry the UI badges as "Recent".
 *
 * `SelectedTabIsActive` is only recorded when an active tab candidate is
 * present in the suggestion list (`hasActiveTabCandidate` is true, or
 * `isActiveTab` is true). In full tab mode or on the New Tab Page, the active
 * tab is an internal chrome:// page and is excluded from the list, so no sample
 * is recorded to avoid skewing the metric.
 */
export function recordTabPickerTabSelected(
    surface: TabPickerSurface, isActiveTab: boolean, index: number,
    hasActiveTabCandidate: boolean = false) {
  if (hasActiveTabCandidate || isActiveTab) {
    recordBoolean(
        `ContextualSearch.TabPicker.SelectedTabIsActive.${surface}`,
        isActiveTab);
  }
  if (index < 0) {
    // The tab is no longer in the suggestion list; position is meaningless.
    return;
  }
  recordEnumerationValue(
      `ContextualSearch.TabPicker.SelectedTabPosition.${surface}`,
      Math.min(index, TAB_PICKER_POSITION_HISTOGRAM_MAX),
      TAB_PICKER_POSITION_HISTOGRAM_MAX + 1);
}
