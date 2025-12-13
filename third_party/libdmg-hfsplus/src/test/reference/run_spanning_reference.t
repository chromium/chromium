This takes about 10 seconds, which is too slow for constant inputs.
Run this like
```
time venv/bin/cram test/run_spanning_reference.t --keep-tmpdir
```
and then copy the reference directory like
```
cp -R /var/folders/3s/_m9prk6n7g5cx6hhs_33q2f80000gn/T/cramtests-0uzbp0wu/run_spanning_reference_reference test/run_spanning_reference
```
to update test inputs.

Make sure we have a fresh build:

  $ export BUILDDIR=$TESTDIR/../../build
  $ mkdir -p $BUILDDIR
  $ cd $BUILDDIR
  $ cmake .. &>/dev/null
  $ make &> /dev/null
  $ cd $CRAMTMP
  $ export STAGEDIR=run_spanning_reference_stagedir
  $ export REFERENCE=run_spanning_reference_reference

Prepare content:

  $ mkdir $STAGEDIR
  $ echo "content-x" >> $STAGEDIR/x
  $ for i in `cat $TESTDIR/offset-files.txt`; do xattr -w "$i" "$i" "$STAGEDIR/x"; done

Create reference DMGs using macOS:

  $ mkdir $REFERENCE
  $ xattr -w 'attr-key' '__MOZILLA__attr-value-and-a-very-long-string-with-some-padding-to-push-this-across-tworuns-EOL-a' $STAGEDIR/x
  $ hdiutil create -megabytes 5 -fs HFS+ -volname myDisk -srcfolder $STAGEDIR $REFERENCE/hdiutila.hfs
  created: */hdiutila.hfs.dmg (glob)
  $ xattr -w 'attr-key' '__MOZILLA__attr-value-and-a-very-long-string-with-some-padding-to-push-this-across-tworuns-EOL-p' $STAGEDIR/x
  $ hdiutil create -megabytes 5 -fs HFS+ -volname myDisk -srcfolder $STAGEDIR $REFERENCE/hdiutilp.hfs
  created: */hdiutilp.hfs.dmg (glob)

Extract reference HFSs:

  $ $BUILDDIR/dmg/dmg extract $REFERENCE/hdiutila.hfs.dmg $REFERENCE/hdiutila.hfs > /dev/null
  $ $BUILDDIR/dmg/dmg extract $REFERENCE/hdiutilp.hfs.dmg $REFERENCE/hdiutilp.hfs > /dev/null

Ensure that the $REFERENCE images only differ in the expected ways. (Some sections have minor differences between them - but always in the same places.)

  $ xxd $REFERENCE/hdiutila.hfs | grep -v 000bc140: | grep -v 000bc150: | grep -v 000bc040 > hdiutila.txt
  $ xxd $REFERENCE/hdiutilp.hfs | grep -v 000bc140: | grep -v 000bc150: | grep -v 000bc040 > hdiutilp.txt
  $ diff --unified=3 hdiutila.txt hdiutilp.txt
  --- *hdiutila.txt* (glob)
  +++ *hdiutilp.txt* (glob)
  @@ -63,12 +63,12 @@
   000003e0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   000003f0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   00000400: 482b 0004 8000 0100 3130 2e30 0000 0000  H+......10.0....
  -00000410:* (glob)
  +00000410:* (glob)
   00000420: 0000 0001 0000 0002 0000 1000 0000 04f6  ................
   00000430: 0000 0480 0000 011e 0001 0000 0001 0000  ................
   00000440: 0000 0013 0000 0002 0000 0000 0000 0001  ................
   00000450: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  -00000460:* (glob)
  +00000460:* (glob)
   00000470: 0000 0000 0000 1000 0000 1000 0000 0001  ................
   00000480: 0000 0001 0000 0001 0000 0000 0000 0000  ................
   00000490: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  @@ -16385,7 +16385,7 @@
   00040000: 7269 6e67 2d77 6974 682d 736f 6d65 2d70  ring-with-some-p
   00040010: 6164 6469 6e67 2d74 6f2d 7075 7368 2d74  adding-to-push-t
   00040020: 6869 732d 6163 726f 7373 2d74 776f 7275  his-across-tworu
  -00040030:* (glob)
  +00040030:* (glob)
   00040040: 0000 0000 0008 0061 0075 0069 0066 0070  .......a.u.i.f.p
   00040050: 0071 006f 0043 0000 0010 0000 0000 0000  .q.o.C..........
   00040060: 0000 0000 0008 6175 6966 7071 6f43 001c  ......auifpqoC..
  @@ -48129,7 +48129,7 @@
   000bc000: 0000 0000 0000 0000 ff01 0008 0000 0012  ................
   000bc010: 0000 0001 0006 006d 0079 0044 0069 0073  .......m.y.D.i.s
   000bc020:* (glob)
  -000bc030:* (glob)
  +000bc030:* (glob)
   000bc050: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   000bc060: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   000bc070: 0005 0000 007e 0000 0000 0006 0000 0002  .....~..........
  @@ -48139,15 +48139,15 @@
   000bc0b0: 0069 0076 0061 0074 0065 0020 0044 0069  .i.v.a.t.e. .D.i
   000bc0c0: 0072 0065 0063 0074 006f 0072 0079 0020  .r.e.c.t.o.r.y. 
   000bc0d0: 0044 0061 0074 0061 000d 0001 0010 0000  .D.a.t.a........
  -000bc0e0:* (glob)
  -000bc0f0:* (glob)
  +000bc0e0:* (glob)
  +000bc0f0:* (glob)
   000bc100: 0000 0002 436d 0000 0001 0000 0000 0000  ....Cm..........
   000bc110: 0000 5000 4000 4000 0000 0000 0000 0000  ..P.@.@.........
   000bc120: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   000bc130: 0000 0008 0000 0002 0001 0078 0002 0086  ...........x....
   000bc160: 0000 0014 0000 81a4 0000 0001 0000 0000  ................
   000bc170: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  -000bc180:* (glob)
  +000bc180:* (glob)
   000bc190: 0000 0000 0000 0000 0000 000a 0000 0000  ................
   000bc1a0: 0000 0001 0000 011e 0000 0001 0000 0000  ................
   000bc1b0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  @@ -48162,8 +48162,8 @@
   000bc240: 0000 0000 0048 0046 0053 002b 0020 0050  .....H.F.S.+. .P
   000bc250: 0072 0069 0076 0061 0074 0065 0020 0044  .r.i.v.a.t.e. .D
   000bc260: 0061 0074 0061 0001 0010 0000 0000 0000  .a.t.a..........
  -000bc270:* (glob)
  -000bc280:* (glob)
  +000bc270:* (glob)
  +000bc280:* (glob)
   000bc290: 4000 0000 0001 0000 0000 0000 0000 5000  @.............P.
   000bc2a0: 4000 4000 0000 0000 0000 0000 0000 0000  @.@.............
   000bc2b0: 0000 0000 0000 0000 0000 0000 0000 0006  ................
  @@ -325052,12 +325052,12 @@
   004f5be0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   004f5bf0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   004f5c00: 482b 0004 8000 0000 3130 2e30 0000 0000  H+......10.0....
  -004f5c10:* (glob)
  +004f5c10:* (glob)
   004f5c20: 0000 0001 0000 0002 0000 1000 0000 04f6  ................
   004f5c30: 0000 0480 0000 011e 0001 0000 0001 0000  ................
   004f5c40: 0000 0013 0000 0002 0000 0000 0000 0001  ................
   004f5c50: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  -004f5c60:* (glob)
  +004f5c60:* (glob)
   004f5c70: 0000 0000 0000 1000 0000 1000 0000 0001  ................
   004f5c80: 0000 0001 0000 0001 0000 0000 0000 0000  ................
   004f5c90: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  [1]

Remove the unneeded dmg:

  $ rm $REFERENCE/hdiutilp.hfs.dmg
