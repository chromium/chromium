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
 * @fileoverview Tests for create_closure_releases.ts.
 */

import * as assert from 'assert';

import {clientImplementationsForTesting, createClosureReleases} from './create_closure_releases';
import {Change, GitClient} from './git_client';
import {GitHubClient} from './github_client';

/** The fake GitHub API token to use in tests. */
const FAKE_TOKEN = 'my-github-token';
/** The fake GitHub Release URL to use in tests. */
const FAKE_RELEASE_URL = 'http://my-github-release';
/** A fake rollback hash used in several tests. */
const FAKE_ROLLBACK_HASH = '0123456701234567012345670123456701234567';

/**
 * An interface that describes data needed to fake GitClient prototype methods.
 */
interface FakeGitData {
  /** A fake commit hash. */
  hash: string;
  /** A fake commit message. */
  message: string;
  /** A fake commit body. Omit as a shorthand for setting it to ''. */
  body?: string;
  /** The value of the 'version' property in package.json at a fake commit. */
  pJsonVersion: string;
}

/**
 * A template tag that formats templated strings, under the
 * release-body-specific assumptions that (1) there are no string substitutions
 * and (2) a trailing newline is desired.
 */
function stripIndentForReleaseBody(
    strings: TemplateStringsArray, ...subs: unknown[]) {
  assert.strictEqual(strings.length - 1, subs.length);
  let output = '';
  for (let i = 0; i < subs.length; i++) {
    output += strings[i] + `${subs[i]}`;
  }
  output += strings[strings.length - 1];
  return output.trimStart().split('\n').map(str => str.trim()).join('\n');
}

/**
 * Enable spies on GitClient and GitHub client, and provides fake data for
 * these functions to return when they are called.
 * @param fakeLatestReleaseHash The hash to return when
 *                          GitHubClient#getLatestRelease is called.
 * @param fakeGitCommits An array of objects that describe commit data returned
 *                    by GitClient instance methods.
 */
function spyOnClients(
    fakeLatestReleaseHash: string, fakeGitCommits: FakeGitData[]) {
  // Validate that no hashes are the same in `fakeGitCommits`.
  assert.strictEqual(
      new Set([...fakeGitCommits.map(x => x.hash)]).size,
      fakeGitCommits.length);

  // Add a `body` to objects that are missing it.
  const gitCommits: Change[] =
      fakeGitCommits.map(commit => Object.assign({body: ''}, commit));
  // Make GitClient#listCommits list commits in `fakeGitCommits`.
  spyOn(GitClient.prototype, 'listCommits').and.callFake(async ({from, to}) => {
    assert.strictEqual(to, 'HEAD');
    // + 1 because listCommits excludes `from`.
    return gitCommits.slice(
        gitCommits.findIndex(commit => commit.hash === from) + 1);
  });

  // Make GitClient#getFile return a minimal package.json with data from
  // `fakeGitCommits`.
  spyOn(GitClient.prototype, 'getFile')
      .and.callFake(async (commitish, file) => {
        assert.strictEqual(file, 'package.json');
        const {pJsonVersion} =
            fakeGitCommits.find(commit => commit.hash === commitish);
        return JSON.stringify({version: pJsonVersion});
      });

  // Make GitHubClient#getLatestReleaseTag return `fakeLatestReleaseHash`.
  spyOn(GitHubClient.prototype, 'getLatestReleaseTag')
      .and.returnValue(Promise.resolve(fakeLatestReleaseHash));
  // Make GitHubClient#getLatestRelease return `FAKE_RELEASE_URL`.
  spyOn(GitHubClient.prototype, 'draftRelease')
      .and.returnValue(Promise.resolve(FAKE_RELEASE_URL));
}

describe('createClosureReleases', () => {
  beforeEach(() => {
    // For capturing logs and preventing logspam.
    spyOn(console, 'error');
  });

  // This isn't a real tested behavior of createClosureReleases, but it ensures
  // that spying on client prototype methods in other tests is safe.
  it('constructs exactly one instance of each client class', async () => {
    spyOnClients('00', [
      {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
    ]);
    spyOn(clientImplementationsForTesting, 'GitClient').and.callThrough();
    spyOn(clientImplementationsForTesting, 'GitHubClient').and.callThrough();
    await createClosureReleases(FAKE_TOKEN);
    expect(clientImplementationsForTesting.GitClient).toHaveBeenCalledTimes(1);
    expect(clientImplementationsForTesting.GitHubClient)
        .toHaveBeenCalledTimes(1);
  });

  it('passes a Closure-library specific args and release token to GitHubClient',
     async () => {
       spyOnClients('00', [
         {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
       ]);
       spyOn(clientImplementationsForTesting, 'GitHubClient').and.callThrough();
       await createClosureReleases(FAKE_TOKEN);
       expect(clientImplementationsForTesting.GitHubClient)
           .toHaveBeenCalledOnceWith({
             owner: 'google',
             repo: 'closure-library',
             userAgent: 'Google-Closure-Library',
             token: FAKE_TOKEN,
           });
     });

  it('creates a release with a correctly-formatted tag', async () => {
    spyOnClients('00', [
      {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
      {hash: '01', pJsonVersion: '20201010.0.0', message: ''},
    ]);
    await createClosureReleases(FAKE_TOKEN);
    expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledOnceWith({
      name: 'Closure Library v20201010',
      tagName: 'v20201010',
      commit: '01',
      body: 'No release notes.',
    });
  });

  it('displays a URL to the drafted GitHub Notes', async () => {
    spyOnClients('00', [
      {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
      {hash: '01', pJsonVersion: '20201010.0.0', message: ''},
    ]);
    await createClosureReleases(FAKE_TOKEN);
    expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledTimes(1);
    expect(console.error)
        .toHaveBeenCalledOnceWith(
            `Drafted release for v20201010 at ${FAKE_RELEASE_URL}`);
  });

  it('creates one release per new version bump', async () => {
    spyOnClients('00', [
      {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
      {hash: '01', pJsonVersion: '20201010.0.0', message: ''},
      {hash: '02', pJsonVersion: '20201010.0.0', message: ''},
      {hash: '03', pJsonVersion: '20201011.0.0', message: ''},
    ]);
    await createClosureReleases(FAKE_TOKEN);
    expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledTimes(2);
    expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledWith({
      name: 'Closure Library v20201010',
      tagName: 'v20201010',
      commit: '01',
      body: 'No release notes.',
    });
    expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledWith({
      name: 'Closure Library v20201011',
      tagName: 'v20201011',
      commit: '03',
      body: 'No release notes.',
    });
  });

  it(`doesn't create a release when there are no new commits found`,
     async () => {
       spyOnClients('00', [
         {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
       ]);
       await createClosureReleases(FAKE_TOKEN);
       expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledTimes(0);
     });

  it('doesn\'t create a release when there are no version bumps found',
     async () => {
       spyOnClients('00', [
         {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
         {hash: '01', pJsonVersion: '20201009.0.0', message: ''},
       ]);
       await createClosureReleases(FAKE_TOKEN);
       expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledTimes(0);
     });

  it('properly handles 3 types of RELNOTES', async () => {
    spyOnClients('00', [
      {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
      {
        hash: '01',
        pJsonVersion: '20201009.0.0',
        message: 'commit 1\nRELNOTES: Stuff happened',
      },
      {
        hash: '02',
        pJsonVersion: '20201009.0.0',
        message: 'commit 2\nRELNOTES[NEW]: New API',
      },
      {
        hash: '03',
        pJsonVersion: '20201009.0.0',
        message: 'commit 3\nRELNOTES[INC]: You are broken',
      },
      {
        hash: '04',
        pJsonVersion: '20201010.0.0',
        message: 'commit 4\nRELNOTES: A version bump',
      },
    ]);
    await createClosureReleases(FAKE_TOKEN);
    expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledOnceWith({
      name: 'Closure Library v20201010',
      tagName: 'v20201010',
      commit: '04',
      body: stripIndentForReleaseBody`
        **New Additions**
        * New API (02)

        **Backwards Incompatible Changes**
        * You are broken (03)

        **Other Changes**
        * Stuff happened (01)
        * A version bump (04)
      `,
    });
  });

  it('places multiple commits under the same release notes section',
     async () => {
       spyOnClients('00', [
         {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
         {
           hash: '01',
           pJsonVersion: '20201009.0.0',
           message: 'commit 1\nRELNOTES[NEW]: New API',
         },
         {
           hash: '02',
           pJsonVersion: '20201009.0.0',
           message: 'RELNOTES[NEW]: Even newer API',
         },
         {hash: '03', pJsonVersion: '20201010.0.0', message: ''},
       ]);
       await createClosureReleases(FAKE_TOKEN);
       expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledOnceWith({
         name: 'Closure Library v20201010',
         tagName: 'v20201010',
         commit: '03',
         body: stripIndentForReleaseBody`
        **New Additions**
        * New API (01)
        * Even newer API (02)
      `,
       });
     });

  it('de-duplicates release notes with the same text and change type',
     async () => {
       spyOnClients('00', [
         {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
         {
           hash: '01',
           pJsonVersion: '20201009.0.0',
           message: 'RELNOTES[NEW]:Stuff happened',
         },
         {
           hash: '02',
           pJsonVersion: '20201009.0.0',
           message: 'RELNOTES[NEW]: Stuff happened',
         },
         {
           hash: '03',
           pJsonVersion: '20201009.0.0',
           message: 'RELNOTES:Stuff happened',
         },
         {
           hash: '04',
           pJsonVersion: '20201009.0.0',
           message: 'RELNOTES: Stuff happened',
         },
         {hash: '05', pJsonVersion: '20201010.0.0', message: ''},
       ]);
       await createClosureReleases(FAKE_TOKEN);
       expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledOnceWith({
         name: 'Closure Library v20201010',
         tagName: 'v20201010',
         commit: '05',
         body: stripIndentForReleaseBody`
        **New Additions**
        * Stuff happened (01, 02)

        **Other Changes**
        * Stuff happened (03, 04)
      `,
       });
     });

  it(`doesn't write valid rollback or rolled back release notes`, async () => {
    spyOnClients('00', [
      {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
      {
        hash: FAKE_ROLLBACK_HASH,
        pJsonVersion: '20201009.0.0',
        message: 'RELNOTES[NEW]: Stuff happened',
      },
      {
        hash: '02',
        pJsonVersion: '20201009.0.0',
        message: `Automated rollback of commit ${FAKE_ROLLBACK_HASH}.`,
      },
      {hash: '03', pJsonVersion: '20201010.0.0', message: ''},
    ]);
    await createClosureReleases(FAKE_TOKEN);
    expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledOnceWith({
      name: 'Closure Library v20201010',
      tagName: 'v20201010',
      commit: '03',
      body: 'No release notes.',
    });
  });

  it('writes a release notes entry for invalid rollbacks under "Other Changes"',
     async () => {
       spyOnClients('00', [
         {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
         {
           hash: '01',
           pJsonVersion: '20201009.0.0',
           message: `Automated rollback of commit ${FAKE_ROLLBACK_HASH}.`,
         },
         {hash: '02', pJsonVersion: '20201010.0.0', message: ''},
       ]);
       await createClosureReleases(FAKE_TOKEN);
       expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledOnceWith({
         name: 'Closure Library v20201010',
         tagName: 'v20201010',
         commit: '02',
         body: stripIndentForReleaseBody`
           **Other Changes**
           * __TODO(user):__ Rollback of ${FAKE_ROLLBACK_HASH} (01)
         `,
       });
     });

  it(`omits a rollback commit for an originally omitted change`, async () => {
    spyOnClients('00', [
      {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
      {
        hash: FAKE_ROLLBACK_HASH,
        pJsonVersion: '20201009.0.0',
        message: 'RELNOTES: n/a',
      },
      {
        hash: '02',
        pJsonVersion: '20201009.0.0',
        message: `Automated rollback of commit ${FAKE_ROLLBACK_HASH}.`,
      },
      {hash: '03', pJsonVersion: '20201010.0.0', message: ''},
    ]);
    await createClosureReleases(FAKE_TOKEN);
    expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledOnceWith({
      name: 'Closure Library v20201010',
      tagName: 'v20201010',
      commit: '03',
      body: 'No release notes.',
    });
  });

  it('doesn\'t write headers for empty sections in release notes', async () => {
    spyOnClients('00', [
      {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
      {
        hash: '01',
        pJsonVersion: '20201009.0.0',
        message: 'commit 1\nRELNOTES: Stuff happened',
      },
      {
        hash: '02',
        pJsonVersion: '20201009.0.0',
        message: 'commit 2\nRELNOTES[NEW]: New API',
      },
      {hash: '03', pJsonVersion: '20201010.0.0', message: ''},
      {
        hash: '04',
        pJsonVersion: '20201010.0.0',
        message: 'commit 3\nRELNOTES[INC]: You are broken',
      },
      {hash: '05', pJsonVersion: '20201011.0.0', message: ''},
    ]);
    await createClosureReleases(FAKE_TOKEN);
    expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledTimes(2);
    expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledWith({
      name: 'Closure Library v20201010',
      tagName: 'v20201010',
      commit: '03',
      body: stripIndentForReleaseBody`
        **New Additions**
        * New API (02)

        **Other Changes**
        * Stuff happened (01)
      `,
    });
    expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledWith({
      name: 'Closure Library v20201011',
      tagName: 'v20201011',
      commit: '05',
      body: stripIndentForReleaseBody`
        **Backwards Incompatible Changes**
        * You are broken (04)
      `,
    });
  });

  it('ignores none or empty release notes', async () => {
    spyOnClients('00', [
      {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
      {
        hash: '01',
        pJsonVersion: '20201009.0.0',
        message: 'commit 1\nRELNOTES:',
      },
      {
        hash: '02',
        pJsonVersion: '20201009.0.0',
        message: 'commit 1\nRELNOTES: n/a',
      },
      {
        hash: '03',
        pJsonVersion: '20201009.0.0',
        message: 'commit 1\nRELNOTES: na',
      },
      {
        hash: '04',
        pJsonVersion: '20201009.0.0',
        message: 'commit 1\nRELNOTES: NA',
      },
      {
        hash: '05',
        pJsonVersion: '20201009.0.0',
        message: 'commit 1\nRELNOTES: Na',
      },
      {
        hash: '06',
        pJsonVersion: '20201009.0.0',
        message: 'commit 1\nRELNOTES: none',
      },
      {
        hash: '07',
        pJsonVersion: '20201009.0.0',
        message: 'commit 1\nRELNOTES: NONE',
      },
      {
        hash: '08',
        pJsonVersion: '20201009.0.0',
        message: 'commit 1\nRELNOTES: NoNe',
      },
      {hash: '09', pJsonVersion: '20201010.0.0', message: ''},
    ]);
    await createClosureReleases(FAKE_TOKEN);
    expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledOnceWith({
      name: 'Closure Library v20201010',
      tagName: 'v20201010',
      commit: '09',
      body: 'No release notes.',
    });
  });

  it('escapes GitHub Markdown', async () => {
    spyOnClients('00', [
      {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
      {
        hash: '01',
        pJsonVersion: '20201009.0.0',
        message: 'RELNOTES: *Stuff* #happened <div>',
      },
      {
        hash: '02',
        pJsonVersion: '20201009.0.0',
        message: 'RELNOTES: [Stuff] @happened _surely_',
      },
      {
        hash: '03',
        pJsonVersion: '20201009.0.0',
        message: 'RELNOTES: (Stuff) `happened`',
      },
      {hash: '04', pJsonVersion: '20201010.0.0', message: ''},
    ]);
    await createClosureReleases(FAKE_TOKEN);
    expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledOnceWith({
      name: 'Closure Library v20201010',
      tagName: 'v20201010',
      commit: '04',
      body: stripIndentForReleaseBody`**Other Changes**
        * \\*Stuff\\* \\#happened \\<div\\> (01)
        * \\[Stuff\\] \`@happened\` \\_surely\\_ (02)
        * \\(Stuff\\) \`happened\` (03)
      `,
    });
  });

  it('treats the commit body as part of the description', async () => {
    spyOnClients('00', [
      {hash: '00', pJsonVersion: '20201009.0.0', message: ''},
      {
        hash: '01',
        pJsonVersion: '20201009.0.0',
        message: 'commit 1',
        body: 'RELNOTES: Stuff happened'
      },
      {hash: '02', pJsonVersion: '20201010.0.0', message: ''},
    ]);
    await createClosureReleases(FAKE_TOKEN);
    expect(GitHubClient.prototype.draftRelease).toHaveBeenCalledOnceWith({
      name: 'Closure Library v20201010',
      tagName: 'v20201010',
      commit: '02',
      body: stripIndentForReleaseBody`**Other Changes**
        * Stuff happened (01)
      `,
    });
  });
});
