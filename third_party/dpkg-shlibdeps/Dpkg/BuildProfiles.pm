# Copyright © 2013 Guillem Jover <guillem@debian.org>
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

package Dpkg::BuildProfiles;

use strict;
use warnings;

our $VERSION = '1.00';
our @EXPORT_OK = qw(
    get_build_profiles
    set_build_profiles
    parse_build_profiles
    evaluate_restriction_formula
);

use Exporter qw(import);
use List::Util qw(any);

use Dpkg::Build::Env;

my $cache_profiles;
my @build_profiles;

=encoding utf8

=head1 NAME

Dpkg::BuildProfiles - handle build profiles

=head1 DESCRIPTION

The Dpkg::BuildProfiles module provides functions to handle the build
profiles.

=head1 FUNCTIONS

=over 4

=item @profiles = get_build_profiles()

Get an array with the currently active build profiles, taken from
the environment variable B<DEB_BUILD_PROFILES>.

=cut

sub get_build_profiles {
    return @build_profiles if $cache_profiles;

    if (Dpkg::Build::Env::has('DEB_BUILD_PROFILES')) {
        @build_profiles = split ' ', Dpkg::Build::Env::get('DEB_BUILD_PROFILES');
    }
    $cache_profiles = 1;

    return @build_profiles;
}

=item set_build_profiles(@profiles)

Set C<@profiles> as the current active build profiles, by setting
the environment variable B<DEB_BUILD_PROFILES>.

=cut

sub set_build_profiles {
    my (@profiles) = @_;

    $cache_profiles = 1;
    @build_profiles = @profiles;
    Dpkg::Build::Env::set('DEB_BUILD_PROFILES', join ' ', @profiles);
}

=item @profiles = parse_build_profiles($string)

Parses a build profiles specification, into an array of array references.

=cut

sub parse_build_profiles {
    my $string = shift;

    $string =~ s/^\s*<\s*(.*)\s*>\s*$/$1/;

    return map { [ split ' ' ] } split /\s*>\s+<\s*/, $string;
}

=item evaluate_restriction_formula(\@formula, \@profiles)

Evaluate whether a restriction formula of the form "<foo bar> <baz>", given as
a nested array, is true or false, given the array of enabled build profiles.

=cut

sub evaluate_restriction_formula {
    my ($formula, $profiles) = @_;

    # Restriction formulas are in disjunctive normal form:
    # (foo AND bar) OR (blub AND bla)
    foreach my $restrlist (@{$formula}) {
        my $seen_profile = 1;

        foreach my $restriction (@$restrlist) {
            next if $restriction !~ m/^(!)?(.+)/;

            my $negated = defined $1 && $1 eq '!';
            my $profile = $2;
            my $found = any { $_ eq $profile } @{$profiles};

            # If a negative set profile is encountered, stop processing.
            # If a positive unset profile is encountered, stop processing.
            if ($found == $negated) {
                $seen_profile = 0;
                last;
            }
        }

        # This conjunction evaluated to true so we don't have to evaluate
        # the others.
        return 1 if $seen_profile;
    }
    return 0;
}

=back

=head1 CHANGES

=head2 Version 1.00 (dpkg 1.17.17)

Mark the module as public.

=cut

1;
