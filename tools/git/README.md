This directory contains some helpful Git tools.

post-checkout and post-merge
============================
These hooks warn you about DEPS modifications so you will remember
to run `gclient sync`.

To install these Git hooks, create symlinks like so:

    ln -s $(pwd)/post-checkout $(git rev-parse --git-dir)/hooks
    ln -s $(pwd)/post-merge    $(git rev-parse --git-dir)/hooks

git-graph
=========
Create a graph of the recent history of occurences of a grep
expression in the project.

suggest_owners
==============
A script to suggest new owners for subdirectories in a git repo based on commit
count to the relevant subdirectory.

usage: suggest_owners.py [-h] [--days-ago DAYS_AGO]
                         [--subdirectory SUBDIRECTORY]
                         [--ignore-authors IGNORE_AUTHORS]
                         [--max-suggestions MAX_SUGGESTIONS]
                         [--author-cl-limit AUTHOR_CL_LIMIT]
                         [--dir-commit-limit DIR_COMMIT_LIMIT]
                         repo_path

positional arguments:
  repo_path

optional arguments:
  -h, --help            show this help message and exit
  --days-ago DAYS_AGO   Number of days of history to search through. (default:
                        365)
  --subdirectory SUBDIRECTORY
                        Limit to this subdirectory (default: None)
  --ignore-authors IGNORE_AUTHORS
                        Ignore this comma separated list of authors (default:
                        None)
  --max-suggestions MAX_SUGGESTIONS
                        Maximum number of suggested authors per directory.
                        (default: 5)
  --author-cl-limit AUTHOR_CL_LIMIT
                        Do not suggest authors who have commited less than
                        this to the directory. (default: 10)
  --dir-commit-limit DIR_COMMIT_LIMIT
                        Merge directories with less than this number of
                        commits into their parent directory. (default: 100)

worktree
========
Automates the creation of git worktrees for Chromium checkouts.

It streamlines the setup by:
*   Validating the parent repository.
*   Copying the `.gclient` configuration.
*   Optionally running `gclient sync` (use `--light` to skip and save ~20GB).
*   Preserving CL dependency chains with `--base` and `--internal-base` forking and upstream tracking.
*   Applying VSCode title bar color presets (red, green, blue).

Usage:
    python3 tools/git/worktree.py /path/to/parent --name my_feature --base main --color green
