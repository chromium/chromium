/**
 * Copyright 2023 Google LLC.
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
import sinon from 'sinon';

import {CdpTarget} from '../context/CdpTarget.js';

import {PreloadScript} from './PreloadScript.js';
import {PreloadScriptStorage} from './PreloadScriptStorage.js';

const MOCKED_UUID_1 = '00000000-0000-0000-0000-00000000000a';
const MOCKED_UUID_2 = '00000000-0000-0000-0000-00000000000b';
const CDP_TARGET_ID = 'TARGET_ID';

describe('PreloadScriptStorage', () => {
  let preloadScriptStorage: PreloadScriptStorage;

  let cdpTarget: sinon.SinonStubbedInstance<CdpTarget>;

  beforeEach(() => {
    preloadScriptStorage = new PreloadScriptStorage();
    cdpTarget = sinon.createStubInstance(CdpTarget);
    sinon.stub(cdpTarget, 'id').get(() => CDP_TARGET_ID);
  });

  it('initial state', () => {
    expect(preloadScriptStorage.find()).to.be.empty;
    expect(preloadScriptStorage.find({})).to.be.empty;
    expect(
      preloadScriptStorage.find({
        id: '',
      })
    ).to.be.empty;
    expect(
      preloadScriptStorage.find({
        targetId: '',
      })
    ).to.be.empty;
  });

  it(`add preload scripts`, () => {
    const preloadScript1 = createPreloadScript(MOCKED_UUID_1);
    const preloadScript2 = createPreloadScript(MOCKED_UUID_2);

    preloadScriptStorage.add(preloadScript1);
    preloadScriptStorage.add(preloadScript2);

    expect(preloadScriptStorage.find()).to.deep.equal([
      preloadScript1,
      preloadScript2,
    ]);
  });

  it(`remove non-existing BiDi id`, () => {
    const preloadScript1 = createPreloadScript(MOCKED_UUID_1);
    const preloadScript2 = createPreloadScript(MOCKED_UUID_2);
    preloadScriptStorage.add(preloadScript1);
    preloadScriptStorage.add(preloadScript2);

    const preloadScripts = preloadScriptStorage.find();

    preloadScriptStorage.remove({
      id: `${MOCKED_UUID_1}_NON_EXISTING`,
    });

    expect(preloadScriptStorage.find()).to.be.deep.equal(preloadScripts);
  });

  [
    {
      filterDescription: 'bidi id',
      filter: {id: MOCKED_UUID_1},
    },
    {
      filterDescription: 'target id',
      filter: {targetId: CDP_TARGET_ID},
    },
  ].forEach(({filterDescription, filter}) => {
    it(`find preload scripts by ${filterDescription}`, () => {
      const preloadScript1 = createPreloadScript(MOCKED_UUID_1);
      preloadScriptStorage.add(preloadScript1);

      expect(preloadScriptStorage.find(filter)).to.deep.equal([preloadScript1]);
    });

    it(`remove preload scripts by ${filterDescription}`, () => {
      const preloadScript1 = createPreloadScript(MOCKED_UUID_1);
      preloadScriptStorage.add(preloadScript1);
      preloadScriptStorage.remove(filter);

      expect(preloadScriptStorage.find(filter)).to.be.empty;
    });
  });

  afterEach(() => {
    sinon.restore();
  });

  function createPreloadScript(id: string): PreloadScript {
    const preloadScript = sinon.createStubInstance(PreloadScript);
    sinon.stub(preloadScript, 'id').get(() => id);
    sinon.stub(preloadScript, 'targetIds').get(() => new Set([CDP_TARGET_ID]));
    return preloadScript;
  }
});
