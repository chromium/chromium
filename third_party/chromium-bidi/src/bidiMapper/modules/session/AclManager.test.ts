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
import sinon from 'sinon';

import {
  type GoogSession,
  UnsupportedOperationException,
} from '../../../protocol/protocol.js';
import {Deferred} from '../../../utils/Deferred.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

import {
  ACL_EXEMPT_COMMANDS,
  AclManager,
  SUPPORTED_ACL_COMMANDS,
  type UpdateAclHandler,
} from './AclManager.js';

const TOP_LEVEL_CONTEXT_1 = 'context-1';
const TOP_LEVEL_CONTEXT_2 = 'context-2';
const CHILD_CONTEXT = 'child-context';
const GRANDCHILD_CONTEXT = 'grandchild-context';
const CHILD_CONTEXT_2 = 'child-context-2';
const UNKNOWN_CONTEXT = 'unknown-context';
const SANDBOX = 'agent-sandbox';

describe('AclManager', () => {
  let browsingContextStorage: BrowsingContextStorage;
  let aclManager: AclManager;

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

    aclManager = new AclManager(browsingContextStorage);
  });

  it('always allows commands when disabled', () => {
    // Disabled by default, even if an empty ACL is configured.
    aclManager.setAcl({});
    assert.isTrue(aclManager.isCommandAllowed('browsingContext.getTree'));
    assert.isTrue(
      aclManager.isCommandAllowed(
        'browsingContext.navigate',
        TOP_LEVEL_CONTEXT_1,
      ),
    );

    // Enabling enforces the configured ACL; disabling allows commands again.
    aclManager.setEnabled(true);
    assert.isFalse(aclManager.isCommandAllowed('browsingContext.getTree'));

    aclManager.setEnabled(false);
    assert.isTrue(aclManager.isCommandAllowed('browsingContext.getTree'));
  });

  it('disallows unlisted commands when enabled and set to an empty object', () => {
    aclManager.setEnabled(true);
    const saved = aclManager.setAcl({});
    assert.deepEqual(saved, {
      default: {commands: []},
      perNavigable: {},
    });
    assert.deepEqual(aclManager.getAcl(), saved);
    assert.isFalse(aclManager.isCommandAllowed('browsingContext.getTree'));
    assert.isFalse(
      aclManager.isCommandAllowed(
        'browsingContext.navigate',
        TOP_LEVEL_CONTEXT_1,
      ),
    );
  });

  it('allows default commands across all navigables', () => {
    aclManager.setEnabled(true);
    // Commands in `default.commands` are allowed both globally and for any
    // specific top-level navigable.
    const saved = aclManager.setAcl({
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
    assert.deepEqual(aclManager.getAcl(), saved);

    // Allowed globally and on a specific navigable.
    assert.isTrue(aclManager.isCommandAllowed('browsingContext.getTree'));
    assert.isTrue(
      aclManager.isCommandAllowed(
        'browsingContext.getTree',
        TOP_LEVEL_CONTEXT_1,
      ),
    );

    // Commands not in `default.commands` remain disallowed.
    assert.isFalse(
      aclManager.isCommandAllowed(
        'browsingContext.navigate',
        TOP_LEVEL_CONTEXT_1,
      ),
    );
  });

  it('allows union of default and perNavigable commands and returns full ACL', () => {
    aclManager.setEnabled(true);
    const saved = aclManager.setAcl({
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
    assert.deepEqual(aclManager.getAcl(), saved);

    // Global scope only allows `default.commands`.
    assert.isTrue(aclManager.isCommandAllowed('browsingContext.getTree'));
    assert.isFalse(aclManager.isCommandAllowed('browsingContext.navigate'));

    // TOP_LEVEL_CONTEXT_1 allows `default.commands` ∪ `perNavigable[TOP_LEVEL_CONTEXT_1].commands`.
    assert.isTrue(
      aclManager.isCommandAllowed(
        'browsingContext.getTree',
        TOP_LEVEL_CONTEXT_1,
      ),
    );
    assert.isTrue(
      aclManager.isCommandAllowed(
        'browsingContext.navigate',
        TOP_LEVEL_CONTEXT_1,
      ),
    );

    // TOP_LEVEL_CONTEXT_2 has no `perNavigable` entry, so only `default.commands` are allowed.
    assert.isTrue(
      aclManager.isCommandAllowed(
        'browsingContext.getTree',
        TOP_LEVEL_CONTEXT_2,
      ),
    );
    assert.isFalse(
      aclManager.isCommandAllowed(
        'browsingContext.navigate',
        TOP_LEVEL_CONTEXT_2,
      ),
    );
  });

  it('respects parent top-level navigable ACL for child and nested navigables', () => {
    aclManager.setEnabled(true);
    aclManager.setAcl({
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
      aclManager.isCommandAllowed('browsingContext.navigate', CHILD_CONTEXT),
    );
    assert.isTrue(
      aclManager.isCommandAllowed(
        'browsingContext.navigate',
        GRANDCHILD_CONTEXT,
      ),
    );
    assert.isFalse(
      aclManager.isCommandAllowed('input.performActions', CHILD_CONTEXT),
    );

    // Child of TOP_LEVEL_CONTEXT_2 only gets `default.commands`.
    assert.isTrue(
      aclManager.isCommandAllowed('browsingContext.getTree', CHILD_CONTEXT_2),
    );
    assert.isFalse(
      aclManager.isCommandAllowed('browsingContext.navigate', CHILD_CONTEXT_2),
    );
  });

  it('ignores unsupported commands and non-top-level or unknown navigables', () => {
    aclManager.setEnabled(true);
    const saved = aclManager.setAcl({
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
    assert.deepEqual(aclManager.getAcl(), saved);

    // Ignored commands and navigables are not allowed, while parent ACL still applies to child.
    assert.isFalse(aclManager.isCommandAllowed('network.addIntercept'));
    assert.isFalse(
      aclManager.isCommandAllowed('storage.getCookies', TOP_LEVEL_CONTEXT_1),
    );
    assert.isFalse(
      aclManager.isCommandAllowed('input.performActions', CHILD_CONTEXT),
    );
    assert.isTrue(
      aclManager.isCommandAllowed('browsingContext.navigate', CHILD_CONTEXT),
    );
    assert.isFalse(
      aclManager.isCommandAllowed('browsingContext.navigate', UNKNOWN_CONTEXT),
    );
  });

  it('replaces previous ACL on reconfiguration', () => {
    aclManager.setEnabled(true);
    aclManager.setAcl({
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
    const updated = aclManager.setAcl({
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
    assert.isFalse(aclManager.isCommandAllowed('browsingContext.getTree'));
    assert.isFalse(
      aclManager.isCommandAllowed(
        'browsingContext.navigate',
        TOP_LEVEL_CONTEXT_1,
      ),
    );
    assert.isTrue(
      aclManager.isCommandAllowed('input.performActions', TOP_LEVEL_CONTEXT_2),
    );
  });

  it('prunes closed top-level navigables from getAcl and isCommandAllowed', () => {
    aclManager.setEnabled(true);
    aclManager.setAcl({
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
      aclManager.isCommandAllowed(
        'browsingContext.navigate',
        TOP_LEVEL_CONTEXT_1,
      ),
    );
    assert.isFalse(
      aclManager.isCommandAllowed('browsingContext.navigate', CHILD_CONTEXT),
    );
    assert.deepEqual(aclManager.getAcl(), {
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
    aclManager.setEnabled(true);
    const allSupportedCommands = [...SUPPORTED_ACL_COMMANDS];
    const saved = aclManager.setAcl({
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
      assert.isTrue(aclManager.isCommandAllowed(command));
    }
  });

  it('interprets invalid ACL data as no access', () => {
    aclManager.setEnabled(true);
    for (const invalidAcl of [
      undefined,
      null,
      false,
      123,
      'invalid',
      ['array'],
    ]) {
      const result = aclManager.setAcl(invalidAcl);
      const expected = {
        default: {commands: []},
        perNavigable: {},
      };
      assert.deepEqual(result, expected);
      assert.deepEqual(aclManager.getAcl(), expected);
      assert.isFalse(aclManager.isCommandAllowed('browsingContext.getTree'));
    }

    const saved = aclManager.setAcl({
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
    assert.isFalse(aclManager.isCommandAllowed('browsingContext.getTree'));
    assert.isFalse(
      aclManager.isCommandAllowed(
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
        assert.doesNotThrow(() => aclManager.assertCommandAllowed(command, {}));
      }
      assert.doesNotThrow(() =>
        aclManager.assertCommandAllowed('script.evaluate', {
          target: {realm: 'any-realm'},
        }),
      );
    });

    it('always allows exempt commands', () => {
      aclManager.setEnabled(true);
      aclManager.setAcl({});

      const expectedExemptCommands = [
        'goog:session.setAcl',
        'session.end',
        'session.new',
        'session.status',
      ];
      assert.sameMembers([...ACL_EXEMPT_COMMANDS], expectedExemptCommands);
      for (const command of expectedExemptCommands) {
        assert.doesNotThrow(() => aclManager.assertCommandAllowed(command, {}));
      }
    });

    it('throws unsupported operation for commands that an ACL cannot grant', () => {
      aclManager.setEnabled(true);
      // Requesting such commands doesn't grant them.
      aclManager.setAcl({
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
            aclManager.assertCommandAllowed(command, {
              context: TOP_LEVEL_CONTEXT_1,
            }),
          UnsupportedOperationException,
          `Command '${command}' is not supported in restricted sessions.`,
        );
      }
    });

    it('throws unsupported operation for commands that the ACL does not grant', () => {
      aclManager.setEnabled(true);
      aclManager.setAcl({default: {commands: ['browsingContext.getTree']}});

      assert.doesNotThrow(() =>
        aclManager.assertCommandAllowed('browsingContext.getTree', {}),
      );
      assert.throws(
        () =>
          aclManager.assertCommandAllowed('browsingContext.create', {
            type: 'tab',
          }),
        UnsupportedOperationException,
        "Command 'browsingContext.create' is not allowed by the session ACL.",
      );
      assert.throws(
        () =>
          aclManager.assertCommandAllowed('browsingContext.navigate', {
            context: TOP_LEVEL_CONTEXT_1,
            url: 'about:blank',
          }),
        UnsupportedOperationException,
        `Command 'browsingContext.navigate' is not allowed by the session ACL for browsing context '${TOP_LEVEL_CONTEXT_1}'.`,
      );
      assert.throws(
        () =>
          aclManager.assertCommandAllowed('script.evaluate', {
            target: {context: TOP_LEVEL_CONTEXT_1},
          }),
        UnsupportedOperationException,
        `Command 'script.evaluate' is not allowed by the session ACL for browsing context '${TOP_LEVEL_CONTEXT_1}'.`,
      );
    });

    it('allows default commands for any browsing context', () => {
      aclManager.setEnabled(true);
      aclManager.setAcl({
        default: {commands: ['browsingContext.navigate', 'script.evaluate']},
      });

      for (const params of [
        {},
        {context: CHILD_CONTEXT_2},
        {context: UNKNOWN_CONTEXT},
      ]) {
        assert.doesNotThrow(() =>
          aclManager.assertCommandAllowed('browsingContext.navigate', params),
        );
      }
      for (const context of [CHILD_CONTEXT_2, UNKNOWN_CONTEXT]) {
        assert.doesNotThrow(() =>
          aclManager.assertCommandAllowed('script.evaluate', {
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
      aclManager.setEnabled(true);
      aclManager.setAcl({
        perNavigable: {
          [TOP_LEVEL_CONTEXT_1]: {
            commands: NO_TARGET_COMMANDS,
          },
        },
      });

      assert.throws(
        () =>
          aclManager.assertCommandAllowed('browsingContext.create', {
            type: 'tab',
            referenceContext: TOP_LEVEL_CONTEXT_1,
          }),
        UnsupportedOperationException,
        "Command 'browsingContext.create' is not allowed by the session ACL.",
      );
      assert.throws(
        () =>
          aclManager.assertCommandAllowed('browsingContext.getTree', {
            root: TOP_LEVEL_CONTEXT_1,
          }),
        UnsupportedOperationException,
        "Command 'browsingContext.getTree' is not allowed by the session ACL.",
      );
      // Unlike most commands, these ignore `context`.
      for (const command of NO_TARGET_COMMANDS) {
        assert.throws(
          () =>
            aclManager.assertCommandAllowed(command, {
              context: TOP_LEVEL_CONTEXT_1,
            }),
          UnsupportedOperationException,
          `Command '${command}' is not allowed by the session ACL.`,
        );
      }
    });

    it('checks `context` against the ACL of its top-level navigable', () => {
      aclManager.setEnabled(true);
      for (const command of CONTEXT_TARGET_COMMANDS) {
        aclManager.setAcl({
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
            aclManager.assertCommandAllowed(command, {
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
            () => aclManager.assertCommandAllowed(command, {context}),
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
            () => aclManager.assertCommandAllowed(command, params),
            UnsupportedOperationException,
            `Command '${command}' is not allowed by the session ACL.`,
          );
        }
      }
    });

    it('checks `target.context` against the ACL of its top-level navigable', () => {
      aclManager.setEnabled(true);
      aclManager.setAcl({
        perNavigable: {[TOP_LEVEL_CONTEXT_1]: {commands: ['script.evaluate']}},
      });

      for (const context of [
        TOP_LEVEL_CONTEXT_1,
        CHILD_CONTEXT,
        GRANDCHILD_CONTEXT,
      ]) {
        assert.doesNotThrow(() =>
          aclManager.assertCommandAllowed('script.evaluate', {
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
          () => aclManager.assertCommandAllowed('script.evaluate', params),
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
          () => aclManager.assertCommandAllowed('script.evaluate', params),
          UnsupportedOperationException,
          "Command 'script.evaluate' is not allowed by the session ACL.",
        );
      }
    });

    it('requires a browsing context target with a non-empty sandbox for `script.evaluate`', () => {
      const sandboxRequiredMessage =
        "Command 'script.evaluate' in restricted sessions requires a browsing context target with a non-empty 'sandbox'.";
      aclManager.setEnabled(true);
      for (const acl of [
        {default: {commands: ['script.evaluate']}},
        {
          perNavigable: {
            [TOP_LEVEL_CONTEXT_1]: {commands: ['script.evaluate']},
          },
        },
      ]) {
        aclManager.setAcl(acl);

        assert.doesNotThrow(() =>
          aclManager.assertCommandAllowed('script.evaluate', {
            target: {context: TOP_LEVEL_CONTEXT_1, sandbox: SANDBOX},
          }),
        );
        // `CommandProcessor` strips `realm` when `context` is present.
        assert.doesNotThrow(() =>
          aclManager.assertCommandAllowed('script.evaluate', {
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
            () => aclManager.assertCommandAllowed('script.evaluate', params),
            UnsupportedOperationException,
            sandboxRequiredMessage,
          );
        }
      }

      // With a `default` grant, `realm` and malformed targets pass the ACL
      // check and must still be rejected by the sandbox check.
      aclManager.setAcl({default: {commands: ['script.evaluate']}});
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
          () => aclManager.assertCommandAllowed('script.evaluate', params),
          UnsupportedOperationException,
          sandboxRequiredMessage,
        );
      }
    });
  });

  describe('updateAcl', () => {
    const GET_TREE_ACL = {
      default: {commands: ['browsingContext.getTree']},
    };
    const NAVIGATE_ACL = {
      default: {commands: ['browsingContext.getTree']},
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {commands: ['browsingContext.navigate']},
      },
    };
    const REASON = 'Fill in the checkout form.';

    let updateAclHandler: sinon.SinonStub<
      Parameters<UpdateAclHandler>,
      ReturnType<UpdateAclHandler>
    >;

    beforeEach(() => {
      // Allows everything, like the allow-all handler.
      updateAclHandler = sinon
        .stub<Parameters<UpdateAclHandler>, ReturnType<UpdateAclHandler>>()
        .callsFake((acl) => Promise.resolve({acl}));
      aclManager = new AclManager(browsingContextStorage, updateAclHandler);
    });

    it('stores the requested ACL as is without a custom handler', async () => {
      aclManager = new AclManager(browsingContextStorage);

      const result = await aclManager.updateAcl(NAVIGATE_ACL);

      // Nothing is refused, so there is no `denied`.
      assert.deepEqual(result, {acl: NAVIGATE_ACL});
    });

    it('passes the normalized ACL and the reason to the handler', async () => {
      const result = await aclManager.updateAcl(
        {
          default: {
            commands: [
              'browsingContext.getTree',
              // Unsupported commands grant nothing.
              'network.addIntercept',
            ],
          },
          perNavigable: {
            [TOP_LEVEL_CONTEXT_1]: {commands: ['browsingContext.navigate']},
            // Unknown or non-top-level navigables grant nothing.
            [CHILD_CONTEXT]: {commands: ['input.performActions']},
            [UNKNOWN_CONTEXT]: {commands: ['input.performActions']},
          },
        },
        REASON,
      );

      sinon.assert.calledOnceWithExactly(
        updateAclHandler,
        NAVIGATE_ACL,
        REASON,
      );
      assert.deepEqual(result, {acl: NAVIGATE_ACL});
      assert.deepEqual(aclManager.getAcl(), NAVIGATE_ACL);
      assert.isTrue(
        aclManager.isCommandAllowed(
          'browsingContext.navigate',
          TOP_LEVEL_CONTEXT_1,
        ),
      );
    });

    it('switches an unrestricted session to restricted mode', async () => {
      assert.isTrue(aclManager.isCommandAllowed('network.addIntercept'));

      const result = await aclManager.updateAcl(NAVIGATE_ACL);

      assert.deepEqual(result, {acl: NAVIGATE_ACL});
      assert.isTrue(aclManager.isEnabled());
      assert.isFalse(aclManager.isCommandAllowed('network.addIntercept'));
    });

    it('replaces the previous ACL', async () => {
      await aclManager.updateAcl(NAVIGATE_ACL);
      updateAclHandler.resetHistory();

      const result = await aclManager.updateAcl(GET_TREE_ACL);

      const getTreeAcl = {...GET_TREE_ACL, perNavigable: {}};
      sinon.assert.calledOnceWithExactly(
        updateAclHandler,
        getTreeAcl,
        undefined,
      );
      assert.deepEqual(result, {acl: getTreeAcl});
      assert.isFalse(
        aclManager.isCommandAllowed(
          'browsingContext.navigate',
          TOP_LEVEL_CONTEXT_1,
        ),
      );
    });

    it('is idempotent', async () => {
      const first = await aclManager.updateAcl(NAVIGATE_ACL);
      const second = await aclManager.updateAcl(NAVIGATE_ACL);

      // The handler gets the full desired ACL every time, not a diff.
      sinon.assert.calledTwice(updateAclHandler);
      sinon.assert.alwaysCalledWithExactly(
        updateAclHandler,
        NAVIGATE_ACL,
        undefined,
      );
      assert.deepEqual(first, {acl: NAVIGATE_ACL});
      assert.deepEqual(second, {acl: NAVIGATE_ACL});
    });

    it('changes the ACL only once the handler decides', async () => {
      await aclManager.updateAcl(NAVIGATE_ACL);
      const decision = new Deferred<GoogSession.SetAclResult>();
      updateAclHandler.returns(decision);

      const pending = aclManager.updateAcl(GET_TREE_ACL);
      // Let the request run as far as it can.
      await new Promise((resolve) => setTimeout(resolve, 0));

      assert.deepEqual(aclManager.getAcl(), NAVIGATE_ACL);
      decision.resolve({acl: GET_TREE_ACL});
      await pending;
      assert.deepEqual(aclManager.getAcl(), {
        ...GET_TREE_ACL,
        perNavigable: {},
      });
    });

    it('stores the ACL the handler allows, normalized', async () => {
      await aclManager.updateAcl(NAVIGATE_ACL);
      updateAclHandler.resolves({
        acl: {
          default: {
            commands: [
              'browsingContext.getTree',
              'browsingContext.close',
              // Unsupported commands are dropped.
              'network.addIntercept',
            ],
          },
          // Unknown navigables are dropped.
          perNavigable: {
            [UNKNOWN_CONTEXT]: {commands: ['input.performActions']},
          },
        },
      });

      const result = await aclManager.updateAcl(NAVIGATE_ACL);

      // The handler's ACL replaces the session ACL as is.
      assert.deepEqual(result, {
        acl: {
          default: {
            commands: ['browsingContext.getTree', 'browsingContext.close'],
          },
          perNavigable: {},
        },
      });
      assert.deepEqual(aclManager.getAcl(), result.acl);
    });

    it('returns what the handler refused', async () => {
      const denied = {
        acl: {
          perNavigable: {
            [TOP_LEVEL_CONTEXT_1]: {commands: ['browsingContext.navigate']},
          },
        },
        reason: 'The user did not allow navigation.',
      };
      updateAclHandler.resolves({acl: GET_TREE_ACL, denied});

      const result = await aclManager.updateAcl(NAVIGATE_ACL, REASON);

      assert.deepEqual(result, {
        acl: {...GET_TREE_ACL, perNavigable: {}},
        denied,
      });
    });

    it('fails without changing the ACL if the handler rejects', async () => {
      await aclManager.updateAcl(NAVIGATE_ACL);
      const error = new Error('handler failed');
      updateAclHandler.rejects(error);

      try {
        await aclManager.updateAcl(GET_TREE_ACL);
        assert.fail('Should have thrown');
      } catch (err) {
        assert.strictEqual(err, error);
      }

      assert.deepEqual(aclManager.getAcl(), NAVIGATE_ACL);
      // The next request is processed instead of waiting forever.
      updateAclHandler.callsFake((acl) => Promise.resolve({acl}));
      assert.deepEqual(await aclManager.updateAcl(GET_TREE_ACL), {
        acl: {...GET_TREE_ACL, perNavigable: {}},
      });
    });

    it('processes requests one at a time, in order', async () => {
      await aclManager.updateAcl(GET_TREE_ACL);
      updateAclHandler.resetHistory();
      const decision = new Deferred<GoogSession.SetAclResult>();
      updateAclHandler.onFirstCall().returns(decision);
      const first = aclManager.updateAcl(NAVIGATE_ACL);
      const second = aclManager.updateAcl({});
      // Let both requests run as far as they can.
      await new Promise((resolve) => setTimeout(resolve, 0));

      // The second request waits for the first one.
      sinon.assert.calledOnceWithExactly(
        updateAclHandler,
        NAVIGATE_ACL,
        undefined,
      );
      decision.resolve({acl: NAVIGATE_ACL});

      assert.deepEqual(await first, {acl: NAVIGATE_ACL});
      const emptyAcl = {default: {commands: []}, perNavigable: {}};
      assert.deepEqual(await second, {acl: emptyAcl});
      sinon.assert.calledTwice(updateAclHandler);
      sinon.assert.calledWithExactly(
        updateAclHandler.secondCall,
        emptyAcl,
        undefined,
      );
      assert.deepEqual(aclManager.getAcl(), emptyAcl);
    });
  });
});
