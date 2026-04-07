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

package Dpkg::Control::FieldsCore;

use strict;
use warnings;

our $VERSION = '1.00';
our @EXPORT = qw(
    field_capitalize
    field_is_official
    field_is_allowed_in
    field_transfer_single
    field_transfer_all
    field_list_src_dep
    field_list_pkg_dep
    field_get_dep_type
    field_get_sep_type
    field_ordered_list
    field_register
    field_insert_after
    field_insert_before
    FIELD_SEP_UNKNOWN
    FIELD_SEP_SPACE
    FIELD_SEP_COMMA
    FIELD_SEP_LINE
);

use Exporter qw(import);

use Dpkg::Gettext;
use Dpkg::ErrorHandling;
use Dpkg::Control::Types;

use constant {
    ALL_PKG => CTRL_INFO_PKG | CTRL_INDEX_PKG | CTRL_PKG_DEB | CTRL_FILE_STATUS,
    ALL_SRC => CTRL_INFO_SRC | CTRL_INDEX_SRC | CTRL_PKG_SRC,
    ALL_CHANGES => CTRL_FILE_CHANGES | CTRL_CHANGELOG,
    ALL_COPYRIGHT => CTRL_COPYRIGHT_HEADER | CTRL_COPYRIGHT_FILES | CTRL_COPYRIGHT_LICENSE,
};

use constant {
    FIELD_SEP_UNKNOWN => 0,
    FIELD_SEP_SPACE => 1,
    FIELD_SEP_COMMA => 2,
    FIELD_SEP_LINE => 4,
};

# The canonical list of fields

# Note that fields used only in dpkg's available file are not listed
# Deprecated fields of dpkg's status file are also not listed
our %FIELDS = (
    'architecture' => {
        name => 'Architecture',
        allowed => (ALL_PKG | ALL_SRC | CTRL_FILE_BUILDINFO | CTRL_FILE_CHANGES) & (~CTRL_INFO_SRC),
        separator => FIELD_SEP_SPACE,
    },
    'architectures' => {
        name => 'Architectures',
        allowed => CTRL_REPO_RELEASE,
        separator => FIELD_SEP_SPACE,
    },
    'auto-built-package' => {
        name => 'Auto-Built-Package',
        allowed => ALL_PKG & ~CTRL_INFO_PKG,
        separator => FIELD_SEP_SPACE,
    },
    'binary' => {
        name => 'Binary',
        allowed => CTRL_PKG_SRC | CTRL_INDEX_SRC | CTRL_FILE_BUILDINFO | CTRL_FILE_CHANGES,
        # XXX: This field values are separated either by space or comma
        # depending on the context.
        separator => FIELD_SEP_SPACE | FIELD_SEP_COMMA,
    },
    'binary-only' => {
        name => 'Binary-Only',
        allowed => ALL_CHANGES,
    },
    'binary-only-changes' => {
        name => 'Binary-Only-Changes',
        allowed => CTRL_FILE_BUILDINFO,
    },
    'breaks' => {
        name => 'Breaks',
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 7,
    },
    'bugs' => {
        name => 'Bugs',
        allowed => (ALL_PKG | CTRL_INFO_SRC | CTRL_FILE_VENDOR) & (~CTRL_INFO_PKG),
    },
    'build-architecture' => {
        name => 'Build-Architecture',
        allowed => CTRL_FILE_BUILDINFO,
    },
    'build-conflicts' => {
        name => 'Build-Conflicts',
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 4,
    },
    'build-conflicts-arch' => {
        name => 'Build-Conflicts-Arch',
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 5,
    },
    'build-conflicts-indep' => {
        name => 'Build-Conflicts-Indep',
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 6,
    },
    'build-date' => {
        name => 'Build-Date',
        allowed => CTRL_FILE_BUILDINFO,
    },
    'build-depends' => {
        name => 'Build-Depends',
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
        dependency => 'normal',
        dep_order => 1,
    },
    'build-depends-arch' => {
        name => 'Build-Depends-Arch',
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
        dependency => 'normal',
        dep_order => 2,
    },
    'build-depends-indep' => {
        name => 'Build-Depends-Indep',
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
        dependency => 'normal',
        dep_order => 3,
    },
    'build-essential' => {
        name => 'Build-Essential',
        allowed => ALL_PKG,
    },
    'build-kernel-version' => {
        name => 'Build-Kernel-Version',
        allowed => CTRL_FILE_BUILDINFO,
    },
    'build-origin' => {
        name => 'Build-Origin',
        allowed => CTRL_FILE_BUILDINFO,
    },
    'build-path' => {
        name => 'Build-Path',
        allowed => CTRL_FILE_BUILDINFO,
    },
    'build-profiles' => {
        name => 'Build-Profiles',
        allowed => CTRL_INFO_PKG,
        separator => FIELD_SEP_SPACE,
    },
    'build-tainted-by' => {
        name => 'Build-Tainted-By',
        allowed => CTRL_FILE_BUILDINFO,
        separator => FIELD_SEP_SPACE,
    },
    'built-for-profiles' => {
        name => 'Built-For-Profiles',
        allowed => ALL_PKG | CTRL_FILE_CHANGES,
        separator => FIELD_SEP_SPACE,
    },
    'built-using' => {
        name => 'Built-Using',
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 10,
    },
    'changed-by' => {
        name => 'Changed-By',
        allowed => CTRL_FILE_CHANGES,
    },
    'changelogs' => {
        name => 'Changelogs',
        allowed => CTRL_REPO_RELEASE,
    },
    'changes' => {
        name => 'Changes',
        allowed => ALL_CHANGES,
    },
    'checksums-md5' => {
        name => 'Checksums-Md5',
        allowed => CTRL_PKG_SRC | CTRL_INDEX_SRC | CTRL_FILE_CHANGES | CTRL_FILE_BUILDINFO,
    },
    'checksums-sha1' => {
        name => 'Checksums-Sha1',
        allowed => CTRL_PKG_SRC | CTRL_INDEX_SRC | CTRL_FILE_CHANGES | CTRL_FILE_BUILDINFO,
    },
    'checksums-sha256' => {
        name => 'Checksums-Sha256',
        allowed => CTRL_PKG_SRC | CTRL_INDEX_SRC | CTRL_FILE_CHANGES | CTRL_FILE_BUILDINFO,
    },
    'classes' => {
        name => 'Classes',
        allowed => CTRL_TESTS,
        separator => FIELD_SEP_COMMA,
    },
    'closes' => {
        name => 'Closes',
        allowed => ALL_CHANGES,
        separator => FIELD_SEP_SPACE,
    },
    'codename' => {
        name => 'Codename',
        allowed => CTRL_REPO_RELEASE,
    },
    'comment' => {
        name => 'Comment',
        allowed => ALL_COPYRIGHT,
    },
    'components' => {
        name => 'Components',
        allowed => CTRL_REPO_RELEASE,
        separator => FIELD_SEP_SPACE,
    },
    'conffiles' => {
        name => 'Conffiles',
        allowed => CTRL_FILE_STATUS,
        separator => FIELD_SEP_LINE | FIELD_SEP_SPACE,
    },
    'config-version' => {
        name => 'Config-Version',
        allowed => CTRL_FILE_STATUS,
    },
    'conflicts' => {
        name => 'Conflicts',
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 6,
    },
    'copyright' => {
        name => 'Copyright',
        allowed => CTRL_COPYRIGHT_HEADER | CTRL_COPYRIGHT_FILES,
    },
    'date' => {
        name => 'Date',
        allowed => ALL_CHANGES | CTRL_REPO_RELEASE,
    },
    'depends' => {
        name => 'Depends',
        allowed => ALL_PKG | CTRL_TESTS,
        separator => FIELD_SEP_COMMA,
        dependency => 'normal',
        dep_order => 2,
    },
    'description' => {
        name => 'Description',
        allowed => ALL_SRC | ALL_PKG | CTRL_FILE_CHANGES | CTRL_REPO_RELEASE,
    },
    'disclaimer' => {
        name => 'Disclaimer',
        allowed => CTRL_COPYRIGHT_HEADER,
    },
    'directory' => {
        name => 'Directory',
        allowed => CTRL_INDEX_SRC,
    },
    'distribution' => {
        name => 'Distribution',
        allowed => ALL_CHANGES,
    },
    'enhances' => {
        name => 'Enhances',
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 5,
    },
    'environment' => {
        name => 'Environment',
        allowed => CTRL_FILE_BUILDINFO,
        separator => FIELD_SEP_LINE,
    },
    'essential' => {
        name => 'Essential',
        allowed => ALL_PKG,
    },
    'features' => {
        name => 'Features',
        allowed => CTRL_TESTS,
        separator => FIELD_SEP_SPACE,
    },
    'filename' => {
        name => 'Filename',
        allowed => CTRL_INDEX_PKG,
        separator => FIELD_SEP_LINE | FIELD_SEP_SPACE,
    },
    'files' => {
        name => 'Files',
        allowed => CTRL_PKG_SRC | CTRL_INDEX_SRC | CTRL_FILE_CHANGES | CTRL_COPYRIGHT_FILES,
        separator => FIELD_SEP_LINE | FIELD_SEP_SPACE,
    },
    'format' => {
        name => 'Format',
        allowed => CTRL_PKG_SRC | CTRL_INDEX_SRC | CTRL_FILE_CHANGES | CTRL_COPYRIGHT_HEADER | CTRL_FILE_BUILDINFO,
    },
    'homepage' => {
        name => 'Homepage',
        allowed => ALL_SRC | ALL_PKG,
    },
    'installed-build-depends' => {
        name => 'Installed-Build-Depends',
        allowed => CTRL_FILE_BUILDINFO,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 11,
    },
    'installed-size' => {
        name => 'Installed-Size',
        allowed => ALL_PKG & ~CTRL_INFO_PKG,
    },
    'installer-menu-item' => {
        name => 'Installer-Menu-Item',
        allowed => ALL_PKG,
    },
    'kernel-version' => {
        name => 'Kernel-Version',
        allowed => ALL_PKG,
    },
    'label' => {
        name => 'Label',
        allowed => CTRL_REPO_RELEASE,
    },
    'license' => {
        name => 'License',
        allowed => ALL_COPYRIGHT,
    },
    'origin' => {
        name => 'Origin',
        allowed => (ALL_PKG | ALL_SRC | CTRL_REPO_RELEASE) & (~CTRL_INFO_PKG),
    },
    'maintainer' => {
        name => 'Maintainer',
        allowed => CTRL_PKG_DEB| CTRL_INDEX_PKG | CTRL_FILE_STATUS | ALL_SRC  | ALL_CHANGES,
    },
    'md5sum' => {
        # XXX: Wrong capitalization due to historical reasons.
        name => 'MD5sum',
        allowed => CTRL_INDEX_PKG | CTRL_REPO_RELEASE,
        separator => FIELD_SEP_LINE | FIELD_SEP_SPACE,
    },
    'multi-arch' => {
        name => 'Multi-Arch',
        allowed => ALL_PKG,
    },
    'package' => {
        name => 'Package',
        allowed => ALL_PKG | CTRL_INDEX_SRC,
    },
    'package-list' => {
        name => 'Package-List',
        allowed => ALL_SRC & ~CTRL_INFO_SRC,
        separator => FIELD_SEP_LINE | FIELD_SEP_SPACE,
    },
    'package-type' => {
        name => 'Package-Type',
        allowed => ALL_PKG,
    },
    'parent' => {
        name => 'Parent',
        allowed => CTRL_FILE_VENDOR,
    },
    'pre-depends' => {
        name => 'Pre-Depends',
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'normal',
        dep_order => 1,
    },
    'priority' => {
        name => 'Priority',
        allowed => CTRL_INFO_SRC | CTRL_INDEX_SRC | ALL_PKG,
    },
    'provides' => {
        name => 'Provides',
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 9,
    },
    'recommends' => {
        name => 'Recommends',
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'normal',
        dep_order => 3,
    },
    'replaces' => {
        name => 'Replaces',
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 8,
    },
    'restrictions' => {
        name => 'Restrictions',
        allowed => CTRL_TESTS,
        separator => FIELD_SEP_SPACE,
    },
    'rules-requires-root' => {
        name => 'Rules-Requires-Root',
        allowed => CTRL_INFO_SRC,
        separator => FIELD_SEP_SPACE,
    },
    'section' => {
        name => 'Section',
        allowed => CTRL_INFO_SRC | CTRL_INDEX_SRC | ALL_PKG,
    },
    'sha1' => {
        # XXX: Wrong capitalization due to historical reasons.
        name => 'SHA1',
        allowed => CTRL_INDEX_PKG | CTRL_REPO_RELEASE,
        separator => FIELD_SEP_LINE | FIELD_SEP_SPACE,
    },
    'sha256' => {
        # XXX: Wrong capitalization due to historical reasons.
        name => 'SHA256',
        allowed => CTRL_INDEX_PKG | CTRL_REPO_RELEASE,
        separator => FIELD_SEP_LINE | FIELD_SEP_SPACE,
    },
    'size' => {
        name => 'Size',
        allowed => CTRL_INDEX_PKG,
        separator => FIELD_SEP_LINE | FIELD_SEP_SPACE,
    },
    'source' => {
        name => 'Source',
        allowed => (ALL_PKG | ALL_SRC | ALL_CHANGES | CTRL_COPYRIGHT_HEADER | CTRL_FILE_BUILDINFO) &
                   (~(CTRL_INDEX_SRC | CTRL_INFO_PKG)),
    },
    'standards-version' => {
        name => 'Standards-Version',
        allowed => ALL_SRC,
    },
    'status' => {
        name => 'Status',
        allowed => CTRL_FILE_STATUS,
        separator => FIELD_SEP_SPACE,
    },
    'subarchitecture' => {
        name => 'Subarchitecture',
        allowed => ALL_PKG,
    },
    'suite' => {
        name => 'Suite',
        allowed => CTRL_REPO_RELEASE,
    },
    'suggests' => {
        name => 'Suggests',
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'normal',
        dep_order => 4,
    },
    'tag' => {
        name => 'Tag',
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
    },
    'task' => {
        name => 'Task',
        allowed => ALL_PKG,
    },
    'test-command' => {
        name => 'Test-Command',
        allowed => CTRL_TESTS,
    },
    'tests' => {
        name => 'Tests',
        allowed => CTRL_TESTS,
        separator => FIELD_SEP_SPACE,
    },
    'tests-directory' => {
        name => 'Tests-Directory',
        allowed => CTRL_TESTS,
    },
    'testsuite' => {
        name => 'Testsuite',
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
    },
    'testsuite-triggers' => {
        name => 'Testsuite-Triggers',
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
    },
    'timestamp' => {
        name => 'Timestamp',
        allowed => CTRL_CHANGELOG,
    },
    'triggers-awaited' => {
        name => 'Triggers-Awaited',
        allowed => CTRL_FILE_STATUS,
        separator => FIELD_SEP_SPACE,
    },
    'triggers-pending' => {
        name => 'Triggers-Pending',
        allowed => CTRL_FILE_STATUS,
        separator => FIELD_SEP_SPACE,
    },
    'uploaders' => {
        name => 'Uploaders',
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
    },
    'upstream-name' => {
        name => 'Upstream-Name',
        allowed => CTRL_COPYRIGHT_HEADER,
    },
    'upstream-contact' => {
        name => 'Upstream-Contact',
        allowed => CTRL_COPYRIGHT_HEADER,
    },
    'urgency' => {
        name => 'Urgency',
        allowed => ALL_CHANGES,
    },
    'valid-until' => {
        name => 'Valid-Until',
        allowed => CTRL_REPO_RELEASE,
    },
    'vcs-browser' => {
        name => 'Vcs-Browser',
        allowed => ALL_SRC,
    },
    'vcs-arch' => {
        name => 'Vcs-Arch',
        allowed => ALL_SRC,
    },
    'vcs-bzr' => {
        name => 'Vcs-Bzr',
        allowed => ALL_SRC,
    },
    'vcs-cvs' => {
        name => 'Vcs-Cvs',
        allowed => ALL_SRC,
    },
    'vcs-darcs' => {
        name => 'Vcs-Darcs',
        allowed => ALL_SRC,
    },
    'vcs-git' => {
        name => 'Vcs-Git',
        allowed => ALL_SRC,
    },
    'vcs-hg' => {
        name => 'Vcs-Hg',
        allowed => ALL_SRC,
    },
    'vcs-mtn' => {
        name => 'Vcs-Mtn',
        allowed => ALL_SRC,
    },
    'vcs-svn' => {
        name => 'Vcs-Svn',
        allowed => ALL_SRC,
    },
    'vendor' => {
        name => 'Vendor',
        allowed => CTRL_FILE_VENDOR,
    },
    'vendor-url' => {
        name => 'Vendor-Url',
        allowed => CTRL_FILE_VENDOR,
    },
    'version' => {
        name => 'Version',
        allowed => (ALL_PKG | ALL_SRC | CTRL_FILE_BUILDINFO | ALL_CHANGES) &
                    (~(CTRL_INFO_SRC | CTRL_INFO_PKG)),
    },
);

my @src_dep_fields = qw(build-depends build-depends-arch build-depends-indep
    build-conflicts build-conflicts-arch build-conflicts-indep);
my @bin_dep_fields = qw(pre-depends depends recommends suggests enhances
    conflicts breaks replaces provides built-using);
my @src_checksums_fields = qw(checksums-md5 checksums-sha1 checksums-sha256);
my @bin_checksums_fields = qw(md5sum sha1 sha256);

our %FIELD_ORDER = (
    CTRL_PKG_DEB() => [
        qw(package package-type source version built-using kernel-version
        built-for-profiles auto-built-package architecture subarchitecture
        installer-menu-item build-essential essential origin bugs
        maintainer installed-size), @bin_dep_fields,
        qw(section priority multi-arch homepage description tag task)
    ],
    CTRL_INDEX_PKG() => [
        qw(package package-type source version built-using kernel-version
        built-for-profiles auto-built-package architecture subarchitecture
        installer-menu-item build-essential essential origin bugs
        maintainer installed-size), @bin_dep_fields,
        qw(filename size), @bin_checksums_fields,
        qw(section priority multi-arch homepage description tag task)
    ],
    CTRL_PKG_SRC() => [
        qw(format source binary architecture version origin maintainer
        uploaders homepage description standards-version vcs-browser
        vcs-arch vcs-bzr vcs-cvs vcs-darcs vcs-git vcs-hg vcs-mtn
        vcs-svn testsuite testsuite-triggers), @src_dep_fields,
        qw(package-list), @src_checksums_fields, qw(files)
    ],
    CTRL_INDEX_SRC() => [
        qw(format package binary architecture version priority section origin
        maintainer uploaders homepage description standards-version vcs-browser
        vcs-arch vcs-bzr vcs-cvs vcs-darcs vcs-git vcs-hg vcs-mtn vcs-svn
        testsuite testsuite-triggers), @src_dep_fields,
        qw(package-list directory), @src_checksums_fields, qw(files)
    ],
    CTRL_FILE_BUILDINFO() => [
        qw(format source binary architecture version binary-only-changes),
        @src_checksums_fields,
        qw(build-origin build-architecture build-kernel-version build-date
        build-path build-tainted-by installed-build-depends environment),
    ],
    CTRL_FILE_CHANGES() => [
        qw(format date source binary binary-only built-for-profiles architecture
        version distribution urgency maintainer changed-by description
        closes changes), @src_checksums_fields, qw(files)
    ],
    CTRL_CHANGELOG() => [
        qw(source binary-only version distribution urgency maintainer
        timestamp date closes changes)
    ],
    CTRL_FILE_STATUS() => [
        # Same as fieldinfos in lib/dpkg/parse.c
        qw(package essential status priority section installed-size origin
        maintainer bugs architecture multi-arch source version config-version
        replaces provides depends pre-depends recommends suggests breaks
        conflicts enhances conffiles description triggers-pending
        triggers-awaited),
        # These are allowed here, but not tracked by lib/dpkg/parse.c.
        qw(auto-built-package build-essential built-for-profiles built-using
        homepage installer-menu-item kernel-version package-type
        subarchitecture tag task)
    ],
    CTRL_REPO_RELEASE() => [
        qw(origin label suite codename changelogs date valid-until
        architectures components description), @bin_checksums_fields
    ],
    CTRL_COPYRIGHT_HEADER() => [
        qw(format upstream-name upstream-contact source disclaimer comment
        license copyright)
    ],
    CTRL_COPYRIGHT_FILES() => [
        qw(files copyright license comment)
    ],
    CTRL_COPYRIGHT_LICENSE() => [
        qw(license comment)
    ],
);

=encoding utf8

=head1 NAME

Dpkg::Control::FieldsCore - manage (list of official) control fields

=head1 DESCRIPTION

The modules contains a list of fieldnames with associated meta-data explaining
in which type of control information they are allowed. The types are the
CTRL_* constants exported by Dpkg::Control.

=head1 FUNCTIONS

=over 4

=item $f = field_capitalize($field_name)

Returns the field name properly capitalized. All characters are lowercase,
except the first of each word (words are separated by a hyphen in field names).

=cut

sub field_capitalize($) {
    my $field = lc(shift);

    # Use known fields first.
    return $FIELDS{$field}{name} if exists $FIELDS{$field};

    # Generic case
    return join '-', map { ucfirst } split /-/, $field;
}

=item field_is_official($fname)

Returns true if the field is official and known.

=cut

sub field_is_official($) {
    my $field = lc shift;

    return exists $FIELDS{$field};
}

=item field_is_allowed_in($fname, @types)

Returns true (1) if the field $fname is allowed in all the types listed in
the list. Note that you can use type sets instead of individual types (ex:
CTRL_FILE_CHANGES | CTRL_CHANGELOG).

field_allowed_in(A|B, C) returns true only if the field is allowed in C
and either A or B.

Undef is returned for non-official fields.

=cut

sub field_is_allowed_in($@) {
    my ($field, @types) = @_;
    $field = lc $field;

    return unless exists $FIELDS{$field};

    return 0 if not scalar(@types);
    foreach my $type (@types) {
        next if $type == CTRL_UNKNOWN; # Always allowed
        return 0 unless $FIELDS{$field}{allowed} & $type;
    }
    return 1;
}

=item field_transfer_single($from, $to, $field)

If appropriate, copy the value of the field named $field taken from the
$from Dpkg::Control object to the $to Dpkg::Control object.

Official fields are copied only if the field is allowed in both types of
objects. Custom fields are treated in a specific manner. When the target
is not among CTRL_PKG_SRC, CTRL_PKG_DEB or CTRL_FILE_CHANGES, then they
are always copied as is (the X- prefix is kept). Otherwise they are not
copied except if the target object matches the target destination encoded
in the field name. The initial X denoting custom fields can be followed by
one or more letters among "S" (Source: corresponds to CTRL_PKG_SRC), "B"
(Binary: corresponds to CTRL_PKG_DEB) or "C" (Changes: corresponds to
CTRL_FILE_CHANGES).

Returns undef if nothing has been copied or the name of the new field
added to $to otherwise.

=cut

sub field_transfer_single($$;$) {
    my ($from, $to, $field) = @_;
    $field //= $_;
    my ($from_type, $to_type) = ($from->get_type(), $to->get_type());
    $field = field_capitalize($field);

    if (field_is_allowed_in($field, $from_type, $to_type)) {
        $to->{$field} = $from->{$field};
        return $field;
    } elsif ($field =~ /^X([SBC]*)-/i) {
        my $dest = $1;
        if (($dest =~ /B/i and $to_type == CTRL_PKG_DEB) or
            ($dest =~ /S/i and $to_type == CTRL_PKG_SRC) or
            ($dest =~ /C/i and $to_type == CTRL_FILE_CHANGES))
        {
            my $new = $field;
            $new =~ s/^X([SBC]*)-//i;
            $to->{$new} = $from->{$field};
            return $new;
        } elsif ($to_type != CTRL_PKG_DEB and
		 $to_type != CTRL_PKG_SRC and
		 $to_type != CTRL_FILE_CHANGES)
	{
	    $to->{$field} = $from->{$field};
	    return $field;
	}
    } elsif (not field_is_allowed_in($field, $from_type)) {
        warning(g_("unknown information field '%s' in input data in %s"),
                $field, $from->get_option('name') || g_('control information'));
    }
    return;
}

=item field_transfer_all($from, $to)

Transfer all appropriate fields from $from to $to. Calls
field_transfer_single() on all fields available in $from.

Returns the list of fields that have been added to $to.

=cut

sub field_transfer_all($$) {
    my ($from, $to) = @_;
    my (@res, $res);
    foreach my $k (keys %$from) {
        $res = field_transfer_single($from, $to, $k);
        push @res, $res if $res and defined wantarray;
    }
    return @res;
}

=item field_ordered_list($type)

Returns an ordered list of fields for a given type of control information.
This list can be used to output the fields in a predictable order.
The list might be empty for types where the order does not matter much.

=cut

sub field_ordered_list($) {
    my $type = shift;

    if (exists $FIELD_ORDER{$type}) {
        return map { $FIELDS{$_}{name} } @{$FIELD_ORDER{$type}};
    }
    return ();
}

=item field_list_src_dep()

List of fields that contains dependencies-like information in a source
Debian package.

=cut

sub field_list_src_dep() {
    my @list = map {
        $FIELDS{$_}{name}
    } sort {
        $FIELDS{$a}{dep_order} <=> $FIELDS{$b}{dep_order}
    } grep {
        field_is_allowed_in($_, CTRL_PKG_SRC) and
        exists $FIELDS{$_}{dependency}
    } keys %FIELDS;
    return @list;
}

=item field_list_pkg_dep()

List of fields that contains dependencies-like information in a binary
Debian package. The fields that express real dependencies are sorted from
the stronger to the weaker.

=cut

sub field_list_pkg_dep() {
    my @list = map {
        $FIELDS{$_}{name}
    } sort {
        $FIELDS{$a}{dep_order} <=> $FIELDS{$b}{dep_order}
    } grep {
        field_is_allowed_in($_, CTRL_PKG_DEB) and
        exists $FIELDS{$_}{dependency}
    } keys %FIELDS;
    return @list;
}

=item field_get_dep_type($field)

Return the type of the dependency expressed by the given field. Can
either be "normal" for a real dependency field (Pre-Depends, Depends, ...)
or "union" for other relation fields sharing the same syntax (Conflicts,
Breaks, ...). Returns undef for fields which are not dependencies.

=cut

sub field_get_dep_type($) {
    my $field = lc shift;

    return unless exists $FIELDS{$field};
    return $FIELDS{$field}{dependency} if exists $FIELDS{$field}{dependency};
    return;
}

=item field_get_sep_type($field)

Return the type of the field value separator. Can be one of FIELD_SEP_UNKNOWN,
FIELD_SEP_SPACE, FIELD_SEP_COMMA or FIELD_SEP_LINE.

=cut

sub field_get_sep_type($) {
    my $field = lc shift;

    return $FIELDS{$field}{separator} if exists $FIELDS{$field}{separator};
    return FIELD_SEP_UNKNOWN;
}

=item field_register($field, $allowed_types, %opts)

Register a new field as being allowed in control information of specified
types. %opts is optional

=cut

sub field_register($$;@) {
    my ($field, $types, %opts) = @_;
    $field = lc $field;
    $FIELDS{$field} = {
        name => field_capitalize($field),
        allowed => $types,
        %opts
    };
}

=item field_insert_after($type, $ref, @fields)

Place field after another one ($ref) in output of control information of
type $type.

=cut
sub field_insert_after($$@) {
    my ($type, $field, @fields) = @_;
    return 0 if not exists $FIELD_ORDER{$type};
    ($field, @fields) = map { lc } ($field, @fields);
    @{$FIELD_ORDER{$type}} = map {
        ($_ eq $field) ? ($_, @fields) : $_
    } @{$FIELD_ORDER{$type}};
    return 1;
}

=item field_insert_before($type, $ref, @fields)

Place field before another one ($ref) in output of control information of
type $type.

=cut
sub field_insert_before($$@) {
    my ($type, $field, @fields) = @_;
    return 0 if not exists $FIELD_ORDER{$type};
    ($field, @fields) = map { lc } ($field, @fields);
    @{$FIELD_ORDER{$type}} = map {
        ($_ eq $field) ? (@fields, $_) : $_
    } @{$FIELD_ORDER{$type}};
    return 1;
}

=back

=head1 CHANGES

=head2 Version 1.00 (dpkg 1.17.0)

Mark the module as public.

=cut

1;
