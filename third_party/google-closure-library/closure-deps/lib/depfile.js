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

const depGraph = require('./depgraph');
const path = require('path');

/**
 * Gets the text of a dependency file for the given dependencies.
 *
 * @param {string} pathToClosure The path to Closure Library. Required as paths
 *      in goog.addDependency statements are relative to Closure's base.js.
 * @param {!Array<!depGraph.Dependency>} dependencies
 * @param {!depGraph.ModuleResolver=} moduleResolver
 * @return {string}
 */
const getDepFileText = exports.getDepFileText = (
    pathToClosure, dependencies,
    moduleResolver = new depGraph.PathModuleResolver()) => {
  const lines = [];
  for (const dep of dependencies) {
    const args = [];

    args.push(`'${path.posix.relative(pathToClosure, dep.path)}'`);
    args.push(`[${dep.closureSymbols.map(s => `'${s}'`).join(', ')}]`);
    const requires = [];
    for (const imported of dep.imports) {
      if (imported.isGoogRequire()) {
        requires.push(imported.symOrPath);
      } else {
        const requiredFilePath =
            moduleResolver.resolve(dep.path, imported.symOrPath);
        const relativePath = path.relative(pathToClosure, requiredFilePath);
        requires.push(relativePath);
      }
    }
    args.push(`[${requires.map(s => `'${s}'`).join(', ')}]`);

    const loadFlags = [];
    if (dep.language != 'es3') {
      loadFlags.push(`'lang': '${dep.language}'`);
    }
    switch (dep.type) {
      case depGraph.DependencyType.CLOSURE_MODULE:
        loadFlags.push(`'module': 'goog'`);
        break;
      case depGraph.DependencyType.ES6_MODULE:
        loadFlags.push(`'module': 'es6'`);
        break;
      default:
        // nothing
    }
    if (loadFlags.length > 0) {
      args.push(`{${loadFlags.join(', ')}}`);
    }

    lines.push(`goog.addDependency(${args.join(', ')});`);
  }
  return lines.sort().join('\n') + '\n';
};
