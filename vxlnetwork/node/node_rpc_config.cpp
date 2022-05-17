#include <vxlnetwork/lib/config.hpp>
#include <vxlnetwork/lib/jsonconfig.hpp>
#include <vxlnetwork/lib/tomlconfig.hpp>
#include <vxlnetwork/node/node_rpc_config.hpp>

#include <boost/property_tree/ptree.hpp>

vxlnetwork::error vxlnetwork::node_rpc_config::serialize_toml (vxlnetwork::tomlconfig & toml) const
{
	toml.put ("enable_sign_hash", enable_sign_hash, "Allow or disallow signing of hashes.\ntype:bool");

	vxlnetwork::tomlconfig child_process_l;
	child_process_l.put ("enable", child_process.enable, "Enable or disable RPC child process. If false, an in-process RPC server is used.\ntype:bool");
	child_process_l.put ("rpc_path", child_process.rpc_path, "Path to the vxlnetwork_rpc executable. Must be set if child process is enabled.\ntype:string,path");
	toml.put_child ("child_process", child_process_l);
	return toml.get_error ();
}

vxlnetwork::error vxlnetwork::node_rpc_config::deserialize_toml (vxlnetwork::tomlconfig & toml)
{
	toml.get_optional ("enable_sign_hash", enable_sign_hash);
	toml.get_optional<bool> ("enable_sign_hash", enable_sign_hash);

	auto child_process_l (toml.get_optional_child ("child_process"));
	if (child_process_l)
	{
		child_process_l->get_optional<bool> ("enable", child_process.enable);
		child_process_l->get_optional<std::string> ("rpc_path", child_process.rpc_path);
	}

	return toml.get_error ();
}

void vxlnetwork::node_rpc_config::set_request_callback (std::function<void (boost::property_tree::ptree const &)> callback_a)
{
	request_callback = std::move (callback_a);
}
