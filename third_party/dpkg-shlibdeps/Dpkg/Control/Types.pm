# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

package Dpkg::Control::Types;

use strict;
use warnings;

our $VERSION = '0.01';
our @EXPORT = qw(
    CTRL_UNKNOWN
    CTRL_INFO_SRC
    CTRL_INFO_PKG
    CTRL_REPO_RELEASE
    CTRL_INDEX_SRC
    CTRL_INDEX_PKG
    CTRL_PKG_SRC
    CTRL_PKG_DEB
    CTRL_FILE_BUILDINFO
    CTRL_FILE_CHANGES
    CTRL_FILE_VENDOR
    CTRL_FILE_STATUS
    CTRL_CHANGELOG
    CTRL_COPYRIGHT_HEADER
    CTRL_COPYRIGHT_FILES
    CTRL_COPYRIGHT_LICENSE
    CTRL_TESTS
);

use Exporter qw(import);

=encoding utf8

=head1 NAME

Dpkg::Control::Types - export CTRL_* constants

=head1 DESCRIPTION

You should not use this module directly. Instead you more likely
want to use Dpkg::Control which also re-exports the same constants.

This module has been introduced solely to avoid a dependency loop
between Dpkg::Control and Dpkg::Control::Fields.

=cut

use constant {
    CTRL_UNKNOWN => 0,
    # First control block in debian/control.
    CTRL_INFO_SRC => 1,
    # Subsequent control blocks in debian/control.
    CTRL_INFO_PKG => 2,
    # Entry in repository's Sources files.
    CTRL_INDEX_SRC => 4,
    # Entry in repository's Packages files.
    CTRL_INDEX_PKG => 8,
    # .dsc file of source package.
    CTRL_PKG_SRC => 16,
    # DEBIAN/control in binary packages.
    CTRL_PKG_DEB => 32,
    # .changes file.
    CTRL_FILE_CHANGES => 64,
    # File in $Dpkg::CONFDIR/origins.
    CTRL_FILE_VENDOR => 128,
    # $Dpkg::ADMINDIR/status.
    CTRL_FILE_STATUS => 256,
    # Output of dpkg-parsechangelog.
    CTRL_CHANGELOG => 512,
    # Repository's (In)Release file.
    CTRL_REPO_RELEASE => 1024,
    # Header control block in debian/copyright.
    CTRL_COPYRIGHT_HEADER => 2048,
    # Files control block in debian/copyright.
    CTRL_COPYRIGHT_FILES => 4096,
    # License control block in debian/copyright.
    CTRL_COPYRIGHT_LICENSE => 8192,
    # Package test suite control file in debian/tests/control.
    CTRL_TESTS => 16384,
    # .buildinfo file
    CTRL_FILE_BUILDINFO => 32768,
};

=head1 CHANGES

=head2 Version 0.xx

This is a private module.

=cut

1;
