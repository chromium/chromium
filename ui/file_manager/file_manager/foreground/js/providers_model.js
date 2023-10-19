// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {VolumeManager} from '../../externs/volume_manager.js';

/**
 * An item in the model. Represents a single providing extension.
 */
class ProvidersModelItem {
  /**
   * @param {string} providerId
   * @param {!chrome.fileManagerPrivate.IconSet} iconSet
   * @param {string} name
   * @param {boolean} configurable
   * @param {boolean} watchable
   * @param {boolean} multipleMounts
   * @param {string} source
   */
  constructor(
      providerId, iconSet, name, configurable, watchable, multipleMounts,
      source) {
    /** @private @const @type {string} */
    this.providerId_ = providerId;

    /** @private @const @type {!chrome.fileManagerPrivate.IconSet} */
    this.iconSet_ = iconSet;

    /** @private @const @type {string} */
    this.name_ = name;

    /** @private @const @type {boolean} */
    this.configurable_ = configurable;

    /** @private @const @type {boolean} */
    this.watchable_ = watchable;

    /** @private @const @type {boolean} */
    this.multipleMounts_ = multipleMounts;

    /** @private @const @type {string} */
    this.source_ = source;
  }

  /**
   * @return {string}
   */
  get providerId() {
    return this.providerId_;
  }

  /**
   * @return {!chrome.fileManagerPrivate.IconSet}
   */
  get iconSet() {
    return this.iconSet_;
  }

  /**
   * @return {string}
   */
  get name() {
    return this.name_;
  }

  /**
   * @return {boolean}
   */
  get configurable() {
    return this.configurable_;
  }

  /**
   * @return {boolean}
   */
  get watchable() {
    return this.watchable_;
  }

  /**
   * @return {boolean}
   */
  get multipleMounts() {
    return this.multipleMounts_;
  }

  /**
   * @return {string}
   */
  get source() {
    return this.source_;
  }
}

/**
 * Model for providing extensions. Providers methods for fetching lists of
 * providing extensions as well as performing operations on them, such as
 * requesting a new mount point.
 */
export class ProvidersModel {
  /**
   * @param {!VolumeManager} volumeManager
   */
  constructor(volumeManager) {
    /** @private @const @type {!VolumeManager} */
    this.volumeManager_ = volumeManager;
  }

  /**
   * @return {!Promise<Array<ProvidersModelItem>>}
   */
  getInstalledProviders() {
    return new Promise((fulfill, reject) => {
      chrome.fileManagerPrivate.getProviders(providers => {
        if (chrome.runtime.lastError) {
          reject(chrome.runtime.lastError.message);
          return;
        }
        // @ts-ignore: error TS7034: Variable 'results' implicitly has type
        // 'any[]' in some locations where its type cannot be determined.
        const results = [];
        providers.forEach(provider => {
          results.push(new ProvidersModelItem(
              provider.providerId, provider.iconSet, provider.name,
              provider.configurable, provider.watchable,
              provider.multipleMounts, provider.source));
        });
        // @ts-ignore: error TS7005: Variable 'results' implicitly has an
        // 'any[]' type.
        fulfill(results);
      });
    });
  }

  /**
   * @return {!Promise<Array<ProvidersModelItem>>}
   */
  getMountableProviders() {
    return this.getInstalledProviders().then(providers => {
      const mountedProviders = {};
      for (let i = 0; i < this.volumeManager_.volumeInfoList.length; i++) {
        const volumeInfo = this.volumeManager_.volumeInfoList.item(i);
        if (volumeInfo.volumeType === VolumeManagerCommon.VolumeType.PROVIDED) {
          // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an
          // index type.
          mountedProviders[volumeInfo.providerId] = true;
        }
      }
      return providers.filter(item => {
        // File systems handling files are mounted via file handlers. Device
        // handlers are mounted when a device is inserted. Only network file
        // systems are mounted manually by user via a menu.
        return item.source === 'network' &&
            // @ts-ignore: error TS7053: Element implicitly has an 'any' type
            // because expression of type 'string' can't be used to index type
            // '{}'.
            (!mountedProviders[item.providerId] || item.multipleMounts);
      });
    });
  }

  /**
   * @param {string} providerId
   */
  requestMount(providerId) {
    chrome.fileManagerPrivate.addProvidedFileSystem(assert(providerId), () => {
      if (chrome.runtime.lastError) {
        console.error(chrome.runtime.lastError.message);
      }
    });
  }
}
