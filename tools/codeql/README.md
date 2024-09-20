# CodeQL & Chromium

This directory contains:
* scripts used for indexing CodeQL databases, and
* sample queries designed to catch potential bugs in Chrome.

## Getting Started

This guide covers how to run CodeQL queries in both **VSCode** and at the
**command line**.

We strongly recommend using VSCode unless you have a specific need to use the
command line.  The CodeQL extension for VSCode features a lot of really nice
autocomplete/fast-debug features for writing CodeQL queries that will generally
make you much more productive.

We try to keep this guide up-to-date but if you run into any issues, check the
official CodeQL documentation
([VSCode](https://codeql.github.com/docs/codeql-for-visual-studio-code/),
[CLI](https://docs.github.com/en/code-security/codeql-cli/getting-started-with-the-codeql-cli)),
if that still doesn't help or you suspect it's an issue specific to the database
build itself, feel free to contact codeql-discuss@chromium.org.

## VSCode Instructions

1. Download a database.

    You can download recent databases at the
    [chrome-codeql-databases GCS bucket](https://commondatastorage.googleapis.com/chrome-codeql-databases/).

    (There are a LOT of databases there.  We'll prune them in the future, but
    for now you probably want to just filter by a date prefix, e.g.
    `codeql-$YEAR-$MONTH-$DAY`. Within the folder for a particular date, you may
    see more than one database. The database for Chrome always begins with
    `chrome`. You may download other databases if you like!  They represent
    other `gn` targets that are used within Chrome and may be of interest to
    security researchers. For instance, we currently build the `libavif` target
    for those interested in inspecting that codec library specifically.)

2. Install the VSCode CodeQL Extension

    Open the Extensions view in VSCode (ctrl+shift+X or cmd+shift+X).  Search
    for "CodeQL" and install the official CodeQL extension.

    You should see a "QL" icon pop up at the bottom of the Activity Bar on the
    left-hand side of the screen once it's installed.

3. Install CodeQL Runtimes.

    Open the Command Palette (Ctrl+Shift+P or Cmd+Shift+P).

    Type CodeQL: Download Packs and press Enter.  Select "Download all core
    query packs."

    Restart VSCode.

4. Load your query files.

    Otherwise, you probably want to start with the query files in the Chromium
    source tree.

    In the File Explorer view, go to
    `File > Open Folder > $YOUR_CHROME_CHECKOUT/src/tools/codeql`.

    This will cause the queries to appear in the File Explorer view, AS WELL as
    in the CodeQL view (accessed by tapping the "QL" icon in the Activity Bar).

5. Load the CodeQL database.

    In the CodeQL view, next to "Databases", click "Choose Database From
    Archive", then choose the `zip` file for the database you downloaded.

    (This import step took ~4-5 minutes on a 128-core 512GB RAM machine; and ~10
    minutes on a 6-core 16GB RAM laptop.)

6. Install pack dependencies.

    Open the Command Palette (Ctrl+Shift+P or Cmd+Shift+P).
    Type "Install Pack Dependencies".
    Type (or select) "chrome-ql-queries".

7. (Optional) Load source code.

    In the CodeQL view, right-click your database and click "Add Database Source
    to Workspace" to make the source code for the database appear in VSCode's
    File Explorer.  This is optional but can be useful for exploring files after
    the completion of a query.

8. Run a query.

    To run one of the preexisting queries, while in the CodeQL view, right-click
    your chosen query ("Queries" should be listed right under "Databases") and
    choose "Run against local database."

    However, the preexisting queries are unlikely to return anything, since
    hopefully Chromium has already run and fixed any issues ourselves, so you
    may want to try writing a new query.

    To write a new query from scratch: go to the File Explorer view, add a new
    file with the extension `.ql` to the `codel/queries` folder, and write your
    query
    there.

    Here's a "Hello world!" style query you can try that will return methods
    from Mojo interface implementations in Chrome.

    ```
    import cpp
    import lib.Ipc

    from MojoInterfaceImpl impl
    select impl, impl.getAMethod()
    ```

    This query took ~5 minutes to run on a 128-core 512GB RAM machine.  In
    general, the more complicated a query is, the more time it will take; in
    particular,
    [global data flow](https://codeql.github.com/docs/writing-codeql-queries/about-data-flow-analysis/)
    (often required for e.g. taint tracking) will take much longer.

## Command-Line Interface Instructions

1. Install the CodeQL CLI.

    You'll want to install the most recent release from
    [here](https://github.com/github/codeql-action/releases).

2. Download and unzip the database.

    This is identical to step 1 in the VSCode Instructions, but you will
    additionally unzip the database after you download it.

3. Start writing queries.

    It's "choose your own adventure" from here (you might find the
    [official CodeQL C++ docs useful](https://codeql.github.com/docs/codeql-language-guides/basic-query-for-cpp-code/)),
    but, if you'd like a "hello world" example to play with:

    Make a directory for your queries.

    Inside that new directory, make a qlpack.yml file.

    ```
    name: my-custom-cpp-pack
    version: 0.0.0
    dependencies: codeql/cpp-all
    ```

    Then create a `myquery.ql` file inside that same directory:

    ```
    import cpp
    import lib.Ipc

    from MojoInterfaceImpl impl
    select impl, impl.getAMethod()
    ```

    Finally:

    `codeql query run -d $PATH_TO_DATABASE -o output.bqrs myquery.ql`

## Known Issues

**"There was no upgrade path to the target dbscheme"**

This suggests there's a version mismatch between the CodeQL CLI used to create
the database, the version of the QL libraries used by the query, and the version
of the CodeQL CLI used by VSCode.

Check [this issue](https://github.com/github/codeql/issues/12331) for how to
check the version of each of those things.  I was able to resolve one instance
of this issue by upgrading my local CLI tools version to match that used to
build the Chromium database.

**CodeQL: View AST "Can't infer database from the provided source"**

The AST viewer seems to be broken at the moment.  There's no workaround for this
right now.

## Automation

The file targets_to_index.py in this directory determines which targets should
be indexed into CodeQL databases in automation.

To add a target, append it to the list of `full_targets` in that file.

The file queries_to_run.json in this directory determines which CodeQL queries
should be run against which CodeQL databases in automation.

To add a query, add the query's filename to the list corresponding to the
database you would like to run that query against.

## Other Resources

* [CodeQL for C and C++](https://codeql.github.com/docs/codeql-language-guides/codeql-for-cpp/)
