#pragma once

// Only the opt-in OHOS TLS build includes this adapter. Do not replace libc,
// embed a host CA bundle, or disable chain/hostname verification.
#if defined(__OHOS__) && defined(PICO_OHOS_SYSTEM_TLS)
#include <network/netstack/net_ssl/net_ssl_c.h>
#include <cstring>
#include <stdexcept>
#include <string>

template <typename Client>
static void common_ohos_system_tls(Client & client, const std::string & hostname) {
    NetStack_Certificates certificates{};
    struct cleanup {
        NetStack_Certificates * certificates;
        ~cleanup() { OH_Netstack_DestroyCertificatesContent(certificates); }
    } owner{&certificates};
    const int32_t status = OH_NetStack_GetCertificatesForHostName(hostname.c_str(), &certificates);
    if (status != 0 || certificates.length == 0 || certificates.length > 4096 || !certificates.content) {
        throw std::runtime_error("OHOS system trust anchors unavailable: " + std::to_string(status));
    }
    std::string pem;
    for (size_t i = 0; i < certificates.length; ++i) {
        if (!certificates.content[i]) {
            throw std::runtime_error("OHOS system trust anchor is null");
        }
        const size_t size = strnlen(certificates.content[i], 65536);
        if (size == 0 || size == 65536 || pem.size() + size + 1 > 4 * 1024 * 1024) {
            throw std::runtime_error("OHOS system trust anchors exceed bounds");
        }
        pem.append(certificates.content[i], size);
        pem.push_back('\n');
    }
    client.load_ca_cert_store(pem.data(), pem.size());
    client.enable_server_certificate_verification(true);
    client.enable_server_hostname_verification(true);
}
#endif
