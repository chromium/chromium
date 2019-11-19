This directory contains a set of basic types related to input events that can
be used across the input pipeline, from the UI layer down to consumers such as
CC and Blink. These types are in the ```ui::input_types``` namespace to
distinguish from the ```ui``` namespace which in general is not able to be
included from all of those components.
Please do not add code/types that should not be reachable from Blink.
