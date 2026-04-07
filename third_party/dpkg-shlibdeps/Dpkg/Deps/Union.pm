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

package Dpkg::Deps::Union;

=encoding utf8

=head1 NAME

Dpkg::Deps::Union - list of unrelated dependencies

=head1 DESCRIPTION

This class represents a list of relationships.
It inherits from Dpkg::Deps::Multiple.

=cut

use strict;
use warnings;

our $VERSION = '1.00';

use parent qw(Dpkg::Deps::Multiple);

=head1 METHODS

=over 4

=item $dep->output([$fh])

The output method uses ", " to join the list of relationships.

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

=item $dep->get_evaluation($other_dep)

These methods are not meaningful for this object and always return undef.

=cut

sub implies {
    # Implication test is not useful on Union.
    return;
}

sub get_evaluation {
    # Evaluation is not useful on Union.
    return;
}

=item $dep->simplify_deps($facts)

The simplification is done to generate an union of all the relationships.
It uses $simple_dep->merge_union($other_dep) to get its job done.

=cut

sub simplify_deps {
    my ($self, $facts) = @_;
    my @new;

WHILELOOP:
    while (@{$self->{list}}) {
        my $odep = shift @{$self->{list}};
        foreach my $dep (@new) {
            next WHILELOOP if $dep->merge_union($odep);
        }
        push @new, $odep;
    }
    $self->{list} = [ @new ];
}

=back

=head1 CHANGES

=head2 Version 1.00 (dpkg 1.15.6)

Mark the module as public.

=cut

1;
