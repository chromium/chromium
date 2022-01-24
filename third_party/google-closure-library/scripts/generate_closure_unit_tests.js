// Copyright 2017 The Closure Library Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

const parse5 = require('parse5');
const fs = require('fs');
const path = require('path');
const process = require('process');

/**
 * @typedef {{
 *   basePath: string,
 *   depsFile: string,
 *   output: string,
 *   overwriteExistingFiles: boolean,
 *   paths: !Array<{path: string, directory: boolean}>,
 *   recursive: boolean,
 * }}
 */
let Args;


/**
 * Entry point for this script.
 */
function main() {
  let args;

  try {
    args = processArgs(process.argv.slice(2));
  } catch (error) {
    showHelp(error.message);
  }

  for (const path of args.paths) {
    if (path.directory) {
      generateHtmlForDirectory(path.path, args);
    } else {
      maybeGenerateHtmlForFile(path.path, args);
    }
  }
}


/**
 * Shows script help.
 * @param {string=} errorMessage
 */
function showHelp(errorMessage = '') {
  if (errorMessage) {
    console.error(errorMessage);
  }

  console.log(`Auto generates _test.html files for Closure unit tests.

      Usage:
        generate_test_html [OPTIONS]... PATH...

      Paths:
        list of paths to directories or _test.js files. For directories a
        _test.html file will be generated for each _test.js file in the
        directory.

      Options:
        --base
          Path to base.js file.
        --dep_file:
          Optional path to a deps file to use for all tests.
        --recursive
          generate _test.html for each _test.js in each directory recursively.
          Defaults to false.
        --output
          output file path, should be named _test.html. Only valid if a single
          _test.js file is specified for PATH.
        --overwrite
          overwrite any existing _test.html files. Defaults to true.`);
  process.exit(errorMessage ? 1 : 0);
}

/**
 * @param {!Array<string>} args
 * @return {!Args}
 */
function processArgs(args) {
  const processedArgs = {
    basePath: '',
    output: '',
    overwriteExistingFiles: true,
    paths: [],
    recursive: false,
  };
  args.forEach((arg) => {
    const [key, value] = arg.split('=');
    switch (key) {
      case '--base':
        processedArgs.basePath = value;
        break;
      case '--dep_file':
        processedArgs.depsFile = value;
        break;
      case '--help':
        showHelp();
        break;
      case '--recursive':
        processedArgs.recursive = String(value).toLowerCase() !== 'false';
        break;
      case '--output':
        processedArgs.output = value;
        break;
      case '--overwrite':
        processedArgs.overwriteExistingFiles =
            String(value).toLowerCase() !== 'false';
        break;
      default:
        const stats = fs.statSync(arg);
        processedArgs.paths.push({path: arg, directory: stats.isDirectory()});
    }
  });

  if (!processedArgs.basePath.endsWith('base.js')) {
    throw new Error(
        `Path to base must end with base.js: ${processedArgs.basePath}.`);
  }

  if (processedArgs.output) {
    if (processedArgs.paths.length > 1) {
      throw new Error(
          'Cannot specify an output file when there is more than one path.');
    }
    if (processedArgs.paths.some(path => path.directory)) {
      throw new Error('Cannot specify an output when path is a directory.');
    }
    if (!processedArgs.output.endsWith('_test.html')) {
      throw new Error('Output file should end with _test.html.');
    }
  }

  if (processedArgs.depsFile && !fs.existsSync(processedArgs.depsFile)) {
    throw new Error(
        'Specified deps file does not exist: ' + processedArgs.depsFile);
  }

  return processedArgs;
}


/**
 * Generates one _test.html file for each _test.js file in the given directory.
 *
 * @param {string} directory Directory to generate files for.
 * @param {!Args} args
 */
function generateHtmlForDirectory(directory, args) {
  const directoryContents = fs.readdirSync(directory);
  const subDirectories = [];

  for (const filename of directoryContents) {
    const fullname = path.join(directory, filename);
    const stats = fs.statSync(fullname);
    if (stats.isDirectory() && args.recursive) {
      subDirectories.push(fullname);
    } else if (stats.isFile()) {
      maybeGenerateHtmlForFile(fullname, args);
    }
  }

  for (const subDirectory of subDirectories) {
    generateHtmlForDirectory(subDirectory, args);
  }
}


/**
 * Generates a _test.html file for the given file if it is a _test.js file.
 * @param {string} filename Full path to the file.
 * @param {!Args} args
 */
function maybeGenerateHtmlForFile(filename, args) {
  if (!filename.endsWith('_test.js')) {
    return;
  }

  const htmlFilename =
      args.output || filename.replace('_test.js', '_test.html');
  if (!args.overwriteExistingFiles && fs.existsSync(htmlFilename)) {
    console.warn(`"${htmlFilename}" exists - skipping.`);
    return;
  }

  const originalJs = fs.readFileSync(filename, 'utf8');
  const provide = /goog\.(?:provide|module)\('([^']*?)'\);/g.exec(originalJs);

  if (!provide || !provide[1]) {
    console.error(
        `File ${filename} does not provide or module the tests, ` +
        'cannot generate html.');
    return;
  }

  const newJS = `goog.require('${provide[1]}');`;
  const title = `Closure Unit Tests - ${provide[1]}`;

  const baseFileName = filename.replace('_test.js', '');
  const testDomFilename = baseFileName + '_test_dom.html';
  const testDom = fs.existsSync(testDomFilename) ?
      fs.readFileSync(testDomFilename, 'utf8') :
      '';

  const testBootstrapFilename = baseFileName + '_test_bootstrap.js';
  const pathToBootstrap = fs.existsSync(testBootstrapFilename) ?
      path.basename(testBootstrapFilename) :
      '';

  const pathToBase = path.relative(path.dirname(htmlFilename), args.basePath);
  const pathToDeps = args.depsFile ?
      path.relative(path.dirname(htmlFilename), args.depsFile) :
      '';

  const html = createHtml(
      title, pathToBootstrap, pathToDeps, newJS, testDom, pathToBase);
  fs.writeFileSync(htmlFilename, html);
}

/**
 * @param {string} title Title of the test.
 * @param {string} pathToBootstrap Path to a bootstrap javascript file to run
 *     first, if any. Generally this file will contain closure defines.
 * @param {string} pathToDeps Path to a custom deps file, if any.
 * @param {string} js Script content of the test.
 * @param {string} testDom Any test DOM related to the test or the empty string
 *     if none.
 * @param {string} pathToBase Path to the base.js file.
 * @return {string} The text content of the test HTML file.
 */
function createHtml(
    title, pathToBootstrap, pathToDeps, js, testDom, pathToBase) {
  // Use parse5 to parse and reserialize the test dom. This generates any
  // optional tags (html, body, head) if missing. Meaning test doms can specify
  // these tags, if needed.
  return parse5.serialize(parse5.parse(`<!DOCTYPE html>
<!-- DO NOT EDIT. This file auto-generated by generate_closure_unit_tests.js -->
<!--
Copyright 2017 The Closure Library Authors. All Rights Reserved.

Use of this source code is governed by the Apache License, Version 2.0.
See the COPYING file for details.
-->
<meta charset="UTF-8" />
${pathToBootstrap ? `<script src=${pathToBootstrap}></script>` : ''}
<script src="${pathToBase}"></script>
${pathToDeps ? `<script src=${pathToDeps}></script>` : ''}
<script>${js}</script>
<title>${title}</title>` + testDom));
}


if (require.main === module) {
  main();
}
