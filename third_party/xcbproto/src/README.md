About xcb-proto
===============

xcb-proto provides the XML-XCB protocol descriptions that libxcb uses to
generate the majority of its code and API. We provide them separately
from libxcb to allow reuse by other projects, such as additional
language bindings, protocol dissectors, or documentation generators.

This separation between the XCB transport layer and the
automatically-generated protocol layer also makes it far easier to write
new extensions. With the Xlib infrastructure, client-side support for
new extensions requires significant duplication of effort. With XCB and
the XML-XCB protocol descriptions, client-side support for a new
extension requires only an XML description of the extension, and not a
single line of code.

Python libraries: xcb-proto also contains language-independent Python
libraries that are used to parse an XML description and create objects
used by Python code generators in individual language bindings.  These
libraries are installed into $(prefix)/lib/pythonX.X/site-packages.  If
this location is not on your system's Python path, scripts that import
them will fail with import errors.  In this case you must add the
install location to your Python path by creating a file with a `.pth'
extension in a directory that _is_ on the Python path, and put the
path to the install location in that file.  For example, on my system
there is a file named 'local.pth' in /usr/lib/python2.5/site-packages,
which contains '/usr/local/lib/python2.5/site-packages'.  Note that
this is only necessary on machines where XCB is being built.

Please report any issues you find to the freedesktop.org bug tracker at:

  https://gitlab.freedesktop.org/xorg/proto/xcbproto/issues

Discussion about XCB occurs on the XCB mailing list:

  https://lists.freedesktop.org/mailman/listinfo/xcb

You can obtain the latest development versions of xcb-proto using GIT from
the xcbproto code repository at:

  https://gitlab.freedesktop.org/xorg/proto/xcbproto

  For anonymous checkouts, use:

    git clone https://gitlab.freedesktop.org/xorg/proto/xcbproto.git

  For developers, use:

    git clone git@gitlab.freedesktop.org:xorg/proto/xcbproto.git
