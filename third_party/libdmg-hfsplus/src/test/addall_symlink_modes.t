Make sure we have a fresh build:

  $ export BUILDDIR=$TESTDIR/../build
  $ cd $BUILDDIR
  $ make 2> /dev/null >/dev/null

Prepare top-level paths:
  $ cd $CRAMTMP
  $ export OUTPUT=addall_symlink_modes_output
  $ export STAGEDIR=addall_symlink_modes_stagedir
  $ mkdir $OUTPUT
  $ mkdir $STAGEDIR

Construct inputs:

  $ mkdir $STAGEDIR/dmg-root
  $ echo "file1" >> $STAGEDIR/dmg-root/file1.txt
  $ echo "file2" >> $STAGEDIR/dmg-root/file2.txt
  $ mkdir $STAGEDIR/adjacent
  $ echo "adjacent to dmg root during packaging" >> $STAGEDIR/adjacent/adjacent.txt
  $ cd $STAGEDIR/dmg-root
  $ ln -s file1.txt ln-file1.txt
  $ ln -s ../adjacent/adjacent.txt ln-adjacent.txt
  $ cd $CRAMTMP

With "fail" symlink disposition, we should not be able to package this:
  $ cp $TESTDIR/empty.hfs $OUTPUT/fail.hfs
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/fail.hfs addall $STAGEDIR/dmg-root/ --symlinks=fail > /dev/null
  error in * (*.c, *): * symlink * (glob)
  [1]

With "clone_link" symlink disposition, we expect to see symlinks:
  $ cp $TESTDIR/empty.hfs $OUTPUT/clone_link.hfs
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/clone_link.hfs addall $STAGEDIR/dmg-root/ --symlinks=clone_link > /dev/null
  $ mkdir -p $OUTPUT/clone_link/extracted
  $ mkdir $OUTPUT/clone_link/adjacent
  $ echo "adjacent to extraction target for clone_link" >> $OUTPUT/clone_link/adjacent/adjacent.txt
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/clone_link.hfs extractall / $OUTPUT/clone_link/extracted/ > /dev/null
  $ stat --format %A $OUTPUT/clone_link/extracted/file1.txt
  -rw-r--r--
  $ cat $OUTPUT/clone_link/extracted/file1.txt
  file1
  $ stat --format %A $OUTPUT/clone_link/extracted/file2.txt
  -rw-r--r--
  $ cat $OUTPUT/clone_link/extracted/file2.txt
  file2
  $ stat --format %A $OUTPUT/clone_link/extracted/ln-file1.txt
  lrwxrwxrwx
  $ cat $OUTPUT/clone_link/extracted/ln-file1.txt
  file1
  $ stat --format %A $OUTPUT/clone_link/extracted/ln-adjacent.txt
  lrwxrwxrwx
  $ cat $OUTPUT/clone_link/extracted/ln-adjacent.txt
  adjacent to extraction target for clone_link

With "traverse" symlink disposition, we expect to see duplicates of the original files:
  $ cp $TESTDIR/empty.hfs $OUTPUT/traverse.hfs
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/traverse.hfs addall $STAGEDIR/dmg-root/ --symlinks=traverse > /dev/null
  $ mkdir -p $OUTPUT/traverse/extracted
  $ mkdir -p $OUTPUT/traverse/adjacent
  $ echo "adjacent to extraction target for traverse; should not be seen" >> $OUTPUT/traverse/adjacent/adjacent.txt
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/traverse.hfs extractall / $OUTPUT/traverse/extracted > /dev/null
  $ stat --format %A $OUTPUT/traverse/extracted/file1.txt
  -rw-r--r--
  $ cat $OUTPUT/traverse/extracted/file1.txt
  file1
  $ stat --format %A $OUTPUT/traverse/extracted/file2.txt
  -rw-r--r--
  $ cat $OUTPUT/traverse/extracted/file2.txt
  file2
  $ stat --format %A $OUTPUT/traverse/extracted/ln-file1.txt
  -rw-r--r--
  $ cat $OUTPUT/traverse/extracted/ln-file1.txt
  file1
  $ stat --format %A $OUTPUT/traverse/extracted/ln-adjacent.txt
  -rw-r--r--
  $ cat $OUTPUT/traverse/extracted/ln-adjacent.txt
  adjacent to dmg root during packaging

Without any symlink disposition, default is equivalent to "traverse":
  $ cp $TESTDIR/empty.hfs $OUTPUT/default.hfs
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/default.hfs addall $STAGEDIR/dmg-root/ > /dev/null
  $ mkdir -p $OUTPUT/default/extracted
  $ mkdir -p $OUTPUT/default/adjacent
  $ echo "adjacent to extraction target for default; should not be seen" >> $OUTPUT/default/adjacent/adjacent.txt
  $ $BUILDDIR/hfs/hfsplus $OUTPUT/traverse.hfs extractall / $OUTPUT/default/extracted > /dev/null
  $ stat --format %A $OUTPUT/default/extracted/file1.txt
  -rw-r--r--
  $ cat $OUTPUT/default/extracted/file1.txt
  file1
  $ stat --format %A $OUTPUT/default/extracted/file2.txt
  -rw-r--r--
  $ cat $OUTPUT/default/extracted/file2.txt
  file2
  $ stat --format %A $OUTPUT/default/extracted/ln-file1.txt
  -rw-r--r--
  $ cat $OUTPUT/default/extracted/ln-file1.txt
  file1
  $ stat --format %A $OUTPUT/default/extracted/ln-adjacent.txt
  -rw-r--r--
  $ cat $OUTPUT/default/extracted/ln-adjacent.txt
  adjacent to dmg root during packaging