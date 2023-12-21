/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * A class that computes and caches `ValidityStateFlags` for a component with
 * a given `State` interface.
 *
 * Cached performance before computing validity is important since constraint
 * validation must be checked frequently and synchronously when properties
 * change.
 *
 * @template State The expected interface of properties relevant to constraint
 *     validation.
 */
export declare abstract class Validator<State> {
    private readonly getCurrentState;
    /**
     * The previous state, used to determine if state changed and validation needs
     * to be re-computed.
     */
    private prevState?;
    /**
     * The current validity state and message. This is cached and returns if
     * constraint validation state does not change.
     */
    private currentValidity;
    /**
     * Creates a new validator.
     *
     * @param getCurrentState A callback that returns the current state of
     *     constraint validation-related properties.
     */
    constructor(getCurrentState: () => State);
    /**
     * Returns the current `ValidityStateFlags` and validation message for the
     * validator.
     *
     * If the constraint validation state has not changed, this will return a
     * cached result. This is important since `getValidity()` can be called
     * frequently in response to synchronous property changes.
     *
     * @return The current validity and validation message.
     */
    getValidity(): ValidityAndMessage;
    /**
     * Computes the `ValidityStateFlags` and validation message for a given set
     * of constraint validation properties.
     *
     * Implementations can use platform elements like `<input>` and `<select>` to
     * sync state and compute validation along with i18n'd messages. This function
     * may be expensive, and is only called when state changes.
     *
     * @param state The new state of constraint validation properties.
     * @return An object containing a `validity` property with
     *     `ValidityStateFlags` and a `validationMessage` property.
     */
    protected abstract computeValidity(state: State): ValidityAndMessage;
    /**
     * Checks if two states are equal. This is used to check against cached state
     * to see if validity needs to be re-computed.
     *
     * @param prev The previous state.
     * @param next The next state.
     * @return True if the states are equal, or false if not.
     */
    protected abstract equals(prev: State, next: State): boolean;
    /**
     * Creates a copy of a state. This is used to cache state and check if it
     * changes.
     *
     * Note: do NOT spread the {...state} to copy it. The actual state object is
     * a web component, and trying to spread its getter/setter properties won't
     * work.
     *
     * @param state The state to copy.
     * @return A copy of the state.
     */
    protected abstract copy(state: State): State;
}
/**
 * An object containing `ValidityStateFlags` and a corresponding validation
 * message.
 */
export interface ValidityAndMessage {
    /**
     * Validity flags.
     */
    validity: ValidityStateFlags;
    /**
     * The validation message for the associated flags. It may not be an empty
     * string if any of the validity flags are `true`.
     */
    validationMessage: string;
}
