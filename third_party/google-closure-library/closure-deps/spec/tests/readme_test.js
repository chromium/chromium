/**
 * @license
 * Copyright 2018 The Closure Library Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS-IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

const {parser, depGraph} = require('../../index');

/**
 * @param {...string} lines
 * @return {string}
 */
function lines(...lines) {
  return lines.join('\n');
}

const [goog] = parser.parseText('/** @provideGoog */', '/base.js').dependencies;

describe('examples in README', function() {
  it('goog modules', function() {
    const [firstFile] =
        parser.parseText(`goog.module('first.module')`, '/first.js')
            .dependencies;
    const [secondFile] = parser.parseText(
        lines(
            'goog.module(\'second.module\');',
            'const firstModule = goog.require(\'first.module\');'),
        '/second.js').dependencies;
    const graph = new depGraph.Graph([goog, firstFile, secondFile]);
    expect(graph.order(secondFile)).toEqual([firstFile, secondFile]);
    expect(graph.depsBySymbol.get('first.module')).toBe(firstFile);
    expect(graph.depsByPath.get('/second.js')).toBe(secondFile);
  });

  it('es6 modules', function() {
    const [firstFile] = parser
                          .parseText(
                              lines(
                                  'goog.declareModuleId(\'first.module\');',
                                  'export const FOO = \'foo\';'),
                              '/first.js')
                          .dependencies;

    const [secondFile] =
        parser.parseText('import {FOO} from "./first.js";', '/second.js')
            .dependencies;

    const [thirdFile] = parser.parseText(
        lines(
            'goog.module("third.module");',
            'const firstModule = goog.require("first.module");'),
        '/third.js').dependencies;

    const graph = new depGraph.Graph([goog, firstFile, secondFile, thirdFile]);
    expect(graph.order(secondFile, thirdFile)).toEqual([
      firstFile, secondFile, thirdFile
    ]);
  });
});
