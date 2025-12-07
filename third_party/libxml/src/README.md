# libxml2

libxml2 is an XML toolkit implemented in C, originally developed for
the GNOME Project.

Official releases can be downloaded from
<https://download.gnome.org/sources/libxml2/>

The git repository is hosted on GNOME's GitLab server:
<https://gitlab.gnome.org/GNOME/libxml2>

Bugs should be reported at
<https://gitlab.gnome.org/GNOME/libxml2/-/issues>.

Documentation is available at
<https://gitlab.gnome.org/GNOME/libxml2/-/wikis>

## License

This code is released under the MIT License, see the Copyright file.

## Security

This is open-source software written by hobbyists, maintained by a single
volunteer, badly tested, written in a memory-unsafe language and full of
security bugs. It is foolish to use this software to process untrusted data.
As such, we treat security issues like any other bug. Each security report
we receive will be made public immediately and won't be prioritized.

## Build instructions

libxml2 can be built with GNU Autotools, CMake or meson.

### Autotools (for POSIX systems like Linux, BSD, macOS)

If you build from a Git tree, you have to install Autotools and start
by generating the configuration files with:

    ./autogen.sh [configuration options]

If you build from a source tarball, extract the archive with:

    tar xf libxml2-xxx.tar.gz
    cd libxml2-xxx

Then you can configure and build the library:

    ./configure [configuration options]
    make

The following options disable or enable code modules and relevant symbols:

    --with-c14n             Canonical XML 1.0 support (on)
    --with-catalog          XML Catalogs support (on)
    --with-debug            debugging module (on)
    --with-docs             Build documentation (off)
    --with-history          history support for xmllint shell (off)
    --with-readline[=DIR]   use readline in DIR for shell (off)
    --with-html             HTML parser (on)
    --with-http             ABI compatibility for removed HTTP support (off)
    --with-iconv[=DIR]      iconv support (on)
    --with-icu              ICU support (off)
    --with-iso8859x         ISO-8859-X support if no iconv (on)
    --with-modules          dynamic modules support (on)
    --with-output           serialization support (on)
    --with-pattern          xmlPattern selection interface (on)
    --with-push             push parser interfaces (on)
    --with-python           Python bindings (off)
    --with-reader           xmlReader parsing interface (on)
    --with-regexps          regular expressions support (on)
    --with-relaxng          RELAX NG support (on)
    --with-sax1             older SAX1 interface (on)
    --with-schemas          XML Schemas 1.0 support (on)
    --with-schematron       Schematron support (off)
    --with-threads          multithreading support (on)
    --with-thread-alloc     per-thread malloc hooks (off)
    --with-valid            DTD validation support (on)
    --with-writer           xmlWriter serialization interface (on)
    --with-xinclude         XInclude 1.0 support (on)
    --with-xpath            XPath 1.0 support (on)
    --with-xptr             XPointer support (on)
    --with-zlib[=DIR]       use libz in DIR (off)

Other options:

    --prefix=DIR            set installation prefix
    --with-minimum          build a minimally sized library (off)
    --with-legacy           maximum ABI compatibility (off)

Note that by default, no optimization options are used. You have to
enable them manually, for example with:

    CFLAGS='-O2 -fno-semantic-interposition' ./configure

Now you can run the test suite with:

    make check

Please report test failures to the bug tracker.

Then you can install the library:

    make install

At that point you may have to rerun ldconfig or a similar utility to
update your list of installed shared libs.

### CMake (mainly for Windows)

Example commands:

    cmake -E tar xf libxml2-xxx.tar.xz
    cmake -S libxml2-xxx -B builddir [options]
    cmake --build builddir
    ctest --test-dir builddir
    cmake --install builddir

Common CMake options include:

    -D BUILD_SHARED_LIBS=OFF            # build static libraries
    -D CMAKE_BUILD_TYPE=Release         # specify build type (single-config)
    --config Release                    # specify build type (multi-config)
    -D CMAKE_INSTALL_PREFIX=/usr/local  # specify the install path
    -D LIBXML2_WITH_ICONV=OFF           # disable iconv
    -D LIBXML2_WITH_ZLIB=ON             # enable zlib

You can also open the libxml source directory with its CMakeLists.txt
directly in various IDEs such as CLion, QtCreator, or Visual Studio.

### Meson

Example commands:

    meson setup [options] builddir
    ninja -C builddir
    meson test -C builddir
    ninja -C builddir install

See the `meson_options.txt` file for options. For example:

    -Dprefix=$prefix
    -Dhistory=enabled
    -Dschemas=disabled
    -Dzlib=enabled

## Dependencies

libxml2 supports POSIX and Windows operating systems.

The iconv function is required for conversion of character encodings.
This function is part of POSIX.1-2001. If your platform doesn't provide
iconv, you need an external libiconv library, for example
[GNU libiconv](https://www.gnu.org/software/libiconv/). Using
[ICU](https://icu.unicode.org/) is also supported but discouraged.

The xmllint executable uses libreadline and libhistory if enabled.

### Build requirements

Besides build system tools, only a C compiler should be required.
Reconfiguration of the Autotools build requires the pkg.m4 macro from
pkg-config. Building the documentation requires Doxygen, xsltproc and the
DocBook 4 XSLT stylesheets. Building the Python bindings requires Doxygen.

## Contributing

The current version of the code can be found in GNOME's GitLab at
<https://gitlab.gnome.org/GNOME/libxml2>. The best way to get involved
is by creating issues and merge requests on GitLab.

All code must conform to C89 and pass the GitLab CI tests. Add regression
tests if possible.

## Authors

- Daniel Veillard
- Bjorn Reese
- William Brack
- Igor Zlatkovic for the Windows port
- Aleksey Sanin
- Nick Wellnhofer

