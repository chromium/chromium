#!/usr/bin/env python3

from __future__ import print_function

import argparse
import generate_amalgamation
import hashlib
import os
import string
import subprocess
import sys


class UnstagedFiles(Exception):
    pass


class UnknownHash(Exception):
    pass


class IncorrectType(Exception):
    pass


class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'


def _print_command(cmd):
    """Print the command to be executed to the console.

    Use a different color so that it can be easily seen amongst the output
    commands.
    """
    if (isinstance(cmd, list)):
        cmd = ' '.join(cmd)
    print('{}{}{}'.format(bcolors.OKBLUE, cmd, bcolors.ENDC))


class ManifestEntry(object):
    """Represents a single entry in a SQLite manifest."""

    def __init__(self, entry_type, items):
        if not len(entry_type) == 1:
            raise IncorrectType(entry_type)
        self.entry_type = entry_type
        self.items = items

    def get_hash_type(self):
        """Return the type of hash used for this entry."""
        last_item = self.items[-1]
        if not all(c in string.hexdigits for c in last_item):
            print(
                '"{}" doesn\'t appear to be a hash.'.format(last_item),
                file=sys.stderr)
            raise UnknownHash()
        elif len(last_item) == 40:
            return 'sha1'
        elif len(last_item) == 64:
            return 'sha3'
        else:
            raise UnknownHash('Incorrect length {} for {}'.format(
                len(last_item), last_item))

    @staticmethod
    def calc_hash(data, method):
        """Return the string sha1 or sha3 hash digest for the given data."""
        if method == 'sha3':
            h = hashlib.sha3_256()
        elif method == 'sha1':
            h = hashlib.sha1()
        else:
            assert False
        h.update(data)
        return h.hexdigest()

    @staticmethod
    def calc_file_hash(fname, method):
        """Return the string sha1 or sha3 hash digest for the given file."""
        with open(fname, 'rb') as input_file:
            return ManifestEntry.calc_hash(input_file.read(), method)

    def update_file_hash(self):
        """Calculates a new file hash for this entry."""
        self.items[1] = ManifestEntry.calc_file_hash(self.items[0],
                                                     self.get_hash_type())

    def __str__(self):
        return '{} {}'.format(self.entry_type, ' '.join(self.items))


class Manifest(object):
    """A deserialized SQLite manifest."""

    def __init__(self):
        self.entries = []

    def find_file_entry(self, fname):
        """Given a file path return the entry. Returns None if none found."""
        for entry in self.entries:
            if entry.entry_type == 'F' and entry.items[0] == fname:
                return entry
        return None


class ManifestSerializer(object):
    """De/serialize SQLite manifests."""

    @staticmethod
    def read_stream(input_stream):
        """Deserialize a manifest from an input stream and return a Manifest
        object."""
        _manifest = Manifest()
        for line in input_stream.readlines():
            items = line.split()
            if not items:
                continue
            _manifest.entries.append(ManifestEntry(items[0], items[1:]))
        return _manifest

    @staticmethod
    def read_file(fname):
        """Deserialize a manifest file and return a Manifest object."""
        with open(fname) as input_stream:
            return ManifestSerializer.read_stream(input_stream)

    @staticmethod
    def write_stream(manifest, output_stream):
        """Serialize the given manifest to the given stream."""
        for entry in manifest.entries:
            print(str(entry), file=output_stream)

    @staticmethod
    def write_file(manifest, fname):
        """Serialize the given manifest to the specified file."""
        with open(fname, 'w') as output_stream:
            ManifestSerializer.write_stream(manifest, output_stream)


class Git(object):
    @staticmethod
    def _get_status():
        changes = []
        for line in subprocess.check_output(['git', 'status',
                                             '--porcelain']).splitlines():
            changes.append(line.decode('utf-8'))
        return changes

    @staticmethod
    def get_staged_changes():
        changes = []
        for line in Git._get_status():
            entry = line[0:2]
            if entry == 'M ':
                changes.append(line.split()[1])
        return changes

    @staticmethod
    def get_unstaged_changes():
        changes = []
        for line in Git._get_status():
            entry = line[0:2]
            if entry == ' M':
                changes.append(line.split()[1])
        return changes

    @staticmethod
    def get_unmerged_changes():
        changes = []
        for line in Git._get_status():
            entry = line[0:2]
            if entry == 'UU':
                changes.append(line.split()[1])
        return changes


class CherryPicker(object):
    """Class to cherry pick commits in a SQLite Git repository."""

    # The binary file extenions for files committed to the SQLite repository.
    # This is used as a simple way of detecting files that cannot (simply) be
    # resolved in a merge conflict. This script will automatically ignore
    # all conflicted files with any of these extensions. If, in the future, new
    # binary types are added then a conflict will arise during cherry-pick and
    # the user will need to resolve it.
    binary_extensions = (
        '.data',
        '.db',
        '.ico',
        '.jpg',
        '.png',
    )

    def __init__(self):
        self._print_cmds = True
        self._update_amangamation = True

    def _take_head_version(self, file_path):
        subprocess.call(
            'git show HEAD:{} > {}'.format(file_path, file_path), shell=True)
        subprocess.call('git add {}'.format(file_path), shell=True)

    @staticmethod
    def _is_binary_file(file_path):
        _, file_extension = os.path.splitext(file_path)
        return file_extension in CherryPicker.binary_extensions

    @staticmethod
    def _append_cherry_pick_comments(comments):
        # TODO(cmumford): Figure out how to append comments on cherry picks
        pass

    def _cherry_pick_git_commit(self, commit_id):
        """Cherry-pick a given Git commit into the current branch."""
        cmd = ['git', 'cherry-pick', '-x', commit_id]
        if self._print_cmds:
            _print_command(' '.join(cmd))
        returncode = subprocess.call(cmd)
        # The manifest and manifest.uuid contain Fossil hashes. Restore to
        # HEAD version and update only when all conflicts have been resolved.
        comments = None
        self._take_head_version('manifest')
        self._take_head_version('manifest.uuid')
        for unmerged_file in Git.get_unmerged_changes():
            if CherryPicker._is_binary_file(unmerged_file):
                print('{} is a binary file, keeping branch version.'.format(
                    unmerged_file))
                self._take_head_version(unmerged_file)
                if not comments:
                    comments = [
                        'Cherry-pick notes', '=============================='
                    ]
                comments.append(
                    '{} is binary file (with conflict). Keeping branch version'
                    .format(unmerged_file))
        if comments:
            CherryPicker._append_cherry_pick_comments(comments)
        self.continue_cherry_pick()

    @staticmethod
    def _is_git_commit_id(commit_id):
        return len(commit_id) == 40

    def _find_git_commit_id(self, fossil_commit_id):
        cmd = [
            'git', '--no-pager', 'log', '--color=never', '--all',
            '--pretty=format:%H', '--grep={}'.format(fossil_commit_id),
            'origin/master'
        ]
        if self._print_cmds:
            _print_command(' '.join(cmd))
        for line in subprocess.check_output(cmd).splitlines():
            return line.decode('utf-8')
        # Not found.
        assert False

    def cherry_pick(self, commit_id):
        """Cherry-pick a given commit into the current branch.

        Can cherry-pick a given Git or a Fossil commit.
        """
        if not CherryPicker._is_git_commit_id(commit_id):
            commit_id = self._find_git_commit_id(commit_id)
        self._cherry_pick_git_commit(commit_id)

    def _generate_amalgamation(self):
        for config_name in ['chromium', 'dev']:
            generate_amalgamation.make_aggregate(config_name)
            generate_amalgamation.extract_sqlite_api(config_name)

    def _add_amalgamation(self):
        os.chdir(generate_amalgamation._SQLITE_SRC_DIR)
        for config_name in ['chromium', 'dev']:
            cmd = [
                'git', 'add',
                generate_amalgamation.get_amalgamation_dir(config_name)
            ]
            if self._print_cmds:
                _print_command(' '.join(cmd))
            subprocess.check_call(cmd)

    def _update_manifests(self):
        """Update the SQLite's Fossil manifest files.

        This isn't strictly necessary as the manifest isn't used during
        any build, and manifest.uuid is the Fossil commit ID (which
        has no meaning in a Git repo). However, keeping these updated
        helps make it more obvious that a commit originated in
        Git and not Fossil.
        """
        manifest = ManifestSerializer.read_file('manifest')
        files_not_in_manifest = ('manifest', 'manifest.uuid')
        for fname in Git.get_staged_changes():
            if fname in files_not_in_manifest:
                continue
            entry = manifest.find_file_entry(fname)
            if not entry:
                print(
                    'Cannot find manifest entry for "{}"'.format(fname),
                    file=sys.stderr)
                sys.exit(1)
            manifest.find_file_entry(fname).update_file_hash()
        ManifestSerializer.write_file(manifest, 'manifest')
        cmd = ['git', 'add', 'manifest']
        if self._print_cmds:
            _print_command(' '.join(cmd))
        subprocess.check_call(cmd)
        # manifest.uuid contains the hash from the Fossil repository which
        # doesn't make sense in a Git branch. Just write all zeros.
        with open('manifest.uuid', 'w') as output_file:
            print('0' * 64, file=output_file)
        cmd = ['git', 'add', 'manifest.uuid']
        if self._print_cmds:
            _print_command(' '.join(cmd))
        subprocess.check_call(cmd)

    def continue_cherry_pick(self):
        if Git.get_unstaged_changes() or Git.get_unmerged_changes():
            raise UnstagedFiles()
        self._update_manifests()
        if self._update_amangamation:
            self._generate_amalgamation()
            self._add_amalgamation()
        cmd = ['git', 'cherry-pick', '--continue']
        if self._print_cmds:
            _print_command(' '.join(cmd))
        subprocess.check_call(cmd)


if __name__ == '__main__':
    desc = 'A script for cherry-picking commits from the SQLite repo.'
    parser = argparse.ArgumentParser(description=desc)
    parser.add_argument(
        'commit', nargs='*', help='The commit ids to cherry pick (in order)')
    parser.add_argument(
        '--continue',
        dest='cont',
        action='store_true',
        help='Continue the cherry-pick once conflicts have been resolved')
    namespace = parser.parse_args()
    cherry_picker = CherryPicker()
    if namespace.cont:
        try:
            cherry_picker.continue_cherry_pick()
            sys.exit(0)
        except UnstagedFiles:
            print(
                'There are still unstaged files to resolve before continuing.')
            sys.exit(1)
    num_picked = 0
    for commit_id in namespace.commit:
        try:
            cherry_picker.cherry_pick(commit_id)
            num_picked += 1
        except UnstagedFiles:
            print(
                '\nThis cherry-pick contains conflicts. Please resolve them ')
            print('(e.g git mergetool) and rerun this script '
                  '`sqlite_cherry_picker.py --continue`')
            print('or `git cherry-pick --abort`.')
            if commit_id != namespace.commit[-1]:
                msg = (
                    'NOTE: You have only successfully cherry-picked {} out of '
                    '{} commits.')
                print(msg.format(num_picked, len(namespace.commit)))
            sys.exit(1)
