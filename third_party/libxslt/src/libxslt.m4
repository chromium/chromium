# Based on:
# Configure paths for LIBXML2
# Toshio Kuratomi 2001-04-21
# Adapted from:
# Configure paths for GLIB
# Owen Taylor     97-11-3
#
# Modified to work with libxslt by Thomas Schraitle 2002/10/25
# Fixed by Edward Rudd 2004/05/12

dnl AM_PATH_XSLT([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for XML, and define XML_CFLAGS and XML_LIBS
dnl
AC_DEFUN([AM_PATH_XSLT],[
AC_ARG_WITH(xslt-prefix,
            [  --with-xslt-prefix=PFX   Prefix where libxslt is installed (optional)],
            xslt_config_prefix="$withval", xslt_config_prefix="")
AC_ARG_WITH(xslt-exec-prefix,
            [  --with-xslt-exec-prefix=PFX Exec prefix where libxslt is installed (optional)],
            xslt_config_exec_prefix="$withval", xslt_config_exec_prefix="")
AC_ARG_ENABLE(xslttest,
              [  --disable-xslttest       Do not try to compile and run a test LIBXSLT program],,
              enable_xslttest=yes)

  if test x$xslt_config_exec_prefix != x ; then
     xslt_config_args="$xslt_config_args --exec-prefix=$xslt_config_exec_prefix"
     if test x${XSLT_CONFIG+set} != xset ; then
        XSLT_CONFIG=$xslt_config_exec_prefix/bin/xslt-config
     fi
  fi
  if test x$xslt_config_prefix != x ; then
     xslt_config_args="$xslt_config_args --prefix=$xslt_config_prefix"
     if test x${XSLT_CONFIG+set} != xset ; then
        XSLT_CONFIG=$xslt_config_prefix/bin/xslt-config
     fi
  fi

  AC_PATH_PROG(XSLT_CONFIG, xslt-config, no)
  min_xslt_version=ifelse([$1], ,1.0.0,[$1])
  AC_MSG_CHECKING(for libxslt - version >= $min_xslt_version)
  no_xslt=""
  if test "$XSLT_CONFIG" = "no" ; then
    no_xslt=yes
  else
    XSLT_CFLAGS=`$XSLT_CONFIG $xslt_config_args --cflags`
    XSLT_LIBS=`$XSLT_CONFIG $xslt_config_args --libs`
    xslt_config_major_version=`$XSLT_CONFIG $xslt_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    xslt_config_minor_version=`$XSLT_CONFIG $xslt_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    xslt_config_micro_version=`$XSLT_CONFIG $xslt_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_xslttest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $XSLT_CFLAGS"
      LIBS="$XSLT_LIBS $LIBS"
dnl
dnl Now check if the installed libxslt is sufficiently new.
dnl (Also sanity checks the results of xslt-config to some extent)
dnl
      rm -f conf.xslttest
      AC_TRY_RUN([
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libxslt/xsltconfig.h>
#include <libxslt/xslt.h>
int 
main()
{
  int xslt_major_version, xslt_minor_version, xslt_micro_version;
  int major, minor, micro;
  char *tmp_version;

  system("touch conf.xslttest");

  /* Capture xslt-config output via autoconf/configure variables */
  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = (char *)strdup("$min_xslt_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string from xslt-config\n", "$min_xslt_version");
     exit(1);
   }
   free(tmp_version);

   /* Capture the version information from the header files */
   tmp_version = (char *)strdup(LIBXSLT_DOTTED_VERSION);
   if (sscanf(tmp_version, "%d.%d.%d", &xslt_major_version, &xslt_minor_version, &xslt_micro_version) != 3) {
     printf("%s, bad version string from libxslt includes\n", "LIBXSLT_DOTTED_VERSION");
     exit(1);
   }
   free(tmp_version);

 /* Compare xslt-config output to the libxslt headers */
  if ((xslt_major_version != $xslt_config_major_version) ||
      (xslt_minor_version != $xslt_config_minor_version) ||
      (xslt_micro_version != $xslt_config_micro_version))
    {
      printf("*** libxslt header files (version %d.%d.%d) do not match\n",
         xslt_major_version, xslt_minor_version, xslt_micro_version);
      printf("*** xslt-config (version %d.%d.%d)\n",
         $xslt_config_major_version, $xslt_config_minor_version, $xslt_config_micro_version);
      return 1;
    } 
/* Compare the headers to the library to make sure we match */
  /* Less than ideal -- doesn't provide us with return value feedback, 
   * only exits if there's a serious mismatch between header and library.
   */
    /* copied from LIBXML_TEST_VERSION; */
    xmlCheckVersion(LIBXML_VERSION);

    /* Test that the library is greater than our minimum version */
    if ((xslt_major_version > major) ||
        ((xslt_major_version == major) && (xslt_minor_version > minor)) ||
        ((xslt_major_version == major) && (xslt_minor_version == minor) &&
        (xslt_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of libxslt (%d.%d.%d) was found.\n",
               xslt_major_version, xslt_minor_version, xslt_micro_version);
        printf("*** You need a version of libxslt newer than %d.%d.%d.\n",
           major, minor, micro);
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the xslt-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of LIBXSLT, but you can also set the XSLT_CONFIG environment to point to the\n");
        printf("*** correct copy of xslt-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
    }
  return 1;
}
],, no_xslt=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi

  if test "x$no_xslt" = x ; then
     AC_MSG_RESULT(yes (version $xslt_config_major_version.$xslt_config_minor_version.$xslt_config_micro_version))
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$XSLT_CONFIG" = "no" ; then
       echo "*** The xslt-config script installed by LIBXSLT could not be found"
       echo "*** If libxslt was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the XSLT_CONFIG environment variable to the"
       echo "*** full path to xslt-config."
     else
       if test -f conf.xslttest ; then
        :
       else
          echo "*** Could not run libxslt test program, checking why..."
          CFLAGS="$CFLAGS $XSLT_CFLAGS"
          LIBS="$LIBS $XSLT_LIBS"
          AC_TRY_LINK([
#include <libxslt/xslt.h>
#include <stdio.h>
],      [ LIBXSLT_TEST_VERSION; return 0;],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding LIBXSLT or finding the wrong"
          echo "*** version of LIBXSLT. If it is not finding LIBXSLT, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
          echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH" ],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means LIBXSLT was incorrectly installed"
          echo "*** or that you have moved LIBXSLT since it was installed. In the latter case, you"
          echo "*** may want to edit the xslt-config script: $XSLT_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi

     XSLT_CFLAGS=""
     XSLT_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(XSLT_CFLAGS)
  AC_SUBST(XSLT_LIBS)
  rm -f conf.xslttest
])
