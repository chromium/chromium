/**
 * Copyright 2022 Google LLC.
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

import * as chai from 'chai';
import {CommonDataTypes} from '../../../protocol/types';
import {getRemoteValuesText} from './logHelper';

const expect = chai.expect;

describe('test getRemoteValuesText', () => {
  it('single line input test', () => {
    const inputArgs = [{type: 'string', value: 'line 1'}];
    const outputString = 'line 1';
    expect(
      getRemoteValuesText(inputArgs as CommonDataTypes.RemoteValue[], false)
    ).to.equal(outputString);
  });

  it('multiple line input test', () => {
    const inputArgs = [
      {type: 'string', value: 'line 1'},
      {type: 'string', value: 'line 2'},
    ];
    const outputString = 'line 1\u0020line 2';
    expect(
      getRemoteValuesText(inputArgs as CommonDataTypes.RemoteValue[], false)
    ).to.equal(outputString);
  });

  it('no input test', () => {
    const inputArgs = [] as any[];
    const outputString = '';
    expect(
      getRemoteValuesText(inputArgs as CommonDataTypes.RemoteValue[], false)
    ).to.equal(outputString);
  });
});
