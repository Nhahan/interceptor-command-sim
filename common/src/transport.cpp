#include "transport_internal.hpp"

#include <stdexcept>

namespace icss::net {

std::unique_ptr<TransportBackend> make_transport(BackendKind kind, const icss::core::RuntimeConfig& config) {
    switch (kind) {
    case BackendKind::InProcess:
        return detail::make_inprocess_transport(config);
    case BackendKind::SocketLive:
        return detail::make_socket_live_transport(config);
    }
    throw std::logic_error("unsupported transport backend kind");
}

}  // namespace icss::net
