This takes about 10 seconds, which is too slow for constant inputs.
Run this like
```
time venv/bin/cram test/attribution.t --keep-tmpdir
```
and then copy the reference directory like
```
cp -R /var/folders/3s/_m9prk6n7g5cx6hhs_33q2f80000gn/T/cramtests-0uzbp0wu/attribution_reference_reference test/attribution_reference
```
to update test inputs.

Make sure we have a fresh build:

  $ export BUILDDIR=$TESTDIR/../../build
  $ cd $BUILDDIR
  $ make &> /dev/null
  $ cd $CRAMTMP
  $ export STAGEDIR=attribution_reference_stagedir
  $ export REFERENCE=attribution_reference_reference

Prepare content:

  $ mkdir $STAGEDIR
  $ echo "content-x" >> $STAGEDIR/x

Create reference DMGs using macOS:

  $ mkdir $REFERENCE
  $ xattr -w 'attr-key' '__MOZILLA__attr-value-a' $STAGEDIR/x
  $ hdiutil create -megabytes 5 -fs HFS+ -volname myDisk -srcfolder $STAGEDIR $REFERENCE/hdiutila.hfs
  created: */hdiutila.hfs.dmg (glob)
  $ xattr -w 'attr-key' '__MOZILLA__attr-value-b' $STAGEDIR/x
  $ hdiutil create -megabytes 5 -fs HFS+ -volname myDisk -srcfolder $STAGEDIR $REFERENCE/hdiutilb.hfs
  created: */hdiutilb.hfs.dmg (glob)
  $ xattr -w 'attr-key' '__MOZILLA__attr-value-p' $STAGEDIR/x
  $ hdiutil create -megabytes 5 -fs HFS+ -volname myDisk -srcfolder $STAGEDIR $REFERENCE/hdiutilp.hfs
  created: */hdiutilp.hfs.dmg (glob)

Extract reference HFSs:

  $ $BUILDDIR/dmg/dmg extract $REFERENCE/hdiutila.hfs.dmg $REFERENCE/hdiutila.hfs > /dev/null
  $ $BUILDDIR/dmg/dmg extract $REFERENCE/hdiutilb.hfs.dmg $REFERENCE/hdiutilb.hfs > /dev/null
  $ $BUILDDIR/dmg/dmg extract $REFERENCE/hdiutilp.hfs.dmg $REFERENCE/hdiutilp.hfs > /dev/null

Remove the unneeded dmg:

  $ rm $REFERENCE/hdiutilp.hfs.dmg
