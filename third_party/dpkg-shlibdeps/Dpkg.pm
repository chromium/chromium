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

package Dpkg;

=encoding utf8

=head1 NAME

Dpkg - module with core variables

=head1 DESCRIPTION

The Dpkg module provides a set of variables with information concerning
this system installation.

It is also the entry point to the Dpkg module hierarchy.

=cut

use strict;
use warnings;

our $VERSION = '2.00';
our @EXPORT_OK = qw(
    $PROGNAME
    $PROGVERSION
    $PROGMAKE
    $PROGTAR
    $PROGPATCH
    $CONFDIR
    $ADMINDIR
    $LIBDIR
    $DATADIR
);

use Exporter qw(import);

=head1 VARIABLES

=over 4

=item $Dpkg::PROGNAME

Contains the name of the current program.

=item $Dpkg::PROGVERSION

Contains the version of the dpkg suite.

=item $Dpkg::PROGMAKE

Contains the name of the system GNU make program.

=item $Dpkg::PROGTAR

Contains the name of the system GNU tar program.

=item $Dpkg::PROGPATCH

Contains the name of the system GNU patch program (or another implementation
that is directory traversal resistant).

=item $Dpkg::CONFDIR

Contains the path to the dpkg system configuration directory.

=item $Dpkg::ADMINDIR

Contains the path to the dpkg database directory.

=item $Dpkg::LIBDIR

Contains the path to the dpkg methods and plugins directory.

=item $Dpkg::DATADIR

Contains the path to the dpkg architecture tables directory.

=back

=cut

our ($PROGNAME) = $0 =~ m{(?:.*/)?([^/]*)};

# The following lines are automatically fixed at install time
our $PROGVERSION = '1.20.x';
our $PROGMAKE = $ENV{DPKG_PROGMAKE} // 'make';
our $PROGTAR = $ENV{DPKG_PROGTAR} // 'tar';
our $PROGPATCH = $ENV{DPKG_PROGPATCH} // 'patch';

our $CONFDIR = '/etc/dpkg';
our $ADMINDIR = '/var/lib/dpkg';
our $LIBDIR = '.';
our $DATADIR = '../data';

$DATADIR = $ENV{DPKG_DATADIR} if defined $ENV{DPKG_DATADIR};

=head1 MODULES

The following is the list of public modules within the Dpkg hierarchy. Only
modules with versions 1.00 or higher, and only the interfaces documented in
their POD are considered public.

=over

=item L<Dpkg>

This module, core variables.

=item L<Dpkg::Arch>

Architecture handling functions.

=item L<Dpkg::Build::Info>

Build information functions.

=item L<Dpkg::BuildFlags>

Set, modify and query compilation build flags.

=item L<Dpkg::BuildOptions>

Parse and manipulate B<DEB_BUILD_OPTIONS>.

=item L<Dpkg::BuildProfiles>

Parse and manipulate build profiles.

=item L<Dpkg::Changelog>

Parse changelogs.

=item L<Dpkg::Changelog::Entry>

Represents a changelog entry.

=item L<Dpkg::Changelog::Parse>

Generic changelog parser for F<dpkg-parsechangelog>.

=item L<Dpkg::Checksums>

Generate and parse checksums.

=item L<Dpkg::Compression>

Simple database of available compression methods.

=item L<Dpkg::Compression::FileHandle>

Transparently compress and decompress files.

=item L<Dpkg::Compression::Process>

Wrapper around compression tools.

=item L<Dpkg::Conf>

Parse F<dpkg> configuration files.

=item L<Dpkg::Control>

Parse and manipulate Debian control information (F<.dsc>, F<.changes>,
F<Packages>/F<Sources> entries, etc.).

=item L<Dpkg::Control::Changelog>

Represent fields output by F<dpkg-parsechangelog>.

=item L<Dpkg::Control::Fields>

Manage (list of known) control fields.

=item L<Dpkg::Control::Hash>

Parse and manipulate a block of RFC822-like fields.

=item L<Dpkg::Control::Info>

Parse files like F<debian/control>.

=item L<Dpkg::Control::Tests>

Parse files like F<debian/tests/control>.

=item L<Dpkg::Control::Tests::Entry>

Represents a F<debian/tests/control> stanza.

=item L<Dpkg::Deps>

Parse and manipulate dependencies.

=item L<Dpkg::Deps::Simple>

Represents a single dependency statement.

=item L<Dpkg::Deps::Multiple>

Base module to represent multiple dependencies.

=item L<Dpkg::Deps::Union>

List of unrelated dependencies.

=item L<Dpkg::Deps::AND>

List of AND dependencies.

=item L<Dpkg::Deps::OR>

List of OR dependencies.

=item L<Dpkg::Deps::KnownFacts>

List of installed and virtual packages.

=item L<Dpkg::Exit>

Push, pop and run exit handlers.

=item L<Dpkg::Gettext>

Wrapper around L<Locale::gettext>.

=item L<Dpkg::IPC>

Spawn sub-processes and feed/retrieve data.

=item L<Dpkg::Index>

Collections of L<Dpkg::Control> (F<Packages>/F<Sources> files for example).

=item L<Dpkg::Interface::Storable>

Base object serializer.

=item L<Dpkg::Path>

Common path handling functions.

=item L<Dpkg::Source::Format>

Parse and manipulate debian/source/format files.

=item L<Dpkg::Source::Package>

Extract Debian source packages.

=item L<Dpkg::Substvars>

Substitute variables in strings.

=item L<Dpkg::Vendor>

Identify current distribution vendor.

=item L<Dpkg::Version>

Parse and manipulate Debian package versions.

=back

=head1 CHANGES

=head2 Version 2.00 (dpkg 1.20.0)

Remove variables: $version, $progname, $admindir, $dpkglibdir and $pkgdatadir.

=head2 Version 1.03 (dpkg 1.18.24)

New variable: $PROGPATCH.

=head2 Version 1.02 (dpkg 1.18.11)

New variable: $PROGTAR, $PROGMAKE.

=head2 Version 1.01 (dpkg 1.17.0)

New variables: $PROGNAME, $PROGVERSION, $CONFDIR, $ADMINDIR, $LIBDIR and
$DATADIR.

Deprecated variables: $version, $admindir, $dpkglibdir and $pkgdatadir.

=head2 Version 1.00 (dpkg 1.15.6)

Mark the module as public.

=cut

1;
