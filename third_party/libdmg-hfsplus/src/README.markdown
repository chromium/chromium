README
======

This project was first conceived to manipulate Apple's software restore
packages (IPSWs) and hence much of it is geared specifically toward that
format. Useful tools to read and manipulate the internal data structures of
those files have been created to that end, and with minor changes, more
generality can be achieved in the general utility. An inexhaustive list of
such changes would be selectively enabling folder counts in HFS+, switching
between case sensitivity and non-sensitivity, and more fine-grained control
over the layout of created dmgs.

**THE CODE HEREIN SHOULD BE CONSIDERED HIGHLY EXPERIMENTAL**

Extensive testing have not been done, but comparatively simple tasks like
adding and removing files from a mostly contiguous filesystem are well
proven.

Please note that these tools and routines are currently only suitable to be
accessed by other programs that know what they're doing. I.e., doing
something you "shouldn't" be able to do, like removing non-existent files is
probably not a very good idea.

LICENSE
-------

This work is released under the terms of the GNU General Public License,
version 3. The full text of the license can be found in the LICENSE file.

DEPENDENCIES
------------

The HFS portion will work on any platform that supports GNU C and POSIX
conventions. The dmg portion has dependencies on zlib (which is included) and
libcrypto from openssl (which is not). If libcrypto is not available, remove
the -DHAVE_CRYPT flags from the CFLAGS of the makefiles. All FileVault
related actions will fail, but everything else should still work. I imagine
most places have libcrypto, and probably statically compiled zlib was a dumb
idea too.

USING
-----

The targets of the current repository are three command-line utilities that
demonstrate the usage of the library functions (except cmd_grow, which really
ought to be moved to catalog.c). To make compilation simpler, a complete,
unmodified copy of the zlib distribution is included. The dmg portion of the
code has dependencies on the HFS+ portion of the code. The "hdutil" section
contains a version of the HFS+ utility that supports directly reading from
dmgs. It is separate from the HFS+ utility in order that the hfs directory
does not have dependencies on the dmg directory.

The makefile in the root folder will make all utilities.

	mkdir build && cd build
	cmake ..
	make # Or only make hfs / make dmg / ...
	sudo make install

### HFS+

	cd hfs
	make

### DMG

	cd dmg/zlib-1.2.3
	./configure
	make
	cd ..
	make

### hdutil
	cd hdiutil
	make

TESTING
-------

To run tests automatically, ensure you have Docker and docker-buildx installed. Then run `./test/run_tests.sh`.

Or to run tests manually:
* Configure in 'build': `cmake -B build`
* Install cram: `pipx install cram`
* Run tests: `cram test/*.t`
