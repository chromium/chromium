// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.feedbackPrivate API */
// TODO(crbug.com/1203307): Auto-generate this file.

declare namespace chrome {
  export namespace feedbackPrivate {
    type AttachedFile = {
      name: string,
      data?: Blob,
    };

    export type SystemInformation = {
      key: string,
      value: string,
    };

    enum FeedbackFlow {
      REGULAR = 'regular',
      LOGIN = 'login',
      SAD_TAB_CRASH = 'sadTabCrash',
      GOOGLE_INTERNAL = 'googleInternal',
    }

    export type FeedbackInfo = {
      description: string,
      attachedFile?: AttachedFile,
      categoryTag?: string,
      descriptionPlaceholder?: string,
      email?: string,
      pageUrl?: string,
      productId?: number,
      screenshot?: Blob,
      traceId?: number,
      systemInformation?: SystemInformation[],
      sendHistograms?: boolean,
      flow?: FeedbackFlow,
      attachedFileBlobUuid?: string,
      screenshotBlobUuid?: string,
      useSystemWindowFrame?: boolean,
      sendBluetoothLogs?: boolean,
      sendTabTitles?: boolean,
      assistantDebugInfoAllowed?: boolean,
      fromAssistant?: boolean,
      includeBluetoothLogs?: boolean,
      showQuestionnaire?: boolean,
    };

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

    type ReadLogSourceParams = {
      source: LogSource,
      incremental: boolean,
      readerId?: number,
    };

    type ReadLogSourceResult = {
      readerId: number,
      logLines: string[],
    };

    export function getUserEmail(callback: (email: string) => void): void;

    export function sendFeedback(
        feedback: FeedbackInfo, loadSystemInfo?: boolean, formOpenTime?: number,
        callback?: (status: Status, landingPage: LandingPageType) => void):
        void;

    export function getSystemInformation(
        callback: (info: SystemInformation[]) => void): void;
  }
}
