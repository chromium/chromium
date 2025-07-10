# Lottie Animations in Chromium

## What is Lottie?

Lottie is an open-source library that allows us to use high-quality, theme-aware animations rendered from After Effects directly in Chrome.

### Key benefits:

*   **Theme-Aware Illustrations**: Animations dynamically tailor their colors to the user's Chrome theme.
*   **Simplified Assets**: A single Lottie file replaces the need for separate light/dark mode and 1x/2x PNG assets.
*   **Density Independent**: Lottie files are vector-based and fully resizable without loss of quality.
*   **Smaller & Faster**: Lottie animations generally have a smaller file size and load faster than their PNG counterparts.

## How to integrate a new Lottie illustration

Follow these steps to integrate a new Lottie animation for the first time.

1.  **Add your JSON file** to an appropriate folder (e.g., `chrome/browser/resources`).

2.  **Define a new structured entry** in an appropriate grit resources file (`.grd`).
    *   Ensure the path to the file is correct.
    *   Use `brotli` compression.

    Example:
    ```xml
    <structure type="lottie" name="IDR_AUTOFILL_SAVE_PASSWORD_LOTTIE" file="resources/autofill/autofill_save_password.json" compress="brotli"/>
    ```

3.  **Retrieve the image** from `ResourceBundle`.
    ```cpp
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    ui::ImageModel image = bundle.GetThemedLottieImageNamed(IDR_YOUR_LOTTIE_IMAGE);
    auto image_view = std::make_unique<views::ImageView>(image);
    ```

## How to convert an existing PNG to a Lottie illustration

If you have an existing illustration in PNG format, follow these steps to convert it to a Lottie animation.

1.  **Reach out to your UX partner** with the PNG you would like to convert.
2.  Once you have received the Lottie JSON file, **double-check it** to ensure that Chrome color tokens are used (e.g., `cdds.sys.color...`).
3.  Follow the steps in [How to integrate a new Lottie illustration](#how-to-integrate-a-new-lottie-illustration).

### Notes:

*   For call sites previously using `ThemeTrackingNonAccessibleImageView`, ensure you call `image_view->GetViewAccessibility().SetIsIgnored(true)` to maintain the same behavior.
*   As an optional follow-up, you can delete the previous PNG files. This can be done as a single cleanup task once everything is implemented, just in case a revert is needed.