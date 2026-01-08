Basic setup
===========

Make sure we have a fresh build:

  $ export BUILDDIR=$TESTDIR/../build
  $ cd $BUILDDIR
  $ make 2> /dev/null >/dev/null

Prepare top-level paths:

  $ cd $CRAMTMP
  $ export OUTPUT=special_mode_bits_options_output
  $ export STAGEDIR=special_mode_bits_options_stagedir
  $ mkdir $OUTPUT
  $ mkdir $STAGEDIR


Constructing the input
======================

Some unremarkable files:

  $ mkdir $STAGEDIR/dmg-root
  $ echo "file1" >> $STAGEDIR/dmg-root/file1.txt
  $ echo "file2" >> $STAGEDIR/dmg-root/file2.txt
  $ chmod 0444 $STAGEDIR/dmg-root/file2.txt

This exact path structure is forced to mode 0755 by "special permission" logic.
This appears to look for the primary executable in an iOS application bundle
under /Applications. (macOS bundles use a different structure, which
special permission logic does not appear to be interested in.)

  $ mkdir -p $STAGEDIR/dmg-root/Applications/special.app
  $ echo "special" >> $STAGEDIR/dmg-root/Applications/special.app/special
  $ chmod 0444 $STAGEDIR/dmg-root/Applications/special.app/special

A scattering of other assorted path prefixes also trigger mode 0755:

  $ mkdir -p $STAGEDIR/dmg-root/bin
  $ echo "some purported binary" >> $STAGEDIR/dmg-root/bin/something

  $ mkdir -p $STAGEDIR/dmg-root/sbin
  $ echo "some purported system binary" >> $STAGEDIR/dmg-root/sbin/sthing

  $ mkdir -p $STAGEDIR/dmg-root/usr/bin
  $ echo "some usr binary" >> $STAGEDIR/dmg-root/usr/bin/ub

  $ mkdir -p $STAGEDIR/dmg-root/usr/sbin
  $ echo "some usr sbinary" >> $STAGEDIR/dmg-root/usr/sbin/bus

  $ mkdir -p $STAGEDIR/dmg-root/usr/libexec
  $ echo "some usr libexec thing" >> $STAGEDIR/dmg-root/usr/libexec/lib

  $ mkdir -p $STAGEDIR/dmg-root/usr/local/bin
  $ echo "some local binary" >> $STAGEDIR/dmg-root/usr/local/bin/ulb

  $ mkdir -p $STAGEDIR/dmg-root/usr/local/sbin
  $ echo "some local sbinary" >> $STAGEDIR/dmg-root/usr/local/sbin/sulb

  $ mkdir -p $STAGEDIR/dmg-root/usr/local/libexec
  $ echo "some local libexec thing" >> $STAGEDIR/dmg-root/usr/local/libexec/loclex

This path prefix appears to be intended to do so, but its check is logically
unreachable because all paths starting with /Applications/ are already handled
in the other branch of the special permission logic, which assigns no changed
permissions to things that do not appear to be the main executable of an iOS
application bundle:

  $ mkdir -p $STAGEDIR/dmg-root/Applications/BootNeuter.app/bin
  $ echo "unreachable special case" >> $STAGEDIR/dmg-root/Applications/BootNeuter.app/bin/oopsie

(To fix this, lose the "else" in the "else if" giving the second set of triggers
for the permission change -- then update this test accordingly.)


/usr/ and /usr/local/ are common prefixes to a bunch of these special cases
but are not, themselves, adequate to provoke the behavior:

  $ echo "not changed" >> $STAGEDIR/dmg-root/usr/skipme
  $ echo "also not changed" >> $STAGEDIR/dmg-root/usr/local/skipme2

Off-target contents of apparent application bundles are also untouched:
  $ echo "also not changed" >> $STAGEDIR/dmg-root/Applications/special.app/not-so-special


Testing behavior
================

Scenario 1. Do not mess with permissions
----------------------------------------

Gather the file tree into an HFS image, with special mode assignment shut off,
then dump it back out and cd into it:
  $ cp $TESTDIR/empty.hfs $OUTPUT/special_modes_no.hfs
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/special_modes_no.hfs addall $STAGEDIR/dmg-root/ --special-modes=no > /dev/null
  $ mkdir $OUTPUT/special_modes_no
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/special_modes_no.hfs extractall / $OUTPUT/special_modes_no/ > /dev/null
  $ cd $OUTPUT/special_modes_no

We should find every file where we left it, with original permissions. Files
in unremarkable places would not change permission in any case:
  $ stat --format %A file1.txt
  -rw-r--r--
  $ stat --format %A file2.txt
  -r--r--r--

The special path code would change the permissions here, although for the
directories it would to change them to what they already were anyway:
  $ stat --format %A Applications/special.app/special
  -r--r--r--
  $ stat --format %A bin/
  drwxr-xr-x
  $ stat --format %A bin/something
  -rw-r--r--
  $ stat --format %A sbin/
  drwxr-xr-x
  $ stat --format %A sbin/sthing
  -rw-r--r--
  $ stat --format %A usr/
  drwxr-xr-x
  $ stat --format %A usr/bin/
  drwxr-xr-x
  $ stat --format %A usr/bin/ub
  -rw-r--r--
  $ stat --format %A usr/sbin/
  drwxr-xr-x
  $ stat --format %A usr/sbin/bus
  -rw-r--r--
  $ stat --format %A usr/libexec/
  drwxr-xr-x
  $ stat --format %A usr/libexec/lib
  -rw-r--r--
  $ stat --format %A usr/local/
  drwxr-xr-x
  $ stat --format %A usr/local/bin
  drwxr-xr-x
  $ stat --format %A usr/local/bin/ulb
  -rw-r--r--
  $ stat --format %A usr/local/sbin
  drwxr-xr-x
  $ stat --format %A usr/local/sbin/sulb
  -rw-r--r--
  $ stat --format %A usr/local/libexec
  drwxr-xr-x
  $ stat --format %A usr/local/libexec/loclex
  -rw-r--r--

The special path code appears to be intended to change these, but would not:
  $ stat --format %A Applications/BootNeuter.app/
  drwxr-xr-x
  $ stat --format %A Applications/BootNeuter.app/bin/
  drwxr-xr-x
  $ stat --format %A Applications/BootNeuter.app/bin/oopsie
  -rw-r--r--

Paths nearly but not covered by special mode logic:
  $ stat --format %A usr/skipme
  -rw-r--r--
  $ stat --format %A usr/local/skipme2
  -rw-r--r--
  $ stat --format %A Applications/special.app/not-so-special
  -rw-r--r--

Reset PWD:
  $ cd $CRAMTMP

Scenario 2. Explicitly mess with permissions
--------------------------------------------

Gather the file tree into an HFS image, with special mode assignment turned on,
then dump it back out and cd into it:
  $ cp $TESTDIR/empty.hfs $OUTPUT/special_modes_yes.hfs
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/special_modes_yes.hfs addall $STAGEDIR/dmg-root/ --special-modes=yes > /dev/null
  $ mkdir $OUTPUT/special_modes_yes
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/special_modes_yes.hfs extractall / $OUTPUT/special_modes_yes/ > /dev/null
  $ cd $OUTPUT/special_modes_yes

Files in unremarkable places would not change permission in any case:
  $ stat --format %A file1.txt
  -rw-r--r--
  $ stat --format %A file2.txt
  -r--r--r--

For directories the change is moot, but these files should now be executable:
  $ stat --format %A Applications/special.app/special
  -rwxr-xr-x
  $ stat --format %A bin/
  drwxr-xr-x
  $ stat --format %A bin/something
  -rwxr-xr-x
  $ stat --format %A sbin/
  drwxr-xr-x
  $ stat --format %A sbin/sthing
  -rwxr-xr-x
  $ stat --format %A usr/
  drwxr-xr-x
  $ stat --format %A usr/bin/
  drwxr-xr-x
  $ stat --format %A usr/bin/ub
  -rwxr-xr-x
  $ stat --format %A usr/sbin/
  drwxr-xr-x
  $ stat --format %A usr/sbin/bus
  -rwxr-xr-x
  $ stat --format %A usr/libexec/
  drwxr-xr-x
  $ stat --format %A usr/libexec/lib
  -rwxr-xr-x
  $ stat --format %A usr/local/
  drwxr-xr-x
  $ stat --format %A usr/local/bin
  drwxr-xr-x
  $ stat --format %A usr/local/bin/ulb
  -rwxr-xr-x
  $ stat --format %A usr/local/sbin
  drwxr-xr-x
  $ stat --format %A usr/local/sbin/sulb
  -rwxr-xr-x
  $ stat --format %A usr/local/libexec
  drwxr-xr-x
  $ stat --format %A usr/local/libexec/loclex
  -rwxr-xr-x

The BootNeuter.app special case does not work:
  $ stat --format %A Applications/BootNeuter.app/
  drwxr-xr-x
  $ stat --format %A Applications/BootNeuter.app/bin/
  drwxr-xr-x
  $ stat --format %A Applications/BootNeuter.app/bin/oopsie
  -rw-r--r--

Paths nearly but not covered by special mode logic are unchanged:
  $ stat --format %A usr/skipme
  -rw-r--r--
  $ stat --format %A usr/local/skipme2
  -rw-r--r--
  $ stat --format %A Applications/special.app/not-so-special
  -rw-r--r--

Reset PWD:
  $ cd $CRAMTMP

Scenario 3. Implicitly mess with permissions
--------------------------------------------

Gather the file tree into an HFS image, with no special mode assignment flag,
then dump it back out and cd into it:
  $ cp $TESTDIR/empty.hfs $OUTPUT/special_modes_default.hfs
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/special_modes_default.hfs addall $STAGEDIR/dmg-root/ > /dev/null
  $ mkdir $OUTPUT/special_modes_default
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/special_modes_default.hfs extractall / $OUTPUT/special_modes_default/ > /dev/null
  $ cd $OUTPUT/special_modes_default

This should be identical to --special-modes=yes, to preserve compatibility
with scripts expecting the behaviors from before the flag existed, so the
rest of this scenario should look very familiar.

Files in unremarkable places would not change permission in any case:
  $ stat --format %A file1.txt
  -rw-r--r--
  $ stat --format %A file2.txt
  -r--r--r--

For directories the change is moot, but these files should now be executable:
  $ stat --format %A Applications/special.app/special
  -rwxr-xr-x
  $ stat --format %A bin/
  drwxr-xr-x
  $ stat --format %A bin/something
  -rwxr-xr-x
  $ stat --format %A sbin/
  drwxr-xr-x
  $ stat --format %A sbin/sthing
  -rwxr-xr-x
  $ stat --format %A usr/
  drwxr-xr-x
  $ stat --format %A usr/bin/
  drwxr-xr-x
  $ stat --format %A usr/bin/ub
  -rwxr-xr-x
  $ stat --format %A usr/sbin/
  drwxr-xr-x
  $ stat --format %A usr/sbin/bus
  -rwxr-xr-x
  $ stat --format %A usr/libexec/
  drwxr-xr-x
  $ stat --format %A usr/libexec/lib
  -rwxr-xr-x
  $ stat --format %A usr/local/
  drwxr-xr-x
  $ stat --format %A usr/local/bin
  drwxr-xr-x
  $ stat --format %A usr/local/bin/ulb
  -rwxr-xr-x
  $ stat --format %A usr/local/sbin
  drwxr-xr-x
  $ stat --format %A usr/local/sbin/sulb
  -rwxr-xr-x
  $ stat --format %A usr/local/libexec
  drwxr-xr-x
  $ stat --format %A usr/local/libexec/loclex
  -rwxr-xr-x

The BootNeuter.app special case does not work:
  $ stat --format %A Applications/BootNeuter.app/
  drwxr-xr-x
  $ stat --format %A Applications/BootNeuter.app/bin/
  drwxr-xr-x
  $ stat --format %A Applications/BootNeuter.app/bin/oopsie
  -rw-r--r--

Paths nearly but not covered by special mode logic are unchanged:
  $ stat --format %A usr/skipme
  -rw-r--r--
  $ stat --format %A usr/local/skipme2
  -rw-r--r--
  $ stat --format %A Applications/special.app/not-so-special
  -rw-r--r--

Reset PWD:
  $ cd $CRAMTMP
