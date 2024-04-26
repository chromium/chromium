// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.usersPrivate API */
// TODO(crbug.com/40179454): Auto-generate this file.

declare namespace chrome {
  export namespace usersPrivate {
    export interface User {
      email: string;
      displayEmail: string;
      name: string;
      isOwner: boolean;
      isChild: boolean;
    }

    export interface LoginStatusDict {
      isLoggedIn: boolean;
      isScreenLocked: boolean;
    }

    export function getUsers(): Promise<User[]>;
    export function isUserInList(email: string): Promise<boolean>;
    export function addUser(email: string): Promise<boolean>;
    export function removeUser(email: string): Promise<boolean>;
    export function isUserListManaged(): Promise<boolean>;
    export function getCurrentUser(): Promise<User>;
    export function getLoginStatus(): Promise<LoginStatusDict[]>;
  }
}
