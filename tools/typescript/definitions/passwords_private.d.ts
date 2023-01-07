// Copyright 2021 The Chromium Authors
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
        REUSED = 'REUSED',
        WEAK = 'WEAK',
      }

      export enum PasswordStoreSet {
        DEVICE = 'DEVICE',
        ACCOUNT = 'ACCOUNT',
        DEVICE_AND_ACCOUNT = 'DEVICE_AND_ACCOUNT',
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

      export enum ImportResultsStatus {
        UNKNOWN_ERROR = 'UNKNOWN_ERROR',
        SUCCESS = 'SUCCESS',
        IO_ERROR = 'IO_ERROR',
        BAD_FORMAT = 'BAD_FORMAT',
        DISMISSED = 'DISMISSED',
        MAX_FILE_SIZE = 'MAX_FILE_SIZE',
        IMPORT_ALREADY_ACTIVE = 'IMPORT_ALREADY_ACTIVE',
        NUM_PASSWORDS_EXCEEDED = 'NUM_PASSWORDS_EXCEEDED',
      }

      export enum ImportEntryStatus {
        UNKNOWN_ERROR = 'UNKNOWN_ERROR',
        MISSING_PASSWORD = 'MISSING_PASSWORD',
        MISSING_URL = 'MISSING_URL',
        INVALID_URL = 'INVALID_URL',
        NON_ASCII_URL = 'NON_ASCII_URL',
        LONG_URL = 'LONG_URL',
        LONG_PASSWORD = 'LONG_PASSWORD',
        LONG_USERNAME = 'LONG_USERNAME',
        CONFLICT_PROFILE = 'CONFLICT_PROFILE',
        CONFLICT_ACCOUNT = 'CONFLICT_ACCOUNT',
      }

      export interface ImportEntry {
        status: ImportEntryStatus;
        url: string;
        username: string;
      }

      export interface ImportResults {
        status: ImportResultsStatus;
        numberImported: number;
        failedImports: ImportEntry[];
        fileName: string;
      }

      export interface UrlCollection {
        signonRealm: string;
        shown: string;
        link: string;
      }

      export interface CompromisedInfo {
        compromiseTime: number;
        elapsedTimeSinceCompromise: string;
        compromiseTypes: CompromiseType[];
        isMuted: boolean;
      }

      export interface PasswordUiEntry {
        urls: UrlCollection;
        username: string;
        password?: string;
        federationText?: string;
        id: number;
        storedIn: PasswordStoreSet;
        isAndroidCredential: boolean;
        note?: string;
        changePasswordUrl?: string;
        hasStartableScript: boolean;
        compromisedInfo?: CompromisedInfo;
      }

      export interface ExceptionEntry {
        urls: UrlCollection;
        id: number;
      }

      export interface PasswordExportProgress {
        status: ExportProgressStatus;
        folderName?: string;
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
          id: number, params: ChangeSavedPasswordParams,
          callback?: (newId: number) => void): void;
      export function removeSavedPassword(
          id: number, fromStores: PasswordStoreSet): void;
      export function removePasswordException(id: number): void;
      export function undoRemoveSavedPasswordOrException(): void;
      export function requestPlaintextPassword(
          id: number, reason: PlaintextReason,
          callback: (password: string) => void): void;
      export function requestCredentialDetails(
          id: number,
          callback: (passwordUiEntry: PasswordUiEntry) => void): void;
      export function getSavedPasswordList(
          callback: (entries: PasswordUiEntry[]) => void): void;
      export function getPasswordExceptionList(
          callback: (entries: ExceptionEntry[]) => void): void;
      export function movePasswordsToAccount(ids: number[]): void;
      export function importPasswords(toStore: PasswordStoreSet,
          callback: (results: ImportResults) => void): void;
      export function exportPasswords(callback: () => void): void;
      export function requestExportProgressStatus(
          callback: (status: ExportProgressStatus) => void): void;
      export function cancelExportPasswords(): void;
      export function isOptedInForAccountStorage(
          callback: (isOptedIn: boolean) => void): void;
      export function optInForAccountStorage(optIn: boolean): void;
      export function getInsecureCredentials(): Promise<PasswordUiEntry[]>;
      export function muteInsecureCredential(
          credential: PasswordUiEntry, callback?: () => void): void;
      export function unmuteInsecureCredential(
          credential: PasswordUiEntry, callback?: () => void): void;
      export function recordChangePasswordFlowStarted(
          credential: PasswordUiEntry, isManualFlow: boolean): void;
      export function refreshScriptsIfNecessary(
          callback?: () => void): void;
      export function startPasswordCheck(callback?: () => void): void;
      export function stopPasswordCheck(callback?: () => void): void;
      export function getPasswordCheckStatus(
          callback: (status: PasswordCheckStatus) => void): void;
      export function startAutomatedPasswordChange(
          credential: PasswordUiEntry,
          callback?: (success: boolean) => void): void;
      export function isAccountStoreDefault(
          callback: (isDefault: boolean) => void): void;
      export function getUrlCollection(
          url: string, callback: (urlCollection: UrlCollection) => void): void;
      export function addPassword(
          options: AddPasswordOptions, callback?: () => void): void;
      export function extendAuthValidity(callback?: () => void): void;
      export function switchBiometricAuthBeforeFillingState(): void;

      export const onSavedPasswordsListChanged:
          ChromeEvent<(entries: PasswordUiEntry[]) => void>;
      export const onPasswordExceptionsListChanged:
          ChromeEvent<(entries: ExceptionEntry[]) => void>;
      export const onPasswordsFileExportProgress:
          ChromeEvent<(progress: PasswordExportProgress) => void>;
      export const onAccountStorageOptInStateChanged:
          ChromeEvent<(optInState: boolean) => void>;
      export const onInsecureCredentialsChanged:
          ChromeEvent<(credentials: PasswordUiEntry[]) => void>;
      export const onPasswordCheckStatusChanged:
          ChromeEvent<(status: PasswordCheckStatus) => void>;
      export const onPasswordManagerAuthTimeout:
          ChromeEvent<() => void>;
    }
  }
}
