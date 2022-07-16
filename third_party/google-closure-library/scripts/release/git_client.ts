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
 * @fileoverview A module that exports a convenience class to interface with
 * git.
 */

import {gitP, SimpleGit} from 'simple-git';

/**
 * An interface that describes a git commit range.
 */
export interface CommitRange {
  /** The _exclusive_ start of the range. */
  from: string;
  /** The _inclusive_ end of the range. */
  to: string;
}

/**
 * An interface that describes a git commit.
 */
export interface Change {
  /** A commit hash. */
  hash: string;
  /** A commit message. */
  message: string;
  /** A commit body. */
  body: string;
}

/**
 * A class that provides an stripped-down interface for git.
 */
export class GitClient {
  private readonly simpleGit: SimpleGit;

  /**
   * Constructs a new GitClient.
   * @param path The path that the git CLI should treat as its working
   * directory.
   */
  constructor(path: string) {
    this.simpleGit = gitP(path);
  }

  /**
   * Returns a list of commits in the given commit range, in order of ascending
   * commit time. Merge commits are excluded.
   * @param range The range of the list of commits.
   * @return A list of commits.
   */
  async listCommits({from, to}: CommitRange): Promise<Change[]> {
    return [
      ...(await this.simpleGit.log(['--no-merges', `${from}..${to}`])).all,
    ].reverse();
  }

  /**
   * Returns the contents of a file at the given commit.
   * @param commitish The commit at which the file should be checked out.
   * @param file The path to the file whose contents should be returned.
   * @return The contents of the file.
   */
  getFile(commitish: string, file: string): Promise<string> {
    return this.simpleGit.show([`${commitish}:${file}`]);
  }
}
