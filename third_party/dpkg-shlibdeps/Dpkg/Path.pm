# Copyright © 2007-2011 Raphaël Hertzog <hertzog@debian.org>
# Copyright © 2011 Linaro Limited
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

package Dpkg::Path;

use strict;
use warnings;

our $VERSION = '1.04';
our @EXPORT_OK = qw(
    canonpath
    resolve_symlink
    check_files_are_the_same
    find_command
    find_build_file
    get_control_path
    get_pkg_root_dir
    guess_pkg_root_dir
    relative_to_pkg_root
);

use Exporter qw(import);
use File::Spec;
use Cwd qw(realpath);

use Dpkg::Arch qw(get_host_arch debarch_to_debtuple);
use Dpkg::IPC;

=encoding utf8

=head1 NAME

Dpkg::Path - some common path handling functions

=head1 DESCRIPTION

It provides some functions to handle various path.

=head1 FUNCTIONS

=over 8

=item get_pkg_root_dir($file)

This function will scan upwards the hierarchy of directory to find out
the directory which contains the "DEBIAN" sub-directory and it will return
its path. This directory is the root directory of a package being built.

If no DEBIAN subdirectory is found, it will return undef.

=cut

sub get_pkg_root_dir($) {
    my $file = shift;
    $file =~ s{/+$}{};
    $file =~ s{/+[^/]+$}{} if not -d $file;
    while ($file) {
	return $file if -d "$file/DEBIAN";
	last if $file !~ m{/};
	$file =~ s{/+[^/]+$}{};
    }
    return;
}

=item relative_to_pkg_root($file)

Returns the filename relative to get_pkg_root_dir($file).

=cut

sub relative_to_pkg_root($) {
    my $file = shift;
    my $pkg_root = get_pkg_root_dir($file);
    if (defined $pkg_root) {
	$pkg_root .= '/';
	return $file if ($file =~ s/^\Q$pkg_root\E//);
    }
    return;
}

=item guess_pkg_root_dir($file)

This function tries to guess the root directory of the package build tree.
It will first use get_pkg_root_dir(), but it will fallback to a more
imprecise check: namely it will use the parent directory that is a
sub-directory of the debian directory.

It can still return undef if a file outside of the debian sub-directory is
provided.

=cut

sub guess_pkg_root_dir($) {
    my $file = shift;
    my $root = get_pkg_root_dir($file);
    return $root if defined $root;

    $file =~ s{/+$}{};
    $file =~ s{/+[^/]+$}{} if not -d $file;
    my $parent = $file;
    while ($file) {
	$parent =~ s{/+[^/]+$}{};
	last if not -d $parent;
	return $file if check_files_are_the_same('debian', $parent);
	$file = $parent;
	last if $file !~ m{/};
    }
    return;
}

=item check_files_are_the_same($file1, $file2, $resolve_symlink)

This function verifies that both files are the same by checking that the device
numbers and the inode numbers returned by stat()/lstat() are the same. If
$resolve_symlink is true then stat() is used, otherwise lstat() is used.

=cut

sub check_files_are_the_same($$;$) {
    my ($file1, $file2, $resolve_symlink) = @_;
    return 0 if ((! -e $file1) || (! -e $file2));
    my (@stat1, @stat2);
    if ($resolve_symlink) {
        @stat1 = stat($file1);
        @stat2 = stat($file2);
    } else {
        @stat1 = lstat($file1);
        @stat2 = lstat($file2);
    }
    my $result = ($stat1[0] == $stat2[0]) && ($stat1[1] == $stat2[1]);
    return $result;
}


=item canonpath($file)

This function returns a cleaned path. It simplifies double //, and remove
/./ and /../ intelligently. For /../ it simplifies the path only if the
previous element is not a symlink. Thus it should only be used on real
filenames.

=cut

sub canonpath($) {
    my $path = shift;
    $path = File::Spec->canonpath($path);
    my ($v, $dirs, $file) = File::Spec->splitpath($path);
    my @dirs = File::Spec->splitdir($dirs);
    my @new;
    foreach my $d (@dirs) {
	if ($d eq '..') {
	    if (scalar(@new) > 0 and $new[-1] ne '..') {
		next if $new[-1] eq ''; # Root directory has no parent
		my $parent = File::Spec->catpath($v,
			File::Spec->catdir(@new), '');
		if (not -l $parent) {
		    pop @new;
		} else {
		    push @new, $d;
		}
	    } else {
		push @new, $d;
	    }
	} else {
	    push @new, $d;
	}
    }
    return File::Spec->catpath($v, File::Spec->catdir(@new), $file);
}

=item $newpath = resolve_symlink($symlink)

Return the filename of the file pointed by the symlink. The new name is
canonicalized by canonpath().

=cut

sub resolve_symlink($) {
    my $symlink = shift;
    my $content = readlink($symlink);
    return unless defined $content;
    if (File::Spec->file_name_is_absolute($content)) {
	return canonpath($content);
    } else {
	my ($link_v, $link_d, $link_f) = File::Spec->splitpath($symlink);
	my ($cont_v, $cont_d, $cont_f) = File::Spec->splitpath($content);
	my $new = File::Spec->catpath($link_v, $link_d . '/' . $cont_d, $cont_f);
	return canonpath($new);
    }
}


=item $cmdpath = find_command($command)

Return the path of the command if defined and available on an absolute or
relative path or on the $PATH, undef otherwise.

=cut

sub find_command($) {
    my $cmd = shift;

    return if not $cmd;
    if ($cmd =~ m{/}) {
	return "$cmd" if -x "$cmd";
    } else {
	foreach my $dir (split(/:/, $ENV{PATH})) {
	    return "$dir/$cmd" if -x "$dir/$cmd";
	}
    }
    return;
}

=item $control_file = get_control_path($pkg, $filetype)

Return the path of the control file of type $filetype for the given
package.

=item @control_files = get_control_path($pkg)

Return the path of all available control files for the given package.

=cut

sub get_control_path($;$) {
    my ($pkg, $filetype) = @_;
    my $control_file;
    my @exec = ('dpkg-query', '--control-path', $pkg);
    push @exec, $filetype if defined $filetype;
    spawn(exec => \@exec, wait_child => 1, to_string => \$control_file);
    chomp($control_file);
    if (defined $filetype) {
	return if $control_file eq '';
	return $control_file;
    }
    return () if $control_file eq '';
    return split(/\n/, $control_file);
}

=item $file = find_build_file($basename)

Selects the right variant of the given file: the arch-specific variant
("$basename.$arch") has priority over the OS-specific variant
("$basename.$os") which has priority over the default variant
("$basename"). If none of the files exists, then it returns undef.

=item @files = find_build_file($basename)

Return the available variants of the given file. Returns an empty
list if none of the files exists.

=cut

sub find_build_file($) {
    my $base = shift;
    my $host_arch = get_host_arch();
    my ($abi, $libc, $host_os, $cpu) = debarch_to_debtuple($host_arch);
    my @files;
    foreach my $f ("$base.$host_arch", "$base.$host_os", "$base") {
        push @files, $f if -f $f;
    }
    return @files if wantarray;
    return $files[0] if scalar @files;
    return;
}

=back

=head1 CHANGES

=head2 Version 1.04 (dpkg 1.17.11)

Update semantics: find_command() now handles an empty or undef argument.

=head2 Version 1.03 (dpkg 1.16.1)

New function: find_build_file()

=head2 Version 1.02 (dpkg 1.16.0)

New function: get_control_path()

=head2 Version 1.01 (dpkg 1.15.8)

New function: find_command()

=head2 Version 1.00 (dpkg 1.15.6)

Mark the module as public.

=cut

1;
