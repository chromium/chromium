# W3C Web Platform Tests in Blink Web Tests

Design Doc: https://goo.gl/iXUaZd

This directory contains checked out and reduced code from web-platform-tests
(https://github.com/web-platform-tests/wpt/) required to run WPT tests as part
of Blink's test infrastructure and some maintenance/configuration code.

For licensing, see README.chromium

**

Files in this directory (non third-party)

README.chromium
===============
Parseable details on the project name, URL, license, etc.

README.md
=========
This file.

checkout.sh
===========
Running this script without arguments will remove the existing checkout
(//third_party/wpt_tools/wpt) and perform a fresh one. See "Rolling in WPT".

WPTIncludeList
==============
The explicit list of files being kept, everything else not on this list is
deleted when running "./checkout.sh reduce". Use this file to control what gets
checked in and try to keep the list as small as possible (use what you need).

certs/
======
This directory contains a private key and a certificate of WPTServe, and files
for self-signed CA. By default, WPTServe generates these files using the
"openssl" command, but we check in pre-generated files to avoid "openssl"
dependency.

These certificates will expire in January 2025. Here is an instruction to
re-generate them:

1. Make sure the following commands are in $PATH.
 - base64
 - git
 - grep
 - openssl
 - sed
2. Run update_certs.py
3. Look at the "Not After" date in the output of the command, and update
  "January 2025" in this document and expiration_date in wptserve.py to new
  expiration date.
4. Update certs/127.0.0.1.sxg.\*.
  Please refer to
  //third_party/blink/web_tests/http/tests/loading/sxg/resources/README.md
5. git commit
6. git cl upload, etc.

Rolling in WPT
==============

If there are new files that need to be rolled in, add the intended files to
the WPTIncludeList. Ensure these files are in the correct order by running
"LC_ALL=C sort WPTIncludeList".

The easiest way to roll is to just call "./roll_wpt.py" which does everything
listed below and uploads a CL to Gerrit. See instructions below for more
manually rolling in WPT.

When rolling in new versions of WPT support, make note of the revision you want
to roll to.  You can then call "./checkout.sh REVISION clone" which will
pull in all the code.

It is also important to update the hashes in the 'Version:' fields of
//third_party/wpt_tools/README.chromium. While you're in this file, look at the
"Local Modifications" section which lists ways in which Chromium has diverged
from WPT. Make sure these modifications are persisted when reviewing the changes
being made.

You can examine what's pulled in and update WPTIncludeList if some new files are
required to run the updated version.

Once you've cloned the repositories you can call "./checkout.sh reduce" to
remove everything that is not listed in WPTIncludeList.

Note that calling "./checkout.sh" with only a revision argument is equivalent
of calling "./checkout.sh clone reduce".

Configuration
=============

Read instructions in WPT README:
https://github.com/web-platform-tests/wpt/blob/master/README.md

Also, check out the WPTServe Documentation
(https://web-platform-tests.org/tools/wptserve/docs/).

Note that editing /etc/hosts is not required for run_web_tests.py since
content_shell is invoked with flags to map all \*.test domains to 127.0.0.1.

Running web-platform-tests with enabled WPTServe on a local machine
===================================================================

WPTServe is now enabled by default in run_web_tests.py for tests that live in
web_tests/external/wpt.

WPTServe starts HTTP/S and WS/S servers as separate processes.

The content_shell used to run the tests will receive the URL of each test
(instead of a filename). The document root http://web-platform.test/ maps to
web_test/external/wpt. HTTPS tests are enabled by default.

Example run:

./tools/run_web_tests.py external/wpt
