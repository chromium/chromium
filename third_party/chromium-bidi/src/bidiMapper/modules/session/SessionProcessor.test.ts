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
  SessionNotCreatedException,
} from '../../../protocol/protocol.js';
import type {MapperOptions} from '../../MapperOptions.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

import {AclManager} from './AclManager.js';
import type {EventManager} from './EventManager.js';
import {SessionProcessor} from './SessionProcessor.js';

const TOP_LEVEL_CONTEXT = 'context-1';
const CHILD_CONTEXT = 'child-context';

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
    browsingContextStorage.addContext({
      id: TOP_LEVEL_CONTEXT,
      parentId: null,
      isTopLevelContext: () => true,
    } as unknown as BrowsingContextImpl);
    browsingContextStorage.addContext({
      id: CHILD_CONTEXT,
      parentId: TOP_LEVEL_CONTEXT,
      isTopLevelContext: () => false,
    } as unknown as BrowsingContextImpl);
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
      assert.notProperty(result.capabilities, 'goog:acl');
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
      assert.notProperty(result.capabilities, 'goog:acl');
      assert.isTrue(aclManager.isEnabled());
      assert.deepEqual(aclManager.getAcl(), {
        default: {commands: []},
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
      assert.notProperty(result.capabilities, 'goog:acl');
      assert.isTrue(aclManager.isEnabled());
      assert.deepEqual(aclManager.getAcl(), {
        default: {commands: []},
        perNavigable: {},
      });
      assert.isFalse(aclManager.isCommandAllowed('browsingContext.getTree'));
    });

    it('ignores goog:acl in session.new', async () => {
      await sessionProcessor.new({
        capabilities: {
          alwaysMatch: {
            'goog:acl': {
              default: {
                commands: ['browsingContext.getTree'],
              },
            },
          },
        },
      });

      assert.isFalse(aclManager.isEnabled());
      assert.isTrue(aclManager.isCommandAllowed('network.addIntercept'));
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
      it(`throws InvalidArgumentException for invalid goog:sessionType ${JSON.stringify(invalidSessionType)}`, async () => {
        try {
          await sessionProcessor.new({
            capabilities: {
              alwaysMatch: {
                'goog:sessionType': invalidSessionType,
              },
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

    describe('restrictedMode', () => {
      let restrictedSessionProcessor: SessionProcessor;

      beforeEach(() => {
        aclManager.setEnabled(true);
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

      it('allows retrying session.new with restricted capability after rejection', async () => {
        try {
          await restrictedSessionProcessor.new({
            capabilities: {},
          });
          assert.fail('Should have thrown');
        } catch (err) {
          assert.instanceOf(err, SessionNotCreatedException);
        }

        const result = await restrictedSessionProcessor.new({
          capabilities: {
            firstMatch: [{}, {'goog:sessionType': 'restricted'}],
          },
        });

        assert.strictEqual(
          result.capabilities['goog:sessionType'],
          'restricted',
        );
        assert.isTrue(aclManager.isEnabled());
        sinon.assert.calledOnce(initConnection);
      });
    });
  });

  describe('setAcl', () => {
    it('delegates to the ACL manager', async () => {
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
  });
});
