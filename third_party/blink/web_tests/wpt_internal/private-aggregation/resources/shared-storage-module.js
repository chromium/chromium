class ContributeToHistogramOperation {
  async run(data) {
    if (data.enableDebugMode) {
      privateAggregation.enableDebugMode(data.enableDebugModeArgs);
    }
    for (const contribution of data.contributions) {
      if (contribution.event) {
        privateAggregation.contributeToHistogramOnEvent(
            contribution.event, contribution);
      } else {
        privateAggregation.contributeToHistogram(contribution);
      }
    }
    if (data.enableDebugModeAfterOp) {
      privateAggregation.enableDebugMode(data.enableDebugModeArgs);
    }
    if (data.exceptionToThrow) {
      throw data.exceptionToThrow;
    }
  }
}

register('contribute-to-histogram', ContributeToHistogramOperation);
