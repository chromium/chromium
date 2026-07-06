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

import {describe, it, beforeEach} from 'node:test';
import {assert} from 'chai';
import sinon from 'sinon';

import {
  DigitalCredentials,
  InvalidArgumentException,
  UnsupportedOperationException,
} from '../../../protocol/protocol.js';
import type {CdpTarget} from '../cdp/CdpTarget.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import {ContextConfigStorage} from '../browser/ContextConfigStorage.js';

import {DigitalCredentialsProcessor} from './DigitalCredentialsProcessor.js';

describe('DigitalCredentialsProcessor', () => {
  let browsingContextStorage: sinon.SinonStubbedInstance<BrowsingContextStorage>;
  let contextConfigStorage: ContextConfigStorage;
  let processor: DigitalCredentialsProcessor;

  beforeEach(() => {
    browsingContextStorage = sinon.createStubInstance(BrowsingContextStorage);
    contextConfigStorage = new ContextConfigStorage();
    processor = new DigitalCredentialsProcessor(
      browsingContextStorage,
      contextConfigStorage,
    );
  });

  describe('parameter validation', () => {
    it("should throw InvalidArgumentException when action is 'respond' but protocol or response is missing", async () => {
      try {
        await processor.setVirtualWalletBehavior({
          action: DigitalCredentials.VirtualWalletAction.Respond,
        });
        assert.fail('Expected InvalidArgumentException to be thrown');
      } catch (error) {
        assert.instanceOf(error, InvalidArgumentException);
        assert.strictEqual(
          (error as Error).message,
          "Protocol and response are required when action is 'respond'",
        );
      }

      try {
        await processor.setVirtualWalletBehavior({
          action: DigitalCredentials.VirtualWalletAction.Respond,
          protocol: 'webauthn',
        });
        assert.fail('Expected InvalidArgumentException to be thrown');
      } catch (error) {
        assert.instanceOf(error, InvalidArgumentException);
        assert.strictEqual(
          (error as Error).message,
          "Protocol and response are required when action is 'respond'",
        );
      }

      try {
        await processor.setVirtualWalletBehavior({
          action: DigitalCredentials.VirtualWalletAction.Respond,
          response: {},
        });
        assert.fail('Expected InvalidArgumentException to be thrown');
      } catch (error) {
        assert.instanceOf(error, InvalidArgumentException);
        assert.strictEqual(
          (error as Error).message,
          "Protocol and response are required when action is 'respond'",
        );
      }
    });

    it("should throw InvalidArgumentException when action is not 'respond' but protocol or response is present", async () => {
      const actions = [
        DigitalCredentials.VirtualWalletAction.Wait,
        DigitalCredentials.VirtualWalletAction.Decline,
        DigitalCredentials.VirtualWalletAction.Clear,
      ] as const;
      for (const action of actions) {
        try {
          await processor.setVirtualWalletBehavior({
            action,
            protocol: 'webauthn',
            response: {},
          });
          assert.fail('Expected InvalidArgumentException to be thrown');
        } catch (error) {
          assert.instanceOf(error, InvalidArgumentException);
          assert.strictEqual(
            (error as Error).message,
            "Protocol and response are only allowed when action is 'respond'",
          );
        }

        try {
          await processor.setVirtualWalletBehavior({
            action,
            protocol: 'webauthn',
          });
          assert.fail('Expected InvalidArgumentException to be thrown');
        } catch (error) {
          assert.instanceOf(error, InvalidArgumentException);
          assert.strictEqual(
            (error as Error).message,
            "Protocol and response are only allowed when action is 'respond'",
          );
        }

        try {
          await processor.setVirtualWalletBehavior({
            action,
            response: {},
          });
          assert.fail('Expected InvalidArgumentException to be thrown');
        } catch (error) {
          assert.instanceOf(error, InvalidArgumentException);
          assert.strictEqual(
            (error as Error).message,
            "Protocol and response are only allowed when action is 'respond'",
          );
        }
      }
    });
  });

  describe('context-specific behavior', () => {
    it('should send CDP command to the specific context target and update config', async () => {
      const mockCdpClient = {
        sendCommand: sinon.stub().resolves({}),
      };
      const mockTarget = {
        cdpClient: mockCdpClient,
        id: 'context_id',
        topLevelId: 'context_id',
        userContext: 'default',
      } as unknown as CdpTarget;
      const mockContext = {
        id: 'context_id',
        parentId: null,
        cdpTarget: mockTarget,
      } as unknown as BrowsingContextImpl;

      browsingContextStorage.getContext
        .withArgs('context_id')
        .returns(mockContext);
      browsingContextStorage.getAllContexts.returns([mockContext]);

      await processor.setVirtualWalletBehavior({
        context: 'context_id',
        action: DigitalCredentials.VirtualWalletAction.Decline,
      });

      assert.isTrue(mockCdpClient.sendCommand.calledOnce);
      assert.isTrue(
        mockCdpClient.sendCommand.calledWith(
          'DigitalCredentials.setVirtualWalletBehavior',
          {
            action: DigitalCredentials.VirtualWalletAction.Decline,
            behavior: DigitalCredentials.VirtualWalletAction.Decline,
            protocol: undefined,
            response: undefined,
          },
        ),
      );

      const config = contextConfigStorage.getActiveConfig(
        'context_id',
        'default',
      );
      assert.deepEqual(config.digitalCredentialsBehavior, {
        action: DigitalCredentials.VirtualWalletAction.Decline,
        protocol: undefined,
        response: undefined,
      });
    });

    it('should throw UnsupportedOperationException if context is not top-level', async () => {
      const mockContext = {
        id: 'child_context_id',
        parentId: 'parent_context_id',
      } as unknown as BrowsingContextImpl;

      browsingContextStorage.getContext
        .withArgs('child_context_id')
        .returns(mockContext);

      try {
        await processor.setVirtualWalletBehavior({
          context: 'child_context_id',
          action: DigitalCredentials.VirtualWalletAction.Decline,
        });
        assert.fail('Expected UnsupportedOperationException to be thrown');
      } catch (error) {
        assert.instanceOf(error, UnsupportedOperationException);
        assert.strictEqual(
          (error as Error).message,
          'Only top-level contexts are supported',
        );
      }
    });
  });

  describe('session-default behavior', () => {
    it('should send CDP command to all active targets and store default in config', async () => {
      const mockCdpClient1 = {sendCommand: sinon.stub().resolves({})};
      const mockCdpClient2 = {sendCommand: sinon.stub().resolves({})};
      const mockTarget1 = {
        cdpClient: mockCdpClient1,
        id: 'context_1',
        topLevelId: 'context_1',
        userContext: 'default',
      } as unknown as CdpTarget;
      const mockTarget2 = {
        cdpClient: mockCdpClient2,
        id: 'context_2',
        topLevelId: 'context_2',
        userContext: 'default',
      } as unknown as CdpTarget;
      const mockContext1 = {
        id: 'context_1',
        parentId: null,
        cdpTarget: mockTarget1,
      } as unknown as BrowsingContextImpl;
      const mockContext2 = {
        id: 'context_2',
        parentId: null,
        cdpTarget: mockTarget2,
      } as unknown as BrowsingContextImpl;

      browsingContextStorage.getAllContexts.returns([
        mockContext1,
        mockContext2,
      ]);

      await processor.setVirtualWalletBehavior({
        action: DigitalCredentials.VirtualWalletAction.Respond,
        protocol: 'openid4vp',
        response: {token: 'abc'},
      });

      assert.isTrue(mockCdpClient1.sendCommand.calledOnce);
      assert.isTrue(mockCdpClient2.sendCommand.calledOnce);

      const expectedCdpParams = {
        action: DigitalCredentials.VirtualWalletAction.Respond,
        behavior: DigitalCredentials.VirtualWalletAction.Respond,
        protocol: 'openid4vp',
        response: {token: 'abc'},
      };
      assert.isTrue(
        mockCdpClient1.sendCommand.calledWith(
          'DigitalCredentials.setVirtualWalletBehavior',
          expectedCdpParams,
        ),
      );

      const config = contextConfigStorage.getGlobalConfig();
      assert.deepEqual(config.digitalCredentialsBehavior, {
        action: DigitalCredentials.VirtualWalletAction.Respond,
        protocol: 'openid4vp',
        response: {token: 'abc'},
      });
    });

    it('should clear default behavior', async () => {
      const mockCdpClient = {sendCommand: sinon.stub().resolves({})};
      const mockTarget = {
        cdpClient: mockCdpClient,
        id: 'context_1',
        topLevelId: 'context_1',
        userContext: 'default',
      } as unknown as CdpTarget;
      const mockContext = {
        id: 'context_1',
        parentId: null,
        cdpTarget: mockTarget,
      } as unknown as BrowsingContextImpl;

      browsingContextStorage.getAllContexts.returns([mockContext]);

      await processor.setVirtualWalletBehavior({
        action: DigitalCredentials.VirtualWalletAction.Decline,
      });

      assert.isTrue(mockCdpClient.sendCommand.calledOnce);

      await processor.setVirtualWalletBehavior({
        action: DigitalCredentials.VirtualWalletAction.Clear,
      });

      assert.isTrue(mockCdpClient.sendCommand.calledTwice);
      assert.isTrue(
        mockCdpClient.sendCommand.secondCall.calledWith(
          'DigitalCredentials.setVirtualWalletBehavior',
          {
            action: DigitalCredentials.VirtualWalletAction.Clear,
            behavior: DigitalCredentials.VirtualWalletAction.Clear,
            protocol: undefined,
            response: undefined,
          },
        ),
      );

      const config = contextConfigStorage.getGlobalConfig();
      assert.isUndefined(config.digitalCredentialsBehavior);
    });

    it('should not inherit behavior from parent context if on different targets', async () => {
      const mockCdpClient1 = {sendCommand: sinon.stub().resolves({})};
      const mockCdpClient2 = {sendCommand: sinon.stub().resolves({})};
      const mockTarget1 = {
        cdpClient: mockCdpClient1,
        id: 'parent_id',
        topLevelId: 'parent_id',
        userContext: 'default',
      } as unknown as CdpTarget;
      const mockTarget2 = {
        cdpClient: mockCdpClient2,
        id: 'child_id',
        topLevelId: 'parent_id',
        userContext: 'default',
      } as unknown as CdpTarget;

      const parentContext = {
        id: 'parent_id',
        parentId: null,
        cdpTarget: mockTarget1,
      } as unknown as BrowsingContextImpl;

      const childContext = {
        id: 'child_id',
        parentId: 'parent_id',
        cdpTarget: mockTarget2,
      } as unknown as BrowsingContextImpl;

      browsingContextStorage.getContext
        .withArgs('parent_id')
        .returns(parentContext);
      browsingContextStorage.getContext
        .withArgs('child_id')
        .returns(childContext);

      browsingContextStorage.getAllContexts.returns([
        parentContext,
        childContext,
      ]);

      await processor.setVirtualWalletBehavior({
        context: 'parent_id',
        action: DigitalCredentials.VirtualWalletAction.Decline,
      });

      assert.isTrue(mockCdpClient1.sendCommand.calledOnce);
      // child_id has different target, and even though it is child of parent_id,
      // #applyBehaviorToTarget should reject it because target.id !== target.topLevelId
      assert.isFalse(mockCdpClient2.sendCommand.called);
    });
  });
});
