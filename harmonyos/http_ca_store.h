#pragma once

// CertificateManager supplies the read-only OS store through the hosting HAP.
// Never add trust anchors, disable chain/hostname checks or change other OSes.
#if defined(__OHOS__) && defined(CPPHTTPLIB_SSL_ENABLED)
#include <cstdlib>
template <typename Client>
static void common_ohos_http_ca_store(Client &client) {
    const char *file=std::getenv("SSL_CERT_FILE");
    const char *directory=std::getenv("SSL_CERT_DIR");
    const char *platform=std::getenv("LLAMA_OHOS_CA_DIRECTORY");
    if (file && *file) client.set_ca_cert_path(file, "");
    else if (platform && *platform) client.set_ca_cert_path("", platform);
    else if (directory && *directory) client.set_ca_cert_path("", directory);
    client.enable_server_certificate_verification(true);
    client.enable_server_hostname_verification(true);
}
#endif
