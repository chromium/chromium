# Copyright © 2007-2009 Raphaël Hertzog <hertzog@debian.org>
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

package Dpkg::Control::Hash;

use strict;
use warnings;

our $VERSION = '1.00';

use Dpkg::Gettext;
use Dpkg::ErrorHandling;
use Dpkg::Control::Fields; # Force execution of vendor hook.

use parent qw(Dpkg::Control::HashCore);

=encoding utf8

=head1 NAME

Dpkg::Control::Hash - parse and manipulate a block of RFC822-like fields

=head1 DESCRIPTION

This module is just like Dpkg::Control::HashCore, with vendor-specific
field knowledge.

=head1 CHANGES

=head2 Version 1.00 (dpkg 1.15.6)

Mark the module as public.

=cut

1;
