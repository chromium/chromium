// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getSanitizedScriptUrl} from './trusted_script_url_policy_util.js';

/**
 * @fileoverview Script loader allows loading scripts at the desired time.
 */

export interface ScriptParams {
  type?: string;
  defer?: boolean;
}

/**
 * Used to load scripts at a runtime. Typical use:
 *
 * await new ScriptLoader('its_time.js').load();
 *
 * Optional parameters may be also specified:
 *
 * await new ScriptLoader('its_time.js', {type: 'module'}).load();
 */
export class ScriptLoader {
  private type_: string|undefined;
  private defer_: boolean|undefined;

  /**
   * Creates a loader that loads the script specified by |src| once the load
   * method is called. Optional |params| can specify other script attributes.
   */
  constructor(private src_: string, params: ScriptParams = {}) {
    this.type_ = params.type;
    this.defer_ = params.defer;
  }

  async load(): Promise<string> {
    return new Promise((resolve, reject) => {
      const script = document.createElement('script');
      if (this.type_ !== undefined) {
        script.type = this.type_;
      }
      if (this.defer_ !== undefined) {
        script.defer = this.defer_;
      }
      script.onload = () => resolve(this.src_);
      script.onerror = (error) => reject(error);
      script.src = getSanitizedScriptUrl(this.src_) as unknown as string;
      document.head.append(script);
    });
  }
}
