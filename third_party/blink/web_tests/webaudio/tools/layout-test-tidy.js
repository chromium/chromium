#!/usr/bin/env node

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const os = require('os');
const fs = require('fs');
const path = require('path');
const glob = require('glob');
const libtidy = require('libtidy');
const cp = require('child_process');
const jsdom = require('jsdom');
const {JSDOM} = jsdom;


/**
 * Options for sub modules.
 */
const OPTIONS = {

  HTMLTidy: {
    'indent': 'yes',
    'indent-spaces': '2',
    'wrap': '80',
    'tidy-mark': 'no',
    'doctype': 'html5'
  },

  ClangFormat: ['-style=Chromium', '-assume-filename=a.js'],

  // RegExp text swap collection (ordered key-value pair) for post-processing.
  RegExpSwapCollection: [
    // Replace |var| with |let|.
    {regexp: /(\n\s{2,}|\(|^)var /, replace: '$1let '},

    // Move one line up the dangling closing script tags.
    {regexp: /\>\n\s{2,}\<\/script\>\n/, replace: '></script>\n'},

    // Remove all the empty lines in html.
    {regexp: /\>\n{2,}/, replace: '>\n'}
  ]
};


/**
 * Basic utilities.
 */
const Util = {

  logAndExit: (moduleName, messageString) => {
    console.error('[layout-test-tidy::' + moduleName + '] ' + messageString);
    process.exit(1);
  },

  loadFileToStringSync: (filePath) => {
    return fs.readFileSync(filePath, 'utf8').toString();
  },

  writeStringToFileSync: (pageString, filePath) => {
    fs.writeFileSync(filePath, pageString);
  }

};


/**
 * Wrapper for external modules like HTMLTidy and clang format.
 * @type {Object}
 */
const Module = {

  /**
   * Perform a batch RegExp string substitution.
   * @param {String} targetString Target string.
   * @param {Array} swapCollection Array of key-value pairs. Each item is an
   *                               object of { regexp_pattern: replace_string }.
   * @return {String}
   */
  runRegExpSwapSync: (targetString, regExpSwapCollection) => {
    let tempString = targetString;
    regExpSwapCollection.forEach((item) => {
      // Use 'global(g)' and 'multi-line(m)' options for RegExp processing.
      let re = new RegExp(item.regexp, 'gm');
      tempString = tempString.replace(re, item.replace);
    });

    return tempString;
  },

  /**
   * Run HTMLTidy on input string with options.
   * @param  {String} pageString [description]
   * @param  {Object} options HTMLTidy option as key-value pair.
   * @param  {Task} task Associated Task object.
   * @return {String}
   */
  runHTMLTidySync: (pageString, options, task) => {
    let tidyDoc = new libtidy.TidyDoc();
    for (let option in options)
      tidyDoc.optSet(option, options[option]);

    // This actually process the data inside of |tidyDoc|.
    let logs = '';
    logs += tidyDoc.parseBufferSync(Buffer(pageString));
    logs += tidyDoc.cleanAndRepairSync();
    logs += tidyDoc.runDiagnosticsSync();

    task.addLog('Module.runHTMLTidySync', logs.split('\n'));

    return tidyDoc.saveBufferSync().toString();
  },

  /**
   * Run clang-format and return a promise.
   * @param {String} codeString JS code to apply clang-format.
   * @param {Array} clangFormatOption options array for clang-format.
   * @param {Number} indentLevel Code indentation level.
   * @param {Task} task Associated Task object.
   * @return {Promise} Processed code as string.
   * @resolve {String} clang-formatted JS code as string.
   * @reject {Error}
   */
  runClangFormat: (codeString, clangFormatOption, indentLevel, task) => {
    let clangFormatBinary = __dirname + '/node_modules/clang-format/bin/';
    clangFormatBinary += (os.platform() === 'win32') ?
        'win32/clang-format.exe' :
        os.platform() + '_' + os.arch() + '/clang-format';

    if (indentLevel > 0) {
      codeString =
          '{'.repeat(indentLevel) + codeString + '}'.repeat(indentLevel);
    }

    return new Promise((resolve, reject) => {
      // Be sure to pipe the result to the child process, not to this process's
      // stdout.
      let result = '';
      let clangFormat = cp.spawn(
          clangFormatBinary, clangFormatOption,
          {stdio: ['pipe', 'pipe', process.stderr]});

      // Capture the data when it's arrived at the pipe.
      clangFormat.stdout.on('data', (data) => {
        result += data;
      });

      // For debug purpose:
      // clangFormat.stdout.pipe(process.stdout);

      clangFormat.stdout.on('close', (exitCode) => {
        if (exitCode) {
          Util.logAndExit('Module.runClangFormat', 'exit code = 1');
        } else {
          task.addLog('Module.runClangFormat', 'clang-format was successful.');

          // Remove shim braces for indentation hack.
          if (indentLevel > 0) {
            let codeStart = 0;
            let codeEnd = result.length - 1;
            for (let i = 0; i < indentLevel; ++i) {
              codeStart = result.indexOf('\n', codeStart + 1);
              codeEnd = result.lastIndexOf('\n', codeEnd - 1);
            }
            result = result.substring(codeStart + 1, codeEnd);
          }

          resolve(result);
        }
      });

      clangFormat.stdin.setEncoding('utf-8');
      clangFormat.stdin.write(codeString);
      clangFormat.stdin.end();
    });
  },

  /**
   * Detect line overflow and record the line number to the task log.
   * @param {String} pageOrCodeString HTML page or JS code data in string.
   * @param {TidyTask} task Associated TidyTask object.
   */
  detectLineOverflow: (pageOrCodeString, task) => {
    let currentLineNumber = 0;
    let index0 = 0;
    let index1 = 0;
    while (index0 < pageOrCodeString.length - 1) {
      index1 = pageOrCodeString.indexOf('\n', index0);
      if (index1 - index0 > 80) {
        task.addLog(
            'Module.detectLineOverflow',
            'Overflow (> 80 cols.) at line ' + currentLineNumber + '.');
      }
      currentLineNumber++;
      index0 = index1 + 1;
    }
  }

};


/**
 * DOM utilities. Process DOM processing after parsing the string by JSDOM.
 */
const DOMUtil = {

  /**
   * Parse string, generate JSDOM object and return |document| element.
   * @param {String} pageString An HTML page in string.
   * @return {Document} A |document| object.
   */
  getJSDOMFromStringSync: (pageString) => {
    return new JSDOM(`${pageString}`);
    // return jsdom_.window.document;
  },

  /**
   * In-place tidy up head element.
   * @param {Document} document A |document| object.
   * @param {Task} task An associated Task object.
   * @return {Void}
   */
  tidyHeadElementSync: (document, task) => {
    try {
      // If the title is missing, add one from the file name.
      let titleElement = document.querySelector('title');
      if (!titleElement) {
        titleElement = document.createElement('title');
        titleElement.textContent = path.basename(task.targetFilePath_);
        task.addLog(
            'DOMUtil.tidyHeadElementSync',
            'Title element was missing thus a new one was added.');
      }

      // The title element should be the first.
      let headElement = document.querySelector('head');
      headElement.insertBefore(titleElement, headElement.firstChild);

      // If a script element in body does not have JS code, move to the head
      // section.
      let scriptElementsInBody = document.body.querySelectorAll('script');
      scriptElementsInBody.forEach((scriptElement) => {
        if (!scriptElement.textContent)
          headElement.appendChild(scriptElement);
      });
    } catch (error) {
      task.addLog('DOMUtil.tidyHeadElementSync', error.toString());
    }
  },

  /**
   * Sanitize and extract |script| element with JS test code.
   * @param {Document} document A |document| object.
   * @param {Task} task An associated Task object.
   * @return {ScriptElement}
   */
  getElementWithTestCodeSync: (document, task) => {
    let numberOfScriptElementsWithCode = 0;
    let scriptElementWithTestCode;
    let scriptElements = document.querySelectorAll('script');

    scriptElements.forEach((scriptElement) => {
      // We don't want type attribute.
      scriptElement.removeAttribute('type');

      if (scriptElement.textContent.length > 0) {
        ++numberOfScriptElementsWithCode;
        scriptElementWithTestCode = scriptElement;
        scriptElement.id = 'layout-test-code';
        // If the element belongs to something else other than body, move it to
        // the body. This fixes script elements that are located in weird
        // positions. (e.g outside of body or head)
        if (scriptElement.parentElement !== document.body)
          document.body.appendChild(scriptElement);
      }
    });

    if (numberOfScriptElementsWithCode !== 1) {
      task.addLog(
          'DOMUtil.getElementWithTestCodeSync',
          numberOfScriptElementsWithCode + ' <script> element(s) with JS ' +
              'code were found.');
      scriptElementWithTestCode = null;
    }

    return scriptElementWithTestCode;
  }

};


/**
 * @class TidyTask
 * @description Per-file processing task. This object should be constructed
 *              directly. The task runner creates this when it is necessary.
 */
class TidyTask {
  /**
   * @param {String} targetFilePath A path to file to be processed.
   * @param {Object} options Task options.
   * @param {Boolean} options.inplace |true| for in-place processing directly
   *                                  writing into the target file. By default,
   *                                  this is |false| and the result is piped
   *                                  into the stdout.
   * @param {Boolean} options.verbose Prints out warnings and logs from the
   *                                  process when |true|. |false| by default.
   */
  constructor(targetFilePath, options) {
    this.targetFilePath_ = targetFilePath;
    this.options_ = options;

    this.fileType_ = path.extname(this.targetFilePath_);
    this.pageString_ = Util.loadFileToStringSync(this.targetFilePath_);
    this.jsdom_ = null;
    this.logs_ = {};
  }

  /**
   * Run processing sequence. Don't call this directly.
   * @param {Function} taskDone Task runner callback function.
   */
  run(taskDone) {
    switch (this.fileType_) {
      case '.html':
        this.processHTML_(taskDone);
        break;
      case '.js':
        this.processJS_(taskDone);
        break;
      default:
        Util.logAndExit(
            'TidyTask.constructor', 'Invalid file type: ' + this.fileType_);
        break;
    }
  }

  /**
   * Process HTML file. The processing performs the following in order:
   *  - DOM parsing to sanitize invalid/incorrect markup structure.
   *  - Extract JS code, apply clang-format and inject the code to element.
   *  - Apply HTMLTidy to the markup.
   *  - RegExp substitution.
   *  - Detect any line overflows 80 columns.
   * @param  {Function} taskDone completion callback.
   */
  processHTML_(taskDone) {
    // Parse page string into JSDOM.element object.
    this.jsdom_ = DOMUtil.getJSDOMFromStringSync(this.pageString_);

    // Clean up the head element section.
    DOMUtil.tidyHeadElementSync(this.jsdom_.window.document, this);

    let scriptElement =
        DOMUtil.getElementWithTestCodeSync(this.jsdom_.window.document, this);

    if (!scriptElement)
      Util.logAndExit('TidyTask.processHTML_', 'Invalid <script> element.');

    // Start with clang-foramt, then HTMLTidy and RegExp substitution.
    Module
        .runClangFormat(scriptElement.textContent, OPTIONS.ClangFormat, 3, this)
        .then((formattedCodeString) => {
          // Replace the original code with clang-formatted code.
          scriptElement.textContent = formattedCodeString;

          // Then tidy the text data from JSDOM. After this point, DOM
          // manipulation is not possible anymore.
          let pageString = this.jsdom_.serialize();
          pageString =
              Module.runHTMLTidySync(pageString, OPTIONS.HTMLTidy, this);
          pageString = Module.runRegExpSwapSync(
              pageString, OPTIONS.RegExpSwapCollection);

          // Detect any line goes over column 80.
          Module.detectLineOverflow(pageString, this);

          this.finish_(pageString, taskDone);
        });
  }

  /**
   * Process JS file. The processing performs the following in order:
   *  - Extract JS code, apply clang-format and inject the code to element.
   *  - RegExp substitution.
   *  - Detect any line overflows 80 columns.
   * @param  {Function} taskDone completion callback.
   */
  processJS_(taskDone) {
    // The file is a JS code: run clang-format, RegExp substitution and check
    // for overflowed lines.
    Module.runClangFormat(this.pageString_, OPTIONS.ClangFormat, 0, this)
        .then((formattedCodeString) => {
          formattedCodeString = Module.runRegExpSwapSync(
              formattedCodeString, [OPTIONS.RegExpSwapCollection[0]]);
          Module.detectLineOverflow(formattedCodeString, this);
          this.finish_(formattedCodeString, taskDone);
        });
  }

  finish_(resultString, taskDone) {
    if (this.options_.inplace) {
      Util.writeStringToFileSync(resultString, this.targetFilePath_);
    } else {
      process.stdout.write(resultString);
    }

    this.printLog();
    taskDone();
  }

  /**
   * Adding log message.
   * @param {String} location Caller information.
   * @param {String} message Log message.
   */
  addLog(location, message) {
    if (!this.logs_.hasOwnProperty(location))
      this.logs_[location] = [];
    this.logs_[location].push(message);
  }

  /**
   * Print log messages at the end of task.
   */
  printLog() {
    if (!this.options_.verbose)
      return;

    console.warn('> Logs from: ' + this.targetFilePath_);
    for (let location in this.logs_) {
      console.warn('  [] ' + location);
      this.logs_[location].forEach((message) => {
        if (Array.isArray(message)) {
          message.forEach((subMessage) => {
            if (subMessage.length > 0)
              console.warn('     - ' + subMessage);
          });
        } else {
          console.warn('     - ' + message);
        }
      });
    }
  }
}


/**
 * @class  TidyTaskRunner
 */
class TidyTaskRunner {
  /**
   * @param {Array} files A list of file paths.
   * @param {Object} options Task options.
   * @param {Boolean} options.inplace |true| for in-place processing directly
   *                                  writing into the target file. By default,
   *                                  this is |false| and the result is piped
   *                                  into the stdout.
   * @param {Boolean} options.verbose Prints out warnings and logs from the
   *                                  process when |true|. |false| by default.
   * @return {TidyTaskRunner} A task runner object.
   */
  constructor(files, options) {
    this.targetFiles_ = files;
    this.options_ = options;
    this.tasks_ = [];
    this.currentTask_ = 0;
  }

  startProcessing() {
    this.targetFiles_.forEach((filePath) => {
      this.tasks_.push(new TidyTask(filePath, this.options_));
    });
    this.log_('Task runner started: ' + this.targetFiles_.length + ' file(s).');
    this.runTask_();
  }

  runTask_() {
    this.log_(
        'Running task #' + (this.currentTask_ + 1) + ': ' +
        this.targetFiles_[this.currentTask_] +
        (this.options_.inplace ? ' (IN-PLACE)' : ''));
    this.tasks_[this.currentTask_].run(this.done_.bind(this));
  }

  done_() {
    this.log_('Task #' + (this.currentTask_ + 1) + ' completed.');
    this.currentTask_++;
    if (this.currentTask_ < this.tasks_.length) {
      this.runTask_();
    } else {
      this.log_(
          'Task runner completed: ' + this.targetFiles_.length +
          ' file(s) processed.');
    }
  }

  log_(message) {
    if (this.options_.verbose)
      console.warn('[layout-test-tidy] ' + message);
  }
}


// Entry point.
function main() {
  let args = process.argv.slice(2);

  // Extract options from the arguments.
  let optionArgs = args.filter((arg, index) => {
    if (arg.startsWith('-') || arg.startsWith('--')) {
      args[index] = null;
      return true;
    }
  });

  args = args.filter(arg => arg);

  // Populate options flags.
  let options = {
    inplace: optionArgs.includes('-i') || optionArgs.includes('--inplace'),
    recursive: optionArgs.includes('-R') || optionArgs.includes('--recursive'),
    verbose: optionArgs.includes('-v') || optionArgs.includes('--verbose'),
  };

  // Collect target file(s) from the file system.
  let files = [];
  args.forEach((targetPath) => {
    try {
      let stat = fs.lstatSync(targetPath);
      if (stat.isFile()) {
        let fileType = path.extname(targetPath);
        if (fileType === '.html' || fileType === '.js') {
          files.push(targetPath);
        }
      } else if (
          stat.isDirectory() && options.recursive &&
          !targetPath.includes('node_modules')) {
        files = files.concat(glob.sync(targetPath + '/**/*.{html,js}'));
      }
    } catch (error) {
      let errorMessage = 'Invalid file path. (' + targetPath + ')\n' +
          '  > ' + error.toString();
      Util.logAndExit('main', errorMessage);
    }
  });

  if (files.length > 0) {
    let taskRunner = new TidyTaskRunner(files, options);
    taskRunner.startProcessing();
  } else {
    Util.logAndExit('main', 'No files to process.');
  }
}

main();
