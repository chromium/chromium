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

package Dpkg::Deps::AND;

=encoding utf8

=head1 NAME

Dpkg::Deps::AND - list of AND dependencies

=head1 DESCRIPTION

This class represents a list of dependencies that must be met at the same
time. It inherits from Dpkg::Deps::Multiple.

=cut

use strict;
use warnings;

our $VERSION = '1.00';

use parent qw(Dpkg::Deps::Multiple);

=head1 METHODS

=over 4

=item $dep->output([$fh])

The output method uses ", " to join the list of sub-dependencies.

=cut

sub output {
    my ($self, $fh) = @_;

    my $res = join(', ', map {
        $_->output()
    } grep {
        not $_->is_empty()
    } $self->get_deps());

    if (defined $fh) {
        print { $fh } $res;
    }
    return $res;
}

=item $dep->implies($other_dep)

Returns 1 when $dep implies $other_dep. Returns 0 when $dep implies
NOT($other_dep). Returns undef when there's no implication. $dep and
$other_dep do not need to be of the same type.

=cut

sub implies {
    my ($self, $o) = @_;

    # If any individual member can imply $o or NOT $o, we're fine
    foreach my $dep ($self->get_deps()) {
        my $implication = $dep->implies($o);
        return 1 if defined $implication and $implication == 1;
        return 0 if defined $implication and $implication == 0;
    }

    # If o is an AND, we might have an implication, if we find an
    # implication within us for each predicate in o
    if ($o->isa('Dpkg::Deps::AND')) {
        my $subset = 1;
        foreach my $odep ($o->get_deps()) {
            my $found = 0;
            foreach my $dep ($self->get_deps()) {
                $found = 1 if $dep->implies($odep);
            }
            $subset = 0 if not $found;
        }
        return 1 if $subset;
    }
    return;
}

=item $dep->get_evaluation($facts)

Evaluates the dependency given a list of installed packages and a list of
virtual packages provided. These lists are part of the Dpkg::Deps::KnownFacts
object given as parameters.

Returns 1 when it's true, 0 when it's false, undef when some information
is lacking to conclude.

=cut

sub get_evaluation {
    my ($self, $facts) = @_;

    # Return 1 only if all members evaluates to true
    # Return 0 if at least one member evaluates to false
    # Return undef otherwise
    my $result = 1;
    foreach my $dep ($self->get_deps()) {
        my $eval = $dep->get_evaluation($facts);
        if (not defined $eval) {
            $result = undef;
        } elsif ($eval == 0) {
            $result = 0;
            last;
        } elsif ($eval == 1) {
            # Still possible
        }
    }
    return $result;
}

=item $dep->simplify_deps($facts, @assumed_deps)

Simplifies the dependency as much as possible given the list of facts (see
object Dpkg::Deps::KnownFacts) and a list of other dependencies that are
known to be true.

=cut

sub simplify_deps {
    my ($self, $facts, @knowndeps) = @_;
    my @new;

WHILELOOP:
    while (@{$self->{list}}) {
        my $dep = shift @{$self->{list}};
        my $eval = $dep->get_evaluation($facts);
        next if defined $eval and $eval == 1;
        foreach my $odep (@knowndeps, @new) {
            next WHILELOOP if $odep->implies($dep);
        }
        # When a dependency is implied by another dependency that
        # follows, then invert them
        # "a | b, c, a"  becomes "a, c" and not "c, a"
        my $i = 0;
        foreach my $odep (@{$self->{list}}) {
            if (defined $odep and $odep->implies($dep)) {
                splice @{$self->{list}}, $i, 1;
                unshift @{$self->{list}}, $odep;
                next WHILELOOP;
            }
            $i++;
        }
        push @new, $dep;
    }
    $self->{list} = [ @new ];
}

=back

=head1 CHANGES

=head2 Version 1.00 (dpkg 1.15.6)

Mark the module as public.

=cut

1;
