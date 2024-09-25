use rstest::*;

enum Application {
    Python,
    Node,
    Go,
}

enum Method {
    GET,
    POST,
    PUT,
    HEAD,
}

#[rstest]
fn name_values(
    #[values(Application::Python, Application::Node, Application::Go)] _val: Application,
    #[values(Method::GET, Method::POST, Method::PUT, Method::HEAD)] _method: Method,
) {
}
