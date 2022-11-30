---
title: Getting Started with Closure Library
section: develop
layout: article
---


<!-- Documentation licensed under CC BY 4.0 -->
<!-- License available at https://creativecommons.org/licenses/by/4.0/ -->

# Getting Started with Closure Library

This getting started guide is designed to teach you some fundamental Closure
development concepts in the context of creating your first Closure application.
This guide also provides the steps common to a Closure development workflow.

[TOC] <!-- MOE:strip_line -->


## Download Closure library

In your working directory (referred to later as `path/to/myapp/`), the following
commands initialize an npm package workspace and installs Closure Library as a
dependency:

```sh
npm init -y
npm install google-closure-library
```

Closure Library sources are downloaded to `node_modules/google-closure-library`.

## Download `google-closure-deps`

`google-closure-deps` is a set of Node.js CLIs to create `deps.js` files for
your application. `deps.js` is a file that maps out the dependency tree in your
project, and is used for _uncompiled_ code.

To install it:

```sh
npm install --save-dev google-closure-deps
```

The `--save-dev` is optional, and indicates that this is a development (not
runtime) dependency.

You will use this later to generate a `deps.js` file for your application.

## Create Closure JavaScript

Create a `hello.js` JavaScript file in `path/to/myapp/` with the
following contents:

```js
/**
 * @fileoverview Closure getting started tutorial code example.
 */
goog.module('hello');

const {TagName, createDom} = goog.require('goog.dom');

/**
 * Appends an `h1` tag to the body with the message "Hello world!".
 */
function sayHi() {
  const newHeader = createDom(TagName.H1, {'style': 'background-color:#EEE'},
    'Hello world!');
  document.body.appendChild(newHeader);
}

sayHi();
```

The `hello.js` JavaScript calls two Closure Library functions:
`goog.dom.createDom()` and `goog.dom.appendChild()`. These functions are defined
in Closure Library's `closure/goog/dom/dom.js` file. The next steps
explains how the correct files are loaded to provide access these functions.



## Create `deps.js`

Use the `closure-make-deps` tool found in the previously-mentioned
`google-closure-deps` package to generate `deps.js`:

```sh
$(npm bin)/closure-make-deps \
  -f hello.js \
  -f node_modules/google-closure-library/closure/goog/deps.js \
  --closure-path node_modules/google-closure-library/closure/goog \
  > deps.js
```

Here's breakdown of the command:
  * `-f hello.js`: Include `hello.js` as a source. As your project grows larger,
    you may want to use `--root` to include an entire directory. Be sure not to
    include node_modules/ (you can use the `--exclude` flag) for that.
  * `-f node_modules/google-closure-library/closure/goog/deps.js`: Include
    Closure's own deps.js file. This helps the tool figure out which files
    correspond to namespaces like `goog.dom`.
  * `--closure-path (...)/goog`: Indicates where Closure library is located.

To learn about the flags accepted by this tool, run:

```sh
$(npm bin)/closure-make-deps --help
```

## Create an HTML file

Create a `hello.html` file in `path/to/myapp/` with the following contents:

```html
<!DOCTYPE html>
<meta charset="UTF-8">
<script src="node_modules/google-closure-library/closure/goog/base.js"></script>
<script src="deps.js"></script>
<div>Content to be rendered below</div>
<script>
  goog.require('hello');
</script>
```


The first thing this HTML file does is load `base.js`, which defines the
function `goog.require` and other core functionality. The file then loads
 `deps.js` later, which gives Closure's source loader (called the
"debug loader") information about where dependency files are located. This
allows `hello.js` to be evaluated once `goog.require('hello')` is called.

The Closure Library loads required files by dynamically adding a script tag to
the document for each needed Closure Library file.


## Test your app with a web server
Use a simple HTTP server, such as Python's [http.server module](https://docs.python.org/3/library/http.server.html)
to test your application.

For example, start a simple server in your project directory with:

```sh
# Node.js
npx http-server
# Python 2
python -m SimpleHTTPServer 8080
# Python 3
python -m http.server 8080
```

And visit http://localhost:8080/path/to/myapp/hello.html.


## Compile your app

Use Closure Compiler to compile your application when your app is ready for
production. Closure Compiler (also known as JSCompiler) creates a single-file
bundle of your application. This file includes all the dependencies your
application needs, but shortens identifier names where possible to minimize the
size of the code and the time it takes to transmit over the network. In
addition, Closure Compiler performs type checking, removes dead code, and
applies optimizations.

To compile your application:

1. Download and install the latest release of Closure Compiler from npm:

    ```
    npm install --save-dev google-closure-compiler
    ```

The `--save-dev` is optional, and indicates that this is a develop-time (not
runtime) dependency.

2. Execute the compiler on your JavaScript file:

    ```sh
    $(npm bin)/google-closure-compiler \
      --js hello.js \
      --js node_modules/google-closure-library/**/*.js \
      --dependency_mode=PRUNE \
      --entry_point=goog:hello \
      --js_output_file hello_compiled.js
    ```

    **Note:** You can compile multiple JavaScript files at once using the Closure
    Compiler. For example:

    ```sh
    $(npm bin)/google-closure-compiler --js_output_file=out.js in1.js in2.js in3.js ...
    ```

3. Reference the compiled file in your HTML file without using
`base.js` to load the dependencies individually (`hello_compiled.js` includes
`hello.js` as well as all of its requirements, in compiled form):

```html
<!DOCTYPE html>
<meta charset="UTF-8">
<div>Content to be rendered below</div>
<script src="/hello_compiled.js"></script>
```

Refer to [Getting Started with Closure Compiler](https://github.com/google/closure-compiler#getting-started)
for more information on the Closure Compiler.


