#ifndef WOLWIC_BROWSER_WEBDATA_SERVICES_WEB_DATA_SERVICE_FACTORY_H_
#define WOLWIC_BROWSER_WEBDATA_SERVICES_WEB_DATA_SERVICE_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace wolvic {

// Singleton that owns all WebDataServiceWrappers and associates them with
// Profiles.
class WebDataServiceFactory
    : public webdata_services::WebDataServiceWrapperFactory {
 public:
  // Returns the WebDataServiceWrapper associated with the |context|.
  static WebDataServiceWrapper* GetForContext(content::BrowserContext* context,
                                              ServiceAccessType access_type);

  static WebDataServiceWrapper* GetForContextIfExists(
      content::BrowserContext* context,
      ServiceAccessType access_type);

  WebDataServiceFactory(const WebDataServiceFactory&) = delete;
  WebDataServiceFactory& operator=(const WebDataServiceFactory&) = delete;

  static WebDataServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<WebDataServiceFactory>;

  WebDataServiceFactory();
  ~WebDataServiceFactory() override;

  // |BrowserContextKeyedServiceFactory| methods:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_WEBDATA_SERVICES_WEB_DATA_SERVICE_FACTORY_H_
