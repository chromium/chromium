About Hunspell
==============

NOTICE: Version 2 is in the works. For contributing see
[version 2 specification][v2spec] and the folder `src/hunspell2`.

[v2spec]: https://github.com/hunspell/hunspell/wiki/Version-2-Specification

Hunspell is a spell checker and morphological analyzer library and program
designed for languages with rich morphology and complex word compounding or
character encoding. Hunspell interfaces: Ispell-like terminal interface
using Curses library, Ispell pipe interface, C++ class and C functions.

Hunspell's code base comes from the OpenOffice.org MySpell
(http://lingucomponent.openoffice.org/MySpell-3.zip). See README.MYSPELL,
AUTHORS.MYSPELL and license.myspell files.
Hunspell is designed to eventually replace Myspell in OpenOffice.org.

Main features of Hunspell spell checker and morphological analyzer:

- Unicode support (affix rules work only with the first 65535 Unicode
  characters)
- Morphological analysis (in custom item and arrangement style) and stemming
- Max. 65535 affix classes and twofold affix stripping (for agglutinative
  languages, like Azeri, Basque, Estonian, Finnish, Hungarian, Turkish, etc.)
- Support complex compoundings (for example, Hungarian and German)
- Support language specific features (for example, special casing of
  Azeri and Turkish dotted i, or German sharp s)
- Handle conditional affixes, circumfixes, fogemorphemes,
  forbidden words, pseudoroots and homonyms.
- Free software. Versions 1.x are licenced under LGPL, GPL, MPL tri-license.
  Version 2 is licenced only under GNU LGPL.

Compiling on GNU/Linux and Unixes
=================================

	autoreconf -vfi
	./configure
	make
	sudo make install
	sudo ldconfig

For dictionary development, use the `--with-warnings` option of configure.

For interactive user interface of Hunspell executable, use the `--with-ui option`.

The developer packages you need to compile Hunspell's interface:

	autoconf automake autopoint libtool g++

Optional developer packages:

- ncurses (need for --with-ui), eg. libncursesw5 for UTF-8
- readline (for fancy input line editing,
  configure parameter: --with-readline)
- locale and gettext (but you can also use the
  --with-included-gettext configure parameter)

Compiling on Windows
====================

## 1. Compiling with Mingw64 and MSYS2

Download Msys2, update everything and install the following packages:

	pacman -S base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-libtool

Open Mingw-w64 Win64 prompt and compile the same way as on Linux, see above.

## 2. Compiling in Cygwin environment

Download and install Cygwin environment for Windows with the following
extra packages: 

- make
- automake
- autoconf
- libtool
- gcc-g++ development package
- ncurses, readline (for user interface)
- iconv (character conversion)

Then compile the same way as on Linux. Cygwin builds depend on Cygwin1.dll.

Debugging
=========

For debugging we need to create a debug build and then we need to start `gdb`.

	make clean
	make CXXFLAGS='-g -O0'
	libtool --mode=execute gdb src/tools/hunspell

Testing
=======

Testing Hunspell (see tests in tests/ subdirectory):

	make check

or with Valgrind debugger:

	make check
	VALGRIND=[Valgrind_tool] make check

For example:

	make check
	VALGRIND=memcheck make check

Documentation
=============

features and dictionary format:

	man 5 hunspell
	man hunspell
	hunspell -h
	
http://hunspell.github.io/

Usage
=====

The src/tools directory contains ten executables after compiling:

- affixcompress: dictionary generation from large (millions of words)
  vocabularies
- analyze: example of spell checking, stemming and morphological analysis
- chmorph: example of automatic morphological generation and conversion
- example: example of spell checking and suggestion
- hunspell: main program for spell checking and others (see manual)
- hunzip: decompressor of hzip format
- hzip: compressor of hzip format
- makealias: alias compression (Hunspell only, not back compatible with MySpell)
- munch: dictionary generation from vocabularies (it needs an affix file, too).
- unmunch: list all recognized words of a MySpell dictionary
- wordforms: word generation (Hunspell version of unmunch)

After compiling and installing (see INSTALL) you can
run the Hunspell spell checker (compiled with user interface)
with a Hunspell or Myspell dictionary:

	hunspell -d en_US text.txt

or without interface:

	hunspell
	hunspell -d en_UK -l <text.txt

Dictionaries consist of an affix and dictionary file, see tests/
or http://wiki.services.openoffice.org/wiki/Dictionaries.

Using Hunspell library with GCC
===============================

Including in your program:

	#include <hunspell.hxx>

Linking with Hunspell static library:

	g++ -lhunspell example.cxx 

Dictionaries
------------

Myspell & Hunspell dictionaries:

- http://extensions.libreoffice.org
- http://cgit.freedesktop.org/libreoffice/dictionaries
- http://extensions.openoffice.org
- http://wiki.services.openoffice.org/wiki/Dictionaries

Aspell dictionaries (need some conversion):

- ftp://ftp.gnu.org/gnu/aspell/dict

Conversion steps: see relevant feature request at http://hunspell.github.io/ .

László Németh
nemeth at numbertext org
