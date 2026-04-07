# Copyright © 2009-2011 Raphaël Hertzog <hertzog@debian.org>
# Copyright © 2009, 2011-2017 Guillem Jover <guillem@debian.org>
#
# Hardening build flags handling derived from work of:
# Copyright © 2009-2011 Kees Cook <kees@debian.org>
# Copyright © 2007-2008 Canonical, Ltd.
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

package Dpkg::Vendor::Debian;

use strict;
use warnings;

our $VERSION = '0.01';

use Dpkg;
use Dpkg::Gettext;
use Dpkg::ErrorHandling;
use Dpkg::Control::Types;

use parent qw(Dpkg::Vendor::Default);

=encoding utf8

=head1 NAME

Dpkg::Vendor::Debian - Debian vendor class

=head1 DESCRIPTION

This vendor class customizes the behaviour of dpkg scripts for Debian
specific behavior and policies.

=cut

sub run_hook {
    my ($self, $hook, @params) = @_;

    if ($hook eq 'package-keyrings') {
        return ('/usr/share/keyrings/debian-keyring.gpg',
                '/usr/share/keyrings/debian-maintainers.gpg');
    } elsif ($hook eq 'archive-keyrings') {
        return ('/usr/share/keyrings/debian-archive-keyring.gpg');
    } elsif ($hook eq 'archive-keyrings-historic') {
        return ('/usr/share/keyrings/debian-archive-removed-keys.gpg');
    } elsif ($hook eq 'builtin-build-depends') {
        return qw(build-essential:native);
    } elsif ($hook eq 'builtin-build-conflicts') {
        return ();
    } elsif ($hook eq 'register-custom-fields') {
    } elsif ($hook eq 'extend-patch-header') {
        my ($textref, $ch_info) = @params;
	if ($ch_info->{'Closes'}) {
	    foreach my $bug (split(/\s+/, $ch_info->{'Closes'})) {
		$$textref .= "Bug-Debian: https://bugs.debian.org/$bug\n";
	    }
	}

	# XXX: Layer violation...
	require Dpkg::Vendor::Ubuntu;
	my $b = Dpkg::Vendor::Ubuntu::find_launchpad_closes($ch_info->{'Changes'});
	foreach my $bug (@$b) {
	    $$textref .= "Bug-Ubuntu: https://bugs.launchpad.net/bugs/$bug\n";
	}
    } elsif ($hook eq 'update-buildflags') {
        $self->_add_build_flags(@params);
    } elsif ($hook eq 'builtin-system-build-paths') {
        return qw(/build/);
    } elsif ($hook eq 'build-tainted-by') {
        return $self->_build_tainted_by();
    } else {
        return $self->SUPER::run_hook($hook, @params);
    }
}

sub _add_build_flags {
    my ($self, $flags) = @_;

    # Default feature states.
    my %use_feature = (
        future => {
            lfs => 0,
        },
        qa => {
            bug => 0,
            canary => 0,
        },
        reproducible => {
            timeless => 1,
            fixfilepath => 0,
            fixdebugpath => 1,
        },
        sanitize => {
            address => 0,
            thread => 0,
            leak => 0,
            undefined => 0,
        },
        hardening => {
            # XXX: This is set to undef so that we can cope with the brokenness
            # of gcc managing this feature builtin.
            pie => undef,
            stackprotector => 1,
            stackprotectorstrong => 1,
            fortify => 1,
            format => 1,
            relro => 1,
            bindnow => 0,
        },
    );

    my %builtin_feature = (
        hardening => {
            pie => 1,
        },
    );

    ## Setup

    require Dpkg::BuildOptions;

    # Adjust features based on user or maintainer's desires.
    my $opts_build = Dpkg::BuildOptions->new(envvar => 'DEB_BUILD_OPTIONS');
    my $opts_maint = Dpkg::BuildOptions->new(envvar => 'DEB_BUILD_MAINT_OPTIONS');

    foreach my $area (sort keys %use_feature) {
        $opts_build->parse_features($area, $use_feature{$area});
        $opts_maint->parse_features($area, $use_feature{$area});
    }

    require Dpkg::Arch;

    my $arch = Dpkg::Arch::get_host_arch();
    my ($abi, $libc, $os, $cpu) = Dpkg::Arch::debarch_to_debtuple($arch);

    unless (defined $abi and defined $libc and defined $os and defined $cpu) {
        warning(g_("unknown host architecture '%s'"), $arch);
        ($abi, $os, $cpu) = ('', '', '');
    }

    ## Global defaults

    my $default_flags;
    if ($opts_build->has('noopt')) {
        $default_flags = '-g -O0';
    } else {
        $default_flags = '-g -O2';
    }
    $flags->append('CFLAGS', $default_flags);
    $flags->append('CXXFLAGS', $default_flags);
    $flags->append('OBJCFLAGS', $default_flags);
    $flags->append('OBJCXXFLAGS', $default_flags);
    $flags->append('FFLAGS', $default_flags);
    $flags->append('FCFLAGS', $default_flags);
    $flags->append('GCJFLAGS', $default_flags);

    ## Area: future

    if ($use_feature{future}{lfs}) {
        my ($abi_bits, $abi_endian) = Dpkg::Arch::debarch_to_abiattrs($arch);
        my $cpu_bits = Dpkg::Arch::debarch_to_cpubits($arch);

        if ($abi_bits == 32 and $cpu_bits == 32) {
            $flags->append('CPPFLAGS',
                           '-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64');
        }
    }

    ## Area: qa

    # Warnings that detect actual bugs.
    if ($use_feature{qa}{bug}) {
        # C flags
        my @cflags = qw(
            implicit-function-declaration
        );
        foreach my $warnflag (@cflags) {
            $flags->append('CFLAGS', "-Werror=$warnflag");
        }

        # C/C++ flags
        my @cfamilyflags = qw(
            array-bounds
            clobbered
            volatile-register-var
        );
        foreach my $warnflag (@cfamilyflags) {
            $flags->append('CFLAGS', "-Werror=$warnflag");
            $flags->append('CXXFLAGS', "-Werror=$warnflag");
        }
    }

    # Inject dummy canary options to detect issues with build flag propagation.
    if ($use_feature{qa}{canary}) {
        require Digest::MD5;
        my $id = Digest::MD5::md5_hex(int rand 4096);

        foreach my $flag (qw(CPPFLAGS CFLAGS OBJCFLAGS CXXFLAGS OBJCXXFLAGS)) {
            $flags->append($flag, "-D__DEB_CANARY_${flag}_${id}__");
        }
        $flags->append('LDFLAGS', "-Wl,-z,deb-canary-${id}");
    }

    ## Area: reproducible

    my $build_path;

    # Mask features that might have an unsafe usage.
    if ($use_feature{reproducible}{fixfilepath} or
        $use_feature{reproducible}{fixdebugpath}) {
        require Cwd;

        $build_path = $ENV{DEB_BUILD_PATH} || Cwd::getcwd();

        # If we have any unsafe character in the path, disable the flag,
        # so that we do not need to worry about escaping the characters
        # on output.
        if ($build_path =~ m/[^-+:.0-9a-zA-Z~\/_]/) {
            $use_feature{reproducible}{fixfilepath} = 0;
            $use_feature{reproducible}{fixdebugpath} = 0;
        }
    }

    # Warn when the __TIME__, __DATE__ and __TIMESTAMP__ macros are used.
    if ($use_feature{reproducible}{timeless}) {
       $flags->append('CPPFLAGS', '-Wdate-time');
    }

    # Avoid storing the build path in the binaries.
    if ($use_feature{reproducible}{fixfilepath} or
        $use_feature{reproducible}{fixdebugpath}) {
        my $map;

        # -ffile-prefix-map is a superset of -fdebug-prefix-map, prefer it
        # if both are set.
        if ($use_feature{reproducible}{fixfilepath}) {
            $map = '-ffile-prefix-map=' . $build_path . '=.';
        } else {
            $map = '-fdebug-prefix-map=' . $build_path . '=.';
        }

        $flags->append('CFLAGS', $map);
        $flags->append('CXXFLAGS', $map);
        $flags->append('OBJCFLAGS', $map);
        $flags->append('OBJCXXFLAGS', $map);
        $flags->append('FFLAGS', $map);
        $flags->append('FCFLAGS', $map);
        $flags->append('GCJFLAGS', $map);
    }

    ## Area: sanitize

    # Handle logical feature interactions.
    if ($use_feature{sanitize}{address} and $use_feature{sanitize}{thread}) {
        # Disable the thread sanitizer when the address one is active, they
        # are mutually incompatible.
        $use_feature{sanitize}{thread} = 0;
    }
    if ($use_feature{sanitize}{address} or $use_feature{sanitize}{thread}) {
        # Disable leak sanitizer, it is implied by the address or thread ones.
        $use_feature{sanitize}{leak} = 0;
    }

    if ($use_feature{sanitize}{address}) {
        my $flag = '-fsanitize=address -fno-omit-frame-pointer';
        $flags->append('CFLAGS', $flag);
        $flags->append('CXXFLAGS', $flag);
        $flags->append('LDFLAGS', '-fsanitize=address');
    }

    if ($use_feature{sanitize}{thread}) {
        my $flag = '-fsanitize=thread';
        $flags->append('CFLAGS', $flag);
        $flags->append('CXXFLAGS', $flag);
        $flags->append('LDFLAGS', $flag);
    }

    if ($use_feature{sanitize}{leak}) {
        $flags->append('LDFLAGS', '-fsanitize=leak');
    }

    if ($use_feature{sanitize}{undefined}) {
        my $flag = '-fsanitize=undefined';
        $flags->append('CFLAGS', $flag);
        $flags->append('CXXFLAGS', $flag);
        $flags->append('LDFLAGS', $flag);
    }

    ## Area: hardening

    # Mask builtin features that are not enabled by default in the compiler.
    my %builtin_pie_arch = map { $_ => 1 } qw(
        amd64
        arm64
        armel
        armhf
        hurd-i386
        i386
        kfreebsd-amd64
        kfreebsd-i386
        mips
        mipsel
        mips64el
        powerpc
        ppc64
        ppc64el
        riscv64
        s390x
        sparc
        sparc64
    );
    if (not exists $builtin_pie_arch{$arch}) {
        $builtin_feature{hardening}{pie} = 0;
    }

    # Mask features that are not available on certain architectures.
    if ($os !~ /^(?:linux|kfreebsd|knetbsd|hurd)$/ or
	$cpu =~ /^(?:hppa|avr32)$/) {
	# Disabled on non-(linux/kfreebsd/knetbsd/hurd).
	# Disabled on hppa, avr32
	#  (#574716).
	$use_feature{hardening}{pie} = 0;
    }
    if ($cpu =~ /^(?:ia64|alpha|hppa|nios2)$/ or $arch eq 'arm') {
	# Stack protector disabled on ia64, alpha, hppa, nios2.
	#   "warning: -fstack-protector not supported for this target"
	# Stack protector disabled on arm (ok on armel).
	#   compiler supports it incorrectly (leads to SEGV)
	$use_feature{hardening}{stackprotector} = 0;
    }
    if ($cpu =~ /^(?:ia64|hppa|avr32)$/) {
	# relro not implemented on ia64, hppa, avr32.
	$use_feature{hardening}{relro} = 0;
    }

    # Mask features that might be influenced by other flags.
    if ($opts_build->has('noopt')) {
      # glibc 2.16 and later warn when using -O0 and _FORTIFY_SOURCE.
      $use_feature{hardening}{fortify} = 0;
    }

    # Handle logical feature interactions.
    if ($use_feature{hardening}{relro} == 0) {
	# Disable bindnow if relro is not enabled, since it has no
	# hardening ability without relro and may incur load penalties.
	$use_feature{hardening}{bindnow} = 0;
    }
    if ($use_feature{hardening}{stackprotector} == 0) {
	# Disable stackprotectorstrong if stackprotector is disabled.
	$use_feature{hardening}{stackprotectorstrong} = 0;
    }

    # PIE
    if (defined $use_feature{hardening}{pie} and
        $use_feature{hardening}{pie} and
        not $builtin_feature{hardening}{pie}) {
	my $flag = "-specs=$Dpkg::DATADIR/pie-compile.specs";
	$flags->append('CFLAGS', $flag);
	$flags->append('OBJCFLAGS',  $flag);
	$flags->append('OBJCXXFLAGS', $flag);
	$flags->append('FFLAGS', $flag);
	$flags->append('FCFLAGS', $flag);
	$flags->append('CXXFLAGS', $flag);
	$flags->append('GCJFLAGS', $flag);
	$flags->append('LDFLAGS', "-specs=$Dpkg::DATADIR/pie-link.specs");
    } elsif (defined $use_feature{hardening}{pie} and
             not $use_feature{hardening}{pie} and
             $builtin_feature{hardening}{pie}) {
	my $flag = "-specs=$Dpkg::DATADIR/no-pie-compile.specs";
	$flags->append('CFLAGS', $flag);
	$flags->append('OBJCFLAGS',  $flag);
	$flags->append('OBJCXXFLAGS', $flag);
	$flags->append('FFLAGS', $flag);
	$flags->append('FCFLAGS', $flag);
	$flags->append('CXXFLAGS', $flag);
	$flags->append('GCJFLAGS', $flag);
	$flags->append('LDFLAGS', "-specs=$Dpkg::DATADIR/no-pie-link.specs");
    }

    # Stack protector
    if ($use_feature{hardening}{stackprotectorstrong}) {
	my $flag = '-fstack-protector-strong';
	$flags->append('CFLAGS', $flag);
	$flags->append('OBJCFLAGS', $flag);
	$flags->append('OBJCXXFLAGS', $flag);
	$flags->append('FFLAGS', $flag);
	$flags->append('FCFLAGS', $flag);
	$flags->append('CXXFLAGS', $flag);
	$flags->append('GCJFLAGS', $flag);
    } elsif ($use_feature{hardening}{stackprotector}) {
	my $flag = '-fstack-protector --param=ssp-buffer-size=4';
	$flags->append('CFLAGS', $flag);
	$flags->append('OBJCFLAGS', $flag);
	$flags->append('OBJCXXFLAGS', $flag);
	$flags->append('FFLAGS', $flag);
	$flags->append('FCFLAGS', $flag);
	$flags->append('CXXFLAGS', $flag);
	$flags->append('GCJFLAGS', $flag);
    }

    # Fortify Source
    if ($use_feature{hardening}{fortify}) {
	$flags->append('CPPFLAGS', '-D_FORTIFY_SOURCE=2');
    }

    # Format Security
    if ($use_feature{hardening}{format}) {
	my $flag = '-Wformat -Werror=format-security';
	$flags->append('CFLAGS', $flag);
	$flags->append('CXXFLAGS', $flag);
	$flags->append('OBJCFLAGS', $flag);
	$flags->append('OBJCXXFLAGS', $flag);
    }

    # Read-only Relocations
    if ($use_feature{hardening}{relro}) {
	$flags->append('LDFLAGS', '-Wl,-z,relro');
    }

    # Bindnow
    if ($use_feature{hardening}{bindnow}) {
	$flags->append('LDFLAGS', '-Wl,-z,now');
    }

    ## Commit

    # Set used features to their builtin setting if unset.
    foreach my $area (sort keys %builtin_feature) {
        foreach my $feature (keys %{$builtin_feature{$area}}) {
            $use_feature{$area}{$feature} //= $builtin_feature{$area}{$feature};
        }
    }

    # Store the feature usage.
    foreach my $area (sort keys %use_feature) {
        while (my ($feature, $enabled) = each %{$use_feature{$area}}) {
            $flags->set_feature($area, $feature, $enabled);
        }
    }
}

sub _build_tainted_by {
    my $self = shift;
    my %tainted;

    foreach my $pathname (qw(/bin /sbin /lib /lib32 /libo32 /libx32 /lib64)) {
        next unless -l $pathname;

        my $linkname = readlink $pathname;
        if ($linkname eq "usr$pathname") {
            $tainted{'merged-usr-via-symlinks'} = 1;
            last;
        }
    }

    require File::Find;
    my %usr_local_types = (
        configs => [ qw(etc) ],
        includes => [ qw(include) ],
        programs => [ qw(bin sbin) ],
        libraries => [ qw(lib) ],
    );
    foreach my $type (keys %usr_local_types) {
        File::Find::find({
            wanted => sub { $tainted{"usr-local-has-$type"} = 1 if -f },
            no_chdir => 1,
        }, grep { -d } map { "/usr/local/$_" } @{$usr_local_types{$type}});
    }

    my @tainted = sort keys %tainted;
    return @tainted;
}

=head1 CHANGES

=head2 Version 0.xx

This is a private module.

=cut

1;
