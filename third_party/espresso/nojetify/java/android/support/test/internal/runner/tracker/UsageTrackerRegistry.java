package android.support.test.internal.runner.tracker;

/**
 * A registry to hold the global {@link UsageTracker}.
 * <p/>
 * Instrumentation will configure this registry at startup.
 */
public final class UsageTrackerRegistry {
    // By default we use a NoOp class.
    private static volatile UsageTracker instance = new UsageTracker.NoOpUsageTracker();

    public static void registerInstance(UsageTracker tracker) {
        instance = tracker;
    }

    public static UsageTracker getInstance() {
        return instance;
    }

    private UsageTrackerRegistry() {}
}
