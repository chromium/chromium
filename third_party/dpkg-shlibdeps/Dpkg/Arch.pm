# Copyright © 2006-2015 Guillem Jover <guillem@debian.org>
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

package Dpkg::Arch;

=encoding utf8

=head1 NAME

Dpkg::Arch - handle architectures

=head1 DESCRIPTION

The Dpkg::Arch module provides functions to handle Debian architectures,
wildcards, and mapping from and to GNU triplets.

No symbols are exported by default. The :all tag can be used to import all
symbols. The :getters, :parsers, :mappers and :operators tags can be used
to import specific symbol subsets.

=cut

use strict;
use warnings;
use feature qw(state);

our $VERSION = '1.03';
our @EXPORT_OK = qw(
    get_raw_build_arch
    get_raw_host_arch
    get_build_arch
    get_host_arch
    get_host_gnu_type
    get_valid_arches
    debarch_eq
    debarch_is
    debarch_is_wildcard
    debarch_is_illegal
    debarch_is_concerned
    debarch_to_abiattrs
    debarch_to_cpubits
    debarch_to_gnutriplet
    debarch_to_debtuple
    debarch_to_multiarch
    debarch_list_parse
    debtuple_to_debarch
    debtuple_to_gnutriplet
    gnutriplet_to_debarch
    gnutriplet_to_debtuple
    gnutriplet_to_multiarch
);
our %EXPORT_TAGS = (
    all => [ @EXPORT_OK ],
    getters => [ qw(
        get_raw_build_arch
        get_raw_host_arch
        get_build_arch
        get_host_arch
        get_host_gnu_type
        get_valid_arches
    ) ],
    parsers => [ qw(
        debarch_list_parse
    ) ],
    mappers => [ qw(
        debarch_to_abiattrs
        debarch_to_gnutriplet
        debarch_to_debtuple
        debarch_to_multiarch
        debtuple_to_debarch
        debtuple_to_gnutriplet
        gnutriplet_to_debarch
        gnutriplet_to_debtuple
        gnutriplet_to_multiarch
    ) ],
    operators => [ qw(
        debarch_eq
        debarch_is
        debarch_is_wildcard
        debarch_is_illegal
        debarch_is_concerned
    ) ],
);


use Exporter qw(import);
use List::Util qw(any);

use Dpkg ();
use Dpkg::Gettext;
use Dpkg::ErrorHandling;
use Dpkg::Build::Env;

my (@cpu, @os);
my (%cputable, %ostable);
my (%cputable_re, %ostable_re);
my (%cpubits, %cpuendian);
my %abibits;

my %debtuple_to_debarch;
my %debarch_to_debtuple;

=head1 FUNCTIONS

=over 4

=item $arch = get_raw_build_arch()

Get the raw build Debian architecture, without taking into account variables
from the environment.

=cut

sub get_raw_build_arch()
{
    state $build_arch;

    return $build_arch if defined $build_arch;

    # Note: We *always* require an installed dpkg when inferring the
    # build architecture. The bootstrapping case is handled by
    # dpkg-architecture itself, by avoiding computing the DEB_BUILD_
    # variables when they are not requested.

    ## no critic (TestingAndDebugging::ProhibitNoWarnings)
    no warnings qw(exec);
    $build_arch = qx(dpkg --print-architecture);
    syserr('dpkg --print-architecture failed') if $? >> 8;

    chomp $build_arch;
    return $build_arch;
}

=item $arch = get_build_arch()

Get the build Debian architecture, using DEB_BUILD_ARCH from the environment
if available.

=cut

sub get_build_arch()
{
    return Dpkg::Build::Env::get('DEB_BUILD_ARCH') || get_raw_build_arch();
}

{
    my %cc_host_gnu_type;

    sub get_host_gnu_type()
    {
        my $CC = $ENV{CC} || 'gcc';

        return $cc_host_gnu_type{$CC} if defined $cc_host_gnu_type{$CC};

        ## no critic (TestingAndDebugging::ProhibitNoWarnings)
        no warnings qw(exec);
        $cc_host_gnu_type{$CC} = qx($CC -dumpmachine);
	if ($? >> 8) {
            $cc_host_gnu_type{$CC} = '';
	} else {
            chomp $cc_host_gnu_type{$CC};
	}

        return $cc_host_gnu_type{$CC};
    }

    sub set_host_gnu_type
    {
        my ($host_gnu_type) = @_;
        my $CC = $ENV{CC} || 'gcc';

        $cc_host_gnu_type{$CC} = $host_gnu_type;
    }
}

=item $arch = get_raw_host_arch()

Get the raw host Debian architecture, without taking into account variables
from the environment.

=cut

sub get_raw_host_arch()
{
    state $host_arch;

    return $host_arch if defined $host_arch;

    my $host_gnu_type = get_host_gnu_type();

    if ($host_gnu_type eq '') {
        warning(g_('cannot determine CC system type, falling back to ' .
                   'default (native compilation)'));
    } else {
        my (@host_archtuple) = gnutriplet_to_debtuple($host_gnu_type);
        $host_arch = debtuple_to_debarch(@host_archtuple);

        if (defined $host_arch) {
            $host_gnu_type = debtuple_to_gnutriplet(@host_archtuple);
        } else {
            warning(g_('unknown CC system type %s, falling back to ' .
                       'default (native compilation)'), $host_gnu_type);
            $host_gnu_type = '';
        }
        set_host_gnu_type($host_gnu_type);
    }

    if (!defined($host_arch)) {
        # Switch to native compilation.
        $host_arch = get_raw_build_arch();
    }

    return $host_arch;
}

=item $arch = get_host_arch()

Get the host Debian architecture, using DEB_HOST_ARCH from the environment
if available.

=cut

sub get_host_arch()
{
    return Dpkg::Build::Env::get('DEB_HOST_ARCH') || get_raw_host_arch();
}

=item @arch_list = get_valid_arches()

Get an array with all currently known Debian architectures.

=cut

sub get_valid_arches()
{
    _load_cputable();
    _load_ostable();

    my @arches;

    foreach my $os (@os) {
	foreach my $cpu (@cpu) {
	    my $arch = debtuple_to_debarch(split(/-/, $os, 3), $cpu);
	    push @arches, $arch if defined($arch);
	}
    }

    return @arches;
}

my %table_loaded;
sub _load_table
{
    my ($table, $loader) = @_;

    return if $table_loaded{$table};

    local $_;
    local $/ = "\n";

    open my $table_fh, '<', "$Dpkg::DATADIR/$table"
	or syserr(g_('cannot open %s'), $table);
    while (<$table_fh>) {
	$loader->($_);
    }
    close $table_fh;

    $table_loaded{$table} = 1;
}

sub _load_cputable
{
    _load_table('cputable', sub {
	if (m/^(?!\#)(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)/) {
	    $cputable{$1} = $2;
	    $cputable_re{$1} = $3;
	    $cpubits{$1} = $4;
	    $cpuendian{$1} = $5;
	    push @cpu, $1;
	}
    });
}

sub _load_ostable
{
    _load_table('ostable', sub {
	if (m/^(?!\#)(\S+)\s+(\S+)\s+(\S+)/) {
	    $ostable{$1} = $2;
	    $ostable_re{$1} = $3;
	    push @os, $1;
	}
    });
}

sub _load_abitable()
{
    _load_table('abitable', sub {
        if (m/^(?!\#)(\S+)\s+(\S+)/) {
            $abibits{$1} = $2;
        }
    });
}

sub _load_tupletable()
{
    _load_cputable();

    _load_table('tupletable', sub {
	if (m/^(?!\#)(\S+)\s+(\S+)/) {
	    my $debtuple = $1;
	    my $debarch = $2;

	    if ($debtuple =~ /<cpu>/) {
		foreach my $_cpu (@cpu) {
		    (my $dt = $debtuple) =~ s/<cpu>/$_cpu/;
		    (my $da = $debarch) =~ s/<cpu>/$_cpu/;

		    next if exists $debarch_to_debtuple{$da}
		         or exists $debtuple_to_debarch{$dt};

		    $debarch_to_debtuple{$da} = $dt;
		    $debtuple_to_debarch{$dt} = $da;
		}
	    } else {
		$debarch_to_debtuple{$2} = $1;
		$debtuple_to_debarch{$1} = $2;
	    }
	}
    });
}

sub debtuple_to_gnutriplet(@)
{
    my ($abi, $libc, $os, $cpu) = @_;

    _load_cputable();
    _load_ostable();

    return unless
        defined $abi && defined $libc && defined $os && defined $cpu &&
        exists $cputable{$cpu} && exists $ostable{"$abi-$libc-$os"};
    return join('-', $cputable{$cpu}, $ostable{"$abi-$libc-$os"});
}

sub gnutriplet_to_debtuple($)
{
    my $gnu = shift;
    return unless defined($gnu);
    my ($gnu_cpu, $gnu_os) = split(/-/, $gnu, 2);
    return unless defined($gnu_cpu) && defined($gnu_os);

    _load_cputable();
    _load_ostable();

    my ($os, $cpu);

    foreach my $_cpu (@cpu) {
	if ($gnu_cpu =~ /^$cputable_re{$_cpu}$/) {
	    $cpu = $_cpu;
	    last;
	}
    }

    foreach my $_os (@os) {
	if ($gnu_os =~ /^(.*-)?$ostable_re{$_os}$/) {
	    $os = $_os;
	    last;
	}
    }

    return if !defined($cpu) || !defined($os);
    return (split(/-/, $os, 3), $cpu);
}

=item $multiarch = gnutriplet_to_multiarch($gnutriplet)

Map a GNU triplet into a Debian multiarch triplet.

=cut

sub gnutriplet_to_multiarch($)
{
    my $gnu = shift;
    my ($cpu, $cdr) = split(/-/, $gnu, 2);

    if ($cpu =~ /^i[4567]86$/) {
	return "i386-$cdr";
    } else {
	return $gnu;
    }
}

=item $multiarch = debarch_to_multiarch($arch)

Map a Debian architecture into a Debian multiarch triplet.

=cut

sub debarch_to_multiarch($)
{
    my $arch = shift;

    return gnutriplet_to_multiarch(debarch_to_gnutriplet($arch));
}

sub debtuple_to_debarch(@)
{
    my ($abi, $libc, $os, $cpu) = @_;

    _load_tupletable();

    if (!defined $abi || !defined $libc || !defined $os || !defined $cpu) {
	return;
    } elsif (exists $debtuple_to_debarch{"$abi-$libc-$os-$cpu"}) {
	return $debtuple_to_debarch{"$abi-$libc-$os-$cpu"};
    } else {
	return;
    }
}

sub debarch_to_debtuple($)
{
    my $arch = shift;

    return if not defined $arch;

    _load_tupletable();

    if ($arch =~ /^linux-([^-]*)/) {
	# XXX: Might disappear in the future, not sure yet.
	$arch = $1;
    }

    my $tuple = $debarch_to_debtuple{$arch};

    if (defined($tuple)) {
        my @tuple = split /-/, $tuple, 4;
        return @tuple if wantarray;
        return {
            abi => $tuple[0],
            libc => $tuple[1],
            os => $tuple[2],
            cpu => $tuple[3],
        };
    } else {
	return;
    }
}

=item $gnutriplet = debarch_to_gnutriplet($arch)

Map a Debian architecture into a GNU triplet.

=cut

sub debarch_to_gnutriplet($)
{
    my $arch = shift;

    return debtuple_to_gnutriplet(debarch_to_debtuple($arch));
}

=item $arch = gnutriplet_to_debarch($gnutriplet)

Map a GNU triplet into a Debian architecture.

=cut

sub gnutriplet_to_debarch($)
{
    my $gnu = shift;

    return debtuple_to_debarch(gnutriplet_to_debtuple($gnu));
}

sub debwildcard_to_debtuple($)
{
    my $arch = shift;
    my @tuple = split /-/, $arch, 4;

    if (any { $_ eq 'any' } @tuple) {
	if (scalar @tuple == 4) {
	    return @tuple;
	} elsif (scalar @tuple == 3) {
	    return ('any', @tuple);
	} elsif (scalar @tuple == 2) {
	    return ('any', 'any', @tuple);
	} else {
	    return ('any', 'any', 'any', 'any');
	}
    } else {
	return debarch_to_debtuple($arch);
    }
}

sub debarch_to_abiattrs($)
{
    my $arch = shift;
    my ($abi, $libc, $os, $cpu) = debarch_to_debtuple($arch);

    if (defined($cpu)) {
        _load_abitable();

        return ($abibits{$abi} // $cpubits{$cpu}, $cpuendian{$cpu});
    } else {
        return;
    }
}

sub debarch_to_cpubits($)
{
    my $arch = shift;
    my (undef, undef, undef, $cpu) = debarch_to_debtuple($arch);

    if (defined $cpu) {
        return $cpubits{$cpu};
    } else {
        return;
    }
}

=item $bool = debarch_eq($arch_a, $arch_b)

Evaluate the equality of a Debian architecture, by comparing with another
Debian architecture. No wildcard matching is performed.

=cut

sub debarch_eq($$)
{
    my ($a, $b) = @_;

    return 1 if ($a eq $b);

    my @a = debarch_to_debtuple($a);
    my @b = debarch_to_debtuple($b);

    return 0 if scalar @a != 4 or scalar @b != 4;

    return $a[0] eq $b[0] && $a[1] eq $b[1] && $a[2] eq $b[2] && $a[3] eq $b[3];
}

=item $bool = debarch_is($arch, $arch_wildcard)

Evaluate the identity of a Debian architecture, by matching with an
architecture wildcard.

=cut

sub debarch_is($$)
{
    my ($real, $alias) = @_;

    return 1 if ($alias eq $real or $alias eq 'any');

    my @real = debarch_to_debtuple($real);
    my @alias = debwildcard_to_debtuple($alias);

    return 0 if scalar @real != 4 or scalar @alias != 4;

    if (($alias[0] eq $real[0] || $alias[0] eq 'any') &&
        ($alias[1] eq $real[1] || $alias[1] eq 'any') &&
        ($alias[2] eq $real[2] || $alias[2] eq 'any') &&
        ($alias[3] eq $real[3] || $alias[3] eq 'any')) {
	return 1;
    }

    return 0;
}

=item $bool = debarch_is_wildcard($arch)

Evaluate whether a Debian architecture is an architecture wildcard.

=cut

sub debarch_is_wildcard($)
{
    my $arch = shift;

    return 0 if $arch eq 'all';

    my @tuple = debwildcard_to_debtuple($arch);

    return 0 if scalar @tuple != 4;
    return 1 if any { $_ eq 'any' } @tuple;
    return 0;
}

=item $bool = debarch_is_illegal($arch, %options)

Validate an architecture name.

If the "positive" option is set to a true value, only positive architectures
will be accepted, otherwise negated architectures are allowed.

=cut

sub debarch_is_illegal
{
    my ($arch, %opts) = @_;
    my $arch_re = qr/[a-zA-Z0-9][a-zA-Z0-9-]*/;

    if ($opts{positive}) {
        return $arch !~ m/^$arch_re$/;
    } else {
        return $arch !~ m/^!?$arch_re$/;
    }
}

=item $bool = debarch_is_concerned($arch, @arches)

Evaluate whether a Debian architecture applies to the list of architecture
restrictions, as usually found in dependencies inside square brackets.

=cut

sub debarch_is_concerned
{
    my ($host_arch, @arches) = @_;

    my $seen_arch = 0;
    foreach my $arch (@arches) {
        $arch = lc $arch;

        if ($arch =~ /^!/) {
            my $not_arch = $arch;
            $not_arch =~ s/^!//;

            if (debarch_is($host_arch, $not_arch)) {
                $seen_arch = 0;
                last;
            } else {
                # !arch includes by default all other arches
                # unless they also appear in a !otherarch
                $seen_arch = 1;
            }
        } elsif (debarch_is($host_arch, $arch)) {
            $seen_arch = 1;
            last;
        }
    }
    return $seen_arch;
}

=item @array = debarch_list_parse($arch_list, %options)

Parse an architecture list.

If the "positive" option is set to a true value, only positive architectures
will be accepted, otherwise negated architectures are allowed.

=cut

sub debarch_list_parse
{
    my ($arch_list, %opts) = @_;
    my @arch_list = split ' ', $arch_list;

    foreach my $arch (@arch_list) {
        if (debarch_is_illegal($arch, %opts)) {
            error(g_("'%s' is not a legal architecture in list '%s'"),
                  $arch, $arch_list);
        }
    }

    return @arch_list;
}

1;

__END__

=back

=head1 CHANGES

=head2 Version 1.03 (dpkg 1.19.1)

New argument: Accept a "positive" option in debarch_is_illegal() and
debarch_list_parse().

=head2 Version 1.02 (dpkg 1.18.19)

New import tags: ":all", ":getters", ":parsers", ":mappers", ":operators".

=head2 Version 1.01 (dpkg 1.18.5)

New functions: debarch_is_illegal(), debarch_list_parse().

=head2 Version 1.00 (dpkg 1.18.2)

Mark the module as public.

=head1 SEE ALSO

dpkg-architecture(1).
