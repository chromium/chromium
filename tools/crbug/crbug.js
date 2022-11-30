// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const process = require('child_process');
const https = require('https');

function log(msg) {
  // console.log(msg);
}

class CrBugUser {
  constructor(json) {
    this.name_ = json.displayName;
    this.id_ = json.name;
    this.email_ = json.email;
  }

  get name() {
    return this.name_;
  }
  get id() {
    return this.id_;
  }
  get email() {
    return this.email_;
  }
};

class CrBugIssue {
  constructor(json) {
    this.number_ = json.name;
    this.reporter_id_ = json.reporter;
    this.owner_id_ = json.owner ? json.owner.user : undefined;
    this.last_update_ = json.modifyTime;
    this.close_ = json.closeTime ? new Date(json.closeTime) : undefined;

    this.url_ = undefined;
    const parts = this.number_.split('/');
    if (parts[0] === 'projects' && parts[2] === 'issues') {
      const project = parts[1];
      const num = parts[3];
      this.url_ =
          `https://bugs.chromium.org/p/${project}/issues/detail?id=${num}`;
    }
  }

  get number() {
    return this.number_;
  }
  get owner_id() {
    return this.owner_id_;
  }
  get reporter_id() {
    return this.reporter_id_;
  }
  get url() {
    return this.url_;
  }
};

class CrBugComment {
  constructor(json) {
    this.user_id_ = json.commenter;
    this.timestamp_ = new Date(json.createTime);
    this.timestamp_.setSeconds(0);
    this.content_ = json.content;
    this.fields_ = json.amendments ?
        json.amendments.map(m => m.fieldName.toLowerCase()) :
        undefined;

    this.json_ = JSON.stringify(json);
  }

  get user_id() {
    return this.user_id_;
  }
  get timestamp() {
    return this.timestamp_;
  }
  get content() {
    return this.content_;
  }
  get updatedFields() {
    return this.fields_;
  }

  isActivity() {
    if (this.content)
      return true;

    const fields = this.updatedFields;

    // If bug A gets merged into bug B, then ignore the update for bug A. There
    // will also be an update for bug B, and that will be counted instead.
    if (fields && fields.indexOf('mergedinto') >= 0) {
      return false;
    }

    // If bug A is marked as blocked on bug B, then that triggers updates for
    // both bugs. So only count 'blockedon', and ignore 'blocking'.
    const allowedFields = [
      'blockedon', 'cc', 'components', 'label', 'owner', 'priority', 'status',
      'summary'
    ];
    if (fields && fields.some(f => allowedFields.indexOf(f) >= 0)) {
      return true;
    }
    return false;
  }
};

class CrBug {
  constructor(project) {
    this.token_ = this.getAuthToken_();
    this.project_ = project;
  }

  getAuthToken_() {
    const scope = 'https://www.googleapis.com/auth/userinfo.email';
    const args = [
      'luci-auth', 'token', '-use-id-token', '-audience',
      'https://monorail-prod.appspot.com', '-scopes', scope, '-json-output', '-'
    ];
    const stdout = process.execSync(args.join(' ')).toString().trim();
    const json = JSON.parse(stdout);
    return json.token;
  }

  async fetchFromServer_(path, message) {
    const hostname = 'api-dot-monorail-prod.appspot.com';
    return new Promise((resolve, reject) => {
      const postData = JSON.stringify(message);
      const options = {
        hostname: hostname,
        method: 'POST',
        path: path,
        headers: {
          'Content-Type': 'application/json',
          'Accept': 'application/json',
          'Authorization': `Bearer ${this.token_}`,
        }
      };

      let data = '';
      const req = https.request(options, (res) => {
        log(`STATUS: ${res.statusCode}`);
        log(`HEADERS: ${JSON.stringify(res.headers)}`);

        res.setEncoding('utf8');
        res.on('data', (chunk) => {
          log(`BODY: ${chunk}`);
          data += chunk;
        });
        res.on('end', () => {
          if (data.startsWith(')]}\'')) {
            resolve(JSON.parse(data.substr(4)));
          } else {
            resolve(data);
          }
        });
      });

      req.on('error', (e) => {
        console.error(`problem with request: ${e.message}`);
        reject(e.message);
      });

      // Write data to request body
      log(`Writing ${postData}`);
      req.write(postData);
      req.end();
    });
  }

  /**
   * Calls SearchIssues with the given parameters.
   *
   * @param {string} query The query to use to search.
   * @param {Number} pageSize The maximum issues to return.
   * @param {string} pageToken The page token from the previous call.
   *
   * @return {JSON}
   */
  async searchIssuesPagination_(query, pageSize, pageToken) {
    const message = {
      'projects': [this.project_],
      'query': query,
      'pageToken': pageToken,
    };
    if (pageSize) {
      message['pageSize'] = pageSize;
    }
    const url = '/prpc/monorail.v3.Issues/SearchIssues';
    return this.fetchFromServer_(url, message);
  }

  /**
   * Searches Monorail for issues using the given query.
   * TODO(crbug.com/monorail/7143): SearchIssues only accepts one project.
   *
   * @param {string} query The query to use to search.
   *
   * @return {Array<CrBugIssue>}
   */
  async search(query) {
    const pageSize = 100;
    let pageToken;
    let issues = [];
    do {
      const resp =
          await this.searchIssuesPagination_(query, pageSize, pageToken);
      if (resp.issues) {
        issues = issues.concat(resp.issues.map(i => new CrBugIssue(i)));
      }
      pageToken = resp.nextPageToken;
    } while (pageToken);
    return issues;
  }

  /**
   * Calls ListComments with the given parameters.
   *
   * @param {string} issueName Resource name of the issue.
   * @param {string} filter The approval filter query.
   * @param {Number} pageSize The maximum number of comments to return.
   * @param {string} pageToken The page token from the previous request.
   *
   * @return {JSON}
   */
  async listCommentsPagination_(issueName, pageToken, pageSize) {
    const message = {
      'parent': issueName,
      'pageToken': pageToken,
      'filter': '',
    };
    if (pageSize) {
      message['pageSize'] = pageSize;
    }
    const url = '/prpc/monorail.v3.Issues/ListComments';
    return this.fetchFromServer_(url, message);
  }

  /**
   * Returns all comments and previous/current descriptions of an issue.
   *
   * @param {CrBugIssue} issue The CrBugIssue instance.
   *
   * @return {Array<CrBugComment>}
   */
  async getComments(issue) {
    let pageToken;
    let comments = [];
    do {
      const resp = await this.listCommentsPagination_(issue.number, pageToken);
      if (resp.comments) {
        comments = comments.concat(resp.comments.map(c => new CrBugComment(c)));
      }
      pageToken = resp.nextPageToken;
    } while (pageToken);
    return comments;
  }

  /**
   * Returns the user associated with 'username'.
   *
   * @param {string} username The username (e.g. linus@chromium.org).
   *
   * @return {CrBugUser}
   */
  async getUser(username) {
    const url = '/prpc/monorail.v3.Users/GetUser';
    const message = {
      name: `users/${username}`,
    };
    return new CrBugUser(await this.fetchFromServer_(url, message));
  }
};

module.exports = {
  CrBug,
};
