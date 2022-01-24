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
 * @fileoverview A module that exports a convenience class to interface with the
 * GitHub REST API.
 */

import {Octokit} from '@octokit/rest';

/**
 * An interface that describes options for the GitHubClient class constructor.
 */
export interface GitHubClientOptions {
  /** The owner of the repository. */
  owner: string;
  /** The repository name. */
  repo: string;
  /**
   * A unique user agent for this application, as requested in GitHub API
   * documentation.
   */
  userAgent: string;
  /**
   * A GitHub API token.
   */
  token: string;
}

/**
 * An interface that describes options for GitHubClient#draftRelease.
 */
export interface DraftReleaseOptions {
  /** The name of the tag to associate with this release. */
  tagName: string;
  /** The commit to tag for this release. */
  commit: string;
  /** The name of the release. */
  name: string;
  /** The body text for the release. */
  body: string;
}

/**
 * A class that provides an stripped-down interface for the GitHub REST API.
 */
export class GitHubClient {
  private readonly owner: string;
  private readonly repo: string;
  private readonly octokit: Octokit;

  /**
   * Constructs a new GitHubClient.
   * @param options Options for constructing this GitHubClient.
   */
  constructor({
    owner,
    repo,
    userAgent,
    token,
  }: GitHubClientOptions) {
    this.owner = owner;
    this.repo = repo;
    this.octokit = new Octokit({
      auth: token,
      userAgent,
    });
  }

  /**
   * Returns the tag associated with the latest GitHub Release.
   * @return The tag associated with the latest GitHub Release.
   */
  async getLatestReleaseTag() {
    const {data} = await this.octokit.repos.getLatestRelease({
      owner: this.owner,
      repo: this.repo,
    });
    return data.tag_name;
  }

  /**
   * Drafts a new GitHub Release.
   * @param options Options for drafting the GitHub Release.
   * @return The URL to the GitHub Web UI for managing the drafted release.
   */
  async draftRelease({
    tagName,
    commit,
    name,
    body,
  }: DraftReleaseOptions) {
    const {data} = await this.octokit.repos.createRelease({
      owner: this.owner,
      repo: this.repo,
      tag_name: tagName,
      target_commitish: commit,
      name,
      body,
      draft: true,
    });
    return data.html_url;
  }
}
