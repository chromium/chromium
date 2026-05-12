// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxContextAddedMethod} from '//resources/cr_components/search/constants.js';
import {assertNotReachedCase} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {DriveUploadError} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
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
}

export interface FileUpload {
  file: File;
}

export interface DriveUpload {
  token: UnguessableToken;
  mimeType: string;
  fileName: string;
  thumbnailUrl: string|null;
}

export enum TabUploadOrigin {
  CONTEXT_MENU = 0,
  RECENT_TAB_CHIP = 1,
  ACTION_CHIP = 2,
  AUTO_ACTIVE = 3,
  OTHER = 4,
}

export interface TabUpload {
  tabId: number;
  url: Url;
  title: string;
  delayUpload: boolean;
  origin: TabUploadOrigin;
}

export type ContextualUpload = TabUpload|FileUpload|DriveUpload;

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
  chrome.histograms.recordEnumerationValue(metricName, value, enumSize);
}

export function recordUserAction(metricName: string) {
  chrome.histograms.recordUserAction(metricName);
}

export function recordBoolean(metricName: string, value: boolean) {
  chrome.histograms.recordBoolean(metricName, value);
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
