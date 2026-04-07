# Copyright © 2007-2010 Raphaël Hertzog <hertzog@debian.org>
# Copyright © 2009, 2012-2015 Guillem Jover <guillem@debian.org>
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

package Dpkg::Control::Info;

use strict;
use warnings;

our $VERSION = '1.01';

use Dpkg::Control;
use Dpkg::ErrorHandling;
use Dpkg::Gettext;

use parent qw(Dpkg::Interface::Storable);

use overload
    '@{}' => sub { return [ $_[0]->{source}, @{$_[0]->{packages}} ] };

=encoding utf8

=head1 NAME

Dpkg::Control::Info - parse files like debian/control

=head1 DESCRIPTION

It provides a class to access data of files that follow the same
syntax as F<debian/control>.

=head1 METHODS

=over 4

=item $c = Dpkg::Control::Info->new(%opts)

Create a new Dpkg::Control::Info object. Loads the file from the filename
option, if no option is specified filename defaults to F<debian/control>.
If a scalar is passed instead, it will be used as the filename. If filename
is "-", it parses the standard input. If filename is undef no loading will
be performed.

=cut

sub new {
    my ($this, @args) = @_;
    my $class = ref($this) || $this;
    my $self = {
	source => undef,
	packages => [],
    };
    bless $self, $class;

    my %opts;
    if (scalar @args == 0) {
        $opts{filename} = 'debian/control';
    } elsif (scalar @args == 1) {
        $opts{filename} = $args[0];
    } else {
        %opts = @args;
    }

    $self->load($opts{filename}) if $opts{filename};

    return $self;
}

=item $c->reset()

Resets what got read.

=cut

sub reset {
    my $self = shift;
    $self->{source} = undef;
    $self->{packages} = [];
}

=item $c->parse($fh, $description)

Parse a control file from the given filehandle. Exits in case of errors.
$description is used to describe the filehandle, ideally it's a filename
or a description of where the data comes from. It is used in error messages.
The data in the object is reset before parsing new control files.

=cut

sub parse {
    my ($self, $fh, $desc) = @_;
    $self->reset();
    my $cdata = Dpkg::Control->new(type => CTRL_INFO_SRC);
    return if not $cdata->parse($fh, $desc);
    $self->{source} = $cdata;
    unless (exists $cdata->{Source}) {
	$cdata->parse_error($desc, g_('first block lacks a Source field'));
    }
    while (1) {
	$cdata = Dpkg::Control->new(type => CTRL_INFO_PKG);
        last if not $cdata->parse($fh, $desc);
	push @{$self->{packages}}, $cdata;
	unless (exists $cdata->{Package}) {
	    $cdata->parse_error($desc, g_("block lacks the '%s' field"),
	                        'Package');
	}
	unless (exists $cdata->{Architecture}) {
	    $cdata->parse_error($desc, g_("block lacks the '%s' field"),
	                        'Architecture');
	}

    }
}

=item $c->load($file)

Load the content of $file. Exits in case of errors. If file is "-", it
loads from the standard input.

=item $c->[0]

=item $c->get_source()

Returns a Dpkg::Control object containing the fields concerning the
source package.

=cut

sub get_source {
    my $self = shift;
    return $self->{source};
}

=item $c->get_pkg_by_idx($idx)

Returns a Dpkg::Control object containing the fields concerning the binary
package numbered $idx (starting at 1).

=cut

sub get_pkg_by_idx {
    my ($self, $idx) = @_;
    return $self->{packages}[--$idx];
}

=item $c->get_pkg_by_name($name)

Returns a Dpkg::Control object containing the fields concerning the binary
package named $name.

=cut

sub get_pkg_by_name {
    my ($self, $name) = @_;
    foreach my $pkg (@{$self->{packages}}) {
	return $pkg if ($pkg->{Package} eq $name);
    }
    return;
}


=item $c->get_packages()

Returns a list containing the Dpkg::Control objects for all binary packages.

=cut

sub get_packages {
    my $self = shift;
    return @{$self->{packages}};
}

=item $str = $c->output([$fh])

Return the content info into a string. If $fh is specified print it into
the filehandle.

=cut

sub output {
    my ($self, $fh) = @_;
    my $str;
    $str .= $self->{source}->output($fh);
    foreach my $pkg (@{$self->{packages}}) {
	print { $fh } "\n" if defined $fh;
	$str .= "\n" . $pkg->output($fh);
    }
    return $str;
}

=item "$c"

Return a string representation of the content.

=item @{$c}

Return a list of Dpkg::Control objects, the first one is corresponding to
source information and the following ones are the binary packages
information.

=back

=head1 CHANGES

=head2 Version 1.01 (dpkg 1.18.0)

New argument: The $c->new() constructor accepts an %opts argument.

=head2 Version 1.00 (dpkg 1.15.6)

Mark the module as public.

=cut

1;
