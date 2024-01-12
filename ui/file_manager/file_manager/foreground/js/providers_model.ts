// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import type {VolumeManager} from '../../background/js/volume_manager.js';
import {VolumeType} from '../../common/js/volume_manager_types.js';

/**
 * An item in the model. Represents a single providing extension.
 */
class ProvidersModelItem {
  constructor(
      public readonly providerId: string,
      public readonly iconSet: chrome.fileManagerPrivate.IconSet,
      public readonly name: string, public readonly configurable: boolean,
      public readonly watchable: boolean,
      public readonly multipleMounts: boolean, public readonly source: string) {
  }
}

/**
 * Model for providing extensions. Providers methods for fetching lists of
 * providing extensions as well as performing operations on them, such as
 * requesting a new mount point.
 */
export class ProvidersModel {
  constructor(private volumeManager_: VolumeManager) {}

  getInstalledProviders(): Promise<ProvidersModelItem[]> {
    return new Promise((fulfill, reject) => {
      chrome.fileManagerPrivate.getProviders(
          (providers: chrome.fileManagerPrivate.Provider[]) => {
            if (chrome.runtime.lastError) {
              reject(chrome.runtime.lastError.message);
              return;
            }
            const results: ProvidersModelItem[] = [];
            providers.forEach(provider => {
              results.push(new ProvidersModelItem(
                  provider.providerId, provider.iconSet, provider.name,
                  provider.configurable, provider.watchable,
                  provider.multipleMounts, provider.source));
            });
            fulfill(results);
          });
    });
  }

  getMountableProviders(): Promise<ProvidersModelItem[]> {
    return this.getInstalledProviders().then(providers => {
      const mountedProviders: Record<string, boolean> = {};
      for (let i = 0; i < this.volumeManager_.volumeInfoList.length; i++) {
        const volumeInfo = this.volumeManager_.volumeInfoList.item(i);
        if (volumeInfo.volumeType === VolumeType.PROVIDED) {
          mountedProviders[volumeInfo.providerId!] = true;
        }
      }
      return providers.filter(item => {
        // File systems handling files are mounted via file handlers. Device
        // handlers are mounted when a device is inserted. Only network file
        // systems are mounted manually by user via a menu.
        return item.source === 'network' &&
            (!mountedProviders[item.providerId] || item.multipleMounts);
      });
    });
  }

  requestMount(providerId: string) {
    assert(providerId);
    chrome.fileManagerPrivate.addProvidedFileSystem(providerId, () => {
      if (chrome.runtime.lastError) {
        console.error(chrome.runtime.lastError.message);
      }
    });
  }
}
