// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.passwordsPrivate API */
// TODO(crbug.com/1203307): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace passwordsPrivate {
      export enum PlaintextReason {
        VIEW = 'VIEW',
        COPY = 'COPY',
        EDIT = 'EDIT',
      }

      export enum ExportProgressStatus {
        NOT_STARTED = 'NOT_STARTED',
        IN_PROGRESS = 'IN_PROGRESS',
        SUCCEEDED = 'SUCCEEDED',
        FAILED_CANCELLED = 'FAILED_CANCELLED',
        FAILED_WRITE_FAILED = 'FAILED_WRITE_FAILED',
      }

      export enum CompromiseType {
        LEAKED = 'LEAKED',
        PHISHED = 'PHISHED',
        PHISHED_AND_LEAKED = 'PHISHED_AND_LEAKED',
      }

      export enum PasswordCheckState {
        IDLE = 'IDLE',
        RUNNING = 'RUNNING',
        CANCELED = 'CANCELED',
        OFFLINE = 'OFFLINE',
        SIGNED_OUT = 'SIGNED_OUT',
        NO_PASSWORDS = 'NO_PASSWORDS',
        QUOTA_LIMIT = 'QUOTA_LIMIT',
        OTHER_ERROR = 'OTHER_ERROR',
      }

      export interface UrlCollection {
        origin: string;
        shown: string;
        link: string;
      }

      export interface PasswordUiEntry {
        urls: UrlCollection;
        username: string;
        federationText?: string;
        id: number;
        frontendId: number;
        fromAccountStore: boolean;
        passwordNote: string;
      }

      export interface ExceptionEntry {
        urls: UrlCollection;
        id: number;
        frontendId: number;
        fromAccountStore: boolean;
      }

      export interface PasswordExportProgress {
        status: ExportProgressStatus;
        folderName?: string;
      }

      export interface CompromisedInfo {
        compromiseTime: number;
        elapsedTimeSinceCompromise: string;
        compromiseType: CompromiseType;
        isMuted: boolean;
      }

      export interface InsecureCredential {
        id: number;
        formattedOrigin: string;
        detailedOrigin: string;
        isAndroidCredential: boolean;
        changePasswordUrl?: string;
        signonRealm: string;
        username: string;
        password?: string;
        compromisedInfo?: CompromisedInfo;
      }

      export interface PasswordCheckStatus {
        state: PasswordCheckState;
        alreadyProcessed?: number;
        remainingInQueue?: number;
        elapsedTimeSinceLastCheck?: string;
      }

      export interface AddPasswordOptions {
        url: string;
        username: string;
        password: string;
        note: string;
        useAccountStore: boolean;
      }

      export interface ChangeSavedPasswordParams {
        username: string;
        password: string;
        note?: string;
      }

      export function recordPasswordsPageAccessInSettings(): void;
      export function changeSavedPassword(
          ids: Array<number>, params: ChangeSavedPasswordParams,
          callback?: () => void): void;
      export function removeSavedPassword(id: number): void;
      export function removeSavedPasswords(ids: Array<number>): void;
      export function removePasswordException(id: number): void;
      export function removePasswordExceptions(ids: Array<number>): void;
      export function undoRemoveSavedPasswordOrException(): void;
      export function requestPlaintextPassword(
          id: number, reason: PlaintextReason,
          callback: (password: string) => void): void;
      export function getSavedPasswordList(
          callback: (entries: Array<PasswordUiEntry>) => void): void;
      export function getPasswordExceptionList(
          callback: (entries: Array<ExceptionEntry>) => void): void;
      export function movePasswordsToAccount(ids: Array<number>): void;
      export function importPasswords(): void;
      export function exportPasswords(callback: () => void): void;
      export function requestExportProgressStatus(
          callback: (status: ExportProgressStatus) => void): void;
      export function cancelExportPasswords(): void;
      export function isOptedInForAccountStorage(
          callback: (isOptedIn: boolean) => void): void;
      export function optInForAccountStorage(optIn: boolean): void;
      export function getCompromisedCredentials(
          callback: (credentials: Array<InsecureCredential>) => void): void;
      export function getWeakCredentials(
          callback: (credentials: Array<InsecureCredential>) => void): void;
      export function getPlaintextInsecurePassword(
          credential: InsecureCredential, reason: PlaintextReason,
          callback: (credential: InsecureCredential) => void): void;
      export function changeInsecureCredential(
          credential: InsecureCredential, newPassword: string,
          callback?: () => void): void;
      export function removeInsecureCredential(
          credential: InsecureCredential, callback?: () => void): void;
      export function muteInsecureCredential(
          credential: InsecureCredential, callback?: () => void): void;
      export function unmuteInsecureCredential(
          credential: InsecureCredential, callback?: () => void): void;
      export function recordChangePasswordFlowStarted(
          credential: InsecureCredential, isManualFlow: boolean): void;
      export function startPasswordCheck(callback?: () => void): void;
      export function stopPasswordCheck(callback?: () => void): void;
      export function getPasswordCheckStatus(
          callback: (status: PasswordCheckStatus) => void): void;
      export function isAccountStoreDefault(
          callback: (isDefault: boolean) => void): void;
      export function getUrlCollection(
          url: string, callback: (urlCollection: UrlCollection) => void): void;
      export function addPassword(
          options: AddPasswordOptions, callback?: () => void): void;

      export const onSavedPasswordsListChanged:
          ChromeEvent<(entries: Array<PasswordUiEntry>) => void>;
      export const onPasswordExceptionsListChanged:
          ChromeEvent<(entries: Array<ExceptionEntry>) => void>;
      export const onPasswordsFileExportProgress:
          ChromeEvent<(progress: PasswordExportProgress) => void>;
      export const onAccountStorageOptInStateChanged:
          ChromeEvent<(optInState: boolean) => void>;
      export const onCompromisedCredentialsChanged:
          ChromeEvent<(credentials: Array<InsecureCredential>) => void>;
      export const onWeakCredentialsChanged:
          ChromeEvent<(credentials: Array<InsecureCredential>) => void>;
      export const onPasswordCheckStatusChanged:
          ChromeEvent<(status: PasswordCheckStatus) => void>;
    }
  }
}
