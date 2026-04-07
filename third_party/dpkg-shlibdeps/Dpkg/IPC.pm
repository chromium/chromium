# Copyright © 2008-2009 Raphaël Hertzog <hertzog@debian.org>
# Copyright © 2008 Frank Lichtenheld <djpig@debian.org>
# Copyright © 2008-2010, 2012-2015 Guillem Jover <guillem@debian.org>
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

package Dpkg::IPC;

use strict;
use warnings;

our $VERSION = '1.02';
our @EXPORT = qw(
    spawn
    wait_child
);

use Carp;
use Exporter qw(import);

use Dpkg::ErrorHandling;
use Dpkg::Gettext;

=encoding utf8

=head1 NAME

Dpkg::IPC - helper functions for IPC

=head1 DESCRIPTION

Dpkg::IPC offers helper functions to allow you to execute
other programs in an easy, yet flexible way, while hiding
all the gory details of IPC (Inter-Process Communication)
from you.

=head1 FUNCTIONS

=over 4

=item $pid = spawn(%opts)

Creates a child process and executes another program in it.
The arguments are interpreted as a hash of options, specifying
how to handle the in and output of the program to execute.
Returns the pid of the child process (unless the wait_child
option was given).

Any error will cause the function to exit with one of the
Dpkg::ErrorHandling functions.

Options:

=over 4

=item exec

Can be either a scalar, i.e. the name of the program to be
executed, or an array reference, i.e. the name of the program
plus additional arguments. Note that the program will never be
executed via the shell, so you can't specify additional arguments
in the scalar string and you can't use any shell facilities like
globbing.

Mandatory Option.

=item from_file, to_file, error_to_file

Filename as scalar. Standard input/output/error of the
child process will be redirected to the file specified.

=item from_handle, to_handle, error_to_handle

Filehandle. Standard input/output/error of the child process will be
dup'ed from the handle.

=item from_pipe, to_pipe, error_to_pipe

Scalar reference or object based on IO::Handle. A pipe will be opened for
each of the two options and either the reading (C<to_pipe> and
C<error_to_pipe>) or the writing end (C<from_pipe>) will be returned in
the referenced scalar. Standard input/output/error of the child process
will be dup'ed to the other ends of the pipes.

=item from_string, to_string, error_to_string

Scalar reference. Standard input/output/error of the child
process will be redirected to the string given as reference. Note
that it wouldn't be strictly necessary to use a scalar reference
for C<from_string>, as the string is not modified in any way. This was
chosen only for reasons of symmetry with C<to_string> and
C<error_to_string>. C<to_string> and C<error_to_string> imply the
C<wait_child> option.

=item wait_child

Scalar. If containing a true value, wait_child() will be called before
returning. The return value of spawn() will be a true value, not the pid.

=item nocheck

Scalar. Option of the wait_child() call.

=item timeout

Scalar. Option of the wait_child() call.

=item chdir

Scalar. The child process will chdir in the indicated directory before
calling exec.

=item env

Hash reference. The child process will populate %ENV with the items of the
hash before calling exec. This allows exporting environment variables.

=item delete_env

Array reference. The child process will remove all environment variables
listed in the array before calling exec.

=item sig

Hash reference. The child process will populate %SIG with the items of the
hash before calling exec. This allows setting signal dispositions.

=item delete_sig

Array reference. The child process will reset all signals listed in the
array to their default dispositions before calling exec.

=back

=cut

sub _sanity_check_opts {
    my (%opts) = @_;

    croak 'exec parameter is mandatory in spawn()'
	unless $opts{exec};

    my $to = my $error_to = my $from = 0;
    foreach my $thing (qw(file handle string pipe)) {
	$to++ if $opts{"to_$thing"};
	$error_to++ if $opts{"error_to_$thing"};
	$from++ if $opts{"from_$thing"};
    }
    croak 'not more than one of to_* parameters is allowed'
	if $to > 1;
    croak 'not more than one of error_to_* parameters is allowed'
	if $error_to > 1;
    croak 'not more than one of from_* parameters is allowed'
	if $from > 1;

    foreach my $param (qw(to_string error_to_string from_string)) {
	if (exists $opts{$param} and
	    (not ref $opts{$param} or ref $opts{$param} ne 'SCALAR')) {
	    croak "parameter $param must be a scalar reference";
	}
    }

    foreach my $param (qw(to_pipe error_to_pipe from_pipe)) {
	if (exists $opts{$param} and
	    (not ref $opts{$param} or (ref $opts{$param} ne 'SCALAR' and
	     not $opts{$param}->isa('IO::Handle')))) {
	    croak "parameter $param must be a scalar reference or " .
	          'an IO::Handle object';
	}
    }

    if (exists $opts{timeout} and defined($opts{timeout}) and
        $opts{timeout} !~ /^\d+$/) {
	croak 'parameter timeout must be an integer';
    }

    if (exists $opts{env} and ref($opts{env}) ne 'HASH') {
	croak 'parameter env must be a hash reference';
    }

    if (exists $opts{delete_env} and ref($opts{delete_env}) ne 'ARRAY') {
	croak 'parameter delete_env must be an array reference';
    }

    if (exists $opts{sig} and ref($opts{sig}) ne 'HASH') {
	croak 'parameter sig must be a hash reference';
    }

    if (exists $opts{delete_sig} and ref($opts{delete_sig}) ne 'ARRAY') {
	croak 'parameter delete_sig must be an array reference';
    }

    return %opts;
}

sub spawn {
    my (%opts) = @_;
    my @prog;

    _sanity_check_opts(%opts);
    $opts{close_in_child} //= [];
    if (ref($opts{exec}) =~ /ARRAY/) {
	push @prog, @{$opts{exec}};
    } elsif (not ref($opts{exec})) {
	push @prog, $opts{exec};
    } else {
	croak 'invalid exec parameter in spawn()';
    }
    my ($from_string_pipe, $to_string_pipe, $error_to_string_pipe);
    if ($opts{to_string}) {
	$opts{to_pipe} = \$to_string_pipe;
	$opts{wait_child} = 1;
    }
    if ($opts{error_to_string}) {
	$opts{error_to_pipe} = \$error_to_string_pipe;
	$opts{wait_child} = 1;
    }
    if ($opts{from_string}) {
	$opts{from_pipe} = \$from_string_pipe;
    }
    # Create pipes if needed
    my ($input_pipe, $output_pipe, $error_pipe);
    if ($opts{from_pipe}) {
	pipe($opts{from_handle}, $input_pipe)
	    or syserr(g_('pipe for %s'), "@prog");
	${$opts{from_pipe}} = $input_pipe;
	push @{$opts{close_in_child}}, $input_pipe;
    }
    if ($opts{to_pipe}) {
	pipe($output_pipe, $opts{to_handle})
	    or syserr(g_('pipe for %s'), "@prog");
	${$opts{to_pipe}} = $output_pipe;
	push @{$opts{close_in_child}}, $output_pipe;
    }
    if ($opts{error_to_pipe}) {
	pipe($error_pipe, $opts{error_to_handle})
	    or syserr(g_('pipe for %s'), "@prog");
	${$opts{error_to_pipe}} = $error_pipe;
	push @{$opts{close_in_child}}, $error_pipe;
    }
    # Fork and exec
    my $pid = fork();
    syserr(g_('cannot fork for %s'), "@prog") unless defined $pid;
    if (not $pid) {
	# Define environment variables
	if ($opts{env}) {
	    foreach (keys %{$opts{env}}) {
		$ENV{$_} = $opts{env}{$_};
	    }
	}
	if ($opts{delete_env}) {
	    delete $ENV{$_} foreach (@{$opts{delete_env}});
	}
	# Define signal dispositions.
	if ($opts{sig}) {
	    foreach (keys %{$opts{sig}}) {
		$SIG{$_} = $opts{sig}{$_};
	    }
	}
	if ($opts{delete_sig}) {
	    delete $SIG{$_} foreach (@{$opts{delete_sig}});
	}
	# Change the current directory
	if ($opts{chdir}) {
	    chdir($opts{chdir}) or syserr(g_('chdir to %s'), $opts{chdir});
	}
	# Redirect STDIN if needed
	if ($opts{from_file}) {
	    open(STDIN, '<', $opts{from_file})
	        or syserr(g_('cannot open %s'), $opts{from_file});
	} elsif ($opts{from_handle}) {
	    open(STDIN, '<&', $opts{from_handle})
		or syserr(g_('reopen stdin'));
	    # has been duped, can be closed
	    push @{$opts{close_in_child}}, $opts{from_handle};
	}
	# Redirect STDOUT if needed
	if ($opts{to_file}) {
	    open(STDOUT, '>', $opts{to_file})
	        or syserr(g_('cannot write %s'), $opts{to_file});
	} elsif ($opts{to_handle}) {
	    open(STDOUT, '>&', $opts{to_handle})
		or syserr(g_('reopen stdout'));
	    # has been duped, can be closed
	    push @{$opts{close_in_child}}, $opts{to_handle};
	}
	# Redirect STDERR if needed
	if ($opts{error_to_file}) {
	    open(STDERR, '>', $opts{error_to_file})
	        or syserr(g_('cannot write %s'), $opts{error_to_file});
	} elsif ($opts{error_to_handle}) {
	    open(STDERR, '>&', $opts{error_to_handle})
	        or syserr(g_('reopen stdout'));
	    # has been duped, can be closed
	    push @{$opts{close_in_child}}, $opts{error_to_handle};
	}
	# Close some inherited filehandles
	close($_) foreach (@{$opts{close_in_child}});
	# Execute the program
	exec({ $prog[0] } @prog) or syserr(g_('unable to execute %s'), "@prog");
    }
    # Close handle that we can't use any more
    close($opts{from_handle}) if exists $opts{from_handle};
    close($opts{to_handle}) if exists $opts{to_handle};
    close($opts{error_to_handle}) if exists $opts{error_to_handle};

    if ($opts{from_string}) {
	print { $from_string_pipe } ${$opts{from_string}};
	close($from_string_pipe);
    }
    if ($opts{to_string}) {
	local $/ = undef;
	${$opts{to_string}} = readline($to_string_pipe);
    }
    if ($opts{error_to_string}) {
	local $/ = undef;
	${$opts{error_to_string}} = readline($error_to_string_pipe);
    }
    if ($opts{wait_child}) {
	my $cmdline = "@prog";
	if ($opts{env}) {
	    foreach (keys %{$opts{env}}) {
		$cmdline = "$_=\"" . $opts{env}{$_} . "\" $cmdline";
	    }
	}
	wait_child($pid, nocheck => $opts{nocheck},
                   timeout => $opts{timeout}, cmdline => $cmdline);
	return 1;
    }

    return $pid;
}


=item wait_child($pid, %opts)

Takes as first argument the pid of the process to wait for.
Remaining arguments are taken as a hash of options. Returns
nothing. Fails if the child has been ended by a signal or
if it exited non-zero.

Options:

=over 4

=item cmdline

String to identify the child process in error messages.
Defaults to "child process".

=item nocheck

If true do not check the return status of the child (and thus
do not fail it has been killed or if it exited with a
non-zero return code).

=item timeout

Set a maximum time to wait for the process, after that kill the process and
fail with an error message.

=back

=cut

sub wait_child {
    my ($pid, %opts) = @_;
    $opts{cmdline} //= g_('child process');
    croak 'no PID set, cannot wait end of process' unless $pid;
    eval {
        local $SIG{ALRM} = sub { die "alarm\n" };
        alarm($opts{timeout}) if defined($opts{timeout});
        $pid == waitpid($pid, 0) or syserr(g_('wait for %s'), $opts{cmdline});
        alarm(0) if defined($opts{timeout});
    };
    if ($@) {
        die $@ unless $@ eq "alarm\n";
        kill 'TERM', $pid;
        error(P_("%s didn't complete in %d second",
                 "%s didn't complete in %d seconds",
                 $opts{timeout}),
              $opts{cmdline}, $opts{timeout});
    }
    unless ($opts{nocheck}) {
	subprocerr($opts{cmdline}) if $?;
    }
}

1;
__END__

=back

=head1 CHANGES

=head2 Version 1.02 (dpkg 1.18.0)

Change options: wait_child() now kills the process when reaching the 'timeout'.

=head2 Version 1.01 (dpkg 1.17.11)

New options: spawn() now accepts 'sig' and 'delete_sig'.

=head2 Version 1.00 (dpkg 1.15.6)

Mark the module as public.

=head1 SEE ALSO

Dpkg, Dpkg::ErrorHandling
