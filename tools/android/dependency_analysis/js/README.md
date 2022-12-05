# Chrome Android Dependency Analysis Visualization

## Development Setup

### Shell variables

This setup assumes Chromium is in a `cr` directory (`~/cr/src/...`). To make
setup easier, you can modify and export the following variables:

```
export DEP_ANALYSIS_DIR=~/cr/src/tools/android/dependency_analysis
export DEP_ANALYSIS_BUILD_DIR=~/cr/src/out/Debug
```

### Generate JSON

See `../README.md` for instructions on using `generate_json_dependency_graph.py`
, then generate a graph file in the `src` directory (`js/src/json_graph.txt`)
with that exact name:

```
cd $DEP_ANALYSIS_DIR
./generate_json_dependency_graph.py -C $DEP_ANALYSIS_BUILD_DIR -o js/src/json_graph.txt
```

**Move into the `$DEP_ANALYSIS_DIR/js` directory before continuing.**

```
cd $DEP_ANALYSIS_DIR/js
```

### Install dependencies

First, ensure you have a sufficiently new Node.js version. The dependency viewer
doesn't work with v8 for example, and v18.10.0 definitely works.

```
node -v
```

Install the latest version with:

```
nvm install node
```

You will also need to install `npm` if it is not already installed (check with
`npm -v`), either [from the site](https://www.npmjs.com/get-npm) or via
[nvm](https://github.com/nvm-sh/nvm#about) (Node Version Manager).

To install dependencies:

```
npm install
```

### Run visualization for development

```
npm run serve
```

You may need this in your `~/.bashrc` if you are on node version 17+:

```
export NODE_OPTIONS=--openssl-legacy-provider
```

This command runs `webpack-dev-server` in development mode, which will bundle
all the dependencies and open `localhost:8888/package_view.html`, the entry
point of the bundled output. Changes made to the core JS will reload the page,
and changes made to individual modules will trigger a
[hot module replacement](https://webpack.js.org/concepts/hot-module-replacement/).

**To view the visualization, open http://localhost:8888/package_view.html.**

### Build the visualization

```
npm run build
```

This command runs `webpack`, which will bundle the all the dependencies into
output files in the `dist/` directory. These files can then be served via other
means, for example:

```
npm run serve-dist
```

This command will open a simple HTTP server serving the contents of the `dist/`
directory.

To build and serve, you can execute the two commands together:

```
npm run build && npm run serve-dist
```

**To view the visualization, open http://localhost:8888/package_view.html.**

### Deploy

The Chromium Dependency Graph Visualizer is hosted at
https://chromium-dependency-graph.firebaseapp.com.

If you are a Googler, you can see this [doc][deploy doc] for how to deploy a new
version of the viewer.

[deploy doc]: https://docs.google.com/document/d/1u4wlB2EAWNx8zkQr60CQbxDD_Ji_mgSGjhBvX6K8IdM/edit?usp=sharing

### Miscellaneous

To run [ESLint](https://eslint.org/) on the JS and Vue files:

```
npm run lint
```

To let ESLint try and fix as many lint errors as it can:

```
npm run lint-fix
```
