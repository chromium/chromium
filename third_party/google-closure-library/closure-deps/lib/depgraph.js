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

const {SourceError} = require('./sourceerror');
const path = require('path');

/** @enum {string} */
const DependencyType = {
  /** A file containing goog.provide statements. */
  CLOSURE_PROVIDE: 'closure provide',
  /** A file containing a goog.module statement. */
  CLOSURE_MODULE: 'closure module',
  /** An ES6 module file. */
  ES6_MODULE: 'es6 module',
  /**
   * A JavaScript file that has no goog.provide/module and is not an ES6 module.
   */
  SCRIPT: 'script',
};


/**
 * A Dependency in the dependency graph (a vertex).
 */
class Dependency {
  /**
   * @param {!DependencyType} type
   * @param {string} filepath
   * @param {!Array<string>} closureSymbols
   * @param {!Array<!Import>} imports
   * @param {string=} language
   */
  constructor(type, filepath, closureSymbols, imports, language = 'es3') {
    /** @const */
    this.type = type;

    /**
     * Full path of this file on disc.
     * @const
     */
    this.path = path.resolve(filepath);

    /**
     * Array of Closure symbols this file provides.
     * @const
     */
    this.closureSymbols = closureSymbols;

    /**
     * Array of imports in this file.
     * @const
     */
    this.imports = imports;

    for (const i of this.imports) {
      i.from = this;
    }

    /**
     * The language level of this file; e.g. "es3", "es6", etc.
     * @const
     */
    this.language = language;
  }

  /**
   * Updates the path to Closure Library for this file. This is useful for
   * ParsedDependency, which cannot know the full path of a file on until it
   * knows the path to Closure Library, as the path in the goog.addDependency
   * call is relative from Closure Library.
   *
   * @param {string} path
   */
  setClosurePath(path) {}

  /**
   * @return {boolean}
   */
  isParsedFromDepsFile() { return false; }
}

let hasWarnedForAssignmentToPath = false;

/**
 * A dependency that was parsed from an goog.addDependnecy call.
 */
class ParsedDependency extends Dependency {
  /**
   * @param {!DependencyType} type
   * @param {string} closureRelativePath
   * @param {!Array<string>} closureSymbols
   * @param {!Array<!Import>} imports
   * @param {string=} language
   */
  constructor(
      type, closureRelativePath, closureSymbols, imports, language = 'es3') {
    super(type, /* filepath= */ '', closureSymbols, imports, language);
    /** @private @const {boolean} */
    this.hasCalledSuper_ = true;
    /** @private {string|undefined} */
    this.path_ = undefined;

    /**
     * Relative path from Closure Library to this file.
     * @const
     */
    this.closureRelativePath = closureRelativePath;
  }

  /** @return {string} */
  get path() {
    if (!this.path_) {
      throw new Error(
          'Must call setClosurePath in order to determine the ' +
          'actual path of this dependency.');
    }
    return this.path_;
  }

  /** @param {string} value */
  set path(value) {
    // Ignore, only here to satisfy super constructor.
    if (!hasWarnedForAssignmentToPath && this.hasCalledSuper_) {
      console.warn(
          'Assigning a path of a ParsedDependency instance was ignored. ' +
          'Use setClosurePath method instead.');
      hasWarnedForAssignmentToPath = true;
    }
  }

  /** @override */
  setClosurePath(closurePath) {
    this.path_ = path.resolve(closurePath, this.closureRelativePath);
  }

  /** @override */
  isParsedFromDepsFile() { return true; }
}


/**
 * Generic super class for all types of imports. This acts as an edge in the
 * dependency graph between two dependencies.
 *
 * @abstract
 */
class Import {
  /**
   * @param {string} symOrPath
   */
  constructor(symOrPath) {
    /**
     * Dependency this import is contained in.
     * @type {!Dependency}
     */
    this.from;
    /**
     * The Closure symbol or path that is required.
     * @const
     */
    this.symOrPath = symOrPath;
  }

  /**
   * Asserts that this import edge is valid.
   * @param {!Dependency} to
   * @abstract
   */
  validate(to) {}

  /**
   * @return {boolean}
   * @abstract
   */
  isGoogRequire() {}

  /**
   * @return {boolean}
   * @abstract
   */
  isEs6Import() {}
}

/** An import using goog.require. */
class GoogRequire extends Import {
  /** @override */
  validate(to) {
    for (const sym of to.closureSymbols) {
      if (sym === this.symOrPath) {
        return;
      }
    }

    throw new SourceError(
        `Invalid dependency edge: File ${to.path} does ` +
            `not provide symbol ${this.symOrPath}.`,
        this.from.path);
  }

  /** @override */
  isGoogRequire() {
    return true;
  }

  /** @override */
  isEs6Import() {
    return false;
  }
}

/** An ES6 import (or export from). */
class Es6Import extends Import {
  /** @override */
  toString() {
    return `import|export '${this.symOrPath}'`;
  }

  /** @override */
  validate(to) {
    if (to.type !== DependencyType.ES6_MODULE) {
      throw new SourceError(
          'Cannot import non-ES6 module: ' + to.path, this.from.path);
    }
  }

  /** @override */
  isGoogRequire() {
    return false;
  }

  /** @override */
  isEs6Import() {
    return true;
  }
}

/**
 * Interface for resolving module specifiers.
 * @interface
 */
class ModuleResolver {
  /**
   * @param {string} fromPath The path of the module that is doing the
   *     importing.
   * @param {string} importSpec The raw text of the import.
   * @return {string} The resolved path of the referenced module.
   */
  resolve(fromPath, importSpec) {}
}

/** @implements {ModuleResolver} */
class PathModuleResolver {
  /** @override */
  resolve(fromPath, importSpec) {
    return path.resolve(path.dirname(fromPath), importSpec);
  }
}

class InvalidCycleError extends Error {
  /**
   * @param {!Array<!Dependency>} deps
   */
  constructor(deps) {
    super(
        'There exists at least one cycle with a Closure file, which is ' +
        'invalid. Files involved in cycle(s):' +
        deps.map(dep => dep.path).join(', '));
    this.deps = deps;
  }
}

/**
 * Dependency graph that provides validation along with a topological sorting
 * of dependencies given an entrypoint.
 *
 * A dependency graph is not validated by default, you must call validate() if
 * you wish to perform validation.
 */
class Graph {
  /**
   * @param {!Array<!Dependency>} dependencies
   * @param {!ModuleResolver=} moduleResolver
   */
  constructor(dependencies, moduleResolver = new PathModuleResolver()) {
    /** @const {!Map<string, !Dependency>} */
    this.depsBySymbol = new Map();
    /** @const {!Map<string, !Dependency>} */
    this.depsByPath = new Map();
    /** @const */
    this.moduleResolver = moduleResolver;

    for (const dep of dependencies) {
      if (this.depsByPath.has(dep.path)) {
        throw new Error('File registered twice? ' + dep.path);
      }
      this.depsByPath.set(dep.path, dep);
      for (const sym of dep.closureSymbols) {
        const previous = this.depsBySymbol.get(sym);
        if (previous) {
          throw new SourceError(
              `The symbol "${sym}" has already been defined at ` +
                  previous.path,
              dep.path);
        }
        this.depsBySymbol.set(sym, dep);
      }
    }
  }

  /**
   * Validates the dependency graph. Throws an error if the graph is invalid.
   *
   * This method uses Tarjan's algorithm to ensure Closure files are not part
   * of any cycle. Check it out:
   * https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
   */
  validate() {
    let index = 0;

    // Map that assigns each dependency an index in visit-order.
    const indexMap = new Map();
    // Stack of dependencies that are potentially part of the current strongly
    // connected component. If any dependency has a back pointer it is kept
    // on the stack. Thus after recursion anything left on the stack above the
    // current dependency is part of the SCC. If the dependency has no back
    // pointer the SCC is created from all dependencies on the stack until the
    // current dependency is popped off and added to the SCC.
    const depStack = new Set();

    const validate = (dep) => {
      const thisIndex = index++;
      indexMap.set(dep, thisIndex);
      let lowIndex = thisIndex;

      depStack.add(dep);

      // We might modify the imports when iterating (drop unrecognized ES6
      // imports), so iterate over a copy.
      for (const i of [...dep.imports]) {
        const to = this.resolve_(i);

        if (!to) {
          if (i.isGoogRequire()) {
            throw new SourceError(`Could not find "${i.symOrPath}".`, dep.path);
          } else {
            // Just drop unrecognized ES6 imports. Assume their entire branch is
            // non-Closure, like a built-in Node module.
            dep.imports.splice(dep.imports.indexOf(i), 1);
            console.warn(
                `Could not find "${i.symOrPath}".` +
                    'Assuming it (and its transitive dependencies) are ' +
                    'non-Closure managed.',
                dep.path);
            continue;
          }
        }

        i.validate(to);

        if (!indexMap.has(to)) {
          // "to" has not been visited, recurse.
          lowIndex = Math.min(lowIndex, validate(to));
        } else if (depStack.has(to)) {
          // "to" is on the stack and thus a part of the SCC.
          lowIndex = Math.min(lowIndex, indexMap.get(to));
        }
        // else visited but not on the stack, so it is not part of the SCC. It
        // is a common dependency of some other tree, but does not reach this
        // dependency and thus is not strongly connected.
      }

      if (lowIndex === thisIndex) {
        let anyClosure = false;
        const deps = [...depStack];
        const scc = [];
        for (let i = deps.length - 1; i > -1; i--) {
          scc.push(deps[i]);
          depStack.delete(deps[i]);
          anyClosure = anyClosure ||
              deps[i].type === DependencyType.CLOSURE_PROVIDE ||
              deps[i].type === DependencyType.CLOSURE_MODULE;
          if (deps[i] === dep) {
            break;
          }
        }
        if (anyClosure && scc.length > 1) {
          throw new InvalidCycleError(scc);
        }
      }

      return lowIndex;
    };

    for (const dep of this.depsByPath.values()) {
      if (!indexMap.has(dep)) {
        validate(dep);
      }
    }
  }

  /**
   * @param {!Import} i
   * @return {!Dependency}
   * @private
   */
  resolve_(i) {
    return i instanceof GoogRequire ?
        this.depsBySymbol.get(i.symOrPath) :
        this.depsByPath.get(
            this.moduleResolver.resolve(i.from.path, i.symOrPath));
  }

  /**
   * Provides a topological sorting of dependencies given the entrypoints.
   *
   * @param {...!Dependency} entrypoints
   * @return {!Array<!Dependency>}
   */
  order(...entrypoints) {
    const deps = [];
    const visited = new Set();

    const visit = (dep) => {
      if (visited.has(dep)) return;
      visited.add(dep);
      for (const i of dep.imports) {
        const next = this.resolve_(i);
        visit(next);
      }
      deps.push(dep);
    };

    for (const entrypoint of entrypoints) {
      visit(entrypoint);
    }

    return deps;
  }
}

module.exports = {
  DependencyType,
  Dependency,
  ParsedDependency,
  GoogRequire,
  Es6Import,
  Graph,
  ModuleResolver,
  PathModuleResolver,
  InvalidCycleError,
};
