Make sure we have a fresh build:

  $ export BUILDDIR=$TESTDIR/../build
  $ cd $BUILDDIR
  $ make 2> /dev/null >/dev/null
  $ cd $CRAMTMP
  $ export STAGEDIR=attribution_shorter_than_sentinel_stagedir
  $ export OUTPUT=attribution_shorter_than_sentinel_output

Prepare content and the attribution code:
  $ mkdir $STAGEDIR
  $ echo "content-a" >> $STAGEDIR/a
  $ sentinel="__MOZILLA__attribution-code"
  $ attribution_code="short"

Create the filesystem with the sentinel value in the attribute:

  $ mkdir $OUTPUT
  $ cp $TESTDIR/empty.hfs $OUTPUT/short.hfs
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/short.hfs add $STAGEDIR/a a
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/short.hfs setattr a attr-key "$sentinel"

Build a DMG:

  $ $BUILDDIR/dmg/dmg build $OUTPUT/short.hfs $OUTPUT/short.dmg "$sentinel" >/dev/null

Now attribute:

  $ $BUILDDIR/dmg/dmg attribute $OUTPUT/short.dmg $OUTPUT/short-attributed.dmg "__MOZILLA__" "$attribution_code" >/dev/null

Extract the HFS from the attributed DMG, and check to see if the code is correct:

  $ $BUILDDIR/dmg/dmg extract $OUTPUT/short-attributed.dmg $OUTPUT/short-attributed.hfs >/dev/null
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/short-attributed.hfs getattr a attr-key | tr -d '\0'
  short (no-eol)
