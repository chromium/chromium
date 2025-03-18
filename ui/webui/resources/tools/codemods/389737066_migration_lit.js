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
        if (classMember.optional) {
          // Workaround for https://github.com/microsoft/TypeScript/issues/54240
          // where accessors aren't allowed to be optional. Instead of using "?"
          // add the "undefined" as part of the type.
          classMember.optional = false;
          const originalType = classMember.typeAnnotation.typeAnnotation;
          classMember.typeAnnotation.typeAnnotation =
              j.tsUnionType([originalType, j.tsUndefinedKeyword()]);
        }

        classMember.decorators = [];
        if (classMember.override) {
          // For an unknown reason jscodeshift drops the "override" keyword
          // once decorators are added below. Add an @override decorator here,
          // which will be replaced back the "override" keyword later using a
          // regular expression.
          const overrideDecorator = j.decorator(j.identifier('override'));
          classMember.decorators.push(overrideDecorator);
        }

        // Add an @accessor decorator since jscodeshift does not support the
        // "accessor" keyword yet. It will be replaced later using a regular
        // expression.
        const accessorDecorator = j.decorator(j.identifier('accessor'));
        classMember.decorators.push(accessorDecorator);
      }
    });
  });

  const outputOptions = {quote: 'single'};

  let out = root.toSource(outputOptions);

  // Restore the "override" keyword if it existed, and place it on the same line
  // as the property.
  out = out.replaceAll(/@override\n\s+/g, 'override ');

  // Replace @accessor with the 'accessor' keyword and place it on the same line
  // as the property.
  out = out.replaceAll(/@accessor\n\s+/g, 'accessor ');

  // Fix error: "error TS1029: 'private' modifier must precede 'accessor'
  // modifier."
  out = out.replaceAll(/\baccessor private\b/g, 'private accessor');
  out = out.replaceAll(/\baccessor protected\b/g, 'protected accessor');

  return out;
};
