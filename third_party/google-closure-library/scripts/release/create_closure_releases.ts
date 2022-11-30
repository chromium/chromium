/**
 * @license
 * Copyright 2020 The Closure Library Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @fileoverview A module that exports `createClosureReleases`, which
 * asynchronously creates GitHub Releases for Closure Library when a new one is
 * warranted. This module can be run as a script, where `createClosureReleases`
 * is simply invoked immediately.
 */

import {Change, GitClient} from './git_client';
import {GitHubClient} from './github_client';

/**
 * Used for testing only, so that a client class constructor can be spied on
 * and mocked.
 */
export const clientImplementationsForTesting = {
  GitHubClient,
  GitClient,
};

/** A regex to match release notes in a commit message. */
const MATCH_RELNOTES = /RELNOTES(?:\[(INC|NEW)\])?:(.*)/;
/** A regex to match indications of a rollback in a commit message. */
const MATCH_ROLLBACK = /Automated rollback of commit ([a-f0-9]{40})./;
/**
 * A regex to match indications that a commit message shouldn't be reflected in
 * a GitHub Release body.
 */
const MATCH_INVALID_NOTE = /(none|n\/?a)\.?$/im;

/**
 * The allowed change types for release notes (NONE being implicit when no
 * change type is specified) and corresponding GitHub Release body header.
 */
const RELEASE_HEADINGS = [
  {
    changeType: 'NEW',
    heading: '**New Additions**',
  },
  {
    changeType: 'INC',
    heading: '**Backwards Incompatible Changes**',
  },
  {
    changeType: 'NONE',
    heading: '**Other Changes**',
  },
];

/**
 * Escape GitHub Markdown in the provided string.
 * @param note The string to escape.
 * @return The escaped string.
 */
function escapeGitHubMarkdown(note: string) {
  // "Escape" GitHub mentions (i.e., "@user") by surrounding in backticks.
  note = note.replace(/(@\w+)/g, '`$1`');
  // Escape known markdown characters with a leading backslash.
  note = note.replace(/([*_(){}#!.<>[\]])/g, '\\$1');
  return note;
}

/**
 * An interface representing a single release notes entry in a GitHub Release
 * body.
 */
interface ReleaseNotesEntry {
  /** The type of change, which must be represented in RELEASE_HEADINGS. */
  changeType: string;
  /** The contents of the release notes. */
  noteText: string;
  /** Hashes of commits that have these release notes. */
  hashes: string[];
}

/**
 * Creates a GitHub Release body based on a list of git commit hashes and
 * messages.
 * @param changes The list of git commit information.
 * @return A string representing a Markdown-formatted GitHub release body.
 */
function createReleaseNotes(changes: Change[]) {
  // Populate `releaseNotes` based on `changes`.
  // One entry in `releaseNotes` may correspond to several entries in `changes`.
  const releaseNotes: ReleaseNotesEntry[] = [];
  // Store identifiers for "invalid" release notes, since they can still get
  // rolled back, in which case we don't want to inadvertently label a rollback
  // as having an unknown original change.
  const skippedChanges = new Set<string>();
  for (const {body, hash, message} of changes) {
    // simple_git splits a commit description on `\n\n`.
    // Re-construct the original commit description, as RELNOTES could be in
    // either the message or the body.
    const desc = `${message}\n\n${body}`;
    // Don't include a message like "RELNOTES: n/a".
    if (MATCH_INVALID_NOTE.test(desc)) {
      skippedChanges.add(hash);
      continue;
    }

    const rollback = MATCH_ROLLBACK.exec(desc);
    if (rollback) {
      // If we find a rollback commit, try to find the original change that got
      // rolled back by this one via commit hash.
      const rolledbackHash = rollback[1];
      const matchingChange = releaseNotes.find(
          change => change.hashes.some(hash => hash === rolledbackHash));
      if (matchingChange) {
        // We found the change that got rolled back. Changes with the same
        // release notes are stored together under the same `ReleaseNotesEntry`
        // object, and are identified by unique entries in the `hashes` field.
        // Remove the entry for this change from that field.
        // If it's the only such change, `hashes` will become empty (indicating
        // that no change correspond to these release notes), and the release
        // notes will be dropped entirely.
        // See [*], where the dropping occurs.
        matchingChange.hashes =
            matchingChange.hashes.filter(hash => hash !== rolledbackHash);
      } else {
        // It's possible that this change rolls back one that was skipped in the
        // release notes. If that's the case, do nothing.
        const originalChangeSkipped = skippedChanges.has(rolledbackHash);
        if (!originalChangeSkipped) {
          // The commit hash extracted from the rollback message doesn't match
          // an entry in `releaseNotes` or `skippedChanges`, which can be due to
          // one of three reasons:
          //   1) The hash is wrong (which may be a result of a rebase).
          //   2) The rolled-back commit is out of the range represented by
          //      `changes`.
          //   3) The rolled-back commit is itself a rollback (making this a
          //      roll-forward commit).
          // Either way, write an entry in "Other Changes" to direct a draft
          // reviewer to find the right commit.
          releaseNotes.push({
            changeType: 'NONE',
            noteText: `__TODO(user):__ Rollback of ${rolledbackHash}`,
            hashes: [hash]
          });
        }
      }
    } else {
      const matchedRelnotes = MATCH_RELNOTES.exec(desc);
      if (matchedRelnotes) {
        const changeType = matchedRelnotes[1] || 'NONE';
        const noteText = escapeGitHubMarkdown(matchedRelnotes[2].trim());
        if (noteText) {
          const matchingChange = releaseNotes.find(
              change => change.noteText === noteText &&
                  change.changeType === changeType);
          if (matchingChange) {
            // Merge entries with the same release notes, for the sake of
            // de-duplicating repetitive changes.
            matchingChange.hashes.push(hash);
          } else {
            releaseNotes.push({
              changeType,
              noteText,
              hashes: [hash],
            });
          }
        }
      }
    }
  }

  // Populate `body` with formatted release notes.
  let body = '';
  for (const {changeType, heading} of RELEASE_HEADINGS) {
    const formattedChangesForHeading =
        releaseNotes
            .filter(changeNote => changeNote.changeType === changeType)
            // [*] It's possible to have an empty hashes list if a change was
            // rolled back.
            .filter(({hashes}) => hashes.length)
            .map(({noteText, hashes}) => `* ${noteText} (${hashes.join(', ')})`)
            .join('\n');

    // Conditionally append a section and its header to `body` if the section
    // isn't empty.
    // We don't want to add headers for empty sections.
    if (formattedChangesForHeading) {
      if (body) body += '\n';
      body += `${heading}\n${formattedChangesForHeading}\n`;
    }
  }
  if (!body) {
    body = 'No release notes.';
  }
  return body;
}

/**
 * Extracts and returns the major version from package.json in the repo managed
 * by `git` at commit `hash`.
 * @param git The GitClient instance to use.
 * @param commitish The commitish at which package.json should be read.
 * @return The major version string from package.json, prefixed with a 'v'.
 */
async function getMajorVersionAtCommit(git: GitClient, commitish: string) {
  const pJsonRaw = await git.getFile(commitish, 'package.json');
  const pJson = JSON.parse(pJsonRaw);
  const matchedPJsonVersion = /^v?(\d+)\.\d+\.\d+$/.exec(pJson.version);
  if (!matchedPJsonVersion) {
    throw new Error(
        `Bad package.json version string '${pJson.version}' @ ${commitish}`);
  }
  return `v${matchedPJsonVersion[1]}`;
}

/**
 * Given an API token, create GitHub Releases for each major version bump to
 * Closure Library since the last GitHub Release.
 * @param gitHubApiToken The GitHub API token.
 */
export async function createClosureReleases(gitHubApiToken: string) {
  // Initialize clients.
  const impls = clientImplementationsForTesting;
  const git = new impls.GitClient(process.cwd());
  const github = new impls.GitHubClient({
    owner: 'google',
    repo: 'closure-library',
    userAgent: 'Google-Closure-Library',
    token: gitHubApiToken,
  });

  // Get the tag of the latest GitHub release.
  const from = await github.getLatestReleaseTag();
  const versionAtLastRelease = await getMajorVersionAtCommit(git, from);

  // Get the list of commits since `from`.
  const commits = await git.listCommits({from, to: 'HEAD'});

  // Identify the commits in which the package.json major version changed.
  const pJsonVersions: Array<{version: string, changes: Change[]}> = [];
  // We need to push a placeholder object for the last release. This helps us
  // figure out whether the immediate next commit has a version bump or not.
  pJsonVersions.push({version: versionAtLastRelease, changes: []});
  let seenCommits: Change[] = [];
  for (const commit of commits) {
    const version = await getMajorVersionAtCommit(git, commit.hash);
    seenCommits.push(commit);
    if (!pJsonVersions.some(entry => entry.version === version)) {
      pJsonVersions.push({
        version,
        changes: seenCommits,
      });
      seenCommits = [];
    }
  }
  // Remove the placeholder object mentioned above.
  pJsonVersions.shift();

  // Draft a new GitHub release for each package.json version change seen.
  for (const {version, changes} of pJsonVersions) {
    const name = `Closure Library ${version}`;
    const tagName = version;
    const commit = changes[changes.length - 1].hash;
    const body = createReleaseNotes(changes);
    // Create the release
    const url = await github.draftRelease({tagName, commit, name, body});
    console.error(`Drafted release for ${version} at ${url}`);
  }
}

// Run this module as a script if specified as the entry point.
if (require.main === module) {
  if (!process.env.GITHUB_TOKEN) {
    console.error('Need GITHUB_TOKEN env var to create releases.');
    process.exit(1);
  }
  createClosureReleases(process.env.GITHUB_TOKEN).catch(err => {
    console.error(err);
    process.exit(1);
  });
}
