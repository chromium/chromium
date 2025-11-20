Make sure we have a fresh build:

  $ export BUILDDIR=$TESTDIR/../build
  $ cd $BUILDDIR
  $ make 2> /dev/null >/dev/null
  $ cd $CRAMTMP
  $ export STAGEDIR=hfs_xattrs_stagedir
  $ export OUTPUT=hfs_xattrs_output

Prepare content:

  $ mkdir $STAGEDIR
  $ echo "content-a" >> $STAGEDIR/a
  $ echo "content-b" >> $STAGEDIR/b

Extract reference HFSs and attributes. We parse the debugattrs a bit because the attribute numbers from hdiutil and hfsplus may not match:

  $ mkdir hfs_xattrs_reference
  $ $BUILDDIR/dmg/dmg extract $TESTDIR/hfs_xattrs_reference/hdiutila.hfs.dmg $CRAMTMP/hfs_xattrs_reference/hdiutila.hfs > /dev/null
  $ $BUILDDIR/dmg/dmg extract $TESTDIR/hfs_xattrs_reference/hdiutilb.hfs.dmg $CRAMTMP/hfs_xattrs_reference/hdiutilb.hfs > /dev/null
  $ $BUILDDIR/dmg/dmg extract $TESTDIR/hfs_xattrs_reference/hdiutilab.hfs.dmg $CRAMTMP/hfs_xattrs_reference/hdiutilab.hfs > /dev/null
  $ $BUILDDIR/hfs/hfsplus hfs_xattrs_reference/hdiutila.hfs debugattrs verbose | grep attribute | sed -e 's/[0-9].*:/:/'  > hfs_xattrs_reference/hdiutila.attrs
  $ $BUILDDIR/hfs/hfsplus hfs_xattrs_reference/hdiutilb.hfs debugattrs verbose | grep attribute | sed -e 's/[0-9].*:/:/' > hfs_xattrs_reference/hdiutilb.attrs
  $ $BUILDDIR/hfs/hfsplus hfs_xattrs_reference/hdiutilab.hfs debugattrs verbose | grep attribute | sed -e 's/[0-9].*:/:/' > hfs_xattrs_reference/hdiutilab.attrs

Generate comparison HFSs:

  $ mkdir $OUTPUT
  $ cp $TESTDIR/empty.hfs $OUTPUT/stageda.hfs
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/stageda.hfs add $STAGEDIR/a a
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/stageda.hfs add $STAGEDIR/b b
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/stageda.hfs setattr a 'attr-key-a' '__MOZILLA__attr-value-a__MOZILLA__'
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/stageda.hfs debugattrs verbose | grep attribute | sed -e 's/[0-9].*:/:/' > $OUTPUT/stageda.attrs
  $ cp $TESTDIR/empty.hfs $OUTPUT/stagedab.hfs
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/stagedab.hfs add $STAGEDIR/a a
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/stagedab.hfs add $STAGEDIR/b b
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/stagedab.hfs setattr a 'attr-key-a' '__MOZILLA__attr-value-a__MOZILLA__'
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/stagedab.hfs setattr b 'attr-key-b' '__MOZILLA__attr-value-b__MOZILLA__'
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/stagedab.hfs debugattrs verbose | grep attribute | sed -e 's/[0-9].*:/:/' > $OUTPUT/stagedab.attrs
  $ cp $TESTDIR/empty.hfs $OUTPUT/stagedb.hfs
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/stagedb.hfs add $STAGEDIR/a a
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/stagedb.hfs add $STAGEDIR/b b
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/stagedb.hfs setattr b 'attr-key-b' '__MOZILLA__attr-value-b__MOZILLA__'
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/stagedb.hfs debugattrs verbose | grep attribute | sed -e 's/[0-9].*:/:/' > $OUTPUT/stagedb.attrs

Compare attributes in the reference images and generated images:

  $ diff --unified=3 hfs_xattrs_reference/hdiutila.attrs $OUTPUT/stageda.attrs
  $ diff --unified=3 hfs_xattrs_reference/hdiutilb.attrs $OUTPUT/stagedb.attrs
  $ diff --unified=3 hfs_xattrs_reference/hdiutilab.attrs $OUTPUT/stagedab.attrs
