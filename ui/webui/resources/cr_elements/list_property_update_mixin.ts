// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview |ListPropertyUpdateMixin| is used to update an existing
 * polymer list property given the list after all the edits were made while
 * maintaining the reference to the original list. This allows
 * dom-repeat/iron-list elements bound to this list property to not fully
 * re-rendered from scratch.
 *
 * The minimal splices needed to transform the original list to the edited list
 * are calculated using |Polymer.ArraySplice.calculateSplices|. All the edits
 * are then applied to the original list. Once completed, a single notification
 * containing information about all the edits is sent to the polyer object.
 */

import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {calculateSplices, dedupingMixin} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

type Constructor<T> = new (...args: any[]) => T;

export const ListPropertyUpdateMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<ListPropertyUpdateMixinInterface> => {
      class ListPropertyUpdateMixin extends superClass implements
          ListPropertyUpdateMixinInterface {
        updateList<T>(
            propertyPath: string, identityGetter: (item: T) => (T | string),
            updatedList: T[], identityBasedUpdate: boolean = false): boolean {
          const list = this.get(propertyPath);
          const splices = calculateSplices(
              updatedList.map(item => identityGetter(item)),
              list.map(identityGetter));

          splices.forEach(splice => {
            const index = splice.index;
            const deleteCount = splice.removed.length;
            // Transform splices to the expected format of notifySplices().
            // Convert !Array<string> to !Array<!Object>.
            splice.removed = list.slice(index, index + deleteCount);
            splice.object = list;
            splice.type = 'splice';

            const added = updatedList.slice(index, index + splice.addedCount);
            const spliceParams = [index, deleteCount].concat(added);
            list.splice.apply(list, spliceParams);
          });

          let updated = splices.length > 0;
          if (!identityBasedUpdate) {
            list.forEach((item: object, index: number) => {
              const updatedItem = updatedList[index];
              if (JSON.stringify(item) !== JSON.stringify(updatedItem)) {
                this.set([propertyPath, index], updatedItem);
                updated = true;
              }
            });
          }

          if (splices.length > 0) {
            this.notifySplices(propertyPath, splices);
          }
          return updated;
        }
      }
      return ListPropertyUpdateMixin;
    });

export interface ListPropertyUpdateMixinInterface {
  /** @return Whether notifySplices was called. */
  updateList<T>(
      propertyPath: string, identityGetter: (item: T) => (T | string),
      updatedList: T[], identityBasedUpdate?: boolean): boolean;
}
