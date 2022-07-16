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

const depFile = require('../../lib/depfile');
const depGraph = require('../../lib/depgraph');

const PATH_TO_CLOSURE = '/root/closure/goog/';

describe('depfile', function() {
  it('closure provide', function() {
    const d = new depGraph.Dependency(
        depGraph.DependencyType.CLOSURE_PROVIDE, PATH_TO_CLOSURE + 'example.js',
        ['my.ns0', 'my.ns1'], []);
    expect(depFile.getDepFileText(PATH_TO_CLOSURE, [d]))
        .toBe(`goog.addDependency('example.js', ['my.ns0', 'my.ns1'], []);\n`);
  });

  it('closure module', function() {
    const d = new depGraph.Dependency(
        depGraph.DependencyType.CLOSURE_MODULE, PATH_TO_CLOSURE + 'example.js',
        ['my.ns'], []);
    expect(depFile.getDepFileText(PATH_TO_CLOSURE, [d]))
        .toBe(
            `goog.addDependency('example.js', ['my.ns'], [], ` +
            `{'module': 'goog'});\n`);
  });

  it('goog.require', function() {
    const d = new depGraph.Dependency(
        depGraph.DependencyType.CLOSURE_PROVIDE, PATH_TO_CLOSURE + 'example.js',
        ['my.ns'],
        [new depGraph.GoogRequire('my.r0'), new depGraph.GoogRequire('my.r1')]);
    expect(depFile.getDepFileText(PATH_TO_CLOSURE, [d]))
        .toBe(
            `goog.addDependency('example.js', ['my.ns'], ['my.r0', 'my.r1']);\n`);
  });

  it('path is relative to closure', function() {
    let d = new depGraph.Dependency(
        depGraph.DependencyType.CLOSURE_PROVIDE,
        PATH_TO_CLOSURE + '/nested/example.js', [], []);
    expect(depFile.getDepFileText(PATH_TO_CLOSURE, [d]))
        .toBe(`goog.addDependency('nested/example.js', [], []);\n`);

    d = new depGraph.Dependency(
        depGraph.DependencyType.CLOSURE_PROVIDE, '/root/foo/bar/example.js', [],
        []);
    expect(depFile.getDepFileText(PATH_TO_CLOSURE, [d]))
        .toBe(`goog.addDependency('../../foo/bar/example.js', [], []);\n`);
  });

  it('langugage level', function() {
    let d = new depGraph.Dependency(
        depGraph.DependencyType.CLOSURE_PROVIDE,
        PATH_TO_CLOSURE + '/example.js', [], [], 'es5');
    expect(depFile.getDepFileText(PATH_TO_CLOSURE, [d]))
        .toBe(`goog.addDependency('example.js', [], [], {'lang': 'es5'});\n`);
  });

  it('script', function() {
    let d = new depGraph.Dependency(
        depGraph.DependencyType.SCRIPT, PATH_TO_CLOSURE + '/example.js', [],
        []);
    expect(depFile.getDepFileText(PATH_TO_CLOSURE, [d]))
        .toBe(`goog.addDependency('example.js', [], []);\n`);
  });

  it('script with requires', function() {
    let d = new depGraph.Dependency(
        depGraph.DependencyType.SCRIPT, PATH_TO_CLOSURE + '/example.js', [],
        [new depGraph.GoogRequire('stuff')]);
    expect(depFile.getDepFileText(PATH_TO_CLOSURE, [d]))
        .toBe(`goog.addDependency('example.js', [], ['stuff']);\n`);
  });

  describe('es6 module', function() {
    it('simple', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, PATH_TO_CLOSURE + 'example.js',
          [], [], 'es6');
      expect(depFile.getDepFileText(PATH_TO_CLOSURE, [d]))
          .toBe(
              `goog.addDependency('example.js', [], [], ` +
              `{'lang': 'es6', 'module': 'es6'});\n`);
    });

    it('declareModuleId', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, PATH_TO_CLOSURE + 'example.js',
          ['my.es6'], []);
      expect(depFile.getDepFileText(PATH_TO_CLOSURE, [d]))
          .toBe(
              `goog.addDependency('example.js', ['my.es6'], [], ` +
              `{'module': 'es6'});\n`);
    });

    it('goog.require', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, PATH_TO_CLOSURE + 'example.js',
          [], [new depGraph.GoogRequire('my.ns')]);
      expect(depFile.getDepFileText(PATH_TO_CLOSURE, [d]))
          .toBe(
              `goog.addDependency('example.js', [], ['my.ns'], ` +
              `{'module': 'es6'});\n`);
    });

    it('import/export from', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, PATH_TO_CLOSURE + 'example.js',
          [], [new depGraph.Es6Import(PATH_TO_CLOSURE + 'imported.js')]);
      expect(depFile.getDepFileText(PATH_TO_CLOSURE, [d]))
          .toBe(
              `goog.addDependency('example.js', [], ['imported.js'], ` +
              `{'module': 'es6'});\n`);
    });

    it('import/export from path is relative to closure', function() {
      const d = new depGraph.Dependency(
          depGraph.DependencyType.ES6_MODULE, PATH_TO_CLOSURE + 'example.js',
          [], [new depGraph.Es6Import('/root/foo/bar/imported.js')]);
      expect(depFile.getDepFileText(PATH_TO_CLOSURE, [d]))
          .toBe(
              `goog.addDependency('example.js', [], ` +
              `['../../foo/bar/imported.js'], {'module': 'es6'});\n`);
    });
  });
});
