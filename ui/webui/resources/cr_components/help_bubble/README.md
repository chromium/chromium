# WebUI HelpBubble Implementation (Frontend)

[Backend documentation can be found here.](/components/user_education/webui/README.md)

Allows a WebUI page to support Polymer-based, blue material design ("Navi")
[HelpBubble](/components/user_education/common/help_bubble.h)s that can be shown in the course of a
[Feature Promo](/components/user_education/common/feature_promo_controller.h) or
[Tutorial](/components/user_education/common/tutorial.h).

This is done by associating HTML elements in a component with an
[ElementIdentifier](/ui/base/interaction/element_identifier.h) so they can be
referenced by a Tutorial step or a `FeaturePromoSpecification`.

Once elements are linked in this way, their visibility is reported via
[ElementTracker](/ui/base/interaction/element_tracker.h) and can be referenced
for any of the usual purposes (e.g. in tests or "hidden" Tutorial steps) and
not just for the purpose of anchoring a help bubble.

## Usage

Please start with the instructions in the
[Backend Usage Guide](/components/user_education/webui/README.md#usage), and
proceed here when you reach the appropriate step.

Once you have performed setup on the backend:

 * Add [HelpBubbleMixin](./help_bubble_mixin.ts) to your Polymer component.

 * In your component's `ready()` or `connectedCallback()` method, call
   `HelpBubbleMixin.registerHelpBubbleIdentifier()` one or more times.
 
   * The first parameter should be the name of an
     [ElementIdentifier](/ui/base/interaction/element_identifier.h) you
     specified when creating your
     [HelpBubbleHandler](/components/user_education/webui/help_bubble_handler.h)
     in your
     [WebUIController](/content/public/browser/web_ui_controller.h)

     * For elements declared with `DECLARE_ELEMENT_IDENTIFIER_VALUE()` this is
       just the name of the constant you specified.

   * The second parameter should be the HTML element id of the element you wish
     to anchor the help bubble to when it is displayed.

   * In this way, you effectively create a mapping between the native identifier
     and the anchor element.

   * You may add multiple mappings, though each ElementIdentifier name and each
     HTML id may only be mapped once.

   * You may also add mappings for elements you do not intend to anchor a help
     bubble to, but whose visibility you care able for a Tutorial step or
     interactive test.

   * It is rare, but if your anchor element is not immediately present in your
     component, you can instead wait to call `registerHelpBubbleIdentifier()`
     until after the element is created.

## Limitations

Currently the frontend has the following limitations (many of these will be
relaxed or removed in the near future):

 * Whether the native code believes that a help bubble can be shown in your
   component is based on the visibility of the corresponding anchor HTML element
   - the one with the ID you passed to
   `HelpBubbleMixin.registerHelpBubbleIdentifier()`.

   * Visibility is not determined relative to the current viewport but rather to
     the entire page. The viewport will automatically scroll to display the
     anchor element when the bubble is shown.

 * Some features of `HelpBubble` are not yet supported (or are not fully
   supported) in WebUI. Support for the following will be added in future
   updates:
   * Timeouts
   * Close button
   * Action buttons
   * Progress indicator
   * Most `user_education::HelpBubbleArrow` values
