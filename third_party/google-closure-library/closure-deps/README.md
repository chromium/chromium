# Closure Dependencies [![Build Status](https://travis-ci.org/google/closure-library.svg?branch=master)](https://travis-ci.org/google/closure-library)

This is a separate utility node package for Closure Library related to
dependency management with Closure files.

For more information on Closure Library visit the
[Google Developers](https://developers.google.com/closure/library) or
[GitHub](https://github.com/google/closure-library) sites.

This is meant to replace similar Python scripts that [already
exist](https://github.com/google/closure-library/tree/master/closure/bin) in
Closure Library. We're JavaScript deps and should be providing tooling written
in JavaScript, where possible! Those Python tools will continue to exist but
will not be updated and should be considered deprecated.

## Installation

Install via npm:

```
npm install google-closure-deps
```

To use the CLI below from any directory install the package globally:

```
npm install -g google-closure-deps
```

## Command Line Interface

### closure-make-deps

`closure-make-deps` is a utility to produce a dependency file for Closure
Library's debug code loader. Closure Library is capable of loading code in a web
browser or in Node but must know the dependency graph ahead of time. Generally
this is done by loading Closure's [base.js] file and then loading a dependency
file containing repeated calls to goog.addDependency`. Closure comes bundled
with a file named [deps.js] for itself and is capable of auto loading this file.

[base.js]: https://github.com/google/closure-library/blob/master/closure/goog/base.js
[deps.js]: https://github.com/google/closure-library/blob/master/closure/goog/deps.js

### get-js-version

`get-js-version` is a generally useful utility that determines the highest level
version of a JavaScript program from stdin. The output format is determined by
the Closure Compiler's FeatureSet#version method and should match what
`goog.addDependency` expects for a `lang` load flag.

```
$ echo "const foo = 0;" | get-js-version
es6
```

## Library

This Node module also exposes standard functions to parse Closure files and
retrieve a dependency graph.

This is an in-code example for clarity, but there are also functions to parse
files rather than strings.

```javascript
const {parser, depGraph} = require('google-closure-deps');

// A file that provides "goog" is required for any file that references Closure.
// Usually this is Closure's base.js file.
const goog = parser.parseText('/** @provideGoog */', '/base.js').dependencies;

const firstFile =
    parser.parseText(`goog.module('first.module')`, '/first.js').dependencies;
const secondFile = parser.parseText(`
goog.module('second.module');
const firstModule = goog.require('first.module');
`, '/second.js').dependencies;
const graph = new depGraph.Graph([...goog, ...firstFile, ...secondFile]);
graph.order(...secondFile); // [goog, firstFile, secondFile]
graph.depsBySymbol.get('first.module'); // firstFile
graph.depsByPath.get('/second.js'); // secondFile
```

This also supports parsing ES6 modules now that Closure Library has support for
them!

```javascript
const {parser, depGraph} = require('google-closure-deps');

// A file that provides "goog" is required for any file that references Closure.
// Usually this is Closure's base.js file.
const goog = parser.parseText('/** @provideGoog */', '/base.js').dependencies;

const firstFile = parser.parseText(
`
goog.declareModuleId('first.module');
export const FOO = 'foo';
`, "/first.js").dependencies;

const secondFile = parser.parseText(
    'import {FOO} from "./first.js";',
    '/second.js').dependencies;

const thirdFile = parser.parseText(
`
goog.module("third.module");
const firstModule = goog.require("first.module");
`, '/third.js').dependencies;

const graph = new depGraph.Graph([...goog, ...firstFile, ...secondFile, ...thirdFile]);
graph.order(...secondFile, ...thirdFile); // [goog, firstFile, secondFile, thirdFile]
```

