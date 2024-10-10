#include "wolvic/browser/webdata_services/web_data_service_factory.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/webdata_services/web_data_service_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

// This class is derived from //chrome/webdata_service/WebDataServiceFactory.

namespace wolvic {

namespace {

// Callback to show error dialog on context load error.
void ContextErrorCallback(WebDataServiceWrapper::ErrorType error_type,
                          sql::InitStatus status,
                          const std::string& diagnostics) {
  // TODO(jfernandez): Implement this error management callback.
}

std::unique_ptr<KeyedService> BuildWebDataService(
    content::BrowserContext* context) {
  const base::FilePath& path = context->GetPath();
  return std::make_unique<WebDataServiceWrapper>(
      path, "" /* application locale */ ,
      content::GetUIThreadTaskRunner({}),
      base::BindRepeating(&ContextErrorCallback));
}

} // namespace

WebDataServiceFactory::WebDataServiceFactory() = default;

WebDataServiceFactory::~WebDataServiceFactory() = default;

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForContext(
    content::BrowserContext* context,
    ServiceAccessType access_type) {
  return GetForBrowserContext(context, access_type);
}

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForContextIfExists(
    content::BrowserContext* context,
    ServiceAccessType access_type) {
  return GetForBrowserContextIfExists(context, access_type);
}

// static
WebDataServiceFactory* WebDataServiceFactory::GetInstance() {
  static base::NoDestructor<WebDataServiceFactory> instance;
  return instance.get();
}

content::BrowserContext* WebDataServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Create a separate instance of the service for the Incognito context.
  return context;
}

std::unique_ptr<KeyedService>
WebDataServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildWebDataService(context);
}

bool WebDataServiceFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

} // wolvic
