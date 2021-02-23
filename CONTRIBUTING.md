<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Contributing to IoTea Event Analytics aka IoTea

First off, thanks for taking the time to contribute!

The following is a set of guidelines for contributing to IoTea Event Analytics and its packages, which are hosted in the [Bosch.IO Analytics](https://<Github IoTea repo>) on GitHub. These are mostly guidelines, not rules. Use your best judgment, and feel free to propose changes to this document in a pull request.

## Table Of Contents

[How can I contribute?](#how-can-i-contribute)

- [Reporting Bugs](#reporting-bugs)
- [Suggesting Enhancements](#suggesting-enhancements)
- [Your First Code Contribution](#your-first-code-contribution)
- [Pull Requests](#pull-requests)

[Styleguides](#styleguides)

- [Git Commit Messages](#git-commit-messages)
- [JavaScript Styleguide](#javascript-styleguide)
- [Python Styleguide](#python-styleguide)
- [Documentation Styleguide](#documentation-styleguide)

## How can I contribute

### Reporting Bugs

- If you find anything, which does not work as expected, feel free to file a bug in our [Bugtracker](<Github IoTea repo>). Please be sure to describe in the ticket, what you expected, what the actual result was and the steps to reproduce the issue (in case it can be reproduced and it's no runtime related issue).

### Suggesting Enhancements

- If you are missing some functionality, your suggestion is highly welcome. Please feel free to file the suggestion in our Backlog. This is no guarantee, that we will implement it asap, but we will have a look at it and decide whether we have the capacity to add this feature in the future. The priority, in which features will be added is decided by our product owner<br>
We won't implement anything which tailors IoTea to be an exact solution for a single problem, if that's the only purpose of your suggestion. Get in contact with our sales person, if you would like to start an assisted integration project.

### Your first code contribution

- Cool, you actually implemented something, which brings the platform to the next level. We highly appreciate collaborative work. Did you already went through our [Styleguides](#Styleguides) to make sure your contribution fits to the rest of the codebase?<br>
  You rebased your feature branch onto our develop-branch and it works with the latest commit? If yes, it's time for your pull request.
- Push your branch in this repository [<Github IoTea repo>](<Github IoTea repo>)
- Create one [here](<Github IoTea repo>)

### Pull requests

- Please always make sure, that your branch is properly rebased on the develop branch and that it does not have any unresolved conflicts
- We chose pull requests as our point of interaction with all our contributers. We will review your changes and comment on that.
- If we have somthing to improve before merge, we will comment your pull request.
- It will stay open, until it is ready to merge
- Please be patient. We have many things to do and only work during common business hours in our Timezone (+01:00 GMT)

## Styleguides

### Git commit messages

- All commits within your pullrequest should contain, what you actually did. A developer should be able to roughly figure out, what you've done, when he reads through your message.
- If you have a User-Story or a bug ticket, prepend this in front of the commit message.
- Always write in present as if the "commit does something to the codebase" e.g. If I execute this commit it will "Define more features in the basic ioTea platform configuration"
- Avoid statements like for any commit:
  - Things done
  - Minor fixes
  - Improvements made
  - Small fixes
  - Enhanced performance

### JavaScript Styleguide

- We are using ESLint to validate our sources. The configuration can be found in the _.eslintrc_ configuration file
- Make sure, you do not break any of these rules in your implementation. If you really need an exception, disable eslint for __exactly this line__ and __exactly this false-positive rule__ i.e.

  ```javascript
  // eslint-disable-next-line no-useless-escape
  ```

### Python Styleguide

- We are using Pylint to validate our sources. The configuration can be found in the _.pylintrc_ configuration file
- Make sure, you do not break any of these rules in your implementation. If you really need an exception, disable eslint for __exactly this line__ and __exactly this false-positive rule__ i.e.

  ```python
  # pylint: disable=wrong-import-position
  ```

### Documentation Styleguide

- We use Markdown for our documentation
- As of readme document, these have to be called README.md
- We are linting our files using markdownlint as VS Code Extension. The configuration can be found in the file _.markdownlintrc_ and must remain unchanged
  - To have a better understanding about the different rules, see [here](https://github.com/DavidAnson/markdownlint/blob/main/doc/Rules.md) for further information
