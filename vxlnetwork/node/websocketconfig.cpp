#include <vxlnetwork/boost/asio/ip/address_v6.hpp>
#include <vxlnetwork/lib/tomlconfig.hpp>
#include <vxlnetwork/node/websocketconfig.hpp>

vxlnetwork::websocket::config::config (vxlnetwork::network_constants & network_constants) :
	network_constants{ network_constants },
	port{ network_constants.default_websocket_port },
	address{ boost::asio::ip::address_v6::loopback ().to_string () }
{
}

vxlnetwork::error vxlnetwork::websocket::config::serialize_toml (vxlnetwork::tomlconfig & toml) const
{
	toml.put ("enable", enabled, "Enable or disable WebSocket server.\ntype:bool");
	toml.put ("address", address, "WebSocket server bind address.\ntype:string,ip");
	toml.put ("port", port, "WebSocket server listening port.\ntype:uint16");
	return toml.get_error ();
}

vxlnetwork::error vxlnetwork::websocket::config::deserialize_toml (vxlnetwork::tomlconfig & toml)
{
	toml.get<bool> ("enable", enabled);
	boost::asio::ip::address_v6 address_l;
	toml.get_optional<boost::asio::ip::address_v6> ("address", address_l, boost::asio::ip::address_v6::loopback ());
	address = address_l.to_string ();
	toml.get<uint16_t> ("port", port);
	return toml.get_error ();
}
