Prepare content:

  $ cd $CRAMTMP
  $ export STAGEDIR=hfs_xattrs_reference_stagedir
  $ export REFERENCE=hfs_xattrs_reference_reference
  $ mkdir $STAGEDIR
  $ echo "content-a" >> $STAGEDIR/a
  $ echo "content-b" >> $STAGEDIR/b

Create reference DMGs using macOS:

  $ mkdir $REFERENCE
  $ hdiutil create -megabytes 5 -fs HFS+ -volname myDisk -srcfolder $STAGEDIR $REFERENCE/hdiutil.hfs
  created: */hdiutil.hfs.dmg (glob)
  $ xattr -w 'attr-key-a' '__MOZILLA__attr-value-a' $STAGEDIR/a
  $ hdiutil create -megabytes 5 -fs HFS+ -volname myDisk -srcfolder $STAGEDIR $REFERENCE/hdiutila.hfs
  created: */hdiutila.hfs.dmg (glob)
  $ xattr -w 'attr-key-b' '__MOZILLA__attr-value-b' $STAGEDIR/b
  $ hdiutil create -megabytes 5 -fs HFS+ -volname myDisk -srcfolder $STAGEDIR $REFERENCE/hdiutilab.hfs
  created: */hdiutilab.hfs.dmg (glob)
  $ xattr -c $STAGEDIR/a
  $ hdiutil create -megabytes 5 -fs HFS+ -volname myDisk -srcfolder $STAGEDIR $REFERENCE/hdiutilb.hfs
  created: */hdiutilb.hfs.dmg (glob)
