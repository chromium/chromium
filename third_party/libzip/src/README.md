# libzip

## A C Library for Reading, Creating, and Modifying Zip Archives

## Why Use libzip?

libzip has been continuously developed since 2005. It is efficient, small, and flexible. It is usable on Linux, macOS, and Windows and many other operating systems.

The main design criteria are:

- Maintain a stable API without breaking backwards compatibility.
- Do not create corrupt files, even in case of errors.
- Do not delete data.
- Be efficient.

It supports the following features:

- Reading archives and file data from files or memory buffers
- Reverting unsaved changes
- Zip64 large archives
- Deflate, bzip2, LZMA, and zstd compression
- Winzip AES and legacy PKWARE encryption

The [BSD license](LICENSE) used for libzip allows its use in commercial products.

## Who Uses libzip?

libzip is used in major open source projects like [KDE](https://kde.org/), [Chromium](https://www.chromium.org/Home), [ImageMagick](https://github.com/ImageMagick/ImageMagick/), and [VeraCrypt](https://www.veracrypt.fr/).

Commercial products using libzip include [Lightroom from Adobe](https://lightroom.adobe.com/) and the [Kobo eReader](http://www.kobo.com/desktop).

There are also bindings for other programming languages: [Python](https://github.com/KOLANICH-libs/libzip.py), [Ruby](http://rubygems.org/gems/zipruby/), [Lua](https://github.com/brimworks/lua-zip), [PHP](http://pecl.php.net/package/zip), and others.

There is a more complete [list of projects](https://libzip.org/users/).

## Getting Started

Most Linux and other Unix distributions include libzip in their package distributions, it is usually called `libzip` or `libzip-dev`.

On macOS, it is included in both Homebrew and Mac Ports.

On Windows, it is in vcpkg.

A list of available packages can be found on [Repology](https://repology.org/project/libzip/versions).

For building and installing libzip from source, see the [INSTALL.md](INSTALL.md) file.

## Using libzip

libzip is fully documented via man pages. HTML versions of the man
pages are on [libzip.org](https://libzip.org/documentation/) and in the [man](man) directory. You can start with
[libzip(3)](https://libzip.org/documentation/libzip.html), which lists
all others. Example source code is in the [examples](examples) and
[src](src) subdirectories.

If you have developed an application using libzip, you can find out
about API changes and how to adapt your code for them in the included
file [API-CHANGES.md](API-CHANGES.md).

## Staying in Touch

More information and the latest version can always be found on [libzip.org](https://libzip.org). The official repository is at [GitHub](https://github.com/nih-at/libzip/).

If you want to reach the authors in private, use <info@libzip.org>.

[![Packaging status](https://repology.org/badge/tiny-repos/libzip.svg)](https://repology.org/project/libzip/versions)

[![Github Actions Build Status](https://github.com/nih-at/libzip/workflows/build/badge.svg)](https://github.com/nih-at/libzip/actions?query=workflow%3Abuild)
[![Appveyor Build status](https://ci.appveyor.com/api/projects/status/f1bqqt9djvf22f5g?svg=true)](https://ci.appveyor.com/project/nih-at/libzip)
[![Coverity Status](https://scan.coverity.com/projects/127/badge.svg)](https://scan.coverity.com/projects/libzip)
[![Fuzzing Status](https://oss-fuzz-build-logs.storage.googleapis.com/badges/libzip.svg)](https://bugs.chromium.org/p/oss-fuzz/issues/list?sort=-opened&can=1&q=proj:libzip)
