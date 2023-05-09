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

import {PreloadScriptStorage} from './PreloadScriptStorage';

// TODO: expand unit tests.
describe('PreloadScriptStorage', () => {
  let preloadScriptStorage: PreloadScriptStorage;

  beforeEach(() => {
    preloadScriptStorage = new PreloadScriptStorage();
  });

  it('initial state', () => {
    expect(preloadScriptStorage.findPreloadScripts()).to.be.empty;
    expect(preloadScriptStorage.findPreloadScripts({})).to.be.empty;
    expect(
      preloadScriptStorage.findPreloadScripts({
        id: '',
      })
    ).to.be.empty;
    expect(
      preloadScriptStorage.findPreloadScripts({
        contextId: null,
      })
    ).to.be.empty;
    expect(
      preloadScriptStorage.findPreloadScripts({
        contextId: '',
      })
    ).to.be.empty;
  });

  afterEach(() => {
    sinon.restore();
  });
});
