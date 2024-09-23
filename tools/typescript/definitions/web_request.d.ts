// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.webRequest API
 * Generated from: extensions/common/api/web_request.json
 * run `tools/json_schema_compiler/compiler.py
 * extensions/common/api/web_request.json -g ts_definitions` to regenerate.
 *
 * In addition to the generated file, some classes and objects have been
 * manually added to match the Closure externs file, and are commented as such.
 */

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {

    export namespace webRequest {

      export const MAX_HANDLER_BEHAVIOR_CHANGED_CALLS_PER_10_MINUTES: number;

      export enum ResourceType {
        MAIN_FRAME = 'main_frame',
        SUB_FRAME = 'sub_frame',
        STYLESHEET = 'stylesheet',
        SCRIPT = 'script',
        IMAGE = 'image',
        FONT = 'font',
        OBJECT = 'object',
        XMLHTTPREQUEST = 'xmlhttprequest',
        PING = 'ping',
        CSP_REPORT = 'csp_report',
        MEDIA = 'media',
        WEBSOCKET = 'websocket',
        WEBBUNDLE = 'webbundle',
        OTHER = 'other',
      }

      export enum OnBeforeRequestOptions {
        BLOCKING = 'blocking',
        REQUEST_BODY = 'requestBody',
        EXTRA_HEADERS = 'extraHeaders',
      }

      export enum OnBeforeSendHeadersOptions {
        REQUEST_HEADERS = 'requestHeaders',
        BLOCKING = 'blocking',
        EXTRA_HEADERS = 'extraHeaders',
      }

      export enum OnSendHeadersOptions {
        REQUEST_HEADERS = 'requestHeaders',
        EXTRA_HEADERS = 'extraHeaders',
      }

      export enum OnHeadersReceivedOptions {
        BLOCKING = 'blocking',
        RESPONSE_HEADERS = 'responseHeaders',
        EXTRA_HEADERS = 'extraHeaders',
      }

      export enum OnAuthRequiredOptions {
        RESPONSE_HEADERS = 'responseHeaders',
        BLOCKING = 'blocking',
        ASYNC_BLOCKING = 'asyncBlocking',
        EXTRA_HEADERS = 'extraHeaders',
      }

      export enum OnResponseStartedOptions {
        RESPONSE_HEADERS = 'responseHeaders',
        EXTRA_HEADERS = 'extraHeaders',
      }

      export enum OnBeforeRedirectOptions {
        RESPONSE_HEADERS = 'responseHeaders',
        EXTRA_HEADERS = 'extraHeaders',
      }

      export enum OnCompletedOptions {
        RESPONSE_HEADERS = 'responseHeaders',
        EXTRA_HEADERS = 'extraHeaders',
      }

      export enum OnErrorOccurredOptions {
        EXTRA_HEADERS = 'extraHeaders',
      }

      export interface RequestFilter {
        urls: string[];
        types?: ResourceType[];
        tabId?: number;
        windowId?: number;
      }

      export type HttpHeaders = Array<{
        name: string,
        value?: string,
        binaryValue?: number[],
      }>;

      export interface BlockingResponse {
        cancel?: boolean;
        redirectUrl?: string;
        requestHeaders?: HttpHeaders;
        responseHeaders?: HttpHeaders;
        authCredentials?: {
          username: string,
          password: string,
        };
      }

      export interface UploadData {
        bytes?: any;
        file?: string;
      }

      export type FormDataItem = ArrayBuffer|string;

      // Manually added to match the web_request.js Closure externs file.
      export interface WebRequestBaseEvent<ListenerType> {
        addListener(
            listener: ListenerType, filter: RequestFilter,
            extraInfoSpec?: string[]): void;
        removeListener(listener: ListenerType): void;
      }

      // Manually added to match the web_request.js Closure externs file.
      export interface WebRequestOptionallySynchronousEvent extends
          WebRequestBaseEvent<(obj: any) => BlockingResponse | null> {}

      export enum IgnoredActionType {
        REDIRECT = 'redirect',
        REQUEST_HEADERS = 'request_headers',
        RESPONSE_HEADERS = 'response_headers',
        AUTH_CREDENTIALS = 'auth_credentials',
      }

      export function handlerBehaviorChanged(): Promise<void>;

      export const onBeforeRequest:
          ChromeEvent<(details: {
                        requestId: string,
                        url: string,
                        method: string,
                        frameId: number,
                        parentFrameId: number,
                        documentId?: string,
                        parentDocumentId?: string,
                        documentLifecycle?: extensionTypes.DocumentLifecycle,
                        frameType?: extensionTypes.FrameType,
                        requestBody?: {
                          error?: string,
                          formData?: {
                            [key: string]: FormDataItem[],
                          },
                          raw?: UploadData[],
                        },
                                   tabId: number,
                                   type: ResourceType,
                        initiator?: string, timeStamp: number,
                      }) => BlockingResponse>;

      export const onBeforeSendHeaders: ChromeEvent<
          (details: {
            requestId: string,
            url: string,
            method: string,
            frameId: number,
            parentFrameId: number,
            documentId: string,
            parentDocumentId?: string,
                            documentLifecycle: extensionTypes.DocumentLifecycle,
                            frameType: extensionTypes.FrameType,
                            tabId: number,
            initiator?: string, type: ResourceType, timeStamp: number,
            requestHeaders?: HttpHeaders,
          }) => BlockingResponse>;

      export const onSendHeaders: ChromeEvent<
          (details: {
            requestId: string,
            url: string,
            method: string,
            frameId: number,
            parentFrameId: number,
            documentId: string,
            parentDocumentId?: string,
                            documentLifecycle: extensionTypes.DocumentLifecycle,
                            frameType: extensionTypes.FrameType,
                            tabId: number,
                            type: ResourceType,
            initiator?: string, timeStamp: number,
            requestHeaders?: HttpHeaders,
          }) => void>;

      export const onHeadersReceived: ChromeEvent<
          (details: {
            requestId: string,
            url: string,
            method: string,
            frameId: number,
            parentFrameId: number,
            documentId: string,
            parentDocumentId?: string,
                            documentLifecycle: extensionTypes.DocumentLifecycle,
                            frameType: extensionTypes.FrameType,
                            tabId: number,
                            type: ResourceType,
            initiator?: string, timeStamp: number, statusLine: string,
            responseHeaders?: HttpHeaders, statusCode: number,
          }) => BlockingResponse>;

      export const onAuthRequired: ChromeEvent<
          (details: {
            requestId: string,
            url: string,
            method: string,
            frameId: number,
            parentFrameId: number,
            documentId: string,
            parentDocumentId?: string,
                            documentLifecycle: extensionTypes.DocumentLifecycle,
                            frameType: extensionTypes.FrameType,
                            tabId: number,
                            type: ResourceType,
            initiator?: string, timeStamp: number, scheme: string,
            realm?: string,
                 challenger: {
                   host: string,
                   port: number,
                 },
                 isProxy: boolean,
            responseHeaders?: HttpHeaders,
                           statusLine: string,
                           statusCode: number,
          },
           asyncCallback?: (response: BlockingResponse) => void) =>
              BlockingResponse>;

      export const onResponseStarted: ChromeEvent<
          (details: {
            requestId: string,
            url: string,
            method: string,
            frameId: number,
            parentFrameId: number,
            documentId: string,
            parentDocumentId?: string,
                            documentLifecycle: extensionTypes.DocumentLifecycle,
                            frameType: extensionTypes.FrameType,
                            tabId: number,
                            type: ResourceType,
            initiator?: string, timeStamp: number,
            ip?: string, fromCache: boolean, statusCode: number,
            responseHeaders?: HttpHeaders, statusLine: string,
          }) => void>;

      export const onBeforeRedirect: ChromeEvent<
          (details: {
            requestId: string,
            url: string,
            method: string,
            frameId: number,
            parentFrameId: number,
            documentId: string,
            parentDocumentId?: string,
                            documentLifecycle: extensionTypes.DocumentLifecycle,
                            frameType: extensionTypes.FrameType,
                            tabId: number,
                            type: ResourceType,
            initiator?: string, timeStamp: number,
            ip?: string,
              fromCache: boolean,
              statusCode: number,
              redirectUrl: string,
            responseHeaders?: HttpHeaders, statusLine: string,
          }) => void>;

      export const onCompleted: ChromeEvent<
          (details: {
            requestId: string,
            url: string,
            method: string,
            frameId: number,
            parentFrameId: number,
            documentId: string,
            parentDocumentId?: string,
                            documentLifecycle: extensionTypes.DocumentLifecycle,
                            frameType: extensionTypes.FrameType,
                            tabId: number,
                            type: ResourceType,
            initiator?: string, timeStamp: number,
            ip?: string, fromCache: boolean, statusCode: number,
            responseHeaders?: HttpHeaders, statusLine: string,
          }) => void>;

      export const onErrorOccurred: ChromeEvent<
          (details: {
            requestId: string,
            url: string,
            method: string,
            frameId: number,
            parentFrameId: number,
            documentId: string,
            parentDocumentId?: string,
                            documentLifecycle: extensionTypes.DocumentLifecycle,
                            frameType: extensionTypes.FrameType,
                            tabId: number,
                            type: ResourceType,
            initiator?: string, timeStamp: number,
            ip?: string, fromCache: boolean, error: string,
          }) => void>;

      export const onActionIgnored: ChromeEvent<(details: {
                                                  requestId: string,
                                                  action: IgnoredActionType,
                                                }) => void>;

    }
  }
}
