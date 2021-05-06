// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.bookmarks API. */
// TODO(crbug.com/1203307): Auto-generate this file.

declare namespace chrome {
  export namespace bookmarks {
    export enum BookmarkTreeNodeUnmodifiable {
      MANAGED = 'managed',
    }

    export interface BookmarkTreeNode {
      id: string;
      parentId?: string;
      index?: number;
      url?: string;
      title: string;
      dateAdded?: number;
      dateGroupModified?: number;
      unmodifiable?: BookmarkTreeNodeUnmodifiable;
      children?: BookmarkTreeNode[];
    }

    export interface CreateDetails {
      parentId?: string;
      index?: number;
      title?: string;
      url?: string;
    }

    export const MAX_WRITE_OPERATIONS_PER_HOUR: number;
    export const MAX_SUSTAINED_WRITE_OPERATIONS_PER_MINUTE: number;

    export function get(
        idOrIdList: string|string[],
        callback: (p1: BookmarkTreeNode[]) => void): void;

    export function getChildren(
        id: string, callback: (p1: BookmarkTreeNode[]) => void): void;

    export function getRecent(
        numberOfItems: number,
        callback: (p1: BookmarkTreeNode[]) => void): void;

    export function getTree(callback: (p1: BookmarkTreeNode[]) => void): void;

    export function getSubTree(
        id: string, callback: (p1: BookmarkTreeNode[]) => void): void;

    export function search(
        query: string|{
          query: string | undefined,
          url: string|undefined,
          title: string|undefined
        },
        callback: (p1: BookmarkTreeNode[]) => void): void;

    export function create(
        bookmark: CreateDetails,
        callback?: (p1: BookmarkTreeNode) => void): void;

    export function move(
        id: string,
        destination: {parentId: string|undefined, index: number|undefined},
        callback?: (p1: BookmarkTreeNode) => void): void;

    export function update(
        id: string, changes: {title: string|undefined, url: string|undefined},
        callback?: (p1: BookmarkTreeNode) => void): void;

    export function remove(id: string, callback?: () => void): void;
    export function removeTree(id: string, callback?: () => void): void;
  }
}
