// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Codemod for adding a "accessor" keyword before every Lit property
// declaration in a class. To be used to update Lit UIs for the
// purposes of fixing cbug.com/389737066.

module.exports = function transformer(file, api) {
  const source = file.source;
  const j = api.jscodeshift;
  const root = j(source);

  const classProperties = new Set();
  root.find(j.Function, {key: {name: 'properties'}})
      .find(j.ObjectExpression)
      .forEach(p => {
        p.value.properties.forEach(property => {
          // Ignore nested ObjectExpressions.
          if (p.parentPath.value.type !== 'ReturnStatement') {
            return;
          }
          classProperties.add(property.key.name);
        });
      });

  root.find(j.ClassDeclaration).forEach(path => {
    path.node.body.body.forEach(classMember => {
      if (classMember.type === 'ClassProperty' &&
          classProperties.has(classMember.key.name)) {
        // Add an @accessor decorator since jscodeshift does not support the
        // "accessor" keyword yet. It will be replaced later using a regular
        // expression.
        const decorator = j.decorator(j.identifier('accessor'));
        classMember.decorators = [decorator];
      }
    });
  });

  const outputOptions = {quote: 'single'};

  let out = root.toSource(outputOptions);
  // Replace @accessor with the 'accessor' keyword and place it on the same line
  // as the property.
  out = out.replaceAll(/@accessor\n\s+/g, 'accessor ');

  // Fix error: "error TS1029: 'private' modifier must precede 'accessor'
  // modifier."
  out = out.replaceAll(/\baccessor private\b/g, 'private accessor');
  out = out.replaceAll(/\baccessor protected\b/g, 'protected accessor');

  return out;
};
