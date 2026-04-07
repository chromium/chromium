# Copyright © 2012 Guillem Jover <guillem@debian.org>
#
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

package Dpkg::Build::Env;

use strict;
use warnings;

our $VERSION = '0.01';

my %env_modified = ();
my %env_accessed = ();

=encoding utf8

=head1 NAME

Dpkg::Build::Env - track build environment

=head1 DESCRIPTION

The Dpkg::Build::Env module is used by dpkg-buildflags to track the build
environment variables being used and modified.

=head1 FUNCTIONS

=over 4

=item set($varname, $value)

Update the build environment variable $varname with value $value. Record
it as being accessed and modified.

=cut

sub set {
    my ($varname, $value) = @_;
    $env_modified{$varname} = 1;
    $env_accessed{$varname} = 1;
    $ENV{$varname} = $value;
}

=item get($varname)

Get the build environment variable $varname value. Record it as being
accessed.

=cut

sub get {
    my $varname = shift;
    $env_accessed{$varname} = 1;
    return $ENV{$varname};
}

=item has($varname)

Return a boolean indicating whether the environment variable exists.
Record it as being accessed.

=cut

sub has {
    my $varname = shift;
    $env_accessed{$varname} = 1;
    return exists $ENV{$varname};
}

=item @list = list_accessed()

Returns a list of all environment variables that have been accessed.

=cut

sub list_accessed {
    my @list = sort keys %env_accessed;
    return @list;
}

=item @list = list_modified()

Returns a list of all environment variables that have been modified.

=cut

sub list_modified {
    my @list = sort keys %env_modified;
    return @list;
}

=back

=head1 CHANGES

=head2 Version 0.xx

This is a private module.

=cut

1;
