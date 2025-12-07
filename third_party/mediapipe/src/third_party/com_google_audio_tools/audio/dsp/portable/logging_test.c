#include "audio/dsp/portable/logging.h"

#ifdef __cplusplus
#error This test must compile in C mode.
#endif

typedef struct Thing {
  int stuff;
} Thing;

static int things_made;

struct Thing* MakeThing() {
  ++things_made;
  return (Thing*) malloc(sizeof(Thing));
}

void DoesntEvaluateTwice() {
  things_made = 0;
  Thing* my_thing = ABSL_CHECK_NOTNULL(MakeThing());
  ABSL_CHECK(things_made == 1);
  free(my_thing);
}

int main(int argc, char** argv) {
  srand(0);
  DoesntEvaluateTwice();

  puts("PASS");
  return EXIT_SUCCESS;
}
