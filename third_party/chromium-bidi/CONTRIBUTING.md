# How to Contribute

We'd love to accept your patches and contributions to this project. There are
just a few small guidelines you need to follow.

## Contributor License Agreement

Contributions to this project must be accompanied by a Contributor License
Agreement (CLA). You (or your employer) retain the copyright to your
contribution; this simply gives us permission to use and redistribute your
contributions as part of the project. Head over to
<https://cla.developers.google.com/> to see your current agreements on file or
to sign a new one.

You generally only need to submit a CLA once, so if you've already submitted one
(even if it was for a different project), you probably don't need to do it
again.

## Code Reviews

All submissions, including submissions by project members, require review. We
use GitHub pull requests for this purpose. Consult
[GitHub Help](https://help.github.com/articles/about-pull-requests/) for more
information on using pull requests.

## Community Guidelines

This project follows
[Google's Open Source Community Guidelines](https://opensource.google/conduct/).

## Adding commands

The BiDi commands are processed in the `src/bidiMapper/commandProcessor.ts`. To add a new command, add it to `_processCommand`, write and call processor for it.

## Debugging

If you use VS Code, you can create folder `.vscode`, and put 2 files in it:

1. `.vscode/launch.json`. Remember to provide the proper `BROWSER_PATH`.

```
{
  // Use IntelliSense to learn about possible attributes.
  // Hover to view descriptions of existing attributes.
  // For more information, visit: https://go.microsoft.com/fwlink/?linkid=830387
  "version": "0.2.0",
  "configurations": [
    {
      "type": "pwa-node",
      "request": "launch",
      "name": "Run Server",
      "skipFiles": ["<node_internals>/**"],
      "cwd": "${workspaceFolder}",
      "console": "externalTerminal",
      "program": "${workspaceFolder}/src/.build/index.js",
      "outFiles": ["${workspaceFolder}/src/.build/**/*.js"],
      "env": {
        "DEBUG": "*",
        "PORT": "8080",
        "BROWSER_PATH": "example/path/to/Chromium"
      }
    }
  ]
}
```

2. `.vscode/settings.json`

```
{
  "python.testing.pytestArgs": ["tests"],
  "python.testing.unittestEnabled": false,
  "python.testing.nosetestsEnabled": false,
  "python.testing.pytestEnabled": true
}
```
