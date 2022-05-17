#pragma once

#include <vxlnetwork/lib/config.hpp>
#include <vxlnetwork/lib/errors.hpp>

#include <memory>

namespace vxlnetwork
{
class tomlconfig;
class tls_config;
namespace websocket
{
	/** websocket configuration */
	class config final
	{
	public:
		config (vxlnetwork::network_constants & network_constants);
		vxlnetwork::error deserialize_toml (vxlnetwork::tomlconfig & toml_a);
		vxlnetwork::error serialize_toml (vxlnetwork::tomlconfig & toml) const;
		vxlnetwork::network_constants & network_constants;
		bool enabled{ false };
		uint16_t port;
		std::string address;
		/** Optional TLS config */
		std::shared_ptr<vxlnetwork::tls_config> tls_config;
	};
}
}
