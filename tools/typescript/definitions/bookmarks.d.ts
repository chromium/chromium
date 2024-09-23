// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.bookmarks API. */
// TODO(crbug.com/40179454): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
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
        dateLastUsed?: number;
        dateGroupModified?: number;
        unmodifiable?: BookmarkTreeNodeUnmodifiable;
        children?: BookmarkTreeNode[];
      }

      export interface CreateDetails {
        parentId?: string|null;
        index?: number;
        title?: string;
        url?: string;
      }

      export const MAX_WRITE_OPERATIONS_PER_HOUR: number;
      export const MAX_SUSTAINED_WRITE_OPERATIONS_PER_MINUTE: number;

      export function get(idOrIdList: string|
                          string[]): Promise<BookmarkTreeNode[]>;

      export function getChildren(id: string): Promise<BookmarkTreeNode[]>;

      export function getRecent(numberOfItems: number):
          Promise<BookmarkTreeNode[]>;

      export function getTree(): Promise<BookmarkTreeNode[]>;

      export function getSubTree(id: string): Promise<BookmarkTreeNode[]>;

      export function search(query: string|{
        query?: string,
        url?: string,
        title?: string,
      }): Promise<BookmarkTreeNode[]>;

      export function create(bookmark: CreateDetails):
          Promise<BookmarkTreeNode>;

      export function move(id: string, destination: {
        parentId?: string,
        index?: number,
      }): Promise<BookmarkTreeNode>;

      export function update(id: string, changes: {
        title?: string,
        url?: string,
      }): Promise<BookmarkTreeNode>;

      export function remove(id: string): Promise<void>;
      export function removeTree(id: string): Promise<void>;

      export const onCreated:
          ChromeEvent<(id: string, bookmark: BookmarkTreeNode) => void>;

      export interface ChangeInfo {
        title: string;
        url?: string;
      }

      export const onChanged:
          ChromeEvent<(id: string, changeInfo: ChangeInfo) => void>;

      export interface ReorderInfo {
        childIds: string[];
      }

      export const onChildrenReordered:
          ChromeEvent<(id: string, reorderInfo: ReorderInfo) => void>;

      export interface RemoveInfo {
        index: number;
        node: BookmarkTreeNode;
        parentId: string;
      }

      export const onRemoved:
          ChromeEvent<(id: string, removeInfo: RemoveInfo) => void>;

      export interface MoveInfo {
        index: number;
        oldIndex: number;
        oldParentId: string;
        parentId: string;
      }

      export const onMoved:
          ChromeEvent<(id: string, moveInfo: MoveInfo) => void>;

      export const onImportEnded: ChromeEvent<() => void>;
      export const onImportBegan: ChromeEvent<() => void>;
    }
  }
}
