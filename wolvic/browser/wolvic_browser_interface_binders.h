#ifndef WOLVIC_BROWSER_CHROME_BROWSER_INTERFACE_BINDERS_H_
#define WOLVIC_BROWSER_CHROME_BROWSER_INTERFACE_BINDERS_H_

#include "mojo/public/cpp/bindings/binder_map.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace wolvic {
namespace internal {

// The mechanism implemented by the PopulateChrome*FrameBinders() functions
// below will replace interface registries and binders used for handling
// InterfaceProvider's GetInterface() calls (see crbug.com/718652).

// PopulateChromeFrameBinders() registers BrowserInterfaceBroker's
// GetInterface() handler callbacks for chrome-specific document-scoped
// interfaces.
void PopulateWolvicFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host);


} // namespace internal

} // namespace wolvic


#endif // WOLVIC_BROWSER_CHROME_BROWSER_INTERFACE_BINDERS_H_
