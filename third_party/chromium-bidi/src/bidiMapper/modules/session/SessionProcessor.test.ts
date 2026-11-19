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
import {InvalidArgumentException} from '../../../protocol/protocol.js';
import type {MapperOptions} from '../../MapperOptions.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

import {AclManager} from './AclManager.js';
import type {EventManager} from './EventManager.js';
import {SessionProcessor} from './SessionProcessor.js';

const TOP_LEVEL_CONTEXT = 'context-1';
const CHILD_CONTEXT = 'child-context';
const UNKNOWN_CONTEXT = 'unknown-context';

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
    it('sets goog:acl to undefined in capabilities when not requested', async () => {
      const result = await sessionProcessor.new({
        capabilities: {},
      });

      assert.isUndefined(result.capabilities['goog:acl']);
      assert.isTrue(aclManager.isCommandAllowed('network.addIntercept'));
    });

    it('interprets invalid goog:acl as no access', async () => {
      const result = await sessionProcessor.new({
        capabilities: {
          alwaysMatch: {
            'goog:acl': null,
          },
        },
      });

      const expectedActualAcl = {
        default: {
          commands: [],
        },
        perNavigable: {},
      };
      assert.deepEqual(result.capabilities['goog:acl'], expectedActualAcl);
      assert.deepEqual(aclManager.getAcl(), expectedActualAcl);
      assert.isFalse(aclManager.isCommandAllowed('browsingContext.getTree'));
    });

    it('stores goog:acl from alwaysMatch and returns actual ACL in capabilities', async () => {
      const requestedAcl = {
        default: {
          commands: [
            'browsingContext.getTree',
            // Disallowed command should be filtered out.
            'network.addIntercept',
          ],
        },
      };

      const result = await sessionProcessor.new({
        capabilities: {
          alwaysMatch: {
            'goog:acl': requestedAcl,
          },
        },
      });

      const expectedActualAcl = {
        default: {
          commands: ['browsingContext.getTree'],
        },
        perNavigable: {},
      };
      assert.deepEqual(result.capabilities['goog:acl'], expectedActualAcl);
      assert.deepEqual(aclManager.getAcl(), expectedActualAcl);
    });

    it('stores goog:acl from firstMatch and returns actual ACL in capabilities', async () => {
      const requestedAcl = {
        default: {
          commands: ['browsingContext.getTree'],
        },
      };

      const result = await sessionProcessor.new({
        capabilities: {
          firstMatch: [
            {
              'goog:acl': requestedAcl,
            },
          ],
        },
      });

      const expectedActualAcl = {
        default: {
          commands: ['browsingContext.getTree'],
        },
        perNavigable: {},
      };
      assert.deepEqual(result.capabilities['goog:acl'], expectedActualAcl);
      assert.deepEqual(aclManager.getAcl(), expectedActualAcl);
    });

    it('ignores perNavigable in goog:acl', async () => {
      const result = await sessionProcessor.new({
        capabilities: {
          alwaysMatch: {
            'goog:acl': {
              default: {
                commands: ['browsingContext.getTree'],
              },
              perNavigable: {
                [TOP_LEVEL_CONTEXT]: {
                  commands: ['browsingContext.navigate'],
                },
                [CHILD_CONTEXT]: {
                  commands: ['browsingContext.navigate'],
                },
                [UNKNOWN_CONTEXT]: {
                  commands: ['browsingContext.navigate'],
                },
              },
            },
          },
        },
      });

      const expectedActualAcl = {
        default: {
          commands: ['browsingContext.getTree'],
        },
        perNavigable: {},
      };
      assert.deepEqual(result.capabilities['goog:acl'], expectedActualAcl);
      assert.deepEqual(aclManager.getAcl(), expectedActualAcl);
      assert.isFalse(
        aclManager.isCommandAllowed(
          'browsingContext.navigate',
          TOP_LEVEL_CONTEXT,
        ),
      );
    });

    it('throws InvalidArgumentException if goog:acl is in both alwaysMatch and firstMatch', async () => {
      try {
        await sessionProcessor.new({
          capabilities: {
            alwaysMatch: {
              'goog:acl': {},
            },
            firstMatch: [
              {
                'goog:acl': {},
              },
            ],
          },
        });
      } catch (err) {
        assert.instanceOf(err, InvalidArgumentException);
        sinon.assert.notCalled(initConnection);
        assert.isTrue(aclManager.isCommandAllowed('network.addIntercept'));
        return;
      }
      assert.fail('Should have thrown');
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
