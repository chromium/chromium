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

package Dpkg::Shlibs::SymbolFile;

use strict;
use warnings;

our $VERSION = '0.01';

use Dpkg::Gettext;
use Dpkg::ErrorHandling;
use Dpkg::Version;
use Dpkg::Control::Fields;
use Dpkg::Shlibs::Symbol;
use Dpkg::Arch qw(get_host_arch);

use parent qw(Dpkg::Interface::Storable);

# Needed by the deprecated key, which is a correct use.
no if $Dpkg::Version::VERSION ge '1.02',
    warnings => qw(Dpkg::Version::semantic_change::overload::bool);

my %blacklist = (
    __bss_end__ => 1,                   # arm
    __bss_end => 1,                     # arm
    _bss_end__ => 1,                    # arm
    __bss_start => 1,                   # ALL
    __bss_start__ => 1,                 # arm
    __data_start => 1,                  # arm
    __do_global_ctors_aux => 1,         # ia64
    __do_global_dtors_aux => 1,         # ia64
    __do_jv_register_classes => 1,      # ia64
    _DYNAMIC => 1,                      # ALL
    _edata => 1,                        # ALL
    _end => 1,                          # ALL
    __end__ => 1,                       # arm
    __exidx_end => 1,                   # armel
    __exidx_start => 1,                 # armel
    _fbss => 1,                         # mips, mipsel
    _fdata => 1,                        # mips, mipsel
    _fini => 1,                         # ALL
    _ftext => 1,                        # mips, mipsel
    _GLOBAL_OFFSET_TABLE_ => 1,         # hppa, mips, mipsel
    __gmon_start__ => 1,                # hppa
    __gnu_local_gp => 1,                # mips, mipsel
    _gp => 1,                           # mips, mipsel
    _init => 1,                         # ALL
    _PROCEDURE_LINKAGE_TABLE_ => 1,     # sparc, alpha
    _SDA2_BASE_ => 1,                   # powerpc
    _SDA_BASE_ => 1,                    # powerpc
);

for my $i (14 .. 31) {
    # Many powerpc specific symbols
    $blacklist{"_restfpr_$i"} = 1;
    $blacklist{"_restfpr_$i\_x"} = 1;
    $blacklist{"_restgpr_$i"} = 1;
    $blacklist{"_restgpr_$i\_x"} = 1;
    $blacklist{"_savefpr_$i"} = 1;
    $blacklist{"_savegpr_$i"} = 1;
}

sub symbol_is_blacklisted {
    my ($symbol, $include_groups) = @_;

    return 1 if exists $blacklist{$symbol};

    # The ARM Embedded ABI spec states symbols under this namespace as
    # possibly appearing in output objects.
    return 1 if not ${$include_groups}{aeabi} and $symbol =~ /^__aeabi_/;

    # The GNU implementation of the OpenMP spec, specifies symbols under
    # this namespace as possibly appearing in output objects.
    return 1 if not ${$include_groups}{gomp}
                and $symbol =~ /^\.gomp_critical_user_/;

    return 0;
}

sub new {
    my ($this, %opts) = @_;
    my $class = ref($this) || $this;
    my $self = \%opts;
    bless $self, $class;
    $self->{arch} //= get_host_arch();
    $self->clear();
    if (exists $self->{file}) {
	$self->load($self->{file}) if -e $self->{file};
    }
    return $self;
}

sub get_arch {
    my $self = shift;
    return $self->{arch};
}

sub clear {
    my $self = shift;
    $self->{objects} = {};
}

sub clear_except {
    my ($self, @ids) = @_;

    my %has = map { $_ => 1 } @ids;
    foreach my $objid (keys %{$self->{objects}}) {
	delete $self->{objects}{$objid} unless exists $has{$objid};
    }
}

sub get_sonames {
    my $self = shift;
    return keys %{$self->{objects}};
}

sub get_symbols {
    my ($self, $soname) = @_;
    if (defined $soname) {
	my $obj = $self->get_object($soname);
	return (defined $obj) ? values %{$obj->{syms}} : ();
    } else {
	my @syms;
	foreach my $soname ($self->get_sonames()) {
	    push @syms, $self->get_symbols($soname);
	}
	return @syms;
    }
}

sub get_patterns {
    my ($self, $soname) = @_;
    my @patterns;
    if (defined $soname) {
	my $obj = $self->get_object($soname);
	foreach my $alias (values %{$obj->{patterns}{aliases}}) {
	    push @patterns, values %$alias;
	}
	return (@patterns, @{$obj->{patterns}{generic}});
    } else {
	foreach my $soname ($self->get_sonames()) {
	    push @patterns, $self->get_patterns($soname);
	}
	return @patterns;
    }
}

# Create a symbol from the supplied string specification.
sub create_symbol {
    my ($self, $spec, %opts) = @_;
    my $symbol = (exists $opts{base}) ? $opts{base} :
	Dpkg::Shlibs::Symbol->new();

    my $ret = $opts{dummy} ? $symbol->parse_symbolspec($spec, default_minver => 0) :
	$symbol->parse_symbolspec($spec);
    if ($ret) {
	$symbol->initialize(arch => $self->get_arch());
	return $symbol;
    }
    return;
}

sub add_symbol {
    my ($self, $symbol, $soname) = @_;
    my $object = $self->get_object($soname);

    if ($symbol->is_pattern()) {
	if (my $alias_type = $symbol->get_alias_type()) {
	    $object->{patterns}{aliases}{$alias_type} //= {};
	    # Alias hash for matching.
	    my $aliases = $object->{patterns}{aliases}{$alias_type};
	    $aliases->{$symbol->get_symbolname()} = $symbol;
	} else {
	    # Otherwise assume this is a generic sequential pattern. This
	    # should be always safe.
	    push @{$object->{patterns}{generic}}, $symbol;
	}
	return 'pattern';
    } else {
	# invalidate the minimum version cache
        $object->{minver_cache} = [];
	$object->{syms}{$symbol->get_symbolname()} = $symbol;
	return 'sym';
    }
}

sub _new_symbol {
    my $base = shift || 'Dpkg::Shlibs::Symbol';
    return (ref $base) ? $base->clone(@_) : $base->new(@_);
}

# Option state is only used for recursive calls.
sub parse {
    my ($self, $fh, $file, %opts) = @_;
    my $state = $opts{state} //= {};

    if (exists $state->{seen}) {
	return if exists $state->{seen}{$file}; # Avoid include loops
    } else {
	$self->{file} = $file;
        $state->{seen} = {};
    }
    $state->{seen}{$file} = 1;

    if (not ref $state->{obj_ref}) { # Init ref to name of current object/lib
        ${$state->{obj_ref}} = undef;
    }

    while (<$fh>) {
	chomp;

	if (/^(?:\s+|#(?:DEPRECATED|MISSING): ([^#]+)#\s*)(.*)/) {
	    if (not defined ${$state->{obj_ref}}) {
		error(g_('symbol information must be preceded by a header (file %s, line %s)'), $file, $.);
	    }
	    # Symbol specification
	    my $deprecated = ($1) ? Dpkg::Version->new($1) : 0;
	    my $sym = _new_symbol($state->{base_symbol}, deprecated => $deprecated);
	    if ($self->create_symbol($2, base => $sym)) {
		$self->add_symbol($sym, ${$state->{obj_ref}});
	    } else {
		warning(g_('failed to parse line in %s: %s'), $file, $_);
	    }
	} elsif (/^(\(.*\))?#include\s+"([^"]+)"/) {
	    my $tagspec = $1;
	    my $filename = $2;
	    my $dir = $file;
	    my $old_base_symbol = $state->{base_symbol};
	    my $new_base_symbol;
	    if (defined $tagspec) {
		$new_base_symbol = _new_symbol($old_base_symbol);
		$new_base_symbol->parse_tagspec($tagspec);
	    }
	    $state->{base_symbol} = $new_base_symbol;
	    $dir =~ s{[^/]+$}{}; # Strip filename
	    $self->load("$dir$filename", %opts);
	    $state->{base_symbol} = $old_base_symbol;
	} elsif (/^#|^$/) {
	    # Skip possible comments and empty lines
	} elsif (/^\|\s*(.*)$/) {
	    # Alternative dependency template
	    push @{$self->{objects}{${$state->{obj_ref}}}{deps}}, "$1";
	} elsif (/^\*\s*([^:]+):\s*(.*\S)\s*$/) {
	    # Add meta-fields
	    $self->{objects}{${$state->{obj_ref}}}{fields}{field_capitalize($1)} = $2;
	} elsif (/^(\S+)\s+(.*)$/) {
	    # New object and dependency template
	    ${$state->{obj_ref}} = $1;
	    if (exists $self->{objects}{${$state->{obj_ref}}}) {
		# Update/override infos only
		$self->{objects}{${$state->{obj_ref}}}{deps} = [ "$2" ];
	    } else {
		# Create a new object
		$self->create_object(${$state->{obj_ref}}, "$2");
	    }
	} else {
	    warning(g_('failed to parse a line in %s: %s'), $file, $_);
	}
    }
    delete $state->{seen}{$file};
}

# Beware: we reuse the data structure of the provided symfile so make
# sure to not modify them after having called this function
sub merge_object_from_symfile {
    my ($self, $src, $objid) = @_;
    if (not $self->has_object($objid)) {
        $self->{objects}{$objid} = $src->get_object($objid);
    } else {
        warning(g_('tried to merge the same object (%s) twice in a symfile'), $objid);
    }
}

sub output {
    my ($self, $fh, %opts) = @_;
    $opts{template_mode} //= 0;
    $opts{with_deprecated} //= 1;
    $opts{with_pattern_matches} //= 0;
    my $res = '';
    foreach my $soname (sort $self->get_sonames()) {
	my @deps = $self->get_dependencies($soname);
	my $dep_first = shift @deps;
	if (exists $opts{package} and not $opts{template_mode}) {
	    $dep_first =~ s/#PACKAGE#/$opts{package}/g;
	}
	print { $fh } "$soname $dep_first\n" if defined $fh;
	$res .= "$soname $dep_first\n" if defined wantarray;

	foreach my $dep_next (@deps) {
	    if (exists $opts{package} and not $opts{template_mode}) {
	        $dep_next =~ s/#PACKAGE#/$opts{package}/g;
	    }
	    print { $fh } "| $dep_next\n" if defined $fh;
	    $res .= "| $dep_next\n" if defined wantarray;
	}
	my $f = $self->{objects}{$soname}{fields};
	foreach my $field (sort keys %{$f}) {
	    my $value = $f->{$field};
	    if (exists $opts{package} and not $opts{template_mode}) {
	        $value =~ s/#PACKAGE#/$opts{package}/g;
	    }
	    print { $fh } "* $field: $value\n" if defined $fh;
	    $res .= "* $field: $value\n" if defined wantarray;
	}

	my @symbols;
	if ($opts{template_mode}) {
	    # Exclude symbols matching a pattern, but include patterns themselves
	    @symbols = grep { not $_->get_pattern() } $self->get_symbols($soname);
	    push @symbols, $self->get_patterns($soname);
	} else {
	    @symbols = $self->get_symbols($soname);
	}
	foreach my $sym (sort { $a->get_symboltempl() cmp
	                        $b->get_symboltempl() } @symbols) {
	    next if $sym->{deprecated} and not $opts{with_deprecated};
	    # Do not dump symbols from foreign arch unless dumping a template.
	    next if not $opts{template_mode} and
	            not $sym->arch_is_concerned($self->get_arch());
	    # Dump symbol specification. Dump symbol tags only in template mode.
	    print { $fh } $sym->get_symbolspec($opts{template_mode}), "\n" if defined $fh;
	    $res .= $sym->get_symbolspec($opts{template_mode}) . "\n" if defined wantarray;
	    # Dump pattern matches as comments (if requested)
	    if ($opts{with_pattern_matches} && $sym->is_pattern()) {
		for my $match (sort { $a->get_symboltempl() cmp
		                      $b->get_symboltempl() } $sym->get_pattern_matches())
		{
		    print { $fh } '#MATCH:', $match->get_symbolspec(0), "\n" if defined $fh;
		    $res .= '#MATCH:' . $match->get_symbolspec(0) . "\n" if defined wantarray;
		}
	    }
	}
    }
    return $res;
}

# Tries to match a symbol name and/or version against the patterns defined.
# Returns a pattern which matches (if any).
sub find_matching_pattern {
    my ($self, $refsym, $sonames, $inc_deprecated) = @_;
    $inc_deprecated //= 0;
    my $name = (ref $refsym) ? $refsym->get_symbolname() : $refsym;

    my $pattern_ok = sub {
	my $p = shift;
	return defined $p && ($inc_deprecated || !$p->{deprecated}) &&
	       $p->arch_is_concerned($self->get_arch());
    };

    foreach my $soname ((ref($sonames) eq 'ARRAY') ? @$sonames : $sonames) {
	my $obj = $self->get_object($soname);
	my ($type, $pattern);
	next unless defined $obj;

	my $all_aliases = $obj->{patterns}{aliases};
	for my $type (Dpkg::Shlibs::Symbol::ALIAS_TYPES) {
	    if (exists $all_aliases->{$type} && keys(%{$all_aliases->{$type}})) {
		my $aliases = $all_aliases->{$type};
		my $converter = $aliases->{(keys %$aliases)[0]};
		if (my $alias = $converter->convert_to_alias($name)) {
		    if ($alias && exists $aliases->{$alias}) {
			$pattern = $aliases->{$alias};
			last if $pattern_ok->($pattern);
			$pattern = undef; # otherwise not found yet
		    }
		}
	    }
	}

	# Now try generic patterns and use the first that matches
	if (not defined $pattern) {
	    for my $p (@{$obj->{patterns}{generic}}) {
		if ($pattern_ok->($p) && $p->matches_rawname($name)) {
		    $pattern = $p;
		    last;
		}
	    }
	}
	if (defined $pattern) {
	    return (wantarray) ?
		( symbol => $pattern, soname => $soname ) : $pattern;
	}
    }
    return;
}

# merge_symbols($object, $minver)
# Needs $Objdump->get_object($soname) as parameter
# Don't merge blacklisted symbols related to the internal (arch-specific)
# machinery
sub merge_symbols {
    my ($self, $object, $minver) = @_;

    my $soname = $object->{SONAME};
    error(g_('cannot merge symbols from objects without SONAME'))
        unless $soname;

    my %include_groups = ();
    my $groups = $self->get_field($soname, 'Ignore-Blacklist-Groups');
    if (defined $groups) {
        $include_groups{$_} = 1 foreach (split ' ', $groups);
    }

    my %dynsyms;
    foreach my $sym ($object->get_exported_dynamic_symbols()) {
        my $name = $sym->{name} . '@' .
                   ($sym->{version} ? $sym->{version} : 'Base');
        my $symobj = $self->lookup_symbol($name, $soname);
        if (symbol_is_blacklisted($sym->{name}, \%include_groups)) {
            next unless (defined $symobj and $symobj->has_tag('ignore-blacklist'));
        }
        $dynsyms{$name} = $sym;
    }

    unless ($self->has_object($soname)) {
	$self->create_object($soname, '');
    }
    # Scan all symbols provided by the objects
    my $obj = $self->get_object($soname);
    # invalidate the minimum version cache - it is not sufficient to
    # invalidate in add_symbol, since we might change a minimum
    # version for a particular symbol without adding it
    $obj->{minver_cache} = [];
    foreach my $name (keys %dynsyms) {
        my $sym;
	if ($sym = $self->lookup_symbol($name, $obj, 1)) {
	    # If the symbol is already listed in the file
	    $sym->mark_found_in_library($minver, $self->get_arch());
	} else {
	    # The exact symbol is not present in the file, but it might match a
	    # pattern.
	    my $pattern = $self->find_matching_pattern($name, $obj, 1);
	    if (defined $pattern) {
		$pattern->mark_found_in_library($minver, $self->get_arch());
		$sym = $pattern->create_pattern_match(symbol => $name);
	    } else {
		# Symbol without any special info as no pattern matched
		$sym = Dpkg::Shlibs::Symbol->new(symbol => $name,
		                                 minver => $minver);
	    }
	    $self->add_symbol($sym, $obj);
	}
    }

    # Process all symbols which could not be found in the library.
    foreach my $sym ($self->get_symbols($soname)) {
	if (not exists $dynsyms{$sym->get_symbolname()}) {
	    $sym->mark_not_found_in_library($minver, $self->get_arch());
	}
    }

    # Deprecate patterns which didn't match anything
    for my $pattern (grep { $_->get_pattern_matches() == 0 }
                          $self->get_patterns($soname)) {
	$pattern->mark_not_found_in_library($minver, $self->get_arch());
    }
}

sub is_empty {
    my $self = shift;
    return scalar(keys %{$self->{objects}}) ? 0 : 1;
}

sub has_object {
    my ($self, $soname) = @_;
    return exists $self->{objects}{$soname};
}

sub get_object {
    my ($self, $soname) = @_;
    return ref($soname) ? $soname : $self->{objects}{$soname};
}

sub create_object {
    my ($self, $soname, @deps) = @_;
    $self->{objects}{$soname} = {
	syms => {},
	fields => {},
	patterns => {
	    aliases => {},
	    generic => [],
	},
	deps => [ @deps ],
        minver_cache => []
    };
}

sub get_dependency {
    my ($self, $soname, $dep_id) = @_;
    $dep_id //= 0;
    return $self->get_object($soname)->{deps}[$dep_id];
}

sub get_smallest_version {
    my ($self, $soname, $dep_id) = @_;
    $dep_id //= 0;
    my $so_object = $self->get_object($soname);
    return $so_object->{minver_cache}[$dep_id]
        if defined $so_object->{minver_cache}[$dep_id];
    my $minver;
    foreach my $sym ($self->get_symbols($so_object)) {
        next if $dep_id != $sym->{dep_id};
        $minver //= $sym->{minver};
        if (version_compare($minver, $sym->{minver}) > 0) {
            $minver = $sym->{minver};
        }
    }
    $so_object->{minver_cache}[$dep_id] = $minver;
    return $minver;
}

sub get_dependencies {
    my ($self, $soname) = @_;
    return @{$self->get_object($soname)->{deps}};
}

sub get_field {
    my ($self, $soname, $name) = @_;
    if (my $obj = $self->get_object($soname)) {
	if (exists $obj->{fields}{$name}) {
	    return $obj->{fields}{$name};
	}
    }
    return;
}

# Tries to find a symbol like the $refsym and returns its descriptor.
# $refsym may also be a symbol name.
sub lookup_symbol {
    my ($self, $refsym, $sonames, $inc_deprecated) = @_;
    $inc_deprecated //= 0;
    my $name = (ref $refsym) ? $refsym->get_symbolname() : $refsym;

    foreach my $so ((ref($sonames) eq 'ARRAY') ? @$sonames : $sonames) {
	if (my $obj = $self->get_object($so)) {
	    my $sym = $obj->{syms}{$name};
	    if ($sym and ($inc_deprecated or not $sym->{deprecated}))
	    {
		return (wantarray) ?
		    ( symbol => $sym, soname => $so ) : $sym;
	    }
	}
    }
    return;
}

# Tries to find a pattern like the $refpat and returns its descriptor.
# $refpat may also be a pattern spec.
sub lookup_pattern {
    my ($self, $refpat, $sonames, $inc_deprecated) = @_;
    $inc_deprecated //= 0;
    # If $refsym is a string, we need to create a dummy ref symbol.
    $refpat = $self->create_symbol($refpat, dummy => 1) if ! ref($refpat);

    if ($refpat && $refpat->is_pattern()) {
	foreach my $soname ((ref($sonames) eq 'ARRAY') ? @$sonames : $sonames) {
	    if (my $obj = $self->get_object($soname)) {
		my $pat;
		if (my $type = $refpat->get_alias_type()) {
		    if (exists $obj->{patterns}{aliases}{$type}) {
			$pat = $obj->{patterns}{aliases}{$type}{$refpat->get_symbolname()};
		    }
		} elsif ($refpat->get_pattern_type() eq 'generic') {
		    for my $p (@{$obj->{patterns}{generic}}) {
			if (($inc_deprecated || !$p->{deprecated}) &&
			    $p->equals($refpat, versioning => 0))
			{
			    $pat = $p;
			    last;
			}
		    }
		}
		if ($pat && ($inc_deprecated || !$pat->{deprecated})) {
		    return (wantarray) ?
			(symbol => $pat, soname => $soname) : $pat;
		}
	    }
	}
    }
    return;
}

# Get symbol object reference either by symbol name or by a reference object.
sub get_symbol_object {
    my ($self, $refsym, $soname) = @_;
    my $sym = $self->lookup_symbol($refsym, $soname, 1);
    if (! defined $sym) {
	$sym = $self->lookup_pattern($refsym, $soname, 1);
    }
    return $sym;
}

sub get_new_symbols {
    my ($self, $ref, %opts) = @_;
    my $with_optional = (exists $opts{with_optional}) ?
	$opts{with_optional} : 0;
    my @res;
    foreach my $soname ($self->get_sonames()) {
	next if not $ref->has_object($soname);

	# Scan raw symbols first.
	foreach my $sym (grep { ($with_optional || ! $_->is_optional())
	                        && $_->is_legitimate($self->get_arch()) }
	                      $self->get_symbols($soname))
	{
	    my $refsym = $ref->lookup_symbol($sym, $soname, 1);
	    my $isnew;
	    if (defined $refsym) {
		# If the symbol exists in the $ref symbol file, it might
		# still be new if $refsym is not legitimate.
		$isnew = not $refsym->is_legitimate($self->get_arch());
	    } else {
		# If the symbol does not exist in the $ref symbol file, it does
		# not mean that it's new. It might still match a pattern in the
		# symbol file. However, due to performance reasons, first check
		# if the pattern that the symbol matches (if any) exists in the
		# ref symbol file as well.
		$isnew = not (
		    ($sym->get_pattern() and $ref->lookup_pattern($sym->get_pattern(), $soname, 1)) or
		    $ref->find_matching_pattern($sym, $soname, 1)
		);
	    }
	    push @res, { symbol => $sym, soname => $soname } if $isnew;
	}

	# Now scan patterns
	foreach my $p (grep { ($with_optional || ! $_->is_optional())
	                      && $_->is_legitimate($self->get_arch()) }
	                    $self->get_patterns($soname))
	{
	    my $refpat = $ref->lookup_pattern($p, $soname, 0);
	    # If reference pattern was not found or it is not legitimate,
	    # considering current one as new.
	    if (not defined $refpat or
	        not $refpat->is_legitimate($self->get_arch()))
	    {
		push @res, { symbol => $p , soname => $soname };
	    }
	}
    }
    return @res;
}

sub get_lost_symbols {
    my ($self, $ref, %opts) = @_;
    return $ref->get_new_symbols($self, %opts);
}


sub get_new_libs {
    my ($self, $ref) = @_;
    my @res;
    foreach my $soname ($self->get_sonames()) {
	push @res, $soname if not $ref->get_object($soname);
    }
    return @res;
}

sub get_lost_libs {
    my ($self, $ref) = @_;
    return $ref->get_new_libs($self);
}

1;
