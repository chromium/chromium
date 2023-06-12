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
        CONFLICTS = 'CONFLICTS',
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
        LONG_NOTE = 'LONG_NOTE',
        LONG_CONCATENATED_NOTE = 'LONG_CONCATENATED_NOTE',
        VALID = 'VALID',
      }

      export interface ImportEntry {
        status: ImportEntryStatus;
        url: string;
        username: string;
        password: string;
        id: number;
      }

      export interface ImportResults {
        status: ImportResultsStatus;
        numberImported: number;
        displayedEntries: ImportEntry[];
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

      export interface DomainInfo {
        name: string;
        url: string;
        signonRealm: string;
      }

      export interface PasswordUiEntry {
        isPasskey: boolean;
        urls: UrlCollection;
        affiliatedDomains?: DomainInfo[];
        username: string;
        displayName?: string;
        password?: string;
        federationText?: string;
        id: number;
        storedIn: PasswordStoreSet;
        isAndroidCredential: boolean;
        note?: string;
        changePasswordUrl?: string;
        compromisedInfo?: CompromisedInfo;
      }

      export interface CredentialGroup {
        name: string;
        iconUrl: string;
        entries: PasswordUiEntry[];
      }

      export interface ExceptionEntry {
        urls: UrlCollection;
        id: number;
      }

      export interface PasswordExportProgress {
        status: ExportProgressStatus;
        filePath?: string;
        folderName?: string;
      }

      export interface PasswordCheckStatus {
        state: PasswordCheckState;
        totalNumberOfPasswords?: number;
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

      export interface PasswordUiEntryList {
        entries: PasswordUiEntry[];
      }

      export function recordPasswordsPageAccessInSettings(): void;
      export function changeSavedPassword(
          id: number, params: ChangeSavedPasswordParams): Promise<number>;
      export function changeCredential(credential: PasswordUiEntry):
          Promise<void>;
      export function removeCredential(
          id: number, fromStores: PasswordStoreSet): void;
      export function removePasswordException(id: number): void;
      export function undoRemoveSavedPasswordOrException(): void;
      export function requestPlaintextPassword(
          id: number, reason: PlaintextReason): Promise<string>;
      export function requestCredentialsDetails(ids: number[]):
          Promise<PasswordUiEntry[]>;
      export function getSavedPasswordList(): Promise<PasswordUiEntry[]>;
      export function getCredentialGroups(): Promise<CredentialGroup[]>;
      export function getPasswordExceptionList(): Promise<ExceptionEntry[]>;
      export function movePasswordsToAccount(ids: number[]): void;
      export function importPasswords(toStore: PasswordStoreSet):
          Promise<ImportResults>;
      export function continueImport(selectedIds: number[]):
          Promise<ImportResults>;
      export function resetImporter(deleteFile: boolean): Promise<void>;
      export function exportPasswords(): Promise<void>;
      export function requestExportProgressStatus():
          Promise<ExportProgressStatus>;
      export function cancelExportPasswords(): void;
      export function isOptedInForAccountStorage(): Promise<boolean>;
      export function optInForAccountStorage(optIn: boolean): void;
      export function getInsecureCredentials(): Promise<PasswordUiEntry[]>;
      export function getCredentialsWithReusedPassword():
          Promise<PasswordUiEntryList[]>;
      export function muteInsecureCredential(credential: PasswordUiEntry):
          Promise<void>;
      export function unmuteInsecureCredential(credential: PasswordUiEntry):
          Promise<void>;
      export function recordChangePasswordFlowStarted(
          credential: PasswordUiEntry): void;
      export function startPasswordCheck(): Promise<void>;
      export function stopPasswordCheck(): Promise<void>;
      export function getPasswordCheckStatus(): Promise<PasswordCheckStatus>;
      export function isAccountStoreDefault(): Promise<boolean>;
      export function getUrlCollection(url: string):
          Promise<UrlCollection|null>;
      export function addPassword(options: AddPasswordOptions): Promise<void>;
      export function extendAuthValidity(): Promise<void>;
      export function switchBiometricAuthBeforeFillingState(): void;
      export function showAddShortcutDialog(): void;
      export function showExportedFileInShell(filePath: string): void;

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
