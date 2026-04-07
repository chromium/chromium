# Copyright © 2006-2009, 2012-2015 Guillem Jover <guillem@debian.org>
# Copyright © 2007-2010 Raphaël Hertzog <hertzog@debian.org>
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

package Dpkg::Substvars;

use strict;
use warnings;

our $VERSION = '2.00';

use Dpkg ();
use Dpkg::Arch qw(get_host_arch);
use Dpkg::Version;
use Dpkg::ErrorHandling;
use Dpkg::Gettext;

use parent qw(Dpkg::Interface::Storable);

my $maxsubsts = 50;

=encoding utf8

=head1 NAME

Dpkg::Substvars - handle variable substitution in strings

=head1 DESCRIPTION

It provides a class which is able to substitute variables in strings.

=cut

use constant {
    SUBSTVAR_ATTR_USED => 1,
    SUBSTVAR_ATTR_AUTO => 2,
    SUBSTVAR_ATTR_AGED => 4,
};

=head1 METHODS

=over 8

=item $s = Dpkg::Substvars->new($file)

Create a new object that can do substitutions. By default it contains
generic substitutions like ${Newline}, ${Space}, ${Tab}, ${dpkg:Version}
and ${dpkg:Upstream-Version}.

Additional substitutions will be read from the $file passed as parameter.

It keeps track of which substitutions were actually used (only counting
substvars(), not get()), and warns about unused substvars when asked to. The
substitutions that are always present are not included in these warnings.

=cut

sub new {
    my ($this, $arg) = @_;
    my $class = ref($this) || $this;
    my $self = {
        vars => {
            'Newline' => "\n",
            'Space' => ' ',
            'Tab' => "\t",
            'dpkg:Version' => $Dpkg::PROGVERSION,
            'dpkg:Upstream-Version' => $Dpkg::PROGVERSION,
            },
        attr => {},
	msg_prefix => '',
    };
    $self->{vars}{'dpkg:Upstream-Version'} =~ s/-[^-]+$//;
    bless $self, $class;

    my $attr = SUBSTVAR_ATTR_USED | SUBSTVAR_ATTR_AUTO;
    $self->{attr}{$_} = $attr foreach keys %{$self->{vars}};
    if ($arg) {
        $self->load($arg) if -e $arg;
    }
    return $self;
}

=item $s->set($key, $value)

Add/replace a substitution.

=cut

sub set {
    my ($self, $key, $value, $attr) = @_;

    $attr //= 0;

    $self->{vars}{$key} = $value;
    $self->{attr}{$key} = $attr;
}

=item $s->set_as_used($key, $value)

Add/replace a substitution and mark it as used (no warnings will be produced
even if unused).

=cut

sub set_as_used {
    my ($self, $key, $value) = @_;

    $self->set($key, $value, SUBSTVAR_ATTR_USED);
}

=item $s->set_as_auto($key, $value)

Add/replace a substitution and mark it as used and automatic (no warnings
will be produced even if unused).

=cut

sub set_as_auto {
    my ($self, $key, $value) = @_;

    $self->set($key, $value, SUBSTVAR_ATTR_USED | SUBSTVAR_ATTR_AUTO);
}

=item $s->get($key)

Get the value of a given substitution.

=cut

sub get {
    my ($self, $key) = @_;
    return $self->{vars}{$key};
}

=item $s->delete($key)

Remove a given substitution.

=cut

sub delete {
    my ($self, $key) = @_;
    delete $self->{attr}{$key};
    return delete $self->{vars}{$key};
}

=item $s->mark_as_used($key)

Prevents warnings about a unused substitution, for example if it is provided by
default.

=cut

sub mark_as_used {
    my ($self, $key) = @_;
    $self->{attr}{$key} |= SUBSTVAR_ATTR_USED;
}

=item $s->parse($fh, $desc)

Add new substitutions read from the filehandle. $desc is used to identify
the filehandle in error messages.

Returns the number of substitutions that have been parsed with success.

=cut

sub parse {
    my ($self, $fh, $varlistfile) = @_;
    my $count = 0;
    local $_;

    binmode($fh);
    while (<$fh>) {
	next if m/^\s*\#/ || !m/\S/;
	s/\s*\n$//;
	if (! m/^(\w[-:0-9A-Za-z]*)\=(.*)$/) {
	    error(g_('bad line in substvars file %s at line %d'),
		  $varlistfile, $.);
	}
	$self->set($1, $2);
        $count++;
    }

    return $count
}

=item $s->load($file)

Add new substitutions read from $file.

=item $s->set_version_substvars($sourceversion, $binaryversion)

Defines ${binary:Version}, ${source:Version} and
${source:Upstream-Version} based on the given version strings.

These will never be warned about when unused.

=cut

sub set_version_substvars {
    my ($self, $sourceversion, $binaryversion) = @_;

    # Handle old function signature taking only one argument.
    $binaryversion //= $sourceversion;

    # For backwards compatibility on binNMUs that do not use the Binary-Only
    # field on the changelog, always fix up the source version.
    $sourceversion =~ s/\+b[0-9]+$//;

    my $vs = Dpkg::Version->new($sourceversion, check => 1);
    if (not defined $vs) {
        error(g_('invalid source version %s'), $sourceversion);
    }
    my $upstreamversion = $vs->as_string(omit_revision => 1);

    my $attr = SUBSTVAR_ATTR_USED | SUBSTVAR_ATTR_AUTO;

    $self->set('binary:Version', $binaryversion, $attr);
    $self->set('source:Version', $sourceversion, $attr);
    $self->set('source:Upstream-Version', $upstreamversion, $attr);

    # XXX: Source-Version is now obsolete, remove in 1.19.x.
    $self->set('Source-Version', $binaryversion, $attr | SUBSTVAR_ATTR_AGED);
}

=item $s->set_arch_substvars()

Defines architecture variables: ${Arch}.

This will never be warned about when unused.

=cut

sub set_arch_substvars {
    my $self = shift;

    my $attr = SUBSTVAR_ATTR_USED | SUBSTVAR_ATTR_AUTO;

    $self->set('Arch', get_host_arch(), $attr);
}

=item $s->set_desc_substvars()

Defines source description variables: ${source:Synopsis} and
${source:Extended-Description}.

These will never be warned about when unused.

=cut

sub set_desc_substvars {
    my ($self, $desc) = @_;

    my ($synopsis, $extended) = split /\n/, $desc, 2;

    my $attr = SUBSTVAR_ATTR_USED | SUBSTVAR_ATTR_AUTO;

    $self->set('source:Synopsis', $synopsis, $attr);
    $self->set('source:Extended-Description', $extended, $attr);
}

=item $s->set_field_substvars($ctrl, $prefix)

Defines field variables from a Dpkg::Control object, with each variable
having the form "${$prefix:$field}".

They will never be warned about when unused.

=cut

sub set_field_substvars {
    my ($self, $ctrl, $prefix) = @_;

    foreach my $field (keys %{$ctrl}) {
        $self->set_as_auto("$prefix:$field", $ctrl->{$field});
    }
}

=item $newstring = $s->substvars($string)

Substitutes variables in $string and return the result in $newstring.

=cut

sub substvars {
    my ($self, $v, %opts) = @_;
    my $lhs;
    my $vn;
    my $rhs = '';
    my $count = 0;
    $opts{msg_prefix} //= $self->{msg_prefix};
    $opts{no_warn} //= 0;

    while ($v =~ m/^(.*?)\$\{([-:0-9a-z]+)\}(.*)$/si) {
        # If we have consumed more from the leftover data, then
        # reset the recursive counter.
        $count = 0 if (length($3) < length($rhs));

        if ($count >= $maxsubsts) {
            error($opts{msg_prefix} .
                  g_("too many substitutions - recursive ? - in '%s'"), $v);
        }
        $lhs = $1;
        $vn = $2;
        $rhs = $3;
        if (defined($self->{vars}{$vn})) {
            $v = $lhs . $self->{vars}{$vn} . $rhs;
            $self->mark_as_used($vn);
            $count++;

            if ($self->{attr}{$vn} & SUBSTVAR_ATTR_AGED) {
                error($opts{msg_prefix} .
                      g_('obsolete substitution variable ${%s}'), $vn);
            }
        } else {
            warning($opts{msg_prefix} .
                    g_('substitution variable ${%s} used, but is not defined'),
	            $vn) unless $opts{no_warn};
            $v = $lhs . $rhs;
        }
    }
    return $v;
}

=item $s->warn_about_unused()

Issues warning about any variables that were set, but not used.

=cut

sub warn_about_unused {
    my ($self, %opts) = @_;
    $opts{msg_prefix} //= $self->{msg_prefix};

    foreach my $vn (sort keys %{$self->{vars}}) {
        next if $self->{attr}{$vn} & SUBSTVAR_ATTR_USED;
        # Empty substitutions variables are ignored on the basis
        # that they are not required in the current situation
        # (example: debhelper's misc:Depends in many cases)
        next if $self->{vars}{$vn} eq '';
        warning($opts{msg_prefix} .
                g_('substitution variable ${%s} unused, but is defined'),
                $vn);
    }
}

=item $s->set_msg_prefix($prefix)

Define a prefix displayed before all warnings/error messages output
by the module.

=cut

sub set_msg_prefix {
    my ($self, $prefix) = @_;
    $self->{msg_prefix} = $prefix;
}

=item $s->filter(remove => $rmfunc)

=item $s->filter(keep => $keepfun)

Filter the substitution variables, either removing or keeping all those
that return true when $rmfunc->($key) or $keepfunc->($key) is called.

=cut

sub filter {
    my ($self, %opts) = @_;

    my $remove = $opts{remove} // sub { 0 };
    my $keep = $opts{keep} // sub { 1 };

    foreach my $vn (keys %{$self->{vars}}) {
        $self->delete($vn) if $remove->($vn) or not $keep->($vn);
    }
}

=item "$s"

Return a string representation of all substitutions variables except the
automatic ones.

=item $str = $s->output([$fh])

Return all substitutions variables except the automatic ones. If $fh
is passed print them into the filehandle.

=cut

sub output {
    my ($self, $fh) = @_;
    my $str = '';
    # Store all non-automatic substitutions only
    foreach my $vn (sort keys %{$self->{vars}}) {
	next if $self->{attr}{$vn} & SUBSTVAR_ATTR_AUTO;
	my $line = "$vn=" . $self->{vars}{$vn} . "\n";
	print { $fh } $line if defined $fh;
	$str .= $line;
    }
    return $str;
}

=item $s->save($file)

Store all substitutions variables except the automatic ones in the
indicated file.

=back

=head1 CHANGES

=head2 Version 2.00 (dpkg 1.20.0)

Remove method: $s->no_warn().

=head2 Version 1.06 (dpkg 1.19.0)

New method: $s->set_desc_substvars().

=head2 Version 1.05 (dpkg 1.18.11)

Obsolete substvar: Emit an error on Source-Version substvar usage.

New return: $s->parse() now returns the number of parsed substvars.

New method: $s->set_field_substvars().

=head2 Version 1.04 (dpkg 1.18.0)

New method: $s->filter().

=head2 Version 1.03 (dpkg 1.17.11)

New method: $s->set_as_auto().

=head2 Version 1.02 (dpkg 1.16.5)

New argument: Accept a $binaryversion in $s->set_version_substvars(),
passing a single argument is still supported.

New method: $s->mark_as_used().

Deprecated method: $s->no_warn(), use $s->mark_as_used() instead.

=head2 Version 1.01 (dpkg 1.16.4)

New method: $s->set_as_used().

=head2 Version 1.00 (dpkg 1.15.6)

Mark the module as public.

=cut

1;
