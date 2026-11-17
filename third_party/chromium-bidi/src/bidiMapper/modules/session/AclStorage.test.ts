/**
 * Copyright 2026 Google LLC.
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

import {beforeEach, describe, it} from 'node:test';

import {assert} from 'chai';

import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

import {ALLOWED_ACL_COMMANDS, AclStorage} from './AclStorage.js';

const TOP_LEVEL_CONTEXT_1 = 'context-1';
const TOP_LEVEL_CONTEXT_2 = 'context-2';
const CHILD_CONTEXT = 'child-context';
const GRANDCHILD_CONTEXT = 'grandchild-context';
const CHILD_CONTEXT_2 = 'child-context-2';
const UNKNOWN_CONTEXT = 'unknown-context';

describe('AclStorage', () => {
  let browsingContextStorage: BrowsingContextStorage;
  let aclStorage: AclStorage;

  beforeEach(() => {
    browsingContextStorage = new BrowsingContextStorage();
    // Register two top-level contexts and nested child (iframe) contexts.
    browsingContextStorage.addContext({
      id: TOP_LEVEL_CONTEXT_1,
      parentId: null,
      isTopLevelContext: () => true,
    } as unknown as BrowsingContextImpl);
    browsingContextStorage.addContext({
      id: TOP_LEVEL_CONTEXT_2,
      parentId: null,
      isTopLevelContext: () => true,
    } as unknown as BrowsingContextImpl);
    browsingContextStorage.addContext({
      id: CHILD_CONTEXT,
      parentId: TOP_LEVEL_CONTEXT_1,
      isTopLevelContext: () => false,
    } as unknown as BrowsingContextImpl);
    browsingContextStorage.addContext({
      id: GRANDCHILD_CONTEXT,
      parentId: CHILD_CONTEXT,
      isTopLevelContext: () => false,
    } as unknown as BrowsingContextImpl);
    browsingContextStorage.addContext({
      id: CHILD_CONTEXT_2,
      parentId: TOP_LEVEL_CONTEXT_2,
      isTopLevelContext: () => false,
    } as unknown as BrowsingContextImpl);

    aclStorage = new AclStorage(browsingContextStorage);
  });

  it('always allows commands when disabled', () => {
    // Disabled by default, even if an empty ACL is configured.
    aclStorage.setAcl({});
    assert.isTrue(aclStorage.isCommandAllowed('browsingContext.getTree'));
    assert.isTrue(
      aclStorage.isCommandAllowed(
        'browsingContext.navigate',
        TOP_LEVEL_CONTEXT_1,
      ),
    );

    // Enabling enforces the configured ACL; disabling allows commands again.
    aclStorage.setEnabled(true);
    assert.isFalse(aclStorage.isCommandAllowed('browsingContext.getTree'));

    aclStorage.setEnabled(false);
    assert.isTrue(aclStorage.isCommandAllowed('browsingContext.getTree'));
  });

  it('disallows unlisted commands when enabled and set to an empty object', () => {
    aclStorage.setEnabled(true);
    const saved = aclStorage.setAcl({});
    assert.deepEqual(saved, {
      default: {commands: []},
      perNavigable: {},
    });
    assert.deepEqual(aclStorage.getAcl(), saved);
    assert.isFalse(aclStorage.isCommandAllowed('browsingContext.getTree'));
    assert.isFalse(
      aclStorage.isCommandAllowed(
        'browsingContext.navigate',
        TOP_LEVEL_CONTEXT_1,
      ),
    );
  });

  it('allows default commands across all navigables', () => {
    aclStorage.setEnabled(true);
    // Commands in `default.commands` are allowed both globally and for any
    // specific top-level navigable.
    const saved = aclStorage.setAcl({
      default: {
        commands: ['browsingContext.getTree', 'browsingContext.create'],
      },
    });

    assert.deepEqual(saved, {
      default: {
        commands: ['browsingContext.getTree', 'browsingContext.create'],
      },
      perNavigable: {},
    });
    assert.deepEqual(aclStorage.getAcl(), saved);

    // Allowed globally and on a specific navigable.
    assert.isTrue(aclStorage.isCommandAllowed('browsingContext.getTree'));
    assert.isTrue(
      aclStorage.isCommandAllowed(
        'browsingContext.getTree',
        TOP_LEVEL_CONTEXT_1,
      ),
    );

    // Commands not in `default.commands` remain disallowed.
    assert.isFalse(
      aclStorage.isCommandAllowed(
        'browsingContext.navigate',
        TOP_LEVEL_CONTEXT_1,
      ),
    );
  });

  it('allows union of default and perNavigable commands and returns full ACL', () => {
    aclStorage.setEnabled(true);
    const saved = aclStorage.setAcl({
      default: {
        commands: ['browsingContext.getTree'],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: ['browsingContext.navigate', 'input.performActions'],
        },
      },
    });

    assert.deepEqual(saved, {
      default: {
        commands: ['browsingContext.getTree'],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: ['browsingContext.navigate', 'input.performActions'],
        },
      },
    });
    assert.deepEqual(aclStorage.getAcl(), saved);

    // Global scope only allows `default.commands`.
    assert.isTrue(aclStorage.isCommandAllowed('browsingContext.getTree'));
    assert.isFalse(aclStorage.isCommandAllowed('browsingContext.navigate'));

    // TOP_LEVEL_CONTEXT_1 allows `default.commands` ∪ `perNavigable[TOP_LEVEL_CONTEXT_1].commands`.
    assert.isTrue(
      aclStorage.isCommandAllowed(
        'browsingContext.getTree',
        TOP_LEVEL_CONTEXT_1,
      ),
    );
    assert.isTrue(
      aclStorage.isCommandAllowed(
        'browsingContext.navigate',
        TOP_LEVEL_CONTEXT_1,
      ),
    );

    // TOP_LEVEL_CONTEXT_2 has no `perNavigable` entry, so only `default.commands` are allowed.
    assert.isTrue(
      aclStorage.isCommandAllowed(
        'browsingContext.getTree',
        TOP_LEVEL_CONTEXT_2,
      ),
    );
    assert.isFalse(
      aclStorage.isCommandAllowed(
        'browsingContext.navigate',
        TOP_LEVEL_CONTEXT_2,
      ),
    );
  });

  it('respects parent top-level navigable ACL for child and nested navigables', () => {
    aclStorage.setEnabled(true);
    aclStorage.setAcl({
      default: {
        commands: ['browsingContext.getTree'],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: ['browsingContext.navigate'],
        },
      },
    });

    // Child and grandchild of TOP_LEVEL_CONTEXT_1 inherit TOP_LEVEL_CONTEXT_1's ACL.
    assert.isTrue(
      aclStorage.isCommandAllowed('browsingContext.navigate', CHILD_CONTEXT),
    );
    assert.isTrue(
      aclStorage.isCommandAllowed(
        'browsingContext.navigate',
        GRANDCHILD_CONTEXT,
      ),
    );
    assert.isFalse(
      aclStorage.isCommandAllowed('input.performActions', CHILD_CONTEXT),
    );

    // Child of TOP_LEVEL_CONTEXT_2 only gets `default.commands`.
    assert.isTrue(
      aclStorage.isCommandAllowed('browsingContext.getTree', CHILD_CONTEXT_2),
    );
    assert.isFalse(
      aclStorage.isCommandAllowed('browsingContext.navigate', CHILD_CONTEXT_2),
    );
  });

  it('ignores unsupported commands and non-top-level or unknown navigables', () => {
    aclStorage.setEnabled(true);
    const saved = aclStorage.setAcl({
      default: {
        commands: [
          'browsingContext.getTree',
          // Duplicate command must be deduplicated.
          'browsingContext.getTree',
          // Unsupported in ACLs.
          'network.addIntercept',
          // Non-existent command.
          'unknown.command',
        ],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: [
            'browsingContext.navigate',
            // Duplicate command must be deduplicated.
            'browsingContext.navigate',
            // Unsupported in ACLs.
            'storage.getCookies',
          ],
        },
        // Valid top-level context with omitted `commands` defaults to `[]`.
        [TOP_LEVEL_CONTEXT_2]: {},
        // Non-top-level (child/iframe) context must be ignored.
        [CHILD_CONTEXT]: {
          commands: ['input.performActions'],
        },
        // Unknown context ID must be ignored.
        [UNKNOWN_CONTEXT]: {
          commands: ['browsingContext.navigate'],
        },
      },
    });

    // Only supported commands and known top-level navigables are retained.
    assert.deepEqual(saved, {
      default: {
        commands: ['browsingContext.getTree'],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: ['browsingContext.navigate'],
        },
        [TOP_LEVEL_CONTEXT_2]: {
          commands: [],
        },
      },
    });
    assert.deepEqual(aclStorage.getAcl(), saved);

    // Ignored commands and navigables are not allowed, while parent ACL still applies to child.
    assert.isFalse(aclStorage.isCommandAllowed('network.addIntercept'));
    assert.isFalse(
      aclStorage.isCommandAllowed('storage.getCookies', TOP_LEVEL_CONTEXT_1),
    );
    assert.isFalse(
      aclStorage.isCommandAllowed('input.performActions', CHILD_CONTEXT),
    );
    assert.isTrue(
      aclStorage.isCommandAllowed('browsingContext.navigate', CHILD_CONTEXT),
    );
    assert.isFalse(
      aclStorage.isCommandAllowed('browsingContext.navigate', UNKNOWN_CONTEXT),
    );
  });

  it('replaces previous ACL on reconfiguration', () => {
    aclStorage.setEnabled(true);
    aclStorage.setAcl({
      default: {
        commands: ['browsingContext.getTree'],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: ['browsingContext.navigate'],
        },
      },
    });

    // Reconfiguring overwrites both `default` and `perNavigable` entries.
    const updated = aclStorage.setAcl({
      default: {
        commands: ['browsingContext.create'],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_2]: {
          commands: ['input.performActions'],
        },
      },
    });
    assert.deepEqual(updated, {
      default: {
        commands: ['browsingContext.create'],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_2]: {
          commands: ['input.performActions'],
        },
      },
    });
    assert.isFalse(aclStorage.isCommandAllowed('browsingContext.getTree'));
    assert.isFalse(
      aclStorage.isCommandAllowed(
        'browsingContext.navigate',
        TOP_LEVEL_CONTEXT_1,
      ),
    );
    assert.isTrue(
      aclStorage.isCommandAllowed('input.performActions', TOP_LEVEL_CONTEXT_2),
    );
  });

  it('prunes closed top-level navigables from getAcl and isCommandAllowed', () => {
    aclStorage.setEnabled(true);
    aclStorage.setAcl({
      default: {
        commands: ['browsingContext.getTree'],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: ['browsingContext.navigate'],
        },
        [TOP_LEVEL_CONTEXT_2]: {
          commands: ['input.performActions'],
        },
      },
    });

    // Simulate closing TOP_LEVEL_CONTEXT_1.
    browsingContextStorage.deleteContextById(TOP_LEVEL_CONTEXT_1);

    assert.isFalse(
      aclStorage.isCommandAllowed(
        'browsingContext.navigate',
        TOP_LEVEL_CONTEXT_1,
      ),
    );
    assert.isFalse(
      aclStorage.isCommandAllowed('browsingContext.navigate', CHILD_CONTEXT),
    );
    assert.deepEqual(aclStorage.getAcl(), {
      default: {
        commands: ['browsingContext.getTree'],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_2]: {
          commands: ['input.performActions'],
        },
      },
    });
  });

  it('accepts all allowed ACL commands in default and perNavigable', () => {
    aclStorage.setEnabled(true);
    const allAllowedCommands = [...ALLOWED_ACL_COMMANDS];
    const saved = aclStorage.setAcl({
      default: {
        commands: allAllowedCommands,
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: allAllowedCommands,
        },
      },
    });

    assert.deepEqual(saved, {
      default: {
        commands: allAllowedCommands,
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: allAllowedCommands,
        },
      },
    });
    for (const command of ALLOWED_ACL_COMMANDS) {
      assert.isTrue(aclStorage.isCommandAllowed(command));
    }
  });

  it('interprets invalid ACL data as no permissions', () => {
    aclStorage.setEnabled(true);
    for (const invalidAcl of [
      undefined,
      null,
      false,
      123,
      'invalid',
      ['array'],
    ]) {
      const result = aclStorage.setAcl(invalidAcl);
      const expected = {
        default: {commands: []},
        perNavigable: {},
      };
      assert.deepEqual(result, expected);
      assert.deepEqual(aclStorage.getAcl(), expected);
      assert.isFalse(aclStorage.isCommandAllowed('browsingContext.getTree'));
    }

    const saved = aclStorage.setAcl({
      default: {
        commands: 'browsingContext.getTree',
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: null,
        [TOP_LEVEL_CONTEXT_2]: {
          commands: 123,
        },
      },
    });
    assert.deepEqual(saved, {
      default: {
        commands: [],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: [],
        },
        [TOP_LEVEL_CONTEXT_2]: {
          commands: [],
        },
      },
    });
    assert.isFalse(aclStorage.isCommandAllowed('browsingContext.getTree'));
    assert.isFalse(
      aclStorage.isCommandAllowed(
        'browsingContext.getTree',
        TOP_LEVEL_CONTEXT_1,
      ),
    );
  });
});
