// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.webviewTag API
 * Generated from: chrome/common/extensions/api/webview_tag.json
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/webview_tag.json -g ts_definitions` to
 * regenerate.
 *
 * In addition to the generated file, some classes and objects have been
 * manually added to match the Closure externs file, and are commented as such.
 */

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {

    export namespace webviewTag {

      export const contentWindow: ContentWindow;

      export const request: WebRequestEventInterface;

      export const contextMenus: ContextMenus;

      export interface ClearDataOptions {
        since?: number;
      }

      export interface ClearDataTypeSet {
        appcache?: boolean;
        cache?: boolean;
        cookies?: boolean;
        sessionCookies?: boolean;
        persistentCookies?: boolean;
        fileSystems?: boolean;
        indexedDB?: boolean;
        localStorage?: boolean;
        webSQL?: boolean;
      }

      export enum ContextType {
        ALL = 'all',
        PAGE = 'page',
        FRAME = 'frame',
        SELECTION = 'selection',
        LINK = 'link',
        EDITABLE = 'editable',
        IMAGE = 'image',
        VIDEO = 'video',
        AUDIO = 'audio',
      }

      export interface InjectDetails {
        code?: string;
        file?: string;
      }

      export interface InjectionItems {
        code?: string;
        files?: string[];
      }

      export interface ContentScriptDetails {
        name: string;
        matches: string[];
        exclude_matches?: string[];
        match_about_blank?: boolean;
        css?: InjectionItems;
        js?: InjectionItems;
        run_at?: extensionTypes.RunAt;
        all_frames?: boolean;
        include_globs?: string[];
        exclude_globs?: string[];
      }

      export interface ContextMenuCreateProperties {
        type?: contextMenus.ItemType;
        id?: string;
        title?: string;
        checked?: boolean;
        contexts?: ContextType[];
        onclick?: (info: contextMenus.OnClickData) => void;
        parentId?: number|string;
        documentUrlPatterns?: string[];
        targetUrlPatterns?: string[];
        enabled?: boolean;
      }

      export interface ContextMenuUpdateProperties {
        type?: contextMenus.ItemType;
        title?: string;
        checked?: boolean;
        contexts?: ContextType[];
        onclick?: (info: contextMenus.OnClickData) => void;
        parentId?: number|string;
        documentUrlPatterns?: string[];
        targetUrlPatterns?: string[];
        enabled?: boolean;
      }

      export interface ContextMenus {
        create(
            createProperties: ContextMenuCreateProperties,
            callback?: () => void): number|string;
        update(
            id: number|string, updateProperties: ContextMenuUpdateProperties,
            callback?: () => void): void;
        remove(menuItemId: number|string, callback?: () => void): void;
        removeAll(callback?: () => void): void;
        onShow: ChromeEvent<(event: {
                              preventDefault: () => void,
                            }) => void>;
      }

      export interface ContentWindow {
        postMessage(message: any, targetOrigin: string): void;
      }

      export interface DialogController {
        ok(response?: string): void;
        cancel(): void;
      }

      export interface FindCallbackResults {
        numberOfMatches: number;
        activeMatchOrdinal: number;
        selectionRect: SelectionRect;
        canceled: boolean;
      }

      export interface FindOptions {
        backward?: boolean;
        matchCase?: boolean;
      }

      export interface NewWindow {
        attach(webview: {
          [key: string]: void,
        }): void;
        discard(): void;
      }

      // Manually added to match the webview_tag.js Closure externs file.
      export interface NewWindowEvent extends Event {
        window: NewWindow;
        targetUrl: string;
        initialWidth: number;
        initialHeight: number;
        name: string;
        windowOpenDisposition: string;
      }

      export interface MediaPermissionRequest {
        url: string;
        allow(): void;
        deny(): void;
      }

      export interface GeolocationPermissionRequest {
        url: string;
        allow(): void;
        deny(): void;
      }

      export interface PointerLockPermissionRequest {
        userGesture: boolean;
        lastUnlockedBySelf: boolean;
        url: string;
        allow(): void;
        deny(): void;
      }

      export interface DownloadPermissionRequest {
        requestMethod: string;
        url: string;
        allow(): void;
        deny(): void;
      }

      export interface FileSystemPermissionRequest {
        url: string;
        allow(): void;
        deny(): void;
      }

      export interface FullscreenPermissionRequest {
        origin: string;
        allow(): void;
        deny(): void;
      }

      export interface LoadPluginPermissionRequest {
        identifier: string;
        name: string;
        allow(): void;
        deny(): void;
      }

      export interface HidPermissionRequest {
        url: string;
        allow(): void;
        deny(): void;
      }

      export interface SelectionRect {
        left: number;
        top: number;
        width: number;
        height: number;
      }

      export enum ZoomMode {
        PER_ORIGIN = 'per-origin',
        PER_VIEW = 'per-view',
        DISABLED = 'disabled',
      }

      export enum StopFindingAction {
        CLEAR = 'clear',
        KEEP = 'keep',
        ACTIVATE = 'activate',
      }

      export enum DialogMessageType {
        ALERT = 'alert',
        CONFIRM = 'confirm',
        PROMPT = 'prompt',
      }

      export enum ExitReason {
        NORMAL = 'normal',
        ABNORMAL = 'abnormal',
        CRASHED = 'crashed',
        KILLED = 'killed',
        OOM_KILLED = 'oom killed',
        OOM = 'oom',
        FAILED_TO_LAUNCH = 'failed to launch',
        INTEGRITY_FAILURE = 'integrity failure',
      }

      export enum LoadAbortReason {
        ERR_ABORTED = 'ERR_ABORTED',
        ERR_INVALID_URL = 'ERR_INVALID_URL',
        ERR_DISALLOWED_URL_SCHEME = 'ERR_DISALLOWED_URL_SCHEME',
        ERR_BLOCKED_BY_CLIENT = 'ERR_BLOCKED_BY_CLIENT',
        ERR_ADDRESS_UNREACHABLE = 'ERR_ADDRESS_UNREACHABLE',
        ERR_EMPTY_RESPONSE = 'ERR_EMPTY_RESPONSE',
        ERR_FILE_NOT_FOUND = 'ERR_FILE_NOT_FOUND',
        ERR_UNKNOWN_URL_SCHEME = 'ERR_UNKNOWN_URL_SCHEME',
      }

      export enum WindowOpenDisposition {
        IGNORE = 'ignore',
        SAVE_TO_DISK = 'save_to_disk',
        CURRENT_TAB = 'current_tab',
        NEW_BACKGROUND_TAB = 'new_background_tab',
        NEW_FOREGROUND_TAB = 'new_foreground_tab',
        NEW_WINDOW = 'new_window',
        NEW_POPUP = 'new_popup',
      }

      export enum PermissionType {
        MEDIA = 'media',
        GEOLOCATION = 'geolocation',
        POINTER_LOCK = 'pointerLock',
        DOWNLOAD = 'download',
        LOADPLUGIN = 'loadplugin',
        FILESYSTEM = 'filesystem',
        FULLSCREEN = 'fullscreen',
        HID = 'hid',
      }

      export function getAudioState(callback: (audible: boolean) => void): void;

      export interface WebRequestEventInterface {
        // Manually added to match the webview_tag.js Closure externs file.
        onBeforeRequest: webRequest.WebRequestOptionallySynchronousEvent;
        onBeforeSendHeaders: webRequest.WebRequestOptionallySynchronousEvent;
        onCompleted: webRequest.WebRequestBaseEvent<(details: object) => void>;
        onSendHeaders: webRequest.WebRequestBaseEvent<(obj: any) => void>;
      }

      // Manually added to match the webview_tag.js Closure externs file.
      export interface WebView extends HTMLIFrameElement {
        request: WebRequestEventInterface;
        back(callback?: (success: boolean) => void): void;
        reload(): void;
        stop(): void;
        addContentScripts(contentScriptList: ContentScriptDetails[]): void;
        removeContentScripts(scriptNameList?: string[]): void;
        clearData(options: ClearDataOptions, types: ClearDataTypeSet, callback?:
            (results: any[]) => void): void;
        executeScript(
            details: InjectDetails, callback?: (results: any[]) => void): void;
        insertCSS(details: InjectDetails,
            callback?: (results: any[]) => void): void;
        terminate(): void;
        getUserAgent(): string;
        setUserAgentOverride(userAgent: string): void;
      }

      export function setAudioMuted(mute: boolean): void;

      export function isAudioMuted(callback: (muted: boolean) => void): void;

      export function captureVisibleRegion(
          options: extensionTypes.ImageDetails|undefined,
          callback: (dataUrl: string) => void): void;

      export function addContentScripts(
          contentScriptList: ContentScriptDetails[]): void;

      export function back(callback?: (success: boolean) => void): void;

      export function canGoBack(): boolean;

      export function canGoForward(): boolean;

      export function clearData(
          options: ClearDataOptions, types: ClearDataTypeSet,
          callback?: () => void): void;

      export function executeScript(
          details: InjectDetails, callback?: (result?: any[]) => void): void;

      export function find(
          searchText: string, options?: FindOptions,
          callback?: (results?: FindCallbackResults) => void): void;

      export function forward(callback?: (success: boolean) => void): void;

      export function getProcessId(): number;

      export function getUserAgent(): string;

      export function getZoom(callback: (zoomFactor: number) => void): void;

      export function getZoomMode(callback: (ZoomMode: ZoomMode) => void): void;

      export function go(
          relativeIndex: number, callback?: (success: boolean) => void): void;

      export function insertCSS(details: InjectDetails, callback?: () => void):
          void;

      export function isUserAgentOverridden(): void;

      export function print(): void;

      export function reload(): void;

      export function removeContentScripts(scriptNameList?: string[]): void;

      export function setUserAgentOverride(userAgent: string): void;

      export function setZoom(zoomFactor: number, callback?: () => void): void;

      export function setZoomMode(ZoomMode: ZoomMode, callback?: () => void):
          void;

      export function stop(): void;

      export function stopFinding(action?: StopFindingAction): void;

      export function loadDataWithBaseUrl(
          dataUrl: string, baseUrl: string, virtualUrl?: string): void;

      export function setSpatialNavigationEnabled(enabled: boolean): void;

      export function isSpatialNavigationEnabled(
          callback: (enabled: boolean) => void): void;

      export function terminate(): void;

      export const close: ChromeEvent<() => void>;

      export const consolemessage: ChromeEvent<
          (level: number, message: string, line: number, sourceId: string) =>
              void>;

      export const contentload: ChromeEvent<() => void>;

      export const dialog: ChromeEvent<
          (messageType: DialogMessageType, messageText: string,
           dialog: DialogController) => void>;

      export const exit: ChromeEvent<(details: {
                                       processID: number,
                                       reason: ExitReason,
                                     }) => void>;

      export const findupdate: ChromeEvent<
          (searchText: string, numberOfMatches: number,
           activeMatchOrdinal: number, selectionRect: SelectionRect,
           canceled: boolean, finalUpdate: string) => void>;

      export const loadabort: ChromeEvent<
          (url: string, isTopLevel: boolean, code: number,
           reason: LoadAbortReason) => void>;

      export const loadcommit:
          ChromeEvent<(url: string, isTopLevel: boolean) => void>;

      export const loadredirect: ChromeEvent<
          (oldUrl: string, newUrl: string, isTopLevel: boolean) => void>;

      export const loadstart:
          ChromeEvent<(url: string, isTopLevel: boolean) => void>;

      export const loadstop: ChromeEvent<() => void>;

      export const newwindow: ChromeEvent<
          (window: NewWindow, targetUrl: string, initialWidth: number,
           initialHeight: number, name: string,
           windowOpenDisposition: WindowOpenDisposition) => void>;

      export const permissionrequest:
          ChromeEvent<(permission: PermissionType, request: {
                        [key: string]: void,
                      }) => void>;

      export const responsive: ChromeEvent<(processID: number) => void>;

      export const sizechanged: ChromeEvent<
          (oldWidth: number, oldHeight: number, newWidth: number,
           newHeight: number) => void>;

      export const unresponsive: ChromeEvent<(processID: number) => void>;

      export const zoomchange:
          ChromeEvent<(oldZoomFactor: number, newZoomFactor: number) => void>;

    }
  }
}

