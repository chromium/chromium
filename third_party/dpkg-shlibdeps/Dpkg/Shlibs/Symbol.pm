# Copyright © 2007 Raphaël Hertzog <hertzog@debian.org>
# Copyright © 2009-2010 Modestas Vainius <modax@debian.org>
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

package Dpkg::Shlibs::Symbol;

use strict;
use warnings;

our $VERSION = '0.01';

use Storable ();
use List::Util qw(any);

use Dpkg::Gettext;
use Dpkg::ErrorHandling;
use Dpkg::Arch qw(debarch_is_concerned debarch_to_abiattrs);
use Dpkg::Version;
use Dpkg::Shlibs::Cppfilt;

# Supported alias types in the order of matching preference
use constant ALIAS_TYPES => qw(c++ symver);

# Needed by the deprecated key, which is a correct use.
no if $Dpkg::Version::VERSION ge '1.02',
    warnings => qw(Dpkg::Version::semantic_change::overload::bool);

sub new {
    my ($this, %args) = @_;
    my $class = ref($this) || $this;
    my $self = bless {
	symbol => undef,
	symbol_templ => undef,
	minver => undef,
	dep_id => 0,
	deprecated => 0,
	tags => {},
	tagorder => [],
    }, $class;
    $self->{$_} = $args{$_} foreach keys %args;
    return $self;
}

# Deep clone
sub clone {
    my ($self, %args) = @_;
    my $clone = Storable::dclone($self);
    $clone->{$_} = $args{$_} foreach keys %args;
    return $clone;
}

sub parse_tagspec {
    my ($self, $tagspec) = @_;

    if ($tagspec =~ /^\s*\((.*?)\)(.*)$/ && $1) {
	# (tag1=t1 value|tag2|...|tagN=tNp)
	# Symbols ()|= cannot appear in the tag names and values
	my $tagspec = $1;
	my $rest = ($2) ? $2 : '';
	my @tags = split(/\|/, $tagspec);

	# Parse each tag
	for my $tag (@tags) {
	    if ($tag =~ /^(.*)=(.*)$/) {
		# Tag with value
		$self->add_tag($1, $2);
	    } else {
		# Tag without value
		$self->add_tag($tag, undef);
	    }
	}
	return $rest;
    }
    return;
}

sub parse_symbolspec {
    my ($self, $symbolspec, %opts) = @_;
    my $symbol;
    my $symbol_templ;
    my $symbol_quoted;
    my $rest;

    if (defined($symbol = $self->parse_tagspec($symbolspec))) {
	# (tag1=t1 value|tag2|...|tagN=tNp)"Foo::Bar::foobar()"@Base 1.0 1
	# Symbols ()|= cannot appear in the tag names and values

	# If the tag specification exists symbol name template might be quoted too
	if ($symbol =~ /^(['"])/ && $symbol =~ /^($1)(.*?)$1(.*)$/) {
	    $symbol_quoted = $1;
	    $symbol_templ = $2;
	    $symbol = $2;
	    $rest = $3;
	} else {
	    if ($symbol =~ m/^(\S+)(.*)$/) {
		$symbol_templ = $1;
		$symbol = $1;
		$rest = $2;
	    }
	}
	error(g_('symbol name unspecified: %s'), $symbolspec) if (!$symbol);
    } else {
	# No tag specification. Symbol name is up to the first space
	# foobarsymbol@Base 1.0 1
	if ($symbolspec =~ m/^(\S+)(.*)$/) {
	    $symbol = $1;
	    $rest = $2;
	} else {
	    return 0;
	}
    }
    $self->{symbol} = $symbol;
    $self->{symbol_templ} = $symbol_templ;
    $self->{symbol_quoted} = $symbol_quoted if ($symbol_quoted);

    # Now parse "the rest" (minver and dep_id)
    if ($rest =~ /^\s(\S+)(?:\s(\d+))?/) {
	$self->{minver} = $1;
	$self->{dep_id} = $2 // 0;
    } elsif (defined $opts{default_minver}) {
	$self->{minver} = $opts{default_minver};
	$self->{dep_id} = 0;
    } else {
	return 0;
    }
    return 1;
}

# A hook for symbol initialization (typically processing of tags). The code
# here may even change symbol name. Called from
# Dpkg::Shlibs::SymbolFile::create_symbol().
sub initialize {
    my $self = shift;

    # Look for tags marking symbol patterns. The pattern may match multiple
    # real symbols.
    my $type;
    if ($self->has_tag('c++')) {
	# Raw symbol name is always demangled to the same alias while demangled
	# symbol name cannot be reliably converted back to raw symbol name.
	# Therefore, we can use hash for mapping.
	$type = 'alias-c++';
    }

    # Support old style wildcard syntax. That's basically a symver
    # with an optional tag.
    if ($self->get_symbolname() =~ /^\*@(.*)$/) {
	$self->add_tag('symver') unless $self->has_tag('symver');
	$self->add_tag('optional') unless $self->has_tag('optional');
	$self->{symbol} = $1;
    }

    if ($self->has_tag('symver')) {
	# Each symbol is matched against its version rather than full
	# name@version string.
	$type = (defined $type) ? 'generic' : 'alias-symver';
	if ($self->get_symbolname() eq 'Base') {
	    error(g_("you can't use symver tag to catch unversioned symbols: %s"),
	          $self->get_symbolspec(1));
	}
    }

    # As soon as regex is involved, we need to match each real
    # symbol against each pattern (aka 'generic' pattern).
    if ($self->has_tag('regex')) {
	$type = 'generic';
	# Pre-compile regular expression for better performance.
	my $regex = $self->get_symbolname();
	$self->{pattern}{regex} = qr/$regex/;
    }
    if (defined $type) {
	$self->init_pattern($type);
    }
}

sub get_symbolname {
    my $self = shift;

    return $self->{symbol};
}

sub get_symboltempl {
    my $self = shift;

    return $self->{symbol_templ} || $self->{symbol};
}

sub set_symbolname {
    my ($self, $name, $templ, $quoted) = @_;

    $name //= $self->{symbol};
    if (!defined $templ && $name =~ /\s/) {
	$templ = $name;
    }
    if (!defined $quoted && defined $templ && $templ =~ /\s/) {
	$quoted = '"';
    }
    $self->{symbol} = $name;
    $self->{symbol_templ} = $templ;
    if ($quoted) {
	$self->{symbol_quoted} = $quoted;
    } else {
	delete $self->{symbol_quoted};
    }
}

sub has_tags {
    my $self = shift;
    return scalar (@{$self->{tagorder}});
}

sub add_tag {
    my ($self, $tagname, $tagval) = @_;
    if (exists $self->{tags}{$tagname}) {
	$self->{tags}{$tagname} = $tagval;
	return 0;
    } else {
	$self->{tags}{$tagname} = $tagval;
	push @{$self->{tagorder}}, $tagname;
    }
    return 1;
}

sub delete_tag {
    my ($self, $tagname) = @_;
    if (exists $self->{tags}{$tagname}) {
	delete $self->{tags}{$tagname};
        $self->{tagorder} = [ grep { $_ ne $tagname } @{$self->{tagorder}} ];
	return 1;
    }
    return 0;
}

sub has_tag {
    my ($self, $tag) = @_;
    return exists $self->{tags}{$tag};
}

sub get_tag_value {
    my ($self, $tag) = @_;
    return $self->{tags}{$tag};
}

# Checks if the symbol is equal to another one (by name and optionally,
# tag sets, versioning info (minver and depid))
sub equals {
    my ($self, $other, %opts) = @_;
    $opts{versioning} //= 1;
    $opts{tags} //= 1;

    return 0 if $self->{symbol} ne $other->{symbol};

    if ($opts{versioning}) {
	return 0 if $self->{minver} ne $other->{minver};
	return 0 if $self->{dep_id} ne $other->{dep_id};
    }

    if ($opts{tags}) {
	return 0 if scalar(@{$self->{tagorder}}) != scalar(@{$other->{tagorder}});

	for my $i (0 .. scalar(@{$self->{tagorder}}) - 1) {
	    my $tag = $self->{tagorder}->[$i];
	    return 0 if $tag ne $other->{tagorder}->[$i];
	    if (defined $self->{tags}{$tag} && defined $other->{tags}{$tag}) {
		return 0 if $self->{tags}{$tag} ne $other->{tags}{$tag};
	    } elsif (defined $self->{tags}{$tag} || defined $other->{tags}{$tag}) {
		return 0;
	    }
	}
    }

    return 1;
}


sub is_optional {
    my $self = shift;
    return $self->has_tag('optional');
}

sub is_arch_specific {
    my $self = shift;
    return $self->has_tag('arch');
}

sub arch_is_concerned {
    my ($self, $arch) = @_;
    my $arches = $self->{tags}{arch};

    return 0 if defined $arch && defined $arches &&
                !debarch_is_concerned($arch, split /[\s,]+/, $arches);

    my ($bits, $endian) = debarch_to_abiattrs($arch);
    return 0 if defined $bits && defined $self->{tags}{'arch-bits'} &&
                $bits ne $self->{tags}{'arch-bits'};
    return 0 if defined $endian && defined $self->{tags}{'arch-endian'} &&
                $endian ne $self->{tags}{'arch-endian'};

    return 1;
}

# Get reference to the pattern the symbol matches (if any)
sub get_pattern {
    my $self = shift;

    return $self->{matching_pattern};
}

### NOTE: subroutines below require (or initialize) $self to be a pattern ###

# Initializes this symbol as a pattern of the specified type.
sub init_pattern {
    my ($self, $type) = @_;

    $self->{pattern}{type} = $type;
    # To be filled with references to symbols matching this pattern.
    $self->{pattern}{matches} = [];
}

# Is this symbol a pattern or not?
sub is_pattern {
    my $self = shift;

    return exists $self->{pattern};
}

# Get pattern type if this symbol is a pattern.
sub get_pattern_type {
    my $self = shift;

    return $self->{pattern}{type} // '';
}

# Get (sub)type of the alias pattern. Returns empty string if current
# pattern is not alias.
sub get_alias_type {
    my $self = shift;

    return ($self->get_pattern_type() =~ /^alias-(.+)/ && $1) || '';
}

# Get a list of symbols matching this pattern if this symbol is a pattern
sub get_pattern_matches {
    my $self = shift;

    return @{$self->{pattern}{matches}};
}

# Create a new symbol based on the pattern (i.e. $self)
# and add it to the pattern matches list.
sub create_pattern_match {
    my $self = shift;
    return unless $self->is_pattern();

    # Leave out 'pattern' subfield while deep-cloning
    my $pattern_stuff = $self->{pattern};
    delete $self->{pattern};
    my $newsym = $self->clone(@_);
    $self->{pattern} = $pattern_stuff;

    # Clean up symbol name related internal fields
    $newsym->set_symbolname();

    # Set newsym pattern reference, add to pattern matches list
    $newsym->{matching_pattern} = $self;
    push @{$self->{pattern}{matches}}, $newsym;
    return $newsym;
}

### END of pattern subroutines ###

# Given a raw symbol name the call returns its alias according to the rules of
# the current pattern ($self). Returns undef if the supplied raw name is not
# transformable to alias.
sub convert_to_alias {
    my ($self, $rawname, $type) = @_;
    $type = $self->get_alias_type() unless $type;

    if ($type) {
	if ($type eq 'symver') {
	    # In case of symver, alias is symbol version. Extract it from the
	    # rawname.
	    return "$1" if ($rawname =~ /\@([^@]+)$/);
	} elsif ($rawname =~ /^_Z/ && $type eq 'c++') {
	    return cppfilt_demangle_cpp($rawname);
	}
    }
    return;
}

sub get_tagspec {
    my $self = shift;
    if ($self->has_tags()) {
	my @tags;
	for my $tagname (@{$self->{tagorder}}) {
	    my $tagval = $self->{tags}{$tagname};
	    if (defined $tagval) {
		push @tags, $tagname . '='  . $tagval;
	    } else {
		push @tags, $tagname;
	    }
	}
	return '(' . join('|', @tags) . ')';
    }
    return '';
}

sub get_symbolspec {
    my $self = shift;
    my $template_mode = shift;
    my $spec = '';
    $spec .= "#MISSING: $self->{deprecated}#" if $self->{deprecated};
    $spec .= ' ';
    if ($template_mode) {
	if ($self->has_tags()) {
	    $spec .= sprintf('%s%3$s%s%3$s', $self->get_tagspec(),
		$self->get_symboltempl(), $self->{symbol_quoted} // '');
	} else {
	    $spec .= $self->get_symboltempl();
	}
    } else {
	$spec .= $self->get_symbolname();
    }
    $spec .= " $self->{minver}";
    $spec .= " $self->{dep_id}" if $self->{dep_id};
    return $spec;
}

# Sanitize the symbol when it is confirmed to be found in
# the respective library.
sub mark_found_in_library {
    my ($self, $minver, $arch) = @_;

    if ($self->{deprecated}) {
	# Symbol reappeared somehow
	$self->{deprecated} = 0;
	$self->{minver} = $minver if (not $self->is_optional());
    } else {
	# We assume that the right dependency information is already
	# there.
	if (version_compare($minver, $self->{minver}) < 0) {
	    $self->{minver} = $minver;
	}
    }
    # Never remove arch tags from patterns
    if (not $self->is_pattern()) {
	if (not $self->arch_is_concerned($arch)) {
	    # Remove arch tags because they are incorrect.
	    $self->delete_tag('arch');
	    $self->delete_tag('arch-bits');
	    $self->delete_tag('arch-endian');
	}
    }
}

# Sanitize the symbol when it is confirmed to be NOT found in
# the respective library.
# Mark as deprecated those that are no more provided (only if the
# minver is later than the version where the symbol was introduced)
sub mark_not_found_in_library {
    my ($self, $minver, $arch) = @_;

    # Ignore symbols from foreign arch
    return if not $self->arch_is_concerned($arch);

    if ($self->{deprecated}) {
	# Bump deprecated if the symbol is optional so that it
	# keeps reappearing in the diff while it's missing
	$self->{deprecated} = $minver if $self->is_optional();
    } elsif (version_compare($minver, $self->{minver}) > 0) {
	$self->{deprecated} = $minver;
    }
}

# Checks if the symbol (or pattern) is legitimate as a real symbol for the
# specified architecture.
sub is_legitimate {
    my ($self, $arch) = @_;
    return ! $self->{deprecated} &&
           $self->arch_is_concerned($arch);
}

# Determine whether a supplied raw symbol name matches against current ($self)
# symbol or pattern.
sub matches_rawname {
    my ($self, $rawname) = @_;
    my $target = $rawname;
    my $ok = 1;
    my $do_eq_match = 1;

    if ($self->is_pattern()) {
	# Process pattern tags in the order they were specified.
	for my $tag (@{$self->{tagorder}}) {
	    if (any { $tag eq $_ } ALIAS_TYPES) {
		$ok = not not ($target = $self->convert_to_alias($target, $tag));
	    } elsif ($tag eq 'regex') {
		# Symbol name is a regex. Match it against the target
		$do_eq_match = 0;
		$ok = ($target =~ $self->{pattern}{regex});
	    }
	    last if not $ok;
	}
    }

    # Equality match by default
    if ($ok && $do_eq_match) {
	$ok = $target eq $self->get_symbolname();
    }
    return $ok;
}

1;
