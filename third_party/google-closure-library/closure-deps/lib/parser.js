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
const depGraph = require('./depgraph');
const fs = require('fs');
const {gjd} = require('./jsfile_parser');
const path = require('path');


class ParseResult {
  /**
   * @param {!Array<!depGraph.Dependency>} dependencies
   * @param {!Array<!ParseError>} errors
   * @param {!ParseResult.Source} source
   */
  constructor(dependencies, errors, source) {
    /** @const */
    this.dependencies = dependencies;
    /** @const */
    this.errors = errors;
    /** @const */
    this.source = source;
    /** @const */
    this.isFromDepsFile = source === ParseResult.Source.GOOG_ADD_DEPENDENCY;
    /** @const {boolean} */
    this.hasFatalError = errors.some(e => e.fatal);
  }
}
exports.ParseResult = ParseResult;


/**
 * @enum {string}
 */
ParseResult.Source = {
  /**
   * Scanned from an actual source file.
   */
  SOURCE_FILE: 'f',

  /**
   * A goog.addDependency statement.
   */
  GOOG_ADD_DEPENDENCY: 'd',
};


class ParseError {
  /**
   * @param {boolean} fatal
   * @param {string} message
   * @param {string} sourceName
   * @param {number} line
   * @param {number} lineOffset
   */
  constructor(fatal, message, sourceName, line, lineOffset) {
    /** @const */
    this.fatal = fatal;
    /** @const */
    this.message = message;
    /** @const */
    this.sourceName = sourceName;
    /** @const */
    this.line = line;
    /** @const */
    this.lineOffset = lineOffset;
  }

  /** @override */
  toString() {
    return `${this.fatal ? 'ERROR' : 'WARNING'} in ${this.sourceName} at ${
        this.line}, ${this.lineOffset}: ${this.message}`;
  }
}
exports.ParseError = ParseError;


/**
 * @param {string} path
 * @return {!ParseResult}
 */
const parseFile = exports.parseFile = function(path) {
  return parseText(fs.readFileSync(path, 'utf8'), path);
};


/**
 * @param {string} path
 * @return {!Promise<!ParseResult>}
 */
const parseFileAsync = exports.parseFileAsync = async function(path) {
  return new Promise((resolve, reject) => {
    fs.readFile(path, 'utf8', (err, data) => {
      if (err) {
        reject(err);
        return;
      }

      try {
        resolve(parseText(data, path));
      } catch (e) {
        reject(e);
      }
    });
  });
};


const MultipleSymbolsInClosureModuleError =
    exports.MultipleSymbolsInClosureModuleError = class extends SourceError {
  /**
   * @param {string} filePath
   */
  constructor(filePath) {
    super('Closure modules cannot contain more than one namespace.', filePath);
  }
};


const MultipleSymbolsInEs6ModuleError =
    exports.MultipleSymbolsInEs6ModuleError = class extends SourceError {
  /**
   * @param {string} filePath
   */
  constructor(filePath) {
    super('ES6 modules cannot contain more than one namespace.', filePath);
  }
};

/**
 * @return {!RegExp} A fresh regular expression object to test for
 *     goog.addDependnecy statements.
 */
function googAddDependency() {
  return /^goog\.addDependency\('(.*?)', (\[.*?\]), (\[.*?\])(?:, ({.*}|true|false))?\);$/mg;
}

/**
 * @param {string} fileContent
 * @return {boolean}
 */
function isDepFile(fileContent) {
  return googAddDependency().test(fileContent);
}

/**
 * @param {string} closureRelativePath
 * @param {string} providesText
 * @param {string} requiresText
 * @param {string} optionsText
 * @return {!depGraph.Dependency}
 */
const parseDependencyResult = function(
    closureRelativePath, providesText, requiresText, optionsText) {
  const provides = JSON.parse(providesText.replace(/'/g, '"'));
  const requires = JSON.parse(requiresText.replace(/'/g, '"'));
  const options = optionsText ? JSON.parse(optionsText.replace(/'/g, '"')) : {};

  let type = depGraph.DependencyType.SCRIPT;

  if (provides.length) {
    type = depGraph.DependencyType.CLOSURE_PROVIDE;
  }

  // true and false are legacy flags that mean a goog.module or not.
  if (options === true) {
    type = depGraph.DependencyType.CLOSURE_MODULE;
  } else if (options !== false) {
    switch (options.module) {
      case 'es6':
        type = depGraph.DependencyType.ES6_MODULE;
        break;
      case 'goog':
        type = depGraph.DependencyType.CLOSURE_MODULE;
        break;
      default:
        break;
    }
  }

  const imports = requires.map(
      // Assume if there is a slash it is an ES import, otherwise
      // goog.require. Any import should have a slash, except for those inside
      // the root of Closure Library (so just goog.js, which we also need to
      // check for). Admittedly not 100% accurate.
      (require) => require.indexOf('/') > -1 || require === 'goog.js' ?
          new depGraph.Es6Import(require) :
          new depGraph.GoogRequire(require));

  return new depGraph.ParsedDependency(
      type, closureRelativePath, provides, imports, options.lang);
};


/**
 * Parses a file that contains only goog.addDependency statements. This is regex
 * based to be lightweight and avoid addtional dependencies.
 *
 * @param {string} text
 * @param {string} filePath
 * @return {!ParseResult}
 */
const parseDependencyFile = function(text, filePath) {
  const dependencies = [];
  const errors = [];
  let regexResult;
  const regex = googAddDependency();

  while (regexResult = regex.exec(text)) {
    try {
      dependencies.push(parseDependencyResult(
          regexResult[1], regexResult[2], regexResult[3], regexResult[4]));
    } catch (e) {
      errors.push(new SourceError(e.toString(), filePath));
    }
  }

  return new ParseResult(
      dependencies, errors, ParseResult.Source.GOOG_ADD_DEPENDENCY);
};
exports.parseDependencyFile = parseDependencyFile;


/**
 * @param {string} text
 * @param {string} filePath
 * @return {!ParseResult}
 */
const parseText = exports.parseText = function(text, filePath) {
  const errors = [];

  /**
   * @param {boolean} fatal
   * @param {string} message
   * @param {string} sourceName
   * @param {number} line
   * @param {number} lineOffset
   */
  function report(fatal, message, sourceName, line, lineOffset) {
    errors.push(new ParseError(fatal, message, sourceName, line, lineOffset));
  }

  if (isDepFile(text)) {
    return parseDependencyFile(text, filePath, report);
  }

  const data = gjd(text, filePath, report);

  if (errors.some(e => e.fatal)) {
    return new ParseResult(
        [new depGraph.Dependency(
            depGraph.DependencyType.SCRIPT, filePath, [], [])],
        errors, ParseResult.Source.SOURCE_FILE);
  }

  function getLoadFlag(key, defaultValue) {
    if (data.load_flags) {
      for (const [k, v] of data.load_flags) {
        if (key === k) {
          return v;
        }
      }
    }
    return defaultValue;
  }

  const loadFlags = new Map(data.load_flags || []);
  const module = loadFlags.get('module');
  const language = loadFlags.get('lang') || 'es3';

  const imports = [];
  if (data.imported_modules) {
    data.imported_modules.forEach(r => imports.push(new depGraph.Es6Import(r)));
  }

  // The special `goog` symbol is implicitly required by files that use
  // goog primitives, and implicitly provided by base.js (in the output
  // produced by jsfile_parser). However, it should be omitted from
  // deps.js files, since the debug loader should never load base.js.

  if (data.requires) {
    data.requires.filter(r => r != 'goog')
        .forEach(r => imports.push(new depGraph.GoogRequire(r)));
  }

  const provides = (data.provides || []).filter(p => p != 'goog');

  let dependency;

  if (module == 'es6') {
    if (provides.length > 1) {
      throw new MultipleSymbolsInEs6ModuleError(filePath);
    }

    dependency = new depGraph.Dependency(
        depGraph.DependencyType.ES6_MODULE, filePath, provides, imports,
        language);
  } else if (module == 'goog') {
    if (provides.length > 1) {
      throw new MultipleSymbolsInClosureModuleError(filePath);
    }

    dependency = new depGraph.Dependency(
        depGraph.DependencyType.CLOSURE_MODULE, filePath, provides, imports,
        language);
  } else if (provides.length) {
    dependency = new depGraph.Dependency(
        depGraph.DependencyType.CLOSURE_PROVIDE, filePath, provides, imports,
        language);
  } else {
    dependency = new depGraph.Dependency(
        depGraph.DependencyType.SCRIPT, filePath, provides, imports, language);
  }

  return new ParseResult([dependency], errors, ParseResult.Source.SOURCE_FILE);
};
