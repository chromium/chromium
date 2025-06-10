# org.chromium.ui.insets

##### Update Jun 9th, 2025

This package provides utilities and classes for managing and observing system
window insets (such as the on-screen soft keyboard, status bar, navigation bar,
and display cutouts) in Android views within Chromium. It allows components to
react to changes in these insets and consume them as needed.

### InsetObserver

The `InsetObserver` class is the core component for managing and dispatching
system window insets. It handles the interaction between the Android system
`WindowInsetsCompat` updates and the various components within Chromium that
need to react to these changes.

#### Inset Consumer Prioritization

The `InsetObserver` allows multiple components (`WindowInsetsConsumer`
instances) to process and potentially consume window insets before they are
dispatched to general observers. This processing happens in a specific order
determined by the `InsetConsumerSource` enum.

When the system calls `onApplyWindowInsets`, the `InsetObserver` iterates
through its registered consumers based on their `InsetConsumerSource` value.
Consumers with a lower `InsetConsumerSource` value have higher priority and
process the insets first. Each consumer receives the current
`WindowInsetsCompat` object and can return a modified version, effectively
"consuming" parts of the insets. The modified insets are then passed to the next
consumer in the priority list.

The `InsetConsumerSource` enum defines the processing order, with lower values
indicating higher priority. This priority system ensures that critical
components can handle and potentially consume insets before less critical ones,
allowing for fine-grained control over how insets affect the layout.

**Important:** When adding or removing a `WindowInsetsConsumer`, the caller is
recommended to explicitly call `retriggerOnApplyWindowInsets`, unless the caller
is registering multiple consumers and / or observers at a time. This action
forces the `InsetObserver` to re-dispatch the current insets through the updated
list of consumers, ensuring that all consumers in the stream receive a fresh set
of insets processed according to the new sequence.

#### Insets Reading

The `InsetObserver` class is created very early in the activity, during
`#onCreate` by design, and attached to the root view of the Chrome activity
(`ChromeBaseAppCompatActivity`) it lives in. This way, this class will always
receive the insets that are dispatched at the root level, and store them as the
source of truth. Due to timing reasons, `View#getRootWindowInsets` can give
unpredictable insets during activity initialization, so it is recommended to
always read or observe the insets that are stored in `InsetObserver`.

#### WindowInsetsObserver

The `WindowInsetsObserver` is an interface that components can implement to be
notified whenever the system window insets change. Unlike
`WindowInsetsConsumer`s which can modify and consume insets,
`WindowInsetsObserver`s are purely for reacting to the final state of the insets
after all consumers have processed them.

Note that different observer interface will give you different types of insets
(pre v.s. post consumption). Please read the javadocs for each interface method.
