# zxcvbn-cpp

This is a C++ port of [`zxcvbn`](https://github.com/dropbox/zxcvbn),
an advanced password strength estimation library. For more details on how
`zxcvbn` works and its advantages, check out
[the blog post](https://tech.dropbox.com/2012/04/zxcvbn-realistic-password-strength-estimation/).

This port is a direct translation of the original CoffeeScript
source. This allows this port to easily stay in sync with the original
source. Additionally, this port uses the same exact test scripts from
the original with the help of emscripten.

This port also provides C, Python, and JS bindings from the same
codebase.

## Python Bindings

### Build

```shell
$ python setup.py install
```

### Use

```python
>>> import zxcvbncpp
>>> print(zxcvbncpp.password_strength("Tr0ub4dour&3"))
```

## JS Bindings

### Build

Building the JS bindings requires a POSIX environment, including
`make`, and [Emscripten](https://emscripten.org/).

First make sure `emcc` is in your `$PATH`. You can do so using the
Emscripten Portable SDK as follows:

```shell
$ source /path/to/emsdk_portable/emsdk_env.sh
```

Then simply run:

```shell
$ RELEASE=1 make -f jsmakefile lib/zxcvbn.js
```

### Use

Add this script to your `index.html`:

``` html
<script src="path/to/zxcvbn.js"></script>
```

To make sure it loaded properly, open in a browser and type
`zxcvbn('Tr0ub4dour&3')` into the console. For more information on how
to use the JS port see the
[original documentation](https://github.com/dropbox/zxcvbn#usage).

### Use From Node

Usage from node is straight-forward:

```javascript
var zxcvbn = require("./path/to/zxcvbn.js");
console.log(zxcvbn("Tr0ub4dour&3"));
```

## How to build for your C/C++ project

Adapt these instructions to your build environment.

First generate adjacency graphs and frequency lists:

```shell
$ python ./data-scripts/build_frequency_lists.py ./data ./native-src/zxcvbn _frequency_lists.hpp
$ python ./data-scripts/build_frequency_lists.py ./data ./native-src/zxcvbn _frequency_lists.cpp
$ python ./data-scripts/build_keyboard_adjacency_graphs.py ./native-src/zxcvbn/adjacency_graphs.hpp
$ python ./data-scripts/build_keyboard_adjacency_graphs.py ./native-src/zxcvbn/adjacency_graphs.cpp
```

Add `/absolute_path/to/zxcvbn-repo/native-src` to your include path,
then build all the `.cpp` files in
`/absolute_path/to/zxcvbn-repo/native-src/zxcvbn`. Make sure you
use the `-std=c++14` compiler flag.

## Testing

`zxcvbn-cpp` uses the test scripts from the original codebase, this
makes it easy to verify that it is 100% compatible with the original.
In addition to requiring a POSIX environment and Emscripten, testing
also requires a NodeJS environment. Here's how you set it up:

```shell
$ npm install
```

Then to run the tests:

```shell
$ make -f jsmakefile test
```

## Development

Bug reports and pull requests welcome!

Please note `zxcvbn-cpp` is written using modern C++14 techniques, no
passing around stray pointers!
