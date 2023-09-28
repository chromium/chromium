# libxml2

libxml2 is an XML toolkit implemented in C, originally developed for
the GNOME Project.

Official releases can be downloaded from
<https://download.gnome.org/sources/libxml2/>

The git repository is hosted on GNOME's GitLab server:
<https://gitlab.gnome.org/GNOME/libxml2>

Bugs should be reported at
<https://gitlab.gnome.org/GNOME/libxml2/-/issues>

Documentation is available at
<https://gitlab.gnome.org/GNOME/libxml2/-/wikis>

## License

This code is released under the MIT License, see the Copyright file.

## Build instructions

libxml2 can be built with GNU Autotools, CMake, or several other build
systems in platform-specific subdirectories.

### Autotools (for POSIX systems like Linux, BSD, macOS)

If you build from a Git tree, you have to install Autotools and start
by generating the configuration files with:

    ./autogen.sh

If you build from a source tarball, extract the archive with:

    tar xf libxml2-xxx.tar.gz
    cd libxml2-xxx

To see a list of build options:

    ./configure --help

Also see the INSTALL file for additional instructions. Then you can
configure and build the library:

    ./configure [possible options]
    make

Note that by default, no optimization options are used. You have to
enable them manually, for example with:

    CFLAGS='-O2 -fno-semantic-interposition' ./configure

Now you can run the test suite with:

    make check

Please report test failures to the mailing list or bug tracker.

Then you can install the library:

    make install

At that point you may have to rerun ldconfig or a similar utility to
update your list of installed shared libs.

### CMake (mainly for Windows)

Another option for compiling libxml is using CMake:

    cmake -E tar xf libxml2-xxx.tar.gz
    cmake -S libxml2-xxx -B libxml2-xxx-build [possible options]
    cmake --build libxml2-xxx-build
    cmake --install libxml2-xxx-build

Common CMake options include:

    -D BUILD_SHARED_LIBS=OFF            # build static libraries
    -D CMAKE_BUILD_TYPE=Release         # specify build type
    -D CMAKE_INSTALL_PREFIX=/usr/local  # specify the install path
    -D LIBXML2_WITH_ICONV=OFF           # disable iconv
    -D LIBXML2_WITH_LZMA=OFF            # disable liblzma
    -D LIBXML2_WITH_PYTHON=OFF          # disable Python
    -D LIBXML2_WITH_ZLIB=OFF            # disable libz

You can also open the libxml source directory with its CMakeLists.txt
directly in various IDEs such as CLion, QtCreator, or Visual Studio.

## Dependencies

Libxml does not require any other libraries. A platform with somewhat
recent POSIX support should be sufficient (please report any violation
to this rule you may find).

However, if found at configuration time, libxml will detect and use
the following libraries:

- [libz](https://zlib.net/), a highly portable and widely available
  compression library.
- [liblzma](https://tukaani.org/xz/), another compression library.
- [libiconv](https://www.gnu.org/software/libiconv/), a character encoding
  conversion library. The iconv function is part of POSIX.1-2001, so
  libiconv isn't required on modern UNIX-like systems like Linux, BSD or
  macOS.
- [ICU](https://icu.unicode.org/), a Unicode library. Mainly useful as an
  alternative to iconv on Windows. Unnecessary on most other systems.

## Contributing

The current version of the code can be found in GNOME's GitLab at 
at <https://gitlab.gnome.org/GNOME/libxml2>. The best way to get involved
is by creating issues and merge requests on GitLab. Alternatively, you can
start discussions and send patches to the mailing list. If you want to
work with patches, please format them with git-format-patch and use plain
text attachments.

All code must conform to C89 and pass the GitLab CI tests. Add regression
tests if possible.

## Authors

- Daniel Veillard
- Bjorn Reese
- William Brack
- Igor Zlatkovic for the Windows port
- Aleksey Sanin
- Nick Wellnhofer

