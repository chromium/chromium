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

import {UnsupportedOperationException} from '../../../protocol/protocol.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

import {
  ACL_EXEMPT_COMMANDS,
  AclStorage,
  SUPPORTED_ACL_COMMANDS,
} from './AclStorage.js';

const TOP_LEVEL_CONTEXT_1 = 'context-1';
const TOP_LEVEL_CONTEXT_2 = 'context-2';
const CHILD_CONTEXT = 'child-context';
const GRANDCHILD_CONTEXT = 'grandchild-context';
const CHILD_CONTEXT_2 = 'child-context-2';
const UNKNOWN_CONTEXT = 'unknown-context';
const SANDBOX = 'agent-sandbox';

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

  it('accepts all supported ACL commands in default and perNavigable', () => {
    aclStorage.setEnabled(true);
    const allSupportedCommands = [...SUPPORTED_ACL_COMMANDS];
    const saved = aclStorage.setAcl({
      default: {
        commands: allSupportedCommands,
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: allSupportedCommands,
        },
      },
    });

    assert.deepEqual(saved, {
      default: {
        commands: allSupportedCommands,
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: allSupportedCommands,
        },
      },
    });
    for (const command of SUPPORTED_ACL_COMMANDS) {
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

  describe('assertCommandAllowed', () => {
    it('allows any command and target when disabled', () => {
      for (const command of [
        'network.addIntercept',
        'goog:cdp.sendCommand',
        'unknown.command',
      ]) {
        assert.doesNotThrow(() => aclStorage.assertCommandAllowed(command, {}));
      }
      assert.doesNotThrow(() =>
        aclStorage.assertCommandAllowed('script.evaluate', {
          target: {realm: 'any-realm'},
        }),
      );
    });

    it('always allows exempt commands', () => {
      aclStorage.setEnabled(true);
      aclStorage.setAcl({});

      const expectedExemptCommands = [
        'session.end',
        'session.new',
        'session.status',
      ];
      assert.sameMembers([...ACL_EXEMPT_COMMANDS], expectedExemptCommands);
      for (const command of expectedExemptCommands) {
        assert.doesNotThrow(() => aclStorage.assertCommandAllowed(command, {}));
      }
    });

    it('throws unsupported operation for commands that an ACL cannot grant', () => {
      aclStorage.setEnabled(true);
      // Requesting such commands doesn't grant them.
      aclStorage.setAcl({
        default: {commands: ['network.addIntercept', 'goog:cdp.sendCommand']},
      });

      for (const command of [
        'network.addIntercept',
        'goog:cdp.sendCommand',
        'script.callFunction',
        'session.subscribe',
        'session.unsubscribe',
        'unknown.command',
      ]) {
        assert.throws(
          () =>
            aclStorage.assertCommandAllowed(command, {
              context: TOP_LEVEL_CONTEXT_1,
            }),
          UnsupportedOperationException,
          `Command '${command}' is not supported in restricted sessions.`,
        );
      }
    });

    it('throws unsupported operation for commands that the ACL does not grant', () => {
      aclStorage.setEnabled(true);
      aclStorage.setAcl({default: {commands: ['browsingContext.getTree']}});

      assert.doesNotThrow(() =>
        aclStorage.assertCommandAllowed('browsingContext.getTree', {}),
      );
      assert.throws(
        () =>
          aclStorage.assertCommandAllowed('browsingContext.create', {
            type: 'tab',
          }),
        UnsupportedOperationException,
        "Command 'browsingContext.create' is not allowed by the session ACL.",
      );
      assert.throws(
        () =>
          aclStorage.assertCommandAllowed('browsingContext.navigate', {
            context: TOP_LEVEL_CONTEXT_1,
            url: 'about:blank',
          }),
        UnsupportedOperationException,
        `Command 'browsingContext.navigate' is not allowed by the session ACL for browsing context '${TOP_LEVEL_CONTEXT_1}'.`,
      );
      assert.throws(
        () =>
          aclStorage.assertCommandAllowed('script.evaluate', {
            target: {context: TOP_LEVEL_CONTEXT_1},
          }),
        UnsupportedOperationException,
        `Command 'script.evaluate' is not allowed by the session ACL for browsing context '${TOP_LEVEL_CONTEXT_1}'.`,
      );
    });

    it('allows default commands for any browsing context', () => {
      aclStorage.setEnabled(true);
      aclStorage.setAcl({
        default: {commands: ['browsingContext.navigate', 'script.evaluate']},
      });

      for (const params of [
        {},
        {context: CHILD_CONTEXT_2},
        {context: UNKNOWN_CONTEXT},
      ]) {
        assert.doesNotThrow(() =>
          aclStorage.assertCommandAllowed('browsingContext.navigate', params),
        );
      }
      for (const context of [CHILD_CONTEXT_2, UNKNOWN_CONTEXT]) {
        assert.doesNotThrow(() =>
          aclStorage.assertCommandAllowed('script.evaluate', {
            target: {context, sandbox: SANDBOX},
          }),
        );
      }
    });

    const NO_TARGET_COMMANDS = [
      'browsingContext.create',
      'browsingContext.getTree',
    ];
    const CONTEXT_TARGET_COMMANDS = [
      'browsingContext.captureScreenshot',
      'browsingContext.close',
      'browsingContext.handleUserPrompt',
      'browsingContext.locateNodes',
      'browsingContext.navigate',
      'browsingContext.reload',
      'input.performActions',
    ];

    it('covers every supported ACL command with a target rule test', () => {
      assert.sameMembers(
        [...SUPPORTED_ACL_COMMANDS],
        [...NO_TARGET_COMMANDS, ...CONTEXT_TARGET_COMMANDS, 'script.evaluate'],
      );
    });

    it('checks commands without a target tab against the default ACL only', () => {
      aclStorage.setEnabled(true);
      aclStorage.setAcl({
        perNavigable: {
          [TOP_LEVEL_CONTEXT_1]: {
            commands: NO_TARGET_COMMANDS,
          },
        },
      });

      assert.throws(
        () =>
          aclStorage.assertCommandAllowed('browsingContext.create', {
            type: 'tab',
            referenceContext: TOP_LEVEL_CONTEXT_1,
          }),
        UnsupportedOperationException,
        "Command 'browsingContext.create' is not allowed by the session ACL.",
      );
      assert.throws(
        () =>
          aclStorage.assertCommandAllowed('browsingContext.getTree', {
            root: TOP_LEVEL_CONTEXT_1,
          }),
        UnsupportedOperationException,
        "Command 'browsingContext.getTree' is not allowed by the session ACL.",
      );
      // Unlike most commands, these ignore `context`.
      for (const command of NO_TARGET_COMMANDS) {
        assert.throws(
          () =>
            aclStorage.assertCommandAllowed(command, {
              context: TOP_LEVEL_CONTEXT_1,
            }),
          UnsupportedOperationException,
          `Command '${command}' is not allowed by the session ACL.`,
        );
      }
    });

    it('checks `context` against the ACL of its top-level navigable', () => {
      aclStorage.setEnabled(true);
      for (const command of CONTEXT_TARGET_COMMANDS) {
        aclStorage.setAcl({
          perNavigable: {
            [TOP_LEVEL_CONTEXT_1]: {commands: [command]},
          },
        });

        for (const context of [
          TOP_LEVEL_CONTEXT_1,
          CHILD_CONTEXT,
          GRANDCHILD_CONTEXT,
        ]) {
          assert.doesNotThrow(() =>
            aclStorage.assertCommandAllowed(command, {
              context,
            }),
          );
        }
        for (const context of [
          TOP_LEVEL_CONTEXT_2,
          CHILD_CONTEXT_2,
          UNKNOWN_CONTEXT,
        ]) {
          assert.throws(
            () => aclStorage.assertCommandAllowed(command, {context}),
            UnsupportedOperationException,
            `Command '${command}' is not allowed by the session ACL for browsing context '${context}'.`,
          );
        }
        for (const params of [
          // Malformed or missing targets get only the `default` ACL.
          {context: 42},
          {},
          null,
          // Only `script` commands run in `target`.
          {target: {context: TOP_LEVEL_CONTEXT_1}},
        ]) {
          assert.throws(
            () => aclStorage.assertCommandAllowed(command, params),
            UnsupportedOperationException,
            `Command '${command}' is not allowed by the session ACL.`,
          );
        }
      }
    });

    it('checks `target.context` against the ACL of its top-level navigable', () => {
      aclStorage.setEnabled(true);
      aclStorage.setAcl({
        perNavigable: {[TOP_LEVEL_CONTEXT_1]: {commands: ['script.evaluate']}},
      });

      for (const context of [
        TOP_LEVEL_CONTEXT_1,
        CHILD_CONTEXT,
        GRANDCHILD_CONTEXT,
      ]) {
        assert.doesNotThrow(() =>
          aclStorage.assertCommandAllowed('script.evaluate', {
            target: {context, sandbox: SANDBOX},
          }),
        );
      }
      for (const params of [
        {target: {context: CHILD_CONTEXT_2, sandbox: SANDBOX}},
        // `script` commands run in `target`, not in `context`.
        {
          context: TOP_LEVEL_CONTEXT_1,
          target: {context: CHILD_CONTEXT_2, sandbox: SANDBOX},
        },
      ]) {
        assert.throws(
          () => aclStorage.assertCommandAllowed('script.evaluate', params),
          UnsupportedOperationException,
          `Command 'script.evaluate' is not allowed by the session ACL for browsing context '${CHILD_CONTEXT_2}'.`,
        );
      }
      for (const params of [
        // Targets without a string `target.context` get only the `default` ACL.
        {target: {context: 42, sandbox: SANDBOX}},
        {target: {realm: 'window-realm'}},
        {target: {realm: 'window-realm', sandbox: SANDBOX}},
        {target: {realm: 42}},
        {target: {}},
        {target: null},
        {target: TOP_LEVEL_CONTEXT_1},
        {},
        null,
      ]) {
        assert.throws(
          () => aclStorage.assertCommandAllowed('script.evaluate', params),
          UnsupportedOperationException,
          "Command 'script.evaluate' is not allowed by the session ACL.",
        );
      }
    });

    it('requires a browsing context target with a non-empty sandbox for `script.evaluate`', () => {
      const sandboxRequiredMessage =
        "Command 'script.evaluate' in restricted sessions requires a browsing context target with a non-empty 'sandbox'.";
      aclStorage.setEnabled(true);
      for (const acl of [
        {default: {commands: ['script.evaluate']}},
        {
          perNavigable: {
            [TOP_LEVEL_CONTEXT_1]: {commands: ['script.evaluate']},
          },
        },
      ]) {
        aclStorage.setAcl(acl);

        assert.doesNotThrow(() =>
          aclStorage.assertCommandAllowed('script.evaluate', {
            target: {context: TOP_LEVEL_CONTEXT_1, sandbox: SANDBOX},
          }),
        );
        // `CommandProcessor` strips `realm` when `context` is present.
        assert.doesNotThrow(() =>
          aclStorage.assertCommandAllowed('script.evaluate', {
            target: {
              context: TOP_LEVEL_CONTEXT_1,
              sandbox: SANDBOX,
              realm: 'ignored-realm',
            },
          }),
        );

        for (const params of [
          // Omitted or empty `sandbox` runs in the page's main world.
          {target: {context: TOP_LEVEL_CONTEXT_1}},
          {target: {context: TOP_LEVEL_CONTEXT_1, sandbox: undefined}},
          {target: {context: TOP_LEVEL_CONTEXT_1, sandbox: ''}},
          {target: {context: TOP_LEVEL_CONTEXT_1, sandbox: 42}},
        ]) {
          assert.throws(
            () => aclStorage.assertCommandAllowed('script.evaluate', params),
            UnsupportedOperationException,
            sandboxRequiredMessage,
          );
        }
      }

      // With a `default` grant, `realm` and malformed targets pass the ACL
      // check and must still be rejected by the sandbox check.
      aclStorage.setAcl({default: {commands: ['script.evaluate']}});
      for (const params of [
        // `realm` targets can address the page's main world or workers.
        {target: {realm: 'window-realm'}},
        {target: {realm: 'window-realm', sandbox: SANDBOX}},
        {target: {context: 42, sandbox: SANDBOX}},
        {target: {}},
        {target: null},
        {},
        null,
      ]) {
        assert.throws(
          () => aclStorage.assertCommandAllowed('script.evaluate', params),
          UnsupportedOperationException,
          sandboxRequiredMessage,
        );
      }
    });
  });
});
