const routerRules = {
  'condition-runningstatus-running-network':
      {condition: {runningStatus: 'running'}, source: 'network'},
  'condition-runningstatus-notrunning-network':
      {condition: {runningStatus: 'not-running'}, source: 'network'},
};

export {routerRules};
