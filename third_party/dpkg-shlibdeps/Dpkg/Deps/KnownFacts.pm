# Copyright © 1998 Richard Braakman
# Copyright © 1999 Darren Benham
# Copyright © 2000 Sean 'Shaleh' Perry
# Copyright © 2004 Frank Lichtenheld
# Copyright © 2006 Russ Allbery
# Copyright © 2007-2009 Raphaël Hertzog <hertzog@debian.org>
# Copyright © 2008-2009, 2012-2014 Guillem Jover <guillem@debian.org>
#
# This program is free software; you may redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

package Dpkg::Deps::KnownFacts;

=encoding utf8

=head1 NAME

Dpkg::Deps::KnownFacts - list of installed real and virtual packages

=head1 DESCRIPTION

This class represents a list of installed packages and a list of virtual
packages provided (by the set of installed packages).

=cut

use strict;
use warnings;

our $VERSION = '2.00';

use Dpkg::Version;

=head1 METHODS

=over 4

=item $facts = Dpkg::Deps::KnownFacts->new();

Creates a new object.

=cut

sub new {
    my $this = shift;
    my $class = ref($this) || $this;
    my $self = {
        pkg => {},
        virtualpkg => {},
    };

    bless $self, $class;
    return $self;
}

=item $facts->add_installed_package($package, $version, $arch, $multiarch)

Records that the given version of the package is installed. If
$version/$arch is undefined we know that the package is installed but we
don't know which version/architecture it is. $multiarch is the Multi-Arch
field of the package. If $multiarch is undef, it will be equivalent to
"Multi-Arch: no".

Note that $multiarch is only used if $arch is provided.

=cut

sub add_installed_package {
    my ($self, $pkg, $ver, $arch, $multiarch) = @_;
    my $p = {
        package => $pkg,
        version => $ver,
        architecture => $arch,
        multiarch => $multiarch // 'no',
    };

    $self->{pkg}{"$pkg:$arch"} = $p if defined $arch;
    push @{$self->{pkg}{$pkg}}, $p;
}

=item $facts->add_provided_package($virtual, $relation, $version, $by)

Records that the "$by" package provides the $virtual package. $relation
and $version correspond to the associated relation given in the Provides
field (if present).

=cut

sub add_provided_package {
    my ($self, $pkg, $rel, $ver, $by) = @_;
    my $v = {
        package => $pkg,
        relation => $rel,
        version => $ver,
        provider => $by,
    };

    $self->{virtualpkg}{$pkg} //= [];
    push @{$self->{virtualpkg}{$pkg}}, $v;
}

##
## The functions below are private to Dpkg::Deps::KnownFacts.
##

sub _find_package {
    my ($self, $dep, $lackinfos) = @_;
    my ($pkg, $archqual) = ($dep->{package}, $dep->{archqual});

    return if not exists $self->{pkg}{$pkg};

    my $host_arch = $dep->{host_arch} // Dpkg::Arch::get_host_arch();
    my $build_arch = $dep->{build_arch} // Dpkg::Arch::get_build_arch();

    foreach my $p (@{$self->{pkg}{$pkg}}) {
        my $a = $p->{architecture};
        my $ma = $p->{multiarch};

        if (not defined $a) {
            $$lackinfos = 1;
            next;
        }
        if (not defined $archqual) {
            return $p if $ma eq 'foreign';
            return $p if $a eq $host_arch or $a eq 'all';
        } elsif ($archqual eq 'any') {
            return $p if $ma eq 'allowed';
        } elsif ($archqual eq 'native') {
            return if $ma eq 'foreign';
            return $p if $a eq $build_arch or $a eq 'all';
        } else {
            return $p if $a eq $archqual;
        }
    }
    return;
}

sub _find_virtual_packages {
    my ($self, $pkg) = @_;

    return () if not exists $self->{virtualpkg}{$pkg};
    return @{$self->{virtualpkg}{$pkg}};
}

=item $facts->evaluate_simple_dep()

This method is private and should not be used except from within Dpkg::Deps.

=cut

sub evaluate_simple_dep {
    my ($self, $dep) = @_;
    my ($lackinfos, $pkg) = (0, $dep->{package});

    my $p = $self->_find_package($dep, \$lackinfos);
    if ($p) {
        if (defined $dep->{relation}) {
            if (defined $p->{version}) {
                return 1 if version_compare_relation($p->{version},
                                                     $dep->{relation},
                                                     $dep->{version});
            } else {
                $lackinfos = 1;
            }
        } else {
            return 1;
        }
    }
    foreach my $virtpkg ($self->_find_virtual_packages($pkg)) {
        next if defined $virtpkg->{relation} and
                $virtpkg->{relation} ne REL_EQ;

        if (defined $dep->{relation}) {
            next if not defined $virtpkg->{version};
            return 1 if version_compare_relation($virtpkg->{version},
                                                 $dep->{relation},
                                                 $dep->{version});
        } else {
            return 1;
        }
    }
    return if $lackinfos;
    return 0;
}

=back

=head1 CHANGES

=head2 Version 2.00 (dpkg 1.20.0)

Remove method: $facts->check_package().

=head2 Version 1.01 (dpkg 1.16.1)

New option: Dpkg::Deps::KnownFacts->add_installed_package() now accepts 2
supplementary parameters ($arch and $multiarch).

Deprecated method: Dpkg::Deps::KnownFacts->check_package() is obsolete,
it should not have been part of the public API.

=head2 Version 1.00 (dpkg 1.15.6)

Mark the module as public.

=cut

1;
