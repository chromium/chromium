# Copyright © 2009-2010 Modestas Vainius <modax@debian.org>
# Copyright © 2010, 2012-2015 Guillem Jover <guillem@debian.org>
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

package Dpkg::Shlibs::Cppfilt;

use strict;
use warnings;

our $VERSION = '0.01';
our @EXPORT = qw(
    cppfilt_demangle_cpp
);
our @EXPORT_OK = qw(
    cppfilt_demangle
);

use Exporter qw(import);

use Dpkg::ErrorHandling;
use Dpkg::IPC;

# A hash of 'objects' referring to preforked c++filt processes for the distinct
# demangling types.
my %cppfilts;

sub get_cppfilt {
    my $type = shift || 'auto';

    # Fork c++filt process for demangling $type unless it is forked already.
    # Keeping c++filt running improves performance a lot.
    my $filt;
    if (exists $cppfilts{$type}) {
	$filt = $cppfilts{$type};
    } else {
	$filt = { from => undef, to => undef,
	            last_symbol => '', last_result => '' };
	$filt->{pid} = spawn(exec => [ 'c++filt', "--format=$type" ],
	                     from_pipe => \$filt->{from},
	                     to_pipe => \$filt->{to});
	syserr(g_('unable to execute %s'), 'c++filt')
	    unless defined $filt->{from};
	$filt->{from}->autoflush(1);

	$cppfilts{$type} = $filt;
    }
    return $filt;
}

# Demangle the given $symbol using demangler for the specified $type (defaults
# to 'auto') . Extraneous characters trailing after a mangled name are kept
# intact. If neither whole $symbol nor portion of it could be demangled, undef
# is returned.
sub cppfilt_demangle {
    my ($symbol, $type) = @_;

    # Start or get c++filt 'object' for the requested type.
    my $filt = get_cppfilt($type);

    # Remember the last result. Such a local optimization is cheap and useful
    # when sequential pattern matching is performed.
    if ($filt->{last_symbol} ne $symbol) {
	# This write/read operation should not deadlock because c++filt flushes
	# output buffer on LF or each invalid character.
	print { $filt->{from} } $symbol, "\n";
	my $demangled = readline($filt->{to});
	chop $demangled;

	# If the symbol was not demangled, return undef
	$demangled = undef if $symbol eq $demangled;

	# Remember the last result
	$filt->{last_symbol} = $symbol;
	$filt->{last_result} = $demangled;
    }
    return $filt->{last_result};
}

sub cppfilt_demangle_cpp {
    my $symbol = shift;
    return cppfilt_demangle($symbol, 'auto');
}

sub terminate_cppfilts {
    foreach my $type (keys %cppfilts) {
	next if not defined $cppfilts{$type}{pid};
	close $cppfilts{$type}{from};
	close $cppfilts{$type}{to};
	wait_child($cppfilts{$type}{pid}, cmdline => 'c++filt',
	                                  nocheck => 1,
	                                  timeout => 5);
	delete $cppfilts{$type};
    }
}

# Close/terminate running c++filt process(es)
END {
    # Make sure exitcode is not changed (by wait_child)
    my $exitcode = $?;
    terminate_cppfilts();
    $? = $exitcode;
}

1;
