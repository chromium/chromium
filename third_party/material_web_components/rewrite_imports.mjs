import fs from 'fs';

import {ArgumentParser} from '../js_code_coverage/node_modules/argparse/argparse.js';
import {SourceMapConsumer, SourceMapGenerator} from '../js_code_coverage/node_modules/source-map/source-map.js';

const INLINE_SOURCE_MAP_REGEX =
    /\/\/# sourceMappingURL=data:application\/json;base64,(.+)/;

function addMapping(
    map, sourceFileName, originalLine, originalColumn, generatedLine,
    generatedColumn) {
  const mapping = {
    source: sourceFileName,
    original: {
      line: originalLine,
      column: originalColumn,
    },
    generated: {
      line: generatedLine,
      column: generatedColumn,
    },
  };
  map.addMapping(mapping);
}

/**
 * Processes a generated js file to correct the inline sourcemap for changes
 * made by rewrite_imports.py
 * e.g. 'lit' is rewritten in imports to '//resources/mwc/lit/index.js'
 * before:  import {css, CSSResult, CSSResultGroup, html, LitElement,
 * PropertyValues} from 'lit';
 * after:   import {css, CSSResult, CSSResultGroup,
 * html, LitElement, PropertyValues} from '//resources/mwc/lit/index.js';
 * @param {string} originalDirectory Path to the compiled js
 * @param {string} file File name of generated file with inline sourcemap
 * @param {string} outputDirectory Path to the rewritten js
 * @param {JSON} changedMappings A JSON list of lines and their changes
 */
async function processOneFile(
    originalDirectory, file, outputDirectory, changedMappings) {
  // Retrieve inline sourcemap from last line of generated file.
  const inputFile = fs.readFileSync(originalDirectory + '/' + file, 'utf8');
  const inputLines = inputFile.split('\n');
  const lastLine = inputLines[inputLines.length - 1];

  // Parse raw sourcemap out of line.
  const lastLineMatcher = lastLine.match(INLINE_SOURCE_MAP_REGEX);
  if (!lastLineMatcher) {
    return;
  }
  const rawSourceMap = Buffer.from(lastLineMatcher[1], 'base64').toString();
  const mapConsumer = await new SourceMapConsumer(rawSourceMap, null);
  const mapGenerator = SourceMapGenerator.fromSourceMap(mapConsumer);
  mapConsumer.destroy();

  // Create a new 1:1 sourcemap of tsc generated js to itself.
  const jsToJsMapGenerator = new SourceMapGenerator(
      { file: mapGenerator._file, sourceRoot: originalDirectory });
  mapGenerator._mappings.unsortedForEach(mapping => {
    addMapping(jsToJsMapGenerator, file, mapping.generatedLine, mapping.generatedColumn, mapping.generatedLine, mapping.generatedColumn);
  });

  // `changedMappings` contains a list of lines and their changes. Convert this
  // to a map of changedLine to change information for easier lookup.
  const changedLinesMap = new Map();
  const mappingsJSON = JSON.parse(changedMappings);
  for (const mapping of mappingsJSON) {
    changedLinesMap.set(mapping.lineNum, mapping);
  }

  // We rewrite the sourcemap by iterating the in-place 1:1 js sourcemap and
  // changing affected mappings.
  const pathToGeneratedJs = `${process.cwd()}/${originalDirectory}/`
  const rewriteImportsMapGenerator = new SourceMapGenerator(
      { file: mapGenerator._file, sourceRoot: pathToGeneratedJs });

  // We iterate over the in-place 1:1 js sourcemap mapping, if the mapping is
  // affected (it is in an affected line after the column change by
  // rewrite_imports.py), we create a new mapping with the correct delta.
  // Otherwise, the original mapping persists.
  jsToJsMapGenerator._mappings.unsortedForEach(mapping => {
    // Check if mapping is in a changed line after the rewritten column.
    const changedLineMapping = changedLinesMap.get(mapping.generatedLine);
    if (changedLineMapping &&
        changedLineMapping.generatedColumn <= mapping.generatedColumn) {
      const delta = changedLineMapping.rewrittenColumn -
          changedLineMapping.generatedColumn;
      // Keep in mind the entire build chain is:
      //  ts [original] -> js [generated] -> js [rewritten]
      // We are creating a new sourcemap for:
      //  ts [original] -> js [rewritten].
      // Nomenclature is confusing since sourcemaps only have an original file
      // and a generated file to map between, since we are creating a sourcemap
      // straight from the original to the rewritten step, we are treating the
      // rewritten file as the (new) generated file.
      addMapping(
          rewriteImportsMapGenerator, file, mapping.originalLine,
          mapping.originalColumn, mapping.generatedLine,
          mapping.generatedColumn + delta)
    } else {
      addMapping(
          rewriteImportsMapGenerator, file, mapping.originalLine,
          mapping.originalColumn, mapping.generatedLine,
          mapping.generatedColumn);
    }
  });

  // Rewrite the inline sourcemap in the last line of the rewritten output file.
  const newSourceMap64 =
      Buffer.from(rewriteImportsMapGenerator.toString()).toString('base64');

  const outputFile = fs.readFileSync(outputDirectory + '/' + file, 'utf8');
  const outputLines = outputFile.split('\n');
  // Kill the previous sourcemap for an overwrite.
  outputLines.pop();
  const outputFileContents = outputLines.join('\n') +
      '\n//# sourceMappingURL=data:application/json;base64,' + newSourceMap64;

  fs.writeFileSync(outputDirectory + '/' + file, outputFileContents);
}

function main() {
  const parser = new ArgumentParser({
    description:
        'Creates source maps for files preprocessed by rewrite_imports',
  });

  parser.addArgument(
      'originalTsDirectory',
      {help: 'Directory of original .ts file', action: 'store'});
  parser.addArgument('fileName', {
    help: 'name of compiled .js file with inline sourcemap',
    action: 'store'
  });
  parser.addArgument(
      'rewrittenJsDirectory',
      {help: 'Directory of the rewritten .js', action: 'store'});
  parser.addArgument(
      'changedMappings',
      {help: 'A JSON list of JSON mappings of line changes', action: 'store'});

  const argv = parser.parseArgs();

  processOneFile(
      argv.originalTsDirectory, argv.fileName, argv.rewrittenJsDirectory,
      argv.changedMappings);
}

main();