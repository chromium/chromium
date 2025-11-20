Make sure we have a fresh build:

  $ export BUILDDIR=$TESTDIR/../build
  $ cd $BUILDDIR
  $ make 2> /dev/null >/dev/null
  $ cd $CRAMTMP
  $ export OUTPUT=attribution_output
  $ mkdir $OUTPUT

  $ $BUILDDIR/dmg/dmg build $TESTDIR/attribution_reference/hdiutila.hfs $OUTPUT/testa.dmg __MOZILLA__attr-value- >/dev/null

Note the "attr-value-p" suffix below!

  $ $BUILDDIR/dmg/dmg build $TESTDIR/attribution_reference/hdiutilp.hfs $OUTPUT/testb.dmg __MOZILLA__attr-value- >/dev/null

  $ xxd $OUTPUT/testa.dmg > $OUTPUT/testa.txt
  $ xxd $OUTPUT/testb.dmg > $OUTPUT/testb.txt
  $ diff --unified=3 $OUTPUT/testa.txt $OUTPUT/testb.txt
  --- *testa.txt* (glob)
  +++ *testb.txt* (glob)
  @@ -3349,7 +3349,7 @@
   0000d140: 7400 7200 2d00 6b00 6500 7900 0000 1000  t.r.-.k.e.y.....
   0000d150: 0000 0000 0000 0000 0000 175f 5f4d 4f5a  ...........__MOZ
   0000d160: 494c 4c41 5f5f 6174 7472 2d76 616c 7565  ILLA__attr-value
  -0000d170: 2d61 0000 0000 0000 0000 0000 0000 0000  -a..............
  +0000d170: 2d70 0000 0000 0000 0000 0000 0000 0000  -p..............
   0000d180: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   0000d190: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   0000d1a0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  @@ -16652,7 +16652,7 @@
   000410b0: 4141 6741 4141 4141 4141 4141 4141 4141  AAgAAAAAAAAAAAAA
   000410c0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
   000410d0: 4141 4141 410a 0909 0909 4141 4941 4141  AAAAA.....AAIAAA
  -000410e0: 4167 3544 5163 6877 4141 4141 4141 4141  Ag5DQchwAAAAAAAA
  +000410e0: 4167 2b53 4962 7641 4141 4141 4141 4141  Ag+SIbvAAAAAAAAA
   000410f0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
   00041100: 4141 4141 4141 0a09 0909 0941 4141 4141  AAAAAA.....AAAAA
   00041110: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  @@ -16831,7 +16831,7 @@
   00041be0: 3c2f 7374 7269 6e67 3e0a 0909 0909 3c6b  </string>.....<k
   00041bf0: 6579 3e44 6174 613c 2f6b 6579 3e0a 0909  ey>Data</key>...
   00041c00: 0909 3c64 6174 613e 0a09 0909 0941 5141  ..<data>.....AQA
  -00041c10: 4341 4141 412f 4d47 4e7a 773d 3d0a 0909  CAAAA/MGNzw==...
  +00041c10: 4341 4141 412b 3845 4638 413d 3d0a 0909  CAAAA+8EF8A==...
   00041c20: 0909 3c2f 6461 7461 3e0a 0909 0909 3c6b  ..</data>.....<k
   00041c30: 6579 3e49 443c 2f6b 6579 3e0a 0909 0909  ey>ID</key>.....
   00041c40: 3c73 7472 696e 673e 323c 2f73 7472 696e  <string>2</strin
  @@ -16952,15 +16952,15 @@
   00042370: 6c6a 6444 344b 4354 7872 5a58 6b2b 5530  ljdD4KCTxrZXk+U0
   00042380: 6842 4c54 4574 5a47 6c6e 5a58 4e30 5043  hBLTEtZGlnZXN0PC
   00042390: 3972 5a58 6b2b 4367 6b38 0a09 0909 095a  9rZXk+Cgk8.....Z
  -000423a0: 4746 3059 5434 4b43 5546 3253 5564 4253  GF0YT4KCUF2SUdBS
  -000423b0: 446c 3552 6d78 4e5a 4856 6f63 4374 6b4d  Dl5RmxNZHVocCtkM
  -000423c0: 6b4a 454d 7a6c 5656 6e4a 7a0a 0909 0909  kJEMzlVVnJz.....
  -000423d0: 5154 304b 4354 7776 5a47 4630 5954 344b  QT0KCTwvZGF0YT4K
  +000423a0: 4746 3059 5434 4b43 564e 6e62 444a 784d  GF0YT4KCVNnbDJxM
  +000423b0: 7a5a 7663 6b64 7659 3164 7564 3052 485a  zZvckdvY1dud0RHZ
  +000423c0: 6b4d 324f 5868 5254 3077 790a 0909 0909  kM2OXhRT0wy.....
  +000423d0: 5754 304b 4354 7776 5a47 4630 5954 344b  WT0KCTwvZGF0YT4K
   000423e0: 4354 7872 5a58 6b2b 596d 7876 5932 7374  CTxrZXk+YmxvY2st
   000423f0: 5932 686c 5932 747a 6457 3074 0a09 0909  Y2hlY2tzdW0t....
   00042400: 094d 6a77 7661 3256 3550 676f 4a50 476c  .Mjwva2V5PgoJPGl
  -00042410: 7564 4756 6e5a 5849 2b4c 5467 784d 6a63  udGVnZXI+LTgxMjc
  -00042420: 354d 7a4d 304f 4477 7661 5735 300a 0909  5MzM0ODwvaW50...
  +00042410: 7564 4756 6e5a 5849 2b4c 5449 324f 4441  udGVnZXI+LTI2ODA
  +00042420: 314f 4445 784e 7a77 7661 5735 300a 0909  1ODExNzwvaW50...
   00042430: 0909 5a57 646c 636a 344b 4354 7872 5a58  ..ZWdlcj4KCTxrZX
   00042440: 6b2b 596e 6c30 5a58 4d38 4c32 746c 6554  k+Ynl0ZXM8L2tleT
   00042450: 344b 4354 7870 626e 526c 5a32 5679 0a09  4KCTxpbnRlZ2Vy..
  @@ -17105,8 +17105,8 @@
   00042d00: 4141 4258 3344 424d 4877 4541 4141 4141  AABX3DBMHwEAAAAA
   00042d10: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
   00042d20: 4141 4166 0a09 0909 0941 5141 4141 4141  AAAf.....AQAAAAA
  -00042d30: 4141 4141 4142 4141 4141 4141 415a 3871  AAAAABAAAAAAAZ8q
  -00042d40: 3836 3646 494d 3855 7442 5141 4141 4141  866FIM8UtBQAAAAA
  +00042d30: 4141 4141 4142 4141 4141 4141 4170 7856  AAAAABAAAAAAApxV
  +00042d40: 6f30 4b46 494d 3855 7442 5141 4141 4141  o0KFIM8UtBQAAAAA
   00042d50: 4141 4a62 690a 0909 0909 4171 3041 5945  AAJbi.....Aq0AYE
   00042d60: 7341 4141 4141 4141 3d3d 0a09 0909 093c  sAAAAAAA==.....<
   00042d70: 2f73 7472 696e 673e 0a09 0909 3c2f 6469  /string>....</di
  @@ -17159,8 +17159,8 @@
   00043060: 0000 0000 0000 0000 0000 0004 064c 0000  .............L..
   00043070: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   00043080: 0001 0000 0001 7348 3366 6998 3c64 c623  ......sH3fi.<d.#
  -00043090: 7b32 6745 8b6b 0000 0002 0000 0020 8e77  {2gE.k....... .w
  -000430a0: 24c9 0000 0000 0000 0000 0000 0000 0000  $...............
  +00043090: 7b32 6745 8b6b 0000 0002 0000 0020 8eb5  {2gE.k....... ..
  +000430a0: c555 0000 0000 0000 0000 0000 0000 0000  .U..............
   000430b0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   000430c0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   000430d0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  @@ -17176,8 +17176,8 @@
   00043170: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   00043180: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   00043190: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  -000431a0: 0000 0000 0000 0000 0002 0000 0020 2339  ............. #9
  -000431b0: ec78 0000 0000 0000 0000 0000 0000 0000  .x..............
  +000431a0: 0000 0000 0000 0000 0002 0000 0020 0b7d  ............. .}
  +000431b0: d859 0000 0000 0000 0000 0000 0000 0000  .Y..............
   000431c0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   000431d0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
   000431e0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  [1]

Check resources:

  $ $BUILDDIR/dmg/dmg res $OUTPUT/testa.dmg - | expand -t 4
  <?xml version="1.0" encoding="UTF-8"?>
  <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
  <plist version="1.0">
  <dict>
      <key>resource-fork</key>
      <dict>
          <key>blkx</key>
          <array>
              <dict>
                  <key>Attributes</key>
                  <string>0x0050</string>
                  <key>CFName</key>
                  <string>Driver Descriptor Map (DDM : 0)</string>
                  <key>Data</key>
                  <data>
                  bWlzaAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA
                  AAII/////wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAIAAAAgXDMYCQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAACgAAABgAAAAAAAAAAAAAAAAAAAAAAAAABAAAA
                  AAAAAAAAAAAAAAAANf////8AAAAAAAAAAAAAAAEAAAAA
                  AAAAAAAAAAAAAAA1AAAAAAAAAAA=
                  </data>
                  <key>ID</key>
                  <string>-1</string>
                  <key>Name</key>
                  <string>Driver Descriptor Map (DDM : 0)</string>
              </dict>
              <dict>
                  <key>Attributes</key>
                  <string>0x0050</string>
                  <key>CFName</key>
                  <string>Apple (Apple_partition_map : 1)</string>
                  <key>Data</key>
                  <data>
                  bWlzaAAAAAEAAAAAAAAAAQAAAAAAAAA/AAAAAAAAAAAA
                  AAIIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAIAAAAg7xHa2gAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAACgAAABgAAAAAAAAAAAAAAAAAAAAAAAAA/AAAA
                  AAAAADUAAAAAAAAAv/////8AAAAAAAAAAAAAAD8AAAAA
                  AAAAAAAAAAAAAAD0AAAAAAAAAAA=
                  </data>
                  <key>ID</key>
                  <string>0</string>
                  <key>Name</key>
                  <string>Apple (Apple_partition_map : 1)</string>
              </dict>
              <dict>
                  <key>Attributes</key>
                  <string>0x0050</string>
                  <key>CFName</key>
                  <string>Macintosh (Apple_Driver_ATAPI : 2)</string>
                  <key>Data</key>
                  <data>
                  bWlzaAAAAAEAAAAAAAAAQAAAAAAAAAAIAAAAAAAAAAAA
                  AAIIAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAIAAAAgxxwAEQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAACgAAABgAAAAAAAAAAAAAAAAAAAAAAAAAIAAAA
                  AAAAAPQAAAAAAAAAK/////8AAAAAAAAAAAAAAAgAAAAA
                  AAAAAAAAAAAAAAEfAAAAAAAAAAA=
                  </data>
                  <key>ID</key>
                  <string>1</string>
                  <key>Name</key>
                  <string>Macintosh (Apple_Driver_ATAPI : 2)</string>
              </dict>
              <dict>
                  <key>Attributes</key>
                  <string>0x0050</string>
                  <key>CFName</key>
                  <string>Mac_OS_X (Apple_HFSX : 3)</string>
                  <key>Data</key>
                  <data>
                  bWlzaAAAAAEAAAAAAAAASAAAAAAAACewAAAAAAAAAAAA
                  AAIIAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAIAAAAg5DQchwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAVAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAIAAAAA
                  AAAAAR8AAAAAAAQAAIAAAAYAAAAAAAAAAAAAAgAAAAAA
                  AAACAAAAAAAABAEfAAAAAAAAAC2AAAAGAAAAAAAAAAAA
                  AAQAAAAAAAAAAgAAAAAAAAQBTAAAAAAAAAF0gAAABgAA
                  AAAAAAAAAAAGAAAAAAAAAAIAAAAAAAAEAsAAAAAAAAAA
                  LYAAAAYAAAAAAAAAAAAACAAAAAAAAAACAAAAAAAABALt
                  AAAAAAAAAEiAAAAGAAAAAAAAAAAAAAoAAAAAAAAAAgAA
                  AAAAAAQDNQAAAAAAAAAtgAAABgAAAAAAAAAAAAAMAAAA
                  AAAAAAIAAAAAAAAEA2IAAAAAAAAALYAAAAYAAAAAAAAA
                  AAAADgAAAAAAAAACAAAAAAAABAOPAAAAAAAAAC2AAAAG
                  AAAAAAAAAAAAABAAAAAAAAAAAgAAAAAAAAQDvAAAAAAA
                  AAAtgAAABgAAAAAAAAAAAAASAAAAAAAAAAIAAAAAAAAE
                  A+kAAAAAAAAALYAAAAYAAAAAAAAAAAAAFAAAAAAAAAAC
                  AAAAAAAABAQWAAAAAAAAAC2AAAAGAAAAAAAAAAAAABYA
                  AAAAAAAAAgAAAAAAAAQEQwAAAAAAAAAtgAAABgAAAAAA
                  AAAAAAAYAAAAAAAAAAIAAAAAAAAEBHAAAAAAAAAALYAA
                  AAYAAAAAAAAAAAAAGgAAAAAAAAACAAAAAAAABASdAAAA
                  AAAAAC2AAAAGAAAAAAAAAAAAABwAAAAAAAAAAgAAAAAA
                  AAQEygAAAAAAAAAtgAAABgAAAAAAAAAAAAAeAAAAAAAA
                  AAIAAAAAAAAEBPcAAAAAAAAALYAAAAYAAAAAAAAAAAAA
                  IAAAAAAAAAACAAAAAAAABAUkAAAAAAAAAC2AAAAGAAAA
                  AAAAAAAAACIAAAAAAAAAAgAAAAAAAAQFUQAAAAAAAAAt
                  gAAABgAAAAAAAAAAAAAkAAAAAAAAAAIAAAAAAAAEBX4A
                  AAAAAAAALYAAAAYAAAAAAAAAAAAAJgAAAAAAAAABsAAA
                  AAAABAWrAAAAAAAAAKH/////AAAAAAAAAAAAACewAAAA
                  AAAAAAAAAAAAAAQGTAAAAAAAAAAA
                  </data>
                  <key>ID</key>
                  <string>2</string>
                  <key>Name</key>
                  <string>Mac_OS_X (Apple_HFSX : 3)</string>
              </dict>
              <dict>
                  <key>Attributes</key>
                  <string>0x0050</string>
                  <key>CFName</key>
                  <string> (Apple_Free : 4)</string>
                  <key>Data</key>
                  <data>
                  bWlzaAAAAAEAAAAAAAAn+AAAAAAAAAAKAAAAAAAAAAAA
                  AAAAAAAAAwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAIAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAACAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAKAAAA
                  AAAEBkwAAAAAAAAAAP////8AAAAAAAAAAAAAAAoAAAAA
                  AAAAAAAAAAAABAZMAAAAAAAAAAA=
                  </data>
                  <key>ID</key>
                  <string>3</string>
                  <key>Name</key>
                  <string> (Apple_Free : 4)</string>
              </dict>
          </array>
          <key>cSum</key>
          <array>
              <dict>
                  <key>Attributes</key>
                  <string>0x0000</string>
                  <key>Data</key>
                  <data>
                  AQACAAAAhY246A==
                  </data>
                  <key>ID</key>
                  <string>0</string>
                  <key>Name</key>
                  <string></string>
              </dict>
              <dict>
                  <key>Attributes</key>
                  <string>0x0000</string>
                  <key>Data</key>
                  <data>
                  AQACAAAAAAAAAA==
                  </data>
                  <key>ID</key>
                  <string>1</string>
                  <key>Name</key>
                  <string></string>
              </dict>
              <dict>
                  <key>Attributes</key>
                  <string>0x0000</string>
                  <key>Data</key>
                  <data>
                  AQACAAAA/MGNzw==
                  </data>
                  <key>ID</key>
                  <string>2</string>
                  <key>Name</key>
                  <string></string>
              </dict>
          </array>
          <key>nsiz</key>
          <array>
              <dict>
                  <key>Attributes</key>
                  <string>0x0000</string>
                  <key>Data</key>
                  <data>
                  PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRG
                  LTgiPz4KPCFET0NUWVBFIHBsaXN0IFBVQkxJQyAiLS8v
                  QXBwbGUvL0RURCBQTElTVCAxLjAvL0VOIiAiaHR0cDov
                  L3d3dy5hcHBsZS5jb20vRFREcy9Qcm9wZXJ0eUxpc3Qt
                  MS4wLmR0ZCI+CjxwbGlzdCB2ZXJzaW9uPSIxLjAiPgo8
                  ZGljdD4KCTxrZXk+YmxvY2stY2hlY2tzdW0tMjwva2V5
                  PgoJPGludGVnZXI+LTM5MDU1ODMzMTwvaW50ZWdlcj4K
                  CTxrZXk+cGFydC1udW08L2tleT4KCTxpbnRlZ2VyPjA8
                  L2ludGVnZXI+Cgk8a2V5PnZlcnNpb248L2tleT4KCTxp
                  bnRlZ2VyPjY8L2ludGVnZXI+CjwvZGljdD4KPC9wbGlz
                  dD4K
                  </data>
                  <key>ID</key>
                  <string>0</string>
                  <key>Name</key>
                  <string></string>
              </dict>
              <dict>
                  <key>Attributes</key>
                  <string>0x0000</string>
                  <key>Data</key>
                  <data>
                  PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRG
                  LTgiPz4KPCFET0NUWVBFIHBsaXN0IFBVQkxJQyAiLS8v
                  QXBwbGUvL0RURCBQTElTVCAxLjAvL0VOIiAiaHR0cDov
                  L3d3dy5hcHBsZS5jb20vRFREcy9Qcm9wZXJ0eUxpc3Qt
                  MS4wLmR0ZCI+CjxwbGlzdCB2ZXJzaW9uPSIxLjAiPgo8
                  ZGljdD4KCTxrZXk+YmxvY2stY2hlY2tzdW0tMjwva2V5
                  PgoJPGludGVnZXI+MDwvaW50ZWdlcj4KCTxrZXk+cGFy
                  dC1udW08L2tleT4KCTxpbnRlZ2VyPjE8L2ludGVnZXI+
                  Cgk8a2V5PnZlcnNpb248L2tleT4KCTxpbnRlZ2VyPjY8
                  L2ludGVnZXI+CjwvZGljdD4KPC9wbGlzdD4K
                  </data>
                  <key>ID</key>
                  <string>1</string>
                  <key>Name</key>
                  <string></string>
              </dict>
              <dict>
                  <key>Attributes</key>
                  <string>0x0000</string>
                  <key>Data</key>
                  <data>
                  PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRG
                  LTgiPz4KPCFET0NUWVBFIHBsaXN0IFBVQkxJQyAiLS8v
                  QXBwbGUvL0RURCBQTElTVCAxLjAvL0VOIiAiaHR0cDov
                  L3d3dy5hcHBsZS5jb20vRFREcy9Qcm9wZXJ0eUxpc3Qt
                  MS4wLmR0ZCI+CjxwbGlzdCB2ZXJzaW9uPSIxLjAiPgo8
                  ZGljdD4KCTxrZXk+U0hBLTEtZGlnZXN0PC9rZXk+Cgk8
                  ZGF0YT4KCUF2SUdBSDl5RmxNZHVocCtkMkJEMzlVVnJz
                  QT0KCTwvZGF0YT4KCTxrZXk+YmxvY2stY2hlY2tzdW0t
                  Mjwva2V5PgoJPGludGVnZXI+LTgxMjc5MzM0ODwvaW50
                  ZWdlcj4KCTxrZXk+Ynl0ZXM8L2tleT4KCTxpbnRlZ2Vy
                  PjE1NTY0ODwvaW50ZWdlcj4KCTxrZXk+ZGF0ZTwva2V5
                  PgoJPGludGVnZXI+LTUzMjU5Nzk4OTwvaW50ZWdlcj4K
                  CTxrZXk+cGFydC1udW08L2tleT4KCTxpbnRlZ2VyPjI8
                  L2ludGVnZXI+Cgk8a2V5PnZlcnNpb248L2tleT4KCTxp
                  bnRlZ2VyPjY8L2ludGVnZXI+Cgk8a2V5PnZvbHVtZS1z
                  aWduYXR1cmU8L2tleT4KCTxpbnRlZ2VyPjE4NDc1PC9p
                  bnRlZ2VyPgo8L2RpY3Q+CjwvcGxpc3Q+Cg==
                  </data>
                  <key>ID</key>
                  <string>2</string>
                  <key>Name</key>
                  <string></string>
              </dict>
          </array>
          <key>plst</key>
          <array>
              <dict>
                  <key>Attributes</key>
                  <string>0x0050</string>
                  <key>Data</key>
                  <data>
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEAAQAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAA
                  </data>
                  <key>ID</key>
                  <string>0</string>
                  <key>Name</key>
                  <string>
                  cnR0YQEAAABX3DBMHwEAAAAAAAAAAAAAAAAAAAAAAAAf
                  AQAAAAAAAAAABAAAAAAAZ8q866FIM8UtBQAAAAAAAJbi
                  Aq0AYEsAAAAAAA==
                  </string>
              </dict>
          </array>
          <key>size</key>
          <array>
              <dict>
                  <key>Attributes</key>
                  <string>0x0000</string>
                  <key>Data</key>
                  <data>
                  BQABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                  AAAAAAAAAAAAABszQeAAAAAAK0gBAA==
                  </data>
                  <key>ID</key>
                  <string>2</string>
                  <key>Name</key>
                  <string></string>
              </dict>
          </array>
      </dict>
  </dict>
  </plist>

Line in the sand:

  $ shasum $OUTPUT/testa.dmg $OUTPUT/testb.dmg
  85f7e85acef53b63fc199e84ff801ae96b3e8c83 *testa.dmg (glob)
  ef6e91b330f5eacd10eb5e37603b8baa06d4a1a4 *testb.dmg (glob)

Attribute:

  $ $BUILDDIR/dmg/dmg attribute $OUTPUT/testa.dmg $OUTPUT/testa_updated.dmg __MOZILLA__attr-value- __MOZILLA__attr-value-p >/dev/null

Unfortunately, attributing builds does not update the cSum block, so we expect some differences here:

  $ xxd $OUTPUT/testa_updated.dmg > $OUTPUT/testa_updated.txt
  $ diff --unified=3 $OUTPUT/testb.txt $OUTPUT/testa_updated.txt
  --- *testb.txt* (glob)
  +++ *testa_updated.txt* (glob)
  @@ -16831,7 +16831,7 @@
   00041be0: 3c2f 7374 7269 6e67 3e0a 0909 0909 3c6b  </string>.....<k
   00041bf0: 6579 3e44 6174 613c 2f6b 6579 3e0a 0909  ey>Data</key>...
   00041c00: 0909 3c64 6174 613e 0a09 0909 0941 5141  ..<data>.....AQA
  -00041c10: 4341 4141 412b 3845 4638 413d 3d0a 0909  CAAAA+8EF8A==...
  +00041c10: 4341 4141 412f 4d47 4e7a 773d 3d0a 0909  CAAAA/MGNzw==...
   00041c20: 0909 3c2f 6461 7461 3e0a 0909 0909 3c6b  ..</data>.....<k
   00041c30: 6579 3e49 443c 2f6b 6579 3e0a 0909 0909  ey>ID</key>.....
   00041c40: 3c73 7472 696e 673e 323c 2f73 7472 696e  <string>2</strin
  @@ -16952,15 +16952,15 @@
   00042370: 6c6a 6444 344b 4354 7872 5a58 6b2b 5530  ljdD4KCTxrZXk+U0
   00042380: 6842 4c54 4574 5a47 6c6e 5a58 4e30 5043  hBLTEtZGlnZXN0PC
   00042390: 3972 5a58 6b2b 4367 6b38 0a09 0909 095a  9rZXk+Cgk8.....Z
  -000423a0: 4746 3059 5434 4b43 564e 6e62 444a 784d  GF0YT4KCVNnbDJxM
  -000423b0: 7a5a 7663 6b64 7659 3164 7564 3052 485a  zZvckdvY1dud0RHZ
  -000423c0: 6b4d 324f 5868 5254 3077 790a 0909 0909  kM2OXhRT0wy.....
  -000423d0: 5754 304b 4354 7776 5a47 4630 5954 344b  WT0KCTwvZGF0YT4K
  +000423a0: 4746 3059 5434 4b43 5546 3253 5564 4253  GF0YT4KCUF2SUdBS
  +000423b0: 446c 3552 6d78 4e5a 4856 6f63 4374 6b4d  Dl5RmxNZHVocCtkM
  +000423c0: 6b4a 454d 7a6c 5656 6e4a 7a0a 0909 0909  kJEMzlVVnJz.....
  +000423d0: 5154 304b 4354 7776 5a47 4630 5954 344b  QT0KCTwvZGF0YT4K
   000423e0: 4354 7872 5a58 6b2b 596d 7876 5932 7374  CTxrZXk+YmxvY2st
   000423f0: 5932 686c 5932 747a 6457 3074 0a09 0909  Y2hlY2tzdW0t....
   00042400: 094d 6a77 7661 3256 3550 676f 4a50 476c  .Mjwva2V5PgoJPGl
  -00042410: 7564 4756 6e5a 5849 2b4c 5449 324f 4441  udGVnZXI+LTI2ODA
  -00042420: 314f 4445 784e 7a77 7661 5735 300a 0909  1ODExNzwvaW50...
  +00042410: 7564 4756 6e5a 5849 2b4c 5467 784d 6a63  udGVnZXI+LTgxMjc
  +00042420: 354d 7a4d 304f 4477 7661 5735 300a 0909  5MzM0ODwvaW50...
   00042430: 0909 5a57 646c 636a 344b 4354 7872 5a58  ..ZWdlcj4KCTxrZX
   00042440: 6b2b 596e 6c30 5a58 4d38 4c32 746c 6554  k+Ynl0ZXM8L2tleT
   00042450: 344b 4354 7870 626e 526c 5a32 5679 0a09  4KCTxpbnRlZ2Vy..
  [1]
  $ shasum $OUTPUT/testb.dmg $OUTPUT/testa_updated.dmg
  ef6e91b330f5eacd10eb5e37603b8baa06d4a1a4  *testb.dmg (glob)
  41fadf80df5625ac458a0c1c548f37afbc41b06e  *testa_updated.dmg (glob)

However, if we revert, the checksums should match again:
We could also revert:

  $ $BUILDDIR/dmg/dmg attribute $OUTPUT/testa_updated.dmg $OUTPUT/testa_reverted.dmg  __MOZILLA__attr-value- __MOZILLA__attr-value-a >/dev/null

Note -- same same:

  $ xxd $OUTPUT/testa_reverted.dmg > $OUTPUT/testa_reverted.txt
  $ diff --unified=3 $OUTPUT/testa.txt $OUTPUT/testa_reverted.txt
  $ shasum $OUTPUT/testa.dmg $OUTPUT/testa_reverted.dmg
  85f7e85acef53b63fc199e84ff801ae96b3e8c83  *testa.dmg (glob)
  85f7e85acef53b63fc199e84ff801ae96b3e8c83  *testa_reverted.dmg (glob)
