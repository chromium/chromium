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

import type {CdpClient} from '../../../cdp/CdpClient.js';
import {
  InvalidArgumentException,
  Session,
  SessionNotCreatedException,
  UnsupportedOperationException,
} from '../../../protocol/protocol.js';
import {Deferred} from '../../../utils/Deferred.js';
import type {MapperOptions} from '../../MapperOptions.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

import {AclManager} from './AclManager.js';
import type {EventManager} from './EventManager.js';
import {SessionProcessor} from './SessionProcessor.js';

describe('SessionProcessor', () => {
  let eventManager: EventManager;
  let browserCdpClient: sinon.SinonStubbedInstance<CdpClient>;
  let initConnection: sinon.SinonStub<[MapperOptions], Promise<void>>;
  let browsingContextStorage: BrowsingContextStorage;
  let aclManager: AclManager;
  let sessionProcessor: SessionProcessor;

  beforeEach(() => {
    eventManager = {} as EventManager;
    browserCdpClient = {
      sendCommand: sinon.stub().resolves({
        product: 'Chrome/140.0.0.0',
        revision: '12345',
        userAgent: 'Mozilla/5.0',
      }),
    } as unknown as sinon.SinonStubbedInstance<CdpClient>;
    browsingContextStorage = new BrowsingContextStorage();
    initConnection = sinon.stub<[MapperOptions], Promise<void>>().resolves();
    aclManager = new AclManager(browsingContextStorage);
    sessionProcessor = new SessionProcessor(
      eventManager,
      browserCdpClient,
      initConnection,
      aclManager,
    );
  });

  describe('new', () => {
    it('omits goog:sessionType and keeps ACL disabled when not requested', async () => {
      const result = await sessionProcessor.new({
        capabilities: {},
      });

      assert.notProperty(result.capabilities, 'goog:sessionType');
      assert.isFalse(aclManager.isEnabled());
      assert.isTrue(aclManager.isCommandAllowed('network.addIntercept'));
    });

    it('enables empty ACL when goog:sessionType is restricted in alwaysMatch', async () => {
      const result = await sessionProcessor.new({
        capabilities: {
          alwaysMatch: {
            'goog:sessionType': 'restricted',
          },
        },
      });

      assert.strictEqual(result.capabilities['goog:sessionType'], 'restricted');
      assert.isTrue(aclManager.isEnabled());
      assert.deepEqual(aclManager.getAcl(), {
        default: {commands: [], events: []},
        perNavigable: {},
      });
      assert.isFalse(aclManager.isCommandAllowed('browsingContext.getTree'));
    });

    it('enables empty ACL when goog:sessionType is restricted in firstMatch', async () => {
      const result = await sessionProcessor.new({
        capabilities: {
          firstMatch: [
            {
              'goog:sessionType': 'restricted',
            },
          ],
        },
      });

      assert.strictEqual(result.capabilities['goog:sessionType'], 'restricted');
      assert.isTrue(aclManager.isEnabled());
      assert.deepEqual(aclManager.getAcl(), {
        default: {commands: [], events: []},
        perNavigable: {},
      });
      assert.isFalse(aclManager.isCommandAllowed('browsingContext.getTree'));
    });

    it('ignores null goog:sessionType and omits it from capabilities', async () => {
      const result = await sessionProcessor.new({
        capabilities: {
          alwaysMatch: {
            'goog:sessionType': null,
          },
        },
      });

      assert.notProperty(result.capabilities, 'goog:sessionType');
      assert.isFalse(aclManager.isEnabled());
    });

    for (const invalidSessionType of [123, 'unrestricted']) {
      it(`ignores invalid goog:sessionType ${JSON.stringify(invalidSessionType)}`, async () => {
        const result = await sessionProcessor.new({
          capabilities: {
            alwaysMatch: {
              'goog:sessionType': invalidSessionType,
            },
          },
        });

        assert.notProperty(result.capabilities, 'goog:sessionType');
        assert.isFalse(aclManager.isEnabled());
      });
    }

    it('throws InvalidArgumentException if goog:sessionType is in both alwaysMatch and firstMatch', async () => {
      try {
        await sessionProcessor.new({
          capabilities: {
            alwaysMatch: {
              'goog:sessionType': 'restricted',
            },
            firstMatch: [
              {
                'goog:sessionType': 'restricted',
              },
            ],
          },
        });
      } catch (err) {
        assert.instanceOf(err, InvalidArgumentException);
        sinon.assert.notCalled(initConnection);
        assert.isFalse(aclManager.isEnabled());
        return;
      }
      assert.fail('Should have thrown');
    });

    it('allows retrying session.new after invalid capabilities', async () => {
      let error: unknown;
      try {
        await sessionProcessor.new({
          capabilities: {
            alwaysMatch: {browserName: 'chrome'},
            firstMatch: [{browserName: 'chrome'}],
          },
        });
      } catch (err) {
        error = err;
      }
      assert.instanceOf(error, InvalidArgumentException);
      assert.isFalse(sessionProcessor.isCreated());

      await sessionProcessor.new({capabilities: {}});

      assert.isTrue(sessionProcessor.isCreated());
      sinon.assert.calledOnce(initConnection);
    });

    for (const [name, capability] of [
      ['acceptInsecureCerts', {acceptInsecureCerts: true}],
      [
        'unhandledPromptBehavior',
        {
          unhandledPromptBehavior: {
            default: Session.UserPromptHandlerType.Accept,
          },
        },
      ],
      [
        'unhandledPromptBehavior string',
        {
          unhandledPromptBehavior:
            'accept' as unknown as Session.UserPromptHandler,
        },
      ],
      ['goog:prerenderingDisabled', {'goog:prerenderingDisabled': true}],
      [
        'goog:disableNetworkDurableMessages',
        {'goog:disableNetworkDurableMessages': true},
      ],
    ] as const) {
      it(`rejects ${name} in a restricted session`, async () => {
        let error: unknown;
        try {
          await sessionProcessor.new({
            capabilities: {
              alwaysMatch: {'goog:sessionType': 'restricted', ...capability},
            },
          });
        } catch (err) {
          error = err;
        }

        assert.instanceOf(error, SessionNotCreatedException);
        assert.isFalse(sessionProcessor.isCreated());
        sinon.assert.notCalled(initConnection);
        assert.isFalse(aclManager.isEnabled());
      });
    }

    it('allows acceptInsecureCerts false in a restricted session', async () => {
      const result = await sessionProcessor.new({
        capabilities: {
          alwaysMatch: {
            'goog:sessionType': 'restricted',
            acceptInsecureCerts: false,
          },
        },
      });

      assert.strictEqual(result.capabilities['goog:sessionType'], 'restricted');
      assert.isTrue(aclManager.isEnabled());
    });

    it('allows unhandledPromptBehavior null in a restricted session', async () => {
      const result = await sessionProcessor.new({
        capabilities: {
          alwaysMatch: {
            'goog:sessionType': 'restricted',
            unhandledPromptBehavior:
              null as unknown as Session.UserPromptHandler,
          },
        },
      });

      assert.strictEqual(result.capabilities['goog:sessionType'], 'restricted');
      assert.isTrue(aclManager.isEnabled());
      assert.isNull(result.capabilities.unhandledPromptBehavior);
      sinon.assert.calledOnceWithExactly(initConnection, {});
    });

    it('preserves other requested capabilities in a restricted session', async () => {
      const result = await sessionProcessor.new({
        capabilities: {
          alwaysMatch: {
            'goog:sessionType': 'restricted',
            'goog:prerenderingDisabled': false,
            'goog:customCapability': 'value',
            proxy: {proxyType: 'direct'},
          },
        },
      });

      assert.strictEqual(result.capabilities['goog:sessionType'], 'restricted');
      assert.isFalse(result.capabilities.acceptInsecureCerts);
      assert.isFalse(result.capabilities['goog:prerenderingDisabled']);
      assert.strictEqual(result.capabilities['goog:customCapability'], 'value');
      assert.deepEqual(result.capabilities.proxy, {proxyType: 'direct'});
      sinon.assert.calledOnceWithExactly(initConnection, {});
    });

    it('keeps isCreated false while initConnection is pending and rejects duplicate session.new with SessionNotCreatedException', async () => {
      const initDeferred = new Deferred<void>();
      initConnection.returns(initDeferred);

      const firstNew = sessionProcessor.new({capabilities: {}});
      assert.isFalse(sessionProcessor.isCreated());

      let secondError: unknown;
      try {
        await sessionProcessor.new({capabilities: {}});
      } catch (err) {
        secondError = err;
      }
      assert.instanceOf(secondError, SessionNotCreatedException);

      initDeferred.resolve();
      await firstNew;
      assert.isTrue(sessionProcessor.isCreated());

      let duplicateError: unknown;
      try {
        await sessionProcessor.new({capabilities: {}});
      } catch (err) {
        duplicateError = err;
      }
      assert.instanceOf(duplicateError, SessionNotCreatedException);
      sinon.assert.calledOnce(initConnection);
    });

    describe('restrictedMode', () => {
      let restrictedSessionProcessor: SessionProcessor;

      beforeEach(() => {
        restrictedSessionProcessor = new SessionProcessor(
          eventManager,
          browserCdpClient,
          initConnection,
          aclManager,
          true,
        );
      });

      it('throws SessionNotCreatedException when goog:sessionType is omitted', async () => {
        try {
          await restrictedSessionProcessor.new({
            capabilities: {},
          });
        } catch (err) {
          assert.instanceOf(err, SessionNotCreatedException);
          sinon.assert.notCalled(initConnection);
          return;
        }
        assert.fail('Should have thrown');
      });

      it('throws SessionNotCreatedException when goog:sessionType is null', async () => {
        try {
          await restrictedSessionProcessor.new({
            capabilities: {
              alwaysMatch: {
                'goog:sessionType': null,
              },
            },
          });
        } catch (err) {
          assert.instanceOf(err, SessionNotCreatedException);
          sinon.assert.notCalled(initConnection);
          return;
        }
        assert.fail('Should have thrown');
      });

      it('throws SessionNotCreatedException for invalid goog:sessionType', async () => {
        let error: unknown;
        try {
          await restrictedSessionProcessor.new({
            capabilities: {alwaysMatch: {'goog:sessionType': 'unrestricted'}},
          });
        } catch (err) {
          error = err;
        }
        assert.instanceOf(error, SessionNotCreatedException);
        sinon.assert.notCalled(initConnection);
      });

      it('allows retrying session.new with restricted capability after rejection', async () => {
        let error: unknown;
        try {
          await restrictedSessionProcessor.new({
            capabilities: {},
          });
        } catch (err) {
          error = err;
        }
        assert.instanceOf(error, SessionNotCreatedException);

        const result = await restrictedSessionProcessor.new({
          capabilities: {
            firstMatch: [{}, {'goog:sessionType': 'restricted'}],
          },
        });

        assert.strictEqual(
          result.capabilities['goog:sessionType'],
          'restricted',
        );
        sinon.assert.calledOnce(initConnection);
      });

      it('rejects acceptInsecureCerts and allows retry in restrictedMode', async () => {
        let error: unknown;
        try {
          await restrictedSessionProcessor.new({
            capabilities: {
              alwaysMatch: {
                'goog:sessionType': 'restricted',
                acceptInsecureCerts: true,
              },
            },
          });
        } catch (err) {
          error = err;
        }

        assert.instanceOf(error, SessionNotCreatedException);
        assert.isFalse(restrictedSessionProcessor.isCreated());
        sinon.assert.notCalled(initConnection);

        const result = await restrictedSessionProcessor.new({
          capabilities: {
            alwaysMatch: {'goog:sessionType': 'restricted'},
          },
        });

        assert.strictEqual(
          result.capabilities['goog:sessionType'],
          'restricted',
        );
        assert.isTrue(restrictedSessionProcessor.isCreated());
        sinon.assert.calledOnce(initConnection);
      });
    });
  });

  describe('setAcl', () => {
    it('delegates to the ACL manager in a restricted session', async () => {
      aclManager.enable();
      const acl = {
        default: {
          commands: ['browsingContext.getTree', 'browsingContext.navigate'],
        },
      };
      const reason = 'Fill in the checkout form.';
      const result = {
        acl: {default: {commands: ['browsingContext.getTree']}},
        denied: {acl: {default: {commands: ['browsingContext.navigate']}}},
      };
      const updateAcl = sinon.stub(aclManager, 'updateAcl').resolves(result);

      assert.deepEqual(await sessionProcessor.setAcl({acl, reason}), result);
      sinon.assert.calledOnceWithExactly(updateAcl, acl, reason);
    });

    it('throws UnsupportedOperationException in a standard session', async () => {
      const updateAcl = sinon.stub(aclManager, 'updateAcl');

      let error: unknown;
      try {
        await sessionProcessor.setAcl({acl: {}});
      } catch (err) {
        error = err;
      }

      assert.instanceOf(error, UnsupportedOperationException);
      sinon.assert.notCalled(updateAcl);
    });
  });
});
