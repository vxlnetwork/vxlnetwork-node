#pragma once

#include <vxlnetwork/lib/config.hpp>
#include <vxlnetwork/lib/errors.hpp>

#include <string>
#include <thread>
#include <vector>

#ifdef VXLNETWORK_SECURE_RPC
#include <boost/asio/ssl/context.hpp>
#endif

namespace boost::filesystem
{
class path;
}

namespace vxlnetwork
{
class logger_mt;
class jsonconfig;
class tomlconfig;

/** Configuration options for secure RPC and WebSocket connections */
class tls_config final
{
public:
	vxlnetwork::error serialize_toml (vxlnetwork::tomlconfig &) const;
	vxlnetwork::error deserialize_toml (vxlnetwork::tomlconfig &);

	/** If true, enable TLS for RPC (only allow https, otherwise only allow http) */
	bool enable_https{ false };

	/** If true, enable TLS for WebSocket (only allow wss, otherwise only allow ws) */
	bool enable_wss{ false };

	/** If true, log certificate verification details */
	bool verbose_logging{ false };

	/** Must be set if the private key PEM is password protected */
	std::string server_key_passphrase;

	/** Path to certificate- or chain file. Must be PEM formatted. */
	std::string server_cert_path;

	/** Path to private key file. Must be PEM formatted.*/
	std::string server_key_path;

	/** Path to dhparam file */
	std::string server_dh_path;

	/** Optional path to directory containing client certificates */
	std::string client_certs_path;

#ifdef VXLNETWORK_SECURE_RPC
	/** The context needs to be shared between sessions to make resumption work */
	boost::asio::ssl::context ssl_context{ boost::asio::ssl::context::tlsv12_server };
#endif
};

vxlnetwork::error read_tls_config_toml (boost::filesystem::path const & data_path_a, vxlnetwork::tls_config & config_a, vxlnetwork::logger_mt & logger_a, std::vector<std::string> const & config_overrides = std::vector<std::string> ());
}
