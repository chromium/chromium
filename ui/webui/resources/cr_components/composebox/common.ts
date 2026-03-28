// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxContextAddedMethod} from '//resources/cr_components/search/constants.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {ContextUploadErrorType, ContextUploadStatus, InputType} from './composebox_query.mojom-webui.js';
import type {InputState, ModelMode, ToolMode} from './composebox_query.mojom-webui.js';

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
  mode: ToolMode;
  model: ModelMode;
}

export interface FileUpload {
  file: File;
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

export type ContextualUpload = TabUpload|FileUpload;

export enum GlifAnimationState {
  INELIGIBLE = 'ineligible',
  SPINNER_ONLY = 'spinner-only',
  STARTED = 'started',
  FINISHED = 'finished',
}

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
