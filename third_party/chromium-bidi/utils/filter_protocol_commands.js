#!/usr/bin/env node

const fs = require('fs');

function convertFile(filename) {
  const data = JSON.parse(fs.readFileSync(filename, 'utf-8'));

  const domains = [];
  for (const domain of data.domains) {
    domains.push({
      domain: domain.domain,
      commands: domain.commands.map(c => c.name),
    });
  }

  fs.writeFileSync(filename.replace('.json', '_commands_only.json'), JSON.stringify({domains}));
}

convertFile('node_modules/devtools-protocol/json/browser_protocol.json');
convertFile('node_modules/devtools-protocol/json/js_protocol.json');
