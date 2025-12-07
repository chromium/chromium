// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Codemod for adding a "declare" keyword before every Polymer property
// declaration in a class. To be used to update remaining Polymer UIs for the
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
        classMember.declare = true;
      }
    });
  });

  const outputOptions = {quote: 'single'};
  return root.toSource(outputOptions);
};
