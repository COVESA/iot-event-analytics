/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const core = require('@actions/core');
const { getOctokit, context } = require('@actions/github');

function checkSignatureFor(commit) {
  console.log(`Checking commit ${commit.sha}...`);

  result = commit.commit.message.indexOf(`Signed-off-by: ${commit.commit.committer.name} <${commit.commit.committer.email}>`) > -1 || 
           commit.commit.message.indexOf(`Signed-off-by: ${commit.commit.author.name} <${commit.commit.author.email}>`) > -1;
  if (!result) {
    console.log(`"Signed-off by:" is missing or does not match neither commit author nor committer ${commit.sha}...`);
    console.log(` Commit Author Name: ${commit.commit.author.name}...`);
    console.log(` Commit Author Email: ${commit.commit.author.email}...`);
    console.log(` Commit Committer Name : ${commit.commit.committer.name}...`);
    console.log(` Commit Committer Email : ${commit.commit.committer.email}...`);
  }
  return result;
}

async function checkSignaturesInPullRequest(token, pullNumber = -1) {
  return getOctokit(token).request('GET /repos/{owner}/{repo}/pulls/{pull_number}/commits', {
    owner: context.repo.owner,
    repo: context.repo.repo,
    pull_number: pullNumber
  })
    .then(commitsResponse => {
      const invalidCommitHashes = [];

      for (let commit of commitsResponse.data) {
        if (!checkSignatureFor(commit)) {
          invalidCommitHashes.push(commit.sha);
        }
      }

      if (invalidCommitHashes.length === 0) {
        return;
      }

      throw new Error(`Missing or mismatching signatures for commits [ ${invalidCommitHashes.join(', ')} ].` +
                      `Signature must match either committer or author of the commit.`);
    });
}

async function main() {
  if (context.payload.pull_request) {
    return checkSignaturesInPullRequest(
      core.getInput('github-token', { required: true }),
      context.payload.pull_request.number
    );
  }

  throw new Error(`Action can only be used on pull requests`);
}

main().catch(err => core.setFailed(err.message));