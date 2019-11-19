This directory contains a set of basic types related to cursors that can
be used across the input pipeline, from the UI layer down to consumers such as
Blink. Despite these types being in the ```ui``` namespace for simplicity, in
general, the ```ui``` namespace is not able to be included from all those
components.
Please do not add code/types that should not be reachable from Blink.