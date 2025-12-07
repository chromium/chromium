Make sure we have a fresh build:

  $ export BUILDDIR=$TESTDIR/../build
  $ cd $BUILDDIR
  $ make 2> /dev/null >/dev/null
  $ cd $CRAMTMP
  $ export STAGEDIR=attribution_full_length_stagedir
  $ export OUTPUT=attribution_full_length_output

Prepare content and the attribution code (997 is 1024 minus the length of the sentinel):
  $ mkdir $STAGEDIR
  $ echo "content-a" >> $STAGEDIR/a
  $ sentinel="__MOZILLA__attribution-code"
  $ code=$(printf "${sentinel}%*s" "997" | tr ' ' '\011')
  $ attributed_code=$(printf "${sentinel}%*s" "997" | tr ' ' '~')

Make a small HFS filesystem with a full length (1024 byte) attribution value:

  $ mkdir $OUTPUT
  $ cp $TESTDIR/empty.hfs $OUTPUT/full-length.hfs
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/full-length.hfs add $STAGEDIR/a a
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/full-length.hfs setattr a attr-key "$code"

Echo it back, make sure it is the right length in the HFS filesystem:

  $ $BUILDDIR/hfs/hfsplus $OUTPUT/full-length.hfs getattr a attr-key | wc -c
  \s*1024 (re)

Build a DMG:

  $ $BUILDDIR/dmg/dmg build $OUTPUT/full-length.hfs $OUTPUT/full-length.dmg "$sentinel" >/dev/null

Now attribute, using printable characters for the full length:

  $ $BUILDDIR/dmg/dmg attribute $OUTPUT/full-length.dmg $OUTPUT/full-length-attributed.dmg "$sentinel" "$attributed_code" >/dev/null

Extract the HFS from the attributed DMG, and check to see if the code is correct:

  $ $BUILDDIR/dmg/dmg extract $OUTPUT/full-length-attributed.dmg $OUTPUT/full-length-attributed.hfs >/dev/null
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/full-length-attributed.hfs getattr a attr-key | grep -c "$attributed_code"
  1
