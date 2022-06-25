# WebUI HelpBubble Implementation (Frontend)

[Backend documentation can be found here.](/components/user_education/webui/README.md)

Allows a WebUI page to support Polymer-based, blue material design ("Navi")
[HelpBubble](/components/user_education/common/help_bubble.h)s that can be shown in the course of a
[Feature Promo](/components/user_education/common/feature_promo_controller.h) or
[Tutorial](/components/user_education/common/tutorial.h).

## Usage

Please start with the instructions in the
[Backend Usage Guide](/components/user_education/webui/README.md#usage), and
proceed here when you reach the appropriate step.

Once you have performed setup on the backend:

 * Add a [HelpBubble](./help_bubble.ts) element to your Polymer component's
   HTML file as a sibling to the element(s) you wish to anchor the help bubble
   to, as in:<br/>`<help-bubble id="..."></help-bubble>`).

 * Add [HelpBubbleMixin](./help_bubble_mixin.ts) to your Polymer component.

 * In your component's `ready()` method, call
   `HelpBubbleMixin.registerHelpBubbleIdentifier()`.

   * The first parameter should be the name of the
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

## Limitations

Currently the frontend has the following limitations (many of these will be
relaxed or removed in the near future):

 * Only one `<help-bubble>` element may be present per component.

 * The `<help-bubble>` must be a sibling of the element it will be anchored to.

 * While theoretically you can call `registerHelpBubbleIdentifier()` multiple
   times, the native code only (currently) supports a single identifier, so
   you may effectively only have one `<help-bubble>` and one target element per
   `WebUIController`.

 * Whether the native code believes that a help bubble can be shown in your
   component is based on the visibility of the component's root element, not a
   potential anchor element within it.

   * This means that the native code may believe that it has successfully shown
     a help bubble but the bubble will not actually display because the specific
     anchor element is hidden.

   * Until this is fixed, make sure that the target anchor element's visibility
     (e.g. `display` property) tracks with the component's.

Again, we have plans to fix most of these limitations, and will update this
document as appropriate.
