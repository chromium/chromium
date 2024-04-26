// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.activityLogPrivate API. */
// TODO(crbug.com/40179454): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace activityLogPrivate {

      export enum ExtensionActivityType {
        API_CALL = 'api_call',
        API_EVENT = 'api_event',
        CONTENT_SCRIPT = 'content_script',
        DOM_ACCESS = 'dom_access',
        DOM_EVENT = 'dom_event',
        WEB_REQUEST = 'web_request',
      }

      export enum ExtensionActivityFilter {
        API_CALL = 'api_call',
        API_EVENT = 'api_event',
        CONTENT_SCRIPT = 'content_script',
        DOM_ACCESS = 'dom_access',
        DOM_EVENT = 'dom_event',
        WEB_REQUEST = 'web_request',
        ANY = 'any',
      }

      export enum ExtensionActivityDomVerb {
        GETTER = 'getter',
        SETTER = 'setter',
        METHOD = 'method',
        INSERTED = 'inserted',
        XHR = 'xhr',
        WEBREQUEST = 'webrequest',
        MODIFIED = 'modified',
      }

      export interface ExtensionActivity {
        activityId?: string;
        extensionId?: string;
        activityType: ExtensionActivityType;
        time?: number;
        apiCall?: string;
        args?: string;
        count?: number;
        pageUrl?: string;
        pageTitle?: string;
        argUrl?: string;
        other?: {
          prerender?: boolean,
          domVerb?: ExtensionActivityDomVerb,
          webRequest?: string,
          extra?: string,
        };
      }

      export interface Filter {
        extensionId?: string;
        activityType: ExtensionActivityFilter;
        apiCall?: string;
        pageUrl?: string;
        argUrl?: string;
        daysAgo?: number;
      }

      export interface ActivityResultSet {
        activities: chrome.activityLogPrivate.ExtensionActivity[];
      }

      type VoidCallback = () => void;

      export function getExtensionActivities(filter: Filter):
          Promise<ActivityResultSet>;
      export function deleteActivities(activityIds: string[]): Promise<void>;
      export function deleteActivitiesByExtension(extensionId: string):
          Promise<void>;
      export function deleteDatabase(): void;
      export function deleteUrls(urls: string[]): void;

      export const onExtensionActivity:
          ChromeEvent<(activity: ExtensionActivity) => void>;
    }
  }
}
