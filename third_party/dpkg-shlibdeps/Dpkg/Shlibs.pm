# Copyright © 2007, 2016 Raphaël Hertzog <hertzog@debian.org>
# Copyright © 2007-2008, 2012-2015 Guillem Jover <guillem@debian.org>
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

package Dpkg::Shlibs;

use strict;
use warnings;
use feature qw(state);

our $VERSION = '0.03';
our @EXPORT_OK = qw(
    blank_library_paths
    setup_library_paths
    get_library_paths
    add_library_dir
    find_library
);

use Exporter qw(import);
use List::Util qw(none);
use File::Spec;

use Dpkg::Gettext;
use Dpkg::ErrorHandling;
use Dpkg::Shlibs::Objdump;
use Dpkg::Path qw(resolve_symlink canonpath);
use Dpkg::Arch qw(get_build_arch get_host_arch :mappers);

use constant DEFAULT_LIBRARY_PATH =>
    qw(/lib /usr/lib);
# XXX: Deprecated multilib paths.
use constant DEFAULT_MULTILIB_PATH =>
    qw(/lib32 /usr/lib32 /lib64 /usr/lib64);

# Library paths set by the user.
my @custom_librarypaths;
# Library paths from the system.
my @system_librarypaths;
my $librarypaths_init;

sub parse_ldso_conf {
    my $file = shift;
    state %visited;
    local $_;

    open my $fh, '<', $file or syserr(g_('cannot open %s'), $file);
    $visited{$file}++;
    while (<$fh>) {
	next if /^\s*$/;
	chomp;
	s{/+$}{};
	if (/^include\s+(\S.*\S)\s*$/) {
	    foreach my $include (glob($1)) {
		parse_ldso_conf($include) if -e $include
		    && !$visited{$include};
	    }
	} elsif (m{^\s*/}) {
	    s/^\s+//;
	    my $libdir = $_;
	    if (none { $_ eq $libdir } (@custom_librarypaths, @system_librarypaths)) {
		push @system_librarypaths, $libdir;
	    }
	}
    }
    close $fh;
}

sub blank_library_paths {
    @custom_librarypaths = ();
    @system_librarypaths = ();
    $librarypaths_init = 1;
}

sub setup_library_paths {
    @custom_librarypaths = ();
    @system_librarypaths = ();

    # XXX: Deprecated. Update library paths with LD_LIBRARY_PATH.
    if ($ENV{LD_LIBRARY_PATH}) {
        require Cwd;
        my $cwd = Cwd::getcwd;

        foreach my $path (split /:/, $ENV{LD_LIBRARY_PATH}) {
            $path =~ s{/+$}{};

            my $realpath = Cwd::realpath($path);
            next unless defined $realpath;
            if ($realpath =~ m/^\Q$cwd\E/) {
                warning(g_('deprecated use of LD_LIBRARY_PATH with private ' .
                           'library directory which interferes with ' .
                           'cross-building, please use -l option instead'));
            }

            # XXX: This should be added to @custom_librarypaths, but as this
            # is deprecated we do not care as the code will go away.
            push @system_librarypaths, $path;
        }
    }

    # Adjust set of directories to consider when we're in a situation of a
    # cross-build or a build of a cross-compiler.
    my $multiarch;

    # Detect cross compiler builds.
    if ($ENV{DEB_TARGET_GNU_TYPE} and
        ($ENV{DEB_TARGET_GNU_TYPE} ne $ENV{DEB_BUILD_GNU_TYPE}))
    {
        $multiarch = gnutriplet_to_multiarch($ENV{DEB_TARGET_GNU_TYPE});
    }
    # Host for normal cross builds.
    if (get_build_arch() ne get_host_arch()) {
        $multiarch = debarch_to_multiarch(get_host_arch());
    }
    # Define list of directories containing crossbuilt libraries.
    if ($multiarch) {
        push @system_librarypaths, "/lib/$multiarch", "/usr/lib/$multiarch";
    }

    push @system_librarypaths, DEFAULT_LIBRARY_PATH;

    # Update library paths with ld.so config.
    parse_ldso_conf('/etc/ld.so.conf') if -e '/etc/ld.so.conf';

    push @system_librarypaths, DEFAULT_MULTILIB_PATH;

    $librarypaths_init = 1;
}

sub add_library_dir {
    my $dir = shift;

    setup_library_paths() if not $librarypaths_init;

    push @custom_librarypaths, $dir;
}

sub get_library_paths {
    setup_library_paths() if not $librarypaths_init;

    return (@custom_librarypaths, @system_librarypaths);
}

# find_library ($soname, \@rpath, $format, $root)
sub find_library {
    my ($lib, $rpath, $format, $root) = @_;

    setup_library_paths() if not $librarypaths_init;

    my @librarypaths = (@{$rpath}, @custom_librarypaths, @system_librarypaths);
    my @libs;

    $root //= '';
    $root =~ s{/+$}{};
    foreach my $dir (@librarypaths) {
	my $checkdir = "$root$dir";
	if (-e "$checkdir/$lib") {
	    my $libformat = Dpkg::Shlibs::Objdump::get_format("$checkdir/$lib");
	    if ($format eq $libformat) {
		push @libs, canonpath("$checkdir/$lib");
	    } else {
		debug(1, "Skipping lib $checkdir/$lib, libabi=0x%s != objabi=0x%s",
		      unpack('H*', $libformat), unpack('H*', $format));
	    }
	}
    }
    return @libs;
}

1;
