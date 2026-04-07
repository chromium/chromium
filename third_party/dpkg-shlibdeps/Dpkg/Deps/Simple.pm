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

package Dpkg::Deps::Simple;

=encoding utf8

=head1 NAME

Dpkg::Deps::Simple - represents a single dependency statement

=head1 DESCRIPTION

This class represents a single dependency statement.
It has several interesting properties:

=over 4

=item package

The package name (can be undef if the dependency has not been initialized
or if the simplification of the dependency lead to its removal).

=item relation

The relational operator: "=", "<<", "<=", ">=" or ">>". It can be
undefined if the dependency had no version restriction. In that case the
following field is also undefined.

=item version

The version.

=item arches

The list of architectures where this dependency is applicable. It is
undefined when there's no restriction, otherwise it is an
array ref. It can contain an exclusion list, in that case each
architecture is prefixed with an exclamation mark.

=item archqual

The arch qualifier of the dependency (can be undef if there is none).
In the dependency "python:any (>= 2.6)", the arch qualifier is "any".

=item restrictions

The restrictions formula for this dependency. It is undefined when there
is no restriction formula. Otherwise it is an array ref.

=back

=head1 METHODS

=over 4

=cut

use strict;
use warnings;

our $VERSION = '1.02';

use Carp;

use Dpkg::Arch qw(debarch_is_concerned debarch_list_parse);
use Dpkg::BuildProfiles qw(parse_build_profiles evaluate_restriction_formula);
use Dpkg::Version;
use Dpkg::ErrorHandling;
use Dpkg::Gettext;

use parent qw(Dpkg::Interface::Storable);

=item $dep = Dpkg::Deps::Simple->new([$dep[, %opts]]);

Creates a new object. Some options can be set through %opts:

=over

=item host_arch

Sets the host architecture.

=item build_arch

Sets the build architecture.

=item build_dep

Specifies whether the parser should consider it a build dependency.
Defaults to 0.

=item tests_dep

Specifies whether the parser should consider it a tests dependency.
Defaults to 0.

=back

=cut

sub new {
    my ($this, $arg, %opts) = @_;
    my $class = ref($this) || $this;
    my $self = {};

    bless $self, $class;
    $self->reset();
    $self->{host_arch} = $opts{host_arch};
    $self->{build_arch} = $opts{build_arch};
    $self->{build_dep} = $opts{build_dep} // 0;
    $self->{tests_dep} = $opts{tests_dep} // 0;
    $self->parse_string($arg) if defined $arg;
    return $self;
}

=item $dep->reset()

Clears any dependency information stored in $dep so that $dep->is_empty()
returns true.

=cut

sub reset {
    my $self = shift;

    $self->{package} = undef;
    $self->{relation} = undef;
    $self->{version} = undef;
    $self->{arches} = undef;
    $self->{archqual} = undef;
    $self->{restrictions} = undef;
}

=item $dep->parse_string($dep_string)

Parses the dependency string and modifies internal properties to match the
parsed dependency.

=cut

sub parse_string {
    my ($self, $dep) = @_;

    my $pkgname_re;
    if ($self->{tests_dep}) {
        $pkgname_re = qr/[\@a-zA-Z0-9][\@a-zA-Z0-9+.-]*/;
    } else {
        $pkgname_re = qr/[a-zA-Z0-9][a-zA-Z0-9+.-]*/;
    }

    return if not $dep =~
           m{^\s*                           # skip leading whitespace
              ($pkgname_re)                 # package name
              (?:                           # start of optional part
                :                           # colon for architecture
                ([a-zA-Z0-9][a-zA-Z0-9-]*)  # architecture name
              )?                            # end of optional part
              (?:                           # start of optional part
                \s* \(                      # open parenthesis for version part
                \s* (<<|<=|=|>=|>>|[<>])    # relation part
                \s* ([^\)\s]+)              # do not attempt to parse version
                \s* \)                      # closing parenthesis
              )?                            # end of optional part
              (?:                           # start of optional architecture
                \s* \[                      # open bracket for architecture
                \s* ([^\]]+)                # don't parse architectures now
                \s* \]                      # closing bracket
              )?                            # end of optional architecture
              (
                (?:                         # start of optional restriction
                \s* <                       # open bracket for restriction
                \s* [^>]+                   # do not parse restrictions now
                \s* >                       # closing bracket
                )+
              )?                            # end of optional restriction
              \s*$                          # trailing spaces at end
            }x;
    if (defined $2) {
        return if $2 eq 'native' and not $self->{build_dep};
        $self->{archqual} = $2;
    }
    $self->{package} = $1;
    $self->{relation} = version_normalize_relation($3) if defined $3;
    if (defined $4) {
        $self->{version} = Dpkg::Version->new($4);
    }
    if (defined $5) {
        $self->{arches} = [ debarch_list_parse($5) ];
    }
    if (defined $6) {
        $self->{restrictions} = [ parse_build_profiles($6) ];
    }
}

=item $dep->parse($fh, $desc)

Parse a dependency line from a filehandle.

=cut

sub parse {
    my ($self, $fh, $desc) = @_;

    my $line = <$fh>;
    chomp $line;
    return $self->parse_string($line);
}

=item $dep->load($filename)

Parse a dependency line from $filename.

=item $dep->output([$fh])

=item "$dep"

Returns a string representing the dependency. If $fh is set, it prints
the string to the filehandle.

=cut

sub output {
    my ($self, $fh) = @_;

    my $res = $self->{package};
    if (defined $self->{archqual}) {
        $res .= ':' . $self->{archqual};
    }
    if (defined $self->{relation}) {
        $res .= ' (' . $self->{relation} . ' ' . $self->{version} .  ')';
    }
    if (defined $self->{arches}) {
        $res .= ' [' . join(' ', @{$self->{arches}}) . ']';
    }
    if (defined $self->{restrictions}) {
        for my $restrlist (@{$self->{restrictions}}) {
            $res .= ' <' . join(' ', @{$restrlist}) . '>';
        }
    }
    if (defined $fh) {
        print { $fh } $res;
    }
    return $res;
}

=item $dep->save($filename)

Save the dependency into the given $filename.

=cut

# _arch_is_superset(\@p, \@q)
#
# Returns true if the arch list @p is a superset of arch list @q.
# The arguments can also be undef in case there's no explicit architecture
# restriction.
sub _arch_is_superset {
    my ($p, $q) = @_;
    my $p_arch_neg = defined $p and $p->[0] =~ /^!/;
    my $q_arch_neg = defined $q and $q->[0] =~ /^!/;

    # If "p" has no arches, it is a superset of q and we should fall through
    # to the version check.
    if (not defined $p) {
        return 1;
    }
    # If q has no arches, it is a superset of p and there are no useful
    # implications.
    elsif (not defined $q) {
        return 0;
    }
    # Both have arches.  If neither are negated, we know nothing useful
    # unless q is a subset of p.
    elsif (not $p_arch_neg and not $q_arch_neg) {
        my %p_arches = map { $_ => 1 } @{$p};
        my $subset = 1;
        for my $arch (@{$q}) {
            $subset = 0 unless $p_arches{$arch};
        }
        return 0 unless $subset;
    }
    # If both are negated, we know nothing useful unless p is a subset of
    # q (and therefore has fewer things excluded, and therefore is more
    # general).
    elsif ($p_arch_neg and $q_arch_neg) {
        my %q_arches = map { $_ => 1 } @{$q};
        my $subset = 1;
        for my $arch (@{$p}) {
            $subset = 0 unless $q_arches{$arch};
        }
        return 0 unless $subset;
    }
    # If q is negated and p isn't, we'd need to know the full list of
    # arches to know if there's any relationship, so bail.
    elsif (not $p_arch_neg and $q_arch_neg) {
        return 0;
    }
    # If p is negated and q isn't, q is a subset of p if none of the
    # negated arches in p are present in q.
    elsif ($p_arch_neg and not $q_arch_neg) {
        my %q_arches = map { $_ => 1 } @{$q};
        my $subset = 1;
        for my $arch (@{$p}) {
            $subset = 0 if $q_arches{substr($arch, 1)};
        }
        return 0 unless $subset;
    }
    return 1;
}

# _arch_qualifier_implies($p, $q)
#
# Returns true if the arch qualifier $p and $q are compatible with the
# implication $p -> $q, false otherwise. $p/$q can be undef/"any"/"native"
# or an architecture string.
#
# Because we are handling dependencies in isolation, and the full context
# of the implications are only known when doing dependency resolution at
# run-time, we can only assert that they are implied if they are equal.
#
# For example dependencies with different arch-qualifiers cannot be simplified
# as these depend on the state of Multi-Arch field in the package depended on.
sub _arch_qualifier_implies {
    my ($p, $q) = @_;

    return $p eq $q if defined $p and defined $q;
    return 1 if not defined $p and not defined $q;
    return 0;
}

# _restrictions_imply($p, $q)
#
# Returns true if the restrictions $p and $q are compatible with the
# implication $p -> $q, false otherwise.
# NOTE: We don't try to be very clever here, so we may conservatively
# return false when there is an implication.
sub _restrictions_imply {
    my ($p, $q) = @_;

    if (not defined $p) {
       return 1;
    } elsif (not defined $q) {
       return 0;
    } else {
       # Check whether set difference is empty.
       my %restr;

       for my $restrlist (@{$q}) {
           my $reststr = join ' ', sort @{$restrlist};
           $restr{$reststr} = 1;
       }
       for my $restrlist (@{$p}) {
           my $reststr = join ' ', sort @{$restrlist};
           delete $restr{$reststr};
       }

       return keys %restr == 0;
    }
}

=item $dep->implies($other_dep)

Returns 1 when $dep implies $other_dep. Returns 0 when $dep implies
NOT($other_dep). Returns undef when there is no implication. $dep and
$other_dep do not need to be of the same type.

=cut

sub implies {
    my ($self, $o) = @_;

    if ($o->isa('Dpkg::Deps::Simple')) {
        # An implication is only possible on the same package
        return if $self->{package} ne $o->{package};

        # Our architecture set must be a superset of the architectures for
        # o, otherwise we can't conclude anything.
        return unless _arch_is_superset($self->{arches}, $o->{arches});

        # The arch qualifier must not forbid an implication
        return unless _arch_qualifier_implies($self->{archqual},
                                              $o->{archqual});

        # Our restrictions must imply the restrictions for o
        return unless _restrictions_imply($self->{restrictions},
                                          $o->{restrictions});

        # If o has no version clause, then our dependency is stronger
        return 1 if not defined $o->{relation};
        # If o has a version clause, we must also have one, otherwise there
        # can't be an implication
        return if not defined $self->{relation};

        return Dpkg::Deps::deps_eval_implication($self->{relation},
                $self->{version}, $o->{relation}, $o->{version});
    } elsif ($o->isa('Dpkg::Deps::AND')) {
        # TRUE: Need to imply all individual elements
        # FALSE: Need to NOT imply at least one individual element
        my $res = 1;
        foreach my $dep ($o->get_deps()) {
            my $implication = $self->implies($dep);
            unless (defined $implication and $implication == 1) {
                $res = $implication;
                last if defined $res;
            }
        }
        return $res;
    } elsif ($o->isa('Dpkg::Deps::OR')) {
        # TRUE: Need to imply at least one individual element
        # FALSE: Need to not apply all individual elements
        # UNDEF: The rest
        my $res = undef;
        foreach my $dep ($o->get_deps()) {
            my $implication = $self->implies($dep);
            if (defined $implication) {
                if (not defined $res) {
                    $res = $implication;
                } else {
                    if ($implication) {
                        $res = 1;
                    } else {
                        $res = 0;
                    }
                }
                last if defined $res and $res == 1;
            }
        }
        return $res;
    } else {
        croak 'Dpkg::Deps::Simple cannot evaluate implication with a ' .
              ref($o);
    }
}

=item $dep->get_deps()

Returns a list of sub-dependencies, which for this object it means it
returns itself.

=cut

sub get_deps {
    my $self = shift;

    return $self;
}

=item $dep->sort()

This method is a no-op for this object.

=cut

sub sort {
    # Nothing to sort
}

=item $dep->arch_is_concerned($arch)

Returns true if the dependency applies to the indicated architecture.

=cut

sub arch_is_concerned {
    my ($self, $host_arch) = @_;

    return 0 if not defined $self->{package}; # Empty dep
    return 1 if not defined $self->{arches};  # Dep without arch spec

    return debarch_is_concerned($host_arch, @{$self->{arches}});
}

=item $dep->reduce_arch($arch)

Simplifies the dependency to contain only information relevant to the given
architecture. This object can be left empty after this operation. This trims
off the architecture restriction list of these objects.

=cut

sub reduce_arch {
    my ($self, $host_arch) = @_;

    if (not $self->arch_is_concerned($host_arch)) {
        $self->reset();
    } else {
        $self->{arches} = undef;
    }
}

=item $dep->has_arch_restriction()

Returns the package name if the dependency applies only to a subset of
architectures.

=cut

sub has_arch_restriction {
    my $self = shift;

    if (defined $self->{arches}) {
        return $self->{package};
    } else {
        return ();
    }
}

=item $dep->profile_is_concerned()

Returns true if the dependency applies to the indicated profile.

=cut

sub profile_is_concerned {
    my ($self, $build_profiles) = @_;

    return 0 if not defined $self->{package}; # Empty dep
    return 1 if not defined $self->{restrictions}; # Dep without restrictions
    return evaluate_restriction_formula($self->{restrictions}, $build_profiles);
}

=item $dep->reduce_profiles()

Simplifies the dependency to contain only information relevant to the given
profile. This object can be left empty after this operation. This trims off
the profile restriction list of this object.

=cut

sub reduce_profiles {
    my ($self, $build_profiles) = @_;

    if (not $self->profile_is_concerned($build_profiles)) {
        $self->reset();
    } else {
        $self->{restrictions} = undef;
    }
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

    return if not defined $self->{package};
    return $facts->evaluate_simple_dep($self);
}

=item $dep->simplify_deps($facts, @assumed_deps)

Simplifies the dependency as much as possible given the list of facts (see
class Dpkg::Deps::KnownFacts) and a list of other dependencies that are
known to be true.

=cut

sub simplify_deps {
    my ($self, $facts) = @_;

    my $eval = $self->get_evaluation($facts);
    $self->reset() if defined $eval and $eval == 1;
}

=item $dep->is_empty()

Returns true if the dependency is empty and doesn't contain any useful
information. This is true when the object has not yet been initialized.

=cut

sub is_empty {
    my $self = shift;

    return not defined $self->{package};
}

=item $dep->merge_union($other_dep)

Returns true if $dep could be modified to represent the union of both
dependencies. Otherwise returns false.

=cut

sub merge_union {
    my ($self, $o) = @_;

    return 0 if not $o->isa('Dpkg::Deps::Simple');
    return 0 if $self->is_empty() or $o->is_empty();
    return 0 if $self->{package} ne $o->{package};
    return 0 if defined $self->{arches} or defined $o->{arches};

    if (not defined $o->{relation} and defined $self->{relation}) {
        # Union is the non-versioned dependency
        $self->{relation} = undef;
        $self->{version} = undef;
        return 1;
    }

    my $implication = $self->implies($o);
    my $rev_implication = $o->implies($self);
    if (defined $implication) {
        if ($implication) {
            $self->{relation} = $o->{relation};
            $self->{version} = $o->{version};
            return 1;
        } else {
            return 0;
        }
    }
    if (defined $rev_implication) {
        if ($rev_implication) {
            # Already merged...
            return 1;
        } else {
            return 0;
        }
    }
    return 0;
}

=back

=head1 CHANGES

=head2 Version 1.02 (dpkg 1.17.10)

New methods: Add $dep->profile_is_concerned() and $dep->reduce_profiles().

=head2 Version 1.01 (dpkg 1.16.1)

New method: Add $dep->reset().

New property: recognizes the arch qualifier "any" and stores it in the
"archqual" property when present.

=head2 Version 1.00 (dpkg 1.15.6)

Mark the module as public.

=cut

1;
