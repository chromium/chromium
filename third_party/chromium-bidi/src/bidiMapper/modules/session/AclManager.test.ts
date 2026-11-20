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
  ChromiumBidi,
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
  SUPPORTED_ACL_EVENTS,
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
    // Disabled by default, even though the ACL is empty.
    assert.isTrue(aclManager.isCommandAllowed('browsingContext.getTree'));
    assert.isTrue(
      aclManager.isCommandAllowed(
        'browsingContext.navigate',
        TOP_LEVEL_CONTEXT_1,
      ),
    );

    // Enabling enforces the configured ACL.
    aclManager.enable();
    assert.isTrue(aclManager.isEnabled());
    assert.isFalse(aclManager.isCommandAllowed('browsingContext.getTree'));
  });

  it('disallows unlisted commands when enabled and set to an empty object', async () => {
    aclManager.enable();
    const {acl: saved} = await aclManager.updateAcl({});
    assert.deepEqual(saved, {
      default: {commands: [], events: []},
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

  it('allows default commands across all navigables', async () => {
    aclManager.enable();
    // Commands in `default.commands` are allowed both globally and for any
    // specific top-level navigable.
    const {acl: saved} = await aclManager.updateAcl({
      default: {
        commands: ['browsingContext.getTree', 'browsingContext.create'],
      },
    });

    assert.deepEqual(saved, {
      default: {
        commands: ['browsingContext.getTree', 'browsingContext.create'],
        events: [],
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

  it('allows union of default and perNavigable commands and returns full ACL', async () => {
    aclManager.enable();
    const {acl: saved} = await aclManager.updateAcl({
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
        events: [],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: ['browsingContext.navigate', 'input.performActions'],
          events: [],
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

  it('respects parent top-level navigable ACL for child and nested navigables', async () => {
    aclManager.enable();
    await aclManager.updateAcl({
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

  it('ignores unsupported commands and non-top-level or unknown navigables', async () => {
    aclManager.enable();
    const {acl: saved} = await aclManager.updateAcl({
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
        events: [],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: ['browsingContext.navigate'],
          events: [],
        },
        [TOP_LEVEL_CONTEXT_2]: {
          commands: [],
          events: [],
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

  it('replaces previous ACL on reconfiguration', async () => {
    aclManager.enable();
    await aclManager.updateAcl({
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
    const {acl: updated} = await aclManager.updateAcl({
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
        events: [],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_2]: {
          commands: ['input.performActions'],
          events: [],
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

  it('prunes closed top-level navigables from getAcl and isCommandAllowed', async () => {
    aclManager.enable();
    await aclManager.updateAcl({
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
        events: [],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_2]: {
          commands: ['input.performActions'],
          events: [],
        },
      },
    });
  });

  it('accepts all supported ACL commands in default and perNavigable', async () => {
    aclManager.enable();
    const allSupportedCommands = [...SUPPORTED_ACL_COMMANDS];
    const {acl: saved} = await aclManager.updateAcl({
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
        events: [],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: allSupportedCommands,
          events: [],
        },
      },
    });
    for (const command of SUPPORTED_ACL_COMMANDS) {
      assert.isTrue(aclManager.isCommandAllowed(command));
    }
  });

  it('interprets invalid ACL data as no access', async () => {
    aclManager.enable();
    for (const invalidAcl of [
      undefined,
      null,
      false,
      123,
      'invalid',
      ['array'],
    ]) {
      const {acl: result} = await aclManager.updateAcl(invalidAcl);
      const expected = {
        default: {commands: [], events: []},
        perNavigable: {},
      };
      assert.deepEqual(result, expected);
      assert.deepEqual(aclManager.getAcl(), expected);
      assert.isFalse(aclManager.isCommandAllowed('browsingContext.getTree'));
    }

    const {acl: saved} = await aclManager.updateAcl({
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
        events: [],
      },
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: [],
          events: [],
        },
        [TOP_LEVEL_CONTEXT_2]: {
          commands: [],
          events: [],
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

    it('always allows exempt commands', async () => {
      aclManager.enable();
      await aclManager.updateAcl({});

      const expectedExemptCommands = [
        'goog:session.setAcl',
        'session.end',
        'session.new',
        'session.status',
        'session.subscribe',
        'session.unsubscribe',
      ];
      assert.sameMembers([...ACL_EXEMPT_COMMANDS], expectedExemptCommands);
      for (const command of expectedExemptCommands) {
        assert.doesNotThrow(() => aclManager.assertCommandAllowed(command, {}));
      }
    });

    it('throws unsupported operation for commands that an ACL cannot grant', async () => {
      aclManager.enable();
      // Requesting such commands doesn't grant them.
      await aclManager.updateAcl({
        default: {commands: ['network.addIntercept', 'goog:cdp.sendCommand']},
      });

      for (const command of [
        'network.addIntercept',
        'goog:cdp.sendCommand',
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

    it('throws unsupported operation for commands that the ACL does not grant', async () => {
      aclManager.enable();
      await aclManager.updateAcl({
        default: {commands: ['browsingContext.getTree']},
      });

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

    it('allows default commands for any browsing context', async () => {
      aclManager.enable();
      await aclManager.updateAcl({
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
      'browsingContext.traverseHistory',
      'input.performActions',
      'input.releaseActions',
    ];

    it('covers every supported ACL command with a target rule test', () => {
      assert.sameMembers(
        [...SUPPORTED_ACL_COMMANDS],
        [
          ...NO_TARGET_COMMANDS,
          ...CONTEXT_TARGET_COMMANDS,
          'script.callFunction',
          'script.evaluate',
        ],
      );
    });

    it('checks commands without a target tab against the default ACL only', async () => {
      aclManager.enable();
      await aclManager.updateAcl({
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

    it('checks `context` against the ACL of its top-level navigable', async () => {
      aclManager.enable();
      for (const command of CONTEXT_TARGET_COMMANDS) {
        await aclManager.updateAcl({
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

    for (const command of ['script.callFunction', 'script.evaluate'] as const) {
      it(`checks \`target.context\` against the ACL of its top-level navigable for \`${command}\``, async () => {
        aclManager.enable();
        await aclManager.updateAcl({
          perNavigable: {[TOP_LEVEL_CONTEXT_1]: {commands: [command]}},
        });

        for (const context of [
          TOP_LEVEL_CONTEXT_1,
          CHILD_CONTEXT,
          GRANDCHILD_CONTEXT,
        ]) {
          assert.doesNotThrow(() =>
            aclManager.assertCommandAllowed(command, {
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
            () => aclManager.assertCommandAllowed(command, params),
            UnsupportedOperationException,
            `Command '${command}' is not allowed by the session ACL for browsing context '${CHILD_CONTEXT_2}'.`,
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
            () => aclManager.assertCommandAllowed(command, params),
            UnsupportedOperationException,
            `Command '${command}' is not allowed by the session ACL.`,
          );
        }
      });

      it(`requires a browsing context target with a non-empty sandbox for \`${command}\``, async () => {
        const sandboxRequiredMessage =
          "Restricted sessions require a browsing context target with a non-empty 'sandbox'.";
        aclManager.enable();
        for (const acl of [
          {default: {commands: [command]}},
          {
            perNavigable: {
              [TOP_LEVEL_CONTEXT_1]: {commands: [command]},
            },
          },
        ]) {
          await aclManager.updateAcl(acl);

          assert.doesNotThrow(() =>
            aclManager.assertCommandAllowed(command, {
              target: {context: TOP_LEVEL_CONTEXT_1, sandbox: SANDBOX},
            }),
          );
          // `CommandProcessor` strips `realm` when `context` is present.
          assert.doesNotThrow(() =>
            aclManager.assertCommandAllowed(command, {
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
              () => aclManager.assertCommandAllowed(command, params),
              UnsupportedOperationException,
              sandboxRequiredMessage,
            );
          }
        }

        // With a `default` grant, `realm` and malformed targets pass the ACL
        // check and must still be rejected by the sandbox check.
        await aclManager.updateAcl({default: {commands: [command]}});
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
            () => aclManager.assertCommandAllowed(command, params),
            UnsupportedOperationException,
            sandboxRequiredMessage,
          );
        }
      });
    }
  });

  describe('isEventAllowed', () => {
    const NAVIGATION_COMMITTED =
      ChromiumBidi.BrowsingContext.EventNames.NavigationCommitted;
    const LOG_ENTRY_ADDED = ChromiumBidi.Log.EventNames.LogEntryAdded;

    it('allows any event when disabled', () => {
      // Disabled by default, even though the ACL is empty.
      assert.isTrue(
        aclManager.isEventAllowed(
          ChromiumBidi.BrowsingContext.EventNames.ContextCreated,
          TOP_LEVEL_CONTEXT_1,
        ),
      );
      assert.isTrue(
        aclManager.isEventAllowed(
          ChromiumBidi.Network.EventNames.BeforeRequestSent,
          null,
        ),
      );
    });

    it('supports only the reviewed events', () => {
      // Spelled out, so adding an event to the ACL needs a test update.
      assert.sameMembers(
        [...SUPPORTED_ACL_EVENTS],
        [
          ChromiumBidi.BrowsingContext.EventNames.ContextCreated,
          ChromiumBidi.BrowsingContext.EventNames.ContextDestroyed,
          ChromiumBidi.BrowsingContext.EventNames.FragmentNavigated,
          ChromiumBidi.BrowsingContext.EventNames.HistoryUpdated,
          ChromiumBidi.BrowsingContext.EventNames.NavigationCommitted,
          ChromiumBidi.BrowsingContext.EventNames.UserPromptClosed,
          ChromiumBidi.BrowsingContext.EventNames.UserPromptOpened,
          ChromiumBidi.Log.EventNames.LogEntryAdded,
        ],
      );
    });

    it('allows default events for every navigable and without one', async () => {
      aclManager.enable();
      await aclManager.updateAcl({default: {events: [NAVIGATION_COMMITTED]}});

      for (const contextId of [TOP_LEVEL_CONTEXT_1, CHILD_CONTEXT, null]) {
        assert.isTrue(
          aclManager.isEventAllowed(NAVIGATION_COMMITTED, contextId),
        );
        assert.isFalse(aclManager.isEventAllowed(LOG_ENTRY_ADDED, contextId));
      }
    });

    it('allows perNavigable events only for that navigable and its children', async () => {
      aclManager.enable();
      await aclManager.updateAcl({
        perNavigable: {[TOP_LEVEL_CONTEXT_1]: {events: [LOG_ENTRY_ADDED]}},
      });

      assert.isTrue(
        aclManager.isEventAllowed(LOG_ENTRY_ADDED, TOP_LEVEL_CONTEXT_1),
      );
      assert.isTrue(
        aclManager.isEventAllowed(LOG_ENTRY_ADDED, GRANDCHILD_CONTEXT),
      );
      assert.isFalse(
        aclManager.isEventAllowed(LOG_ENTRY_ADDED, TOP_LEVEL_CONTEXT_2),
      );
      assert.isFalse(aclManager.isEventAllowed(LOG_ENTRY_ADDED, null));
      assert.isFalse(
        aclManager.isEventAllowed(LOG_ENTRY_ADDED, UNKNOWN_CONTEXT),
      );
      assert.deepEqual(aclManager.getAcl(), {
        default: {commands: [], events: []},
        perNavigable: {
          [TOP_LEVEL_CONTEXT_1]: {commands: [], events: [LOG_ENTRY_ADDED]},
        },
      });
    });

    it('revokes events that a new ACL no longer contains', async () => {
      aclManager.enable();
      await aclManager.updateAcl({
        default: {events: [NAVIGATION_COMMITTED]},
        perNavigable: {[TOP_LEVEL_CONTEXT_1]: {events: [LOG_ENTRY_ADDED]}},
      });
      assert.isTrue(aclManager.isEventAllowed(NAVIGATION_COMMITTED, null));
      assert.isTrue(
        aclManager.isEventAllowed(LOG_ENTRY_ADDED, TOP_LEVEL_CONTEXT_1),
      );

      await aclManager.updateAcl({perNavigable: {[TOP_LEVEL_CONTEXT_2]: {}}});

      assert.isFalse(aclManager.isEventAllowed(NAVIGATION_COMMITTED, null));
      assert.isFalse(
        aclManager.isEventAllowed(LOG_ENTRY_ADDED, TOP_LEVEL_CONTEXT_1),
      );
    });

    it('ignores unsupported events', async () => {
      aclManager.enable();
      const {acl: saved} = await aclManager.updateAcl({
        default: {
          events: [
            NAVIGATION_COMMITTED,
            ChromiumBidi.Network.EventNames.BeforeRequestSent,
            'goog:cdp.Page.frameNavigated',
          ],
        },
      });

      assert.deepEqual(saved.default?.events, [NAVIGATION_COMMITTED]);
      assert.isFalse(
        aclManager.isEventAllowed(
          ChromiumBidi.Network.EventNames.BeforeRequestSent,
          TOP_LEVEL_CONTEXT_1,
        ),
      );
      assert.isFalse(
        aclManager.isEventAllowed(
          'goog:cdp.Page.frameNavigated',
          TOP_LEVEL_CONTEXT_1,
        ),
      );
    });
  });

  describe('updateAcl', () => {
    const GET_TREE_ACL = {
      default: {commands: ['browsingContext.getTree'], events: []},
    };
    const NAVIGATE_ACL = {
      default: {commands: ['browsingContext.getTree'], events: []},
      perNavigable: {
        [TOP_LEVEL_CONTEXT_1]: {
          commands: ['browsingContext.navigate'],
          events: [],
        },
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
      // ACL changes happen in restricted sessions, where the ACL is enabled.
      aclManager.enable();
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

    it('does not enable the ACL', async () => {
      aclManager = new AclManager(browsingContextStorage, updateAclHandler);

      await aclManager.updateAcl(NAVIGATE_ACL);

      assert.isFalse(aclManager.isEnabled());
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
            events: [],
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
      const emptyAcl = {
        default: {commands: [], events: []},
        perNavigable: {},
      };
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
