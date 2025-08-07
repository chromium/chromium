/*
 * Copyright 2025 Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import {expect} from 'chai';

import {ContextConfig} from './ContextConfig.js';
import {ContextConfigStorage} from './ContextConfigStorage.js';

describe('ContextConfigStorage', () => {
  const USER_CONTEXT = 'some user context';
  const BROWSER_CONTEXT = 'some browsing context';

  let storage: ContextConfigStorage;

  beforeEach(() => {
    storage = new ContextConfigStorage();
  });

  describe('getActiveConfig', () => {
    // Test cases for various configuration scenarios.
    [
      // Global config only.
      {
        global: {acceptInsecureCerts: true, prerenderingDisabled: true},
        expected: {acceptInsecureCerts: true, prerenderingDisabled: true},
        name: 'should return the global config if no other configs are set',
      },
      // User context overrides global.
      {
        global: {acceptInsecureCerts: true, prerenderingDisabled: true},
        user: {acceptInsecureCerts: false},
        expected: {acceptInsecureCerts: false, prerenderingDisabled: true},
        name: 'should override global config with user context config',
      },
      // Browsing context overrides global and user context.
      {
        global: {acceptInsecureCerts: false, prerenderingDisabled: true},
        user: {acceptInsecureCerts: false, prerenderingDisabled: false},
        browsing: {acceptInsecureCerts: true},
        expected: {acceptInsecureCerts: true, prerenderingDisabled: false},
        name: 'should override global and user context configs with browsing context config',
      },
      // Undefined in user context does not override global.
      {
        global: {acceptInsecureCerts: true, prerenderingDisabled: true},
        user: {acceptInsecureCerts: undefined, prerenderingDisabled: false},
        expected: {acceptInsecureCerts: true, prerenderingDisabled: false},
        name: 'should not override with undefined from user context config',
      },
      // Undefined in browsing context does not override user context.
      {
        global: {acceptInsecureCerts: true, prerenderingDisabled: true},
        user: {acceptInsecureCerts: false, prerenderingDisabled: false},
        browsing: {acceptInsecureCerts: undefined, prerenderingDisabled: true},
        expected: {acceptInsecureCerts: false, prerenderingDisabled: true},
        name: 'should not override with undefined from browsing context config',
      },
      // Undefined in both user and browsing context does not override global.
      {
        global: {acceptInsecureCerts: true, prerenderingDisabled: true},
        user: {acceptInsecureCerts: undefined, prerenderingDisabled: false},
        browsing: {
          acceptInsecureCerts: undefined,
          prerenderingDisabled: undefined,
        },
        expected: {acceptInsecureCerts: true, prerenderingDisabled: false},
        name: 'should not override with undefined from either user or browsing context config',
      },
      // Sequential updates to global context.
      {
        global: [{acceptInsecureCerts: true}, {acceptInsecureCerts: undefined}],
        expected: {acceptInsecureCerts: true},
        name: 'should ignore undefined when updating global config',
      },
      // Sequential updates to user context.
      {
        global: {acceptInsecureCerts: true},
        user: [{acceptInsecureCerts: false}, {acceptInsecureCerts: undefined}],
        expected: {acceptInsecureCerts: false},
        name: 'should ignore undefined when updating user context config',
      },
      // Sequential updates to browsing context.
      {
        global: {acceptInsecureCerts: true},
        user: {acceptInsecureCerts: false},
        browsing: [
          {acceptInsecureCerts: true},
          {acceptInsecureCerts: undefined},
        ],
        expected: {acceptInsecureCerts: true},
        name: 'should ignore undefined when updating browsing context config',
      },
      // Null in user context overrides global.
      {
        global: {viewport: {width: 1, height: 1}},
        user: {viewport: null},
        expected: {viewport: null},
        name: 'should override with null from user context config',
      },
      // Null in browsing context overrides user context.
      {
        global: {viewport: {width: 1, height: 1}},
        user: {viewport: {width: 2, height: 2}},
        browsing: {viewport: null},
        expected: {viewport: null},
        name: 'should override with null from browsing context config',
      },
      // Value in browsing context overrides null in user context.
      {
        global: {viewport: {width: 1, height: 1}},
        user: {viewport: null},
        browsing: {viewport: {width: 3, height: 3}},
        expected: {viewport: {width: 3, height: 3}},
        name: 'should override null with value from browsing context config',
      },
    ].forEach(({name, global, user, browsing, expected}) => {
      it(name, () => {
        if (global) {
          for (const config of Array.isArray(global) ? global : [global]) {
            storage.updateGlobalConfig(config);
          }
        }
        if (user) {
          for (const config of Array.isArray(user) ? user : [user]) {
            storage.updateUserContextConfig(USER_CONTEXT, config);
          }
        }
        if (browsing) {
          for (const config of Array.isArray(browsing)
            ? browsing
            : [browsing]) {
            storage.updateBrowsingContextConfig(BROWSER_CONTEXT, config);
          }
        }

        const activeConfig = storage.getActiveConfig(
          BROWSER_CONTEXT,
          USER_CONTEXT,
        );

        const expectedConfig = new ContextConfig();
        Object.assign(expectedConfig, expected);

        expect(activeConfig).to.deep.equal(expectedConfig);
      });
    });
  });
});
