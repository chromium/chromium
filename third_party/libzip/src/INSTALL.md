# Building libzip

This guide shows how to build libzip from source. It assumes basic familiarity with using the command line and building software from source.

This guide uses cmake to invoke all required commands to be platform-independent. You can, of course, also use the underlying commands directly.


## Dependencies

libzip uses [cmake](https://cmake.org) to build.

You'll need [zlib](http://www.zlib.net/) (at least version 1.1.2). It
comes with most operating systems.

For supporting bzip2-compressed zip archives, you need
[bzip2](http://bzip.org/).

For supporting lzma- and xz-compressed zip archives, you need
[liblzma](https://tukaani.org/xz/) which is part of xz, at least version 5.2.

For supporting zstd-compressed zip archives, you need
[zstd](https://github.com/facebook/zstd/).

For AES (encryption) support, you need one of these cryptographic libraries,
listed in order of preference:

- Apple's CommonCrypto (available on macOS and iOS)
- Microsoft Windows Cryptography Framework
- [OpenSSL](https://www.openssl.org/) >= 1.0.
- [GnuTLS](https://www.gnutls.org/) and [Nettle](https://www.lysator.liu.se/~nisse/nettle/) (at least nettle 3.0)

If you don't want to use a library even if it is installed, you can
pass `-DENABLE_<LIBRARY>=OFF` to cmake, where `<LIBRARY>` is one of
`COMMONCRYPTO`, `GNUTLS`, or `OPENSSL`.

For running the tests, you need to have
[Python](https://www.python.org/) and
[nihtest](https://pypi.org/project/nihtest/) installed.

For code coverage report of the test suite, you need to have [lcov](https://github.com/linux-test-project/lcov) installed.

All dependencies except cmake and zlib are optional. If they are not found, the corresponding features will be disabled, but libzip will still build and work.

How to install the dependencies depends on your operating system. On Linux, you can usually install them via your package manager. On macOS, you can use Homebrew or Mac Ports. On Windows, you can use vcpkg. Details on how to use these package managers is beyond the scope of this guide, but should be easy to find online.


## Building libzip

### 1. Download the source code.

The newest release can be found on the [download page](download/) or on [GitHub](https://github.com/nih-at/libzip/releases/latest).


### 2. Unpack the source code and navigate to the source directory.

The version in the file name may differ, adjust the command accordingly.

```sh
cmake -E tar xf libzip-1.14.1.tar.xz
cd libzip-1.14.1
```


### 3. Create a build directory and navigate to it.

```sh
cmake -E make_directory build
cd build
```


### 4. Run cmake to configure the build system.

```sh
cmake ..
```
If you want to customize the build, you can pass additional parameters to cmake after `..` in the form of `-Dparameter=value`. See below for some useful parameters:

`BUILD_SHARED_LIBS`
: set to `ON` or `OFF` to enable/disable building of shared libraries, defaults to `ON`

`CMAKE_INSTALL_PREFIX`
: for setting the installation path
  
`DOCUMENTATION_FORMAT`
: choose one of `man`, `mdoc`, and `html` for  the installed documentation (default: decided by cmake depending on available tools)
  
`LIBZIP_DO_INSTALL`
: If you include libzip as a subproject, link it  statically and do not want to let it install its files, set this variable to `OFF`. Defaults to `ON`.

If you want to compile with custom `CFLAGS`, set them in the environment
before running `cmake`:
```sh
CFLAGS=-DMY_CUSTOM_FLAG cmake ..
```

You can also check the [cmake FAQ](https://gitlab.kitware.com/cmake/community/-/wikis/FAQ) for more information.


### 5. Build libzip.

```sh
cmake --build .
```


### 6. Run the test suite (optional).

To check that libzip works as expected, run the test suite.

```sh
ctest -j20
```

The number after `-j` specifies how many tests to run in parallel. 20 is a good default for modern systems.


### 7. Install libzip (optional).

```sh
cmake --install .
```

Installing to the default location may require root privileges. You can specify a different installation location by passing `-DCMAKE_INSTALL_PREFIX=/path/to/install` to cmake in step 4.


## Test Suite Code Coverage

To enable collecting code coverage, pass `-DENABLE_COVERAGE=ON` to `cmake`. After running the tests with `ctest`, run `cmake --build . --target coverage` to create the report in `coverage/index.html`.

Please note that this builds libzip with coverage gathering enabled. You should not use such a build in production.


## Special System Requirements

### Small Stack Size

For efficiency, libzip allocates buffers on the stack by default. These buffers are small enough to fit on the stack on modern systems, but especially on embedded systems, they might exhaust available stack space. 

If you are compiling on a system with a small stack size, add
`-DZIP_ALLOCATE_BUFFER` to `CFLAGS`. This causes libzip to allocate buffers on the heap instead. The performance impact should not be significant.

### 32-bit Linux

If you are building on a 32-bit Linux system it might be necessary
to define `_FILE_OFFSET_BITS` to `64`. Your distro will need to provide
a `fts.h` file that is new enough to support this, or the build
will break in `zipcmp`.
