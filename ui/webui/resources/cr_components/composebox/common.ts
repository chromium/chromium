// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxContextAddedMethod} from '//resources/cr_components/search/constants.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {ContextUploadErrorType} from './composebox_query.mojom-webui.js';
import type {ContextUploadStatus} from './composebox_query.mojom-webui.js';

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

export interface ComposeboxFile {
  uuid: UnguessableToken;
  name: string;
  objectUrl: string|null;
  dataUrl: string|null;
  type: string;
  status: ContextUploadStatus;
  url: Url|null;
  tabId: number|null;
  isDeletable: boolean;
  iconName: string|null;
  supportsUnimodal: boolean;
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
