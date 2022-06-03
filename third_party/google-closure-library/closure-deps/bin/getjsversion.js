#!/usr/bin/env node
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

/**
 * @fileoverview Reads JS from stdin and then outputs the language version of
 * the given JS (e.g. "es6").
 */

const parser = require('../lib/parser');
const readline = require('readline');

const rl = readline.createInterface({input: process.stdin});

const lines = [];

rl.on('line', (line) => {
  lines.push(line);
});

rl.on('close', () => {
  const result = parser.parseText(lines.join('\n'), 'stdin');
  let fatal = false;
  for (const error of result.errors) {
    fatal = fatal || error.fatal;
    console.error(error.toString());
  }
  if (!fatal) {
    console.log(result.dependencies[0].language);
  }
});
