#include <vxlnetwork/lib/tomlconfig.hpp>
#include <vxlnetwork/node/node_pow_server_config.hpp>

vxlnetwork::error vxlnetwork::node_pow_server_config::serialize_toml (vxlnetwork::tomlconfig & toml) const
{
	toml.put ("enable", enable, "Value is currently not in use. Enable or disable starting Vxlnetwork PoW Server as a child process.\ntype:bool");
	toml.put ("vxlnetwork_pow_server_path", pow_server_path, "Value is currently not in use. Path to the vxlnetwork_pow_server executable.\ntype:string,path");
	return toml.get_error ();
}

vxlnetwork::error vxlnetwork::node_pow_server_config::deserialize_toml (vxlnetwork::tomlconfig & toml)
{
	toml.get_optional<bool> ("enable", enable);
	toml.get_optional<std::string> ("vxlnetwork_pow_server_path", pow_server_path);

	return toml.get_error ();
}
