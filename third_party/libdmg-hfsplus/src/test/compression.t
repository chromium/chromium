Make sure we have a fresh build.

  $ export BUILDDIR=$TESTDIR/../build
  $ cd $BUILDDIR
  $ make 2> /dev/null >/dev/null
  $ cd $CRAMTMP
  $ export OUTPUT=compression_output

  $ mkdir -p $OUTPUT
  $ cp $TESTDIR/empty.hfs $OUTPUT/orig.hfs
  $ shasum $OUTPUT/orig.hfs
  c89aae137a58e6a2b7df602d0963970e3a88de08  */orig.hfs (glob)

Round trip bzip2.

  $ rm -f $OUTPUT/out.hfs
  $ $BUILDDIR/dmg/dmg -c bzip2 build $OUTPUT/orig.hfs $OUTPUT/built.dmg >/dev/null
  $ export BZIP2_SHA=$(shasum $OUTPUT/built.dmg | awk '{print $1}')
  $ $BUILDDIR/dmg/dmg extract $OUTPUT/built.dmg $OUTPUT/out.hfs >/dev/null
  $ shasum $OUTPUT/out.hfs
  c89aae137a58e6a2b7df602d0963970e3a88de08  */out.hfs (glob)

Round trip zlib. Ensure we produce a different dmg file from bzip2.

  $ rm -f $OUTPUT/out.hfs
  $ $BUILDDIR/dmg/dmg -c zlib build $OUTPUT/orig.hfs $OUTPUT/built.dmg >/dev/null
  $ test "$(shasum $OUTPUT/built.dmg | awk '{print $1}')" != $BZIP2_SHA
  $ $BUILDDIR/dmg/dmg extract $OUTPUT/built.dmg $OUTPUT/out.hfs >/dev/null
  $ shasum $OUTPUT/out.hfs
  c89aae137a58e6a2b7df602d0963970e3a88de08  */out.hfs (glob)

Round trip lzma. Ensure we produce a different dmg file from bzip2.

  $ rm -f $OUTPUT/out.hfs
  $ $BUILDDIR/dmg/dmg -c lzma build $OUTPUT/orig.hfs $OUTPUT/built.dmg >/dev/null
  $ test "$(shasum $OUTPUT/built.dmg | awk '{print $1}')" != $BZIP2_SHA
  $ $BUILDDIR/dmg/dmg extract $OUTPUT/built.dmg $OUTPUT/out.hfs >/dev/null
  $ shasum $OUTPUT/out.hfs
  c89aae137a58e6a2b7df602d0963970e3a88de08  */out.hfs (glob)

Round trip lzfse. Ensure we produce a different dmg file from bzip2.

  $ rm -f $OUTPUT/out.hfs
  $ $BUILDDIR/dmg/dmg -c lzfse build $OUTPUT/orig.hfs $OUTPUT/built.dmg >/dev/null
  $ test "$(shasum $OUTPUT/built.dmg | awk '{print $1}')" != $BZIP2_SHA
  $ $BUILDDIR/dmg/dmg extract $OUTPUT/built.dmg $OUTPUT/out.hfs >/dev/null
  $ shasum $OUTPUT/out.hfs
  c89aae137a58e6a2b7df602d0963970e3a88de08  */out.hfs (glob)

Round trip with level and run-sector parameters.

  $ rm -f $OUTPUT/out.hfs
  $ $BUILDDIR/dmg/dmg -c zlib -l 2 -r 2048 build $OUTPUT/orig.hfs $OUTPUT/built.dmg >/dev/null
  $ test "$(shasum $OUTPUT/built.dmg | awk '{print $1}')" != $BZIP2_SHA
  $ $BUILDDIR/dmg/dmg extract $OUTPUT/built.dmg $OUTPUT/out.hfs >/dev/null
  $ shasum $OUTPUT/out.hfs
  c89aae137a58e6a2b7df602d0963970e3a88de08  */out.hfs (glob)
