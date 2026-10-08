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

import {describe, it} from 'node:test';
import {assert} from 'chai';
import sinon from 'sinon';

import type {CdpClient} from '../../../cdp/CdpClient.js';
import {
  InvalidWebExtensionException,
  UnsupportedOperationException,
  type WebExtension,
} from '../../../protocol/protocol.js';
import {CdpErrorConstants} from '../../../utils/cdpErrorConstants.js';

import {WebExtensionProcessor} from './WebExtensionProcessor.js';

const pathParams: WebExtension.InstallParameters = {
  extensionData: {type: 'path', path: '/path/to/extension'},
};

async function rejectedWith(promise: Promise<unknown>): Promise<unknown> {
  try {
    await promise;
  } catch (error) {
    return error;
  }
  assert.fail('Expected the command to reject');
}

describe('WebExtensionProcessor.install', () => {
  it('reports unsupported operation when the CDP Extensions domain is unavailable', async () => {
    const methodNotFound = {
      code: CdpErrorConstants.METHOD_NOT_FOUND,
      message: "'Extensions.loadUnpacked' wasn't found",
    };
    const sendCommand = sinon.stub().rejects(methodNotFound);
    const processor = new WebExtensionProcessor({
      sendCommand,
    } as unknown as CdpClient);

    const error = await rejectedWith(processor.install(pathParams));

    assert.instanceOf(error, UnsupportedOperationException);
    assert.strictEqual(
      (error as Error).message,
      'Installing web extensions is not supported',
    );
    sinon.assert.calledOnceWithExactly(sendCommand, 'Extensions.loadUnpacked', {
      path: '/path/to/extension',
    });
  });

  it('preserves successful path installation', async () => {
    const sendCommand = sinon.stub().resolves({id: 'extension-id'});
    const processor = new WebExtensionProcessor({
      sendCommand,
    } as unknown as CdpClient);

    assert.deepEqual(await processor.install(pathParams), {
      extension: 'extension-id',
    });
    sinon.assert.calledOnceWithExactly(sendCommand, 'Extensions.loadUnpacked', {
      path: '/path/to/extension',
    });
  });

  it('preserves invalid web extension errors', async () => {
    const sendCommand = sinon.stub().rejects({
      code: CdpErrorConstants.GENERIC_ERROR,
      message: 'invalid web extension: missing manifest',
    });
    const processor = new WebExtensionProcessor({
      sendCommand,
    } as unknown as CdpClient);

    const error = await rejectedWith(processor.install(pathParams));

    assert.instanceOf(error, InvalidWebExtensionException);
    assert.strictEqual(
      (error as Error).message,
      'invalid web extension: missing manifest',
    );
  });

  it('preserves unrelated CDP errors', async () => {
    const cdpError = {
      code: CdpErrorConstants.GENERIC_ERROR,
      message: 'backend failure',
    };
    const sendCommand = sinon.stub().rejects(cdpError);
    const processor = new WebExtensionProcessor({
      sendCommand,
    } as unknown as CdpClient);

    assert.strictEqual(
      await rejectedWith(processor.install(pathParams)),
      cdpError,
    );
  });

  const unexpectedRejections: [string, unknown][] = [
    ['null', null],
    ['undefined', undefined],
    ['missing message', {}],
    ['null message', {message: null}],
    ['non-string message', {message: 42}],
    ['string', 'backend failure'],
    ['Error', new Error('backend failure')],
  ];
  for (const [description, rejection] of unexpectedRejections) {
    it(`preserves unexpected rejection values: ${description}`, async () => {
      const sendCommand = sinon
        .stub()
        .callsFake(() => Promise.reject(rejection));
      const processor = new WebExtensionProcessor({
        sendCommand,
      } as unknown as CdpClient);

      assert.strictEqual(
        await rejectedWith(processor.install(pathParams)),
        rejection,
      );
      sinon.assert.calledOnceWithExactly(
        sendCommand,
        'Extensions.loadUnpacked',
        {
          path: '/path/to/extension',
        },
      );
    });
  }

  it('keeps archive and base64 unsupported without calling CDP', async () => {
    const sendCommand = sinon.stub();
    const processor = new WebExtensionProcessor({
      sendCommand,
    } as unknown as CdpClient);
    const params: WebExtension.InstallParameters[] = [
      {extensionData: {type: 'archivePath', path: '/path/to/archive'}},
      {extensionData: {type: 'base64', value: 'dGVzdA=='}},
    ];

    for (const param of params) {
      const error = await rejectedWith(processor.install(param));
      assert.instanceOf(error, UnsupportedOperationException);
      assert.strictEqual(
        (error as Error).message,
        'Archived and Base64 extensions are not supported',
      );
    }
    sinon.assert.notCalled(sendCommand);
  });
});
