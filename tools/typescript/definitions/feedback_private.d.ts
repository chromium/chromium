// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.feedbackPrivate API */
// TODO(crbug.com/40179454): Auto-generate this file.

declare namespace chrome {
  export namespace feedbackPrivate {
    interface AttachedFile {
      name: string;
      data?: Blob;
    }

    export interface LogsMapEntry {
      key: string;
      value: string;
    }

    enum FeedbackFlow {
      REGULAR = 'regular',
      LOGIN = 'login',
      SAD_TAB_CRASH = 'sadTabCrash',
      GOOGLE_INTERNAL = 'googleInternal',
      AI = 'ai',
    }

    export interface FeedbackInfo {
      description: string;
      attachedFile?: AttachedFile;
      categoryTag?: string;
      descriptionPlaceholder?: string;
      email?: string;
      pageUrl?: string;
      productId?: number;
      screenshot?: Blob;
      traceId?: number;
      systemInformation?: LogsMapEntry[];
      sendHistograms?: boolean;
      flow?: FeedbackFlow;
      attachedFileBlobUuid?: string;
      screenshotBlobUuid?: string;
      useSystemWindowFrame?: boolean;
      sendAutofillMetadata?: boolean;
      sendBluetoothLogs?: boolean;
      sendTabTitles?: boolean;
      assistantDebugInfoAllowed?: boolean;
      fromAssistant?: boolean;
      includeBluetoothLogs?: boolean;
      showQuestionnaire?: boolean;
      fromAutofill?: boolean;
      autofillMetadata?: string;
      isOffensiveOrUnsafe?: boolean;
      aiMetadata?: string;
    }

    enum Status {
      SUCCESS = 'success',
      DELAYED = 'delayed',
    }

    enum LandingPageType {
      NORMAL = 'normal',
      TECHSTOP = 'techstop',
      NO_LANDING_PAGE = 'noLandingPage',
    }

    enum LogSource {
      MESSAGES = 'messages',
      UI_LATEST = 'uiLatest',
      DRM_MODETEST = 'drmModetest',
      LSUSB = 'lsusb',
      ATRUS_LOG = 'atrusLog',
      NET_LOG = 'netLog',
      EVENT_LOG = 'eventLog',
      UPDATE_ENGINE_LOG = 'updateEngineLog',
      POWERD_LATEST = 'powerdLatest',
      POWERD_PREVIOUS = 'powerdPrevious',
      LSPCI = 'lspci',
      IFCONFIG = 'ifconfig',
      UPTIME = 'uptime',
    }

    interface ReadLogSourceParams {
      source: LogSource;
      incremental: boolean;
      readerId?: number;
    }

    interface ReadLogSourceResult {
      readerId: number;
      logLines: string[];
    }

    export interface SendFeedbackResult {
      status: Status;
      landingPageType: LandingPageType;
    }

    export function getUserEmail(callback: (email: string) => void): void;

    export function sendFeedback(
        feedback: FeedbackInfo, loadSystemInfo?: boolean,
        formOpenTime?: number): Promise<SendFeedbackResult>;

    export function getSystemInformation(
      callback: (info: LogsMapEntry[]) => void): void;
  }
}
