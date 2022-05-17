#pragma once

#include <vxlnetwork/lib/errors.hpp>
#include <vxlnetwork/node/node_pow_server_config.hpp>
#include <vxlnetwork/node/node_rpc_config.hpp>
#include <vxlnetwork/node/nodeconfig.hpp>
#include <vxlnetwork/node/openclconfig.hpp>

#include <vector>

namespace vxlnetwork
{
class tomlconfig;
class daemon_config
{
public:
	daemon_config () = default;
	daemon_config (boost::filesystem::path const & data_path, vxlnetwork::network_params & network_params);
	vxlnetwork::error deserialize_toml (vxlnetwork::tomlconfig &);
	vxlnetwork::error serialize_toml (vxlnetwork::tomlconfig &);
	bool rpc_enable{ false };
	vxlnetwork::node_rpc_config rpc;
	vxlnetwork::node_config node;
	bool opencl_enable{ false };
	vxlnetwork::opencl_config opencl;
	vxlnetwork::node_pow_server_config pow_server;
	boost::filesystem::path data_path;
};

vxlnetwork::error read_node_config_toml (boost::filesystem::path const &, vxlnetwork::daemon_config & config_a, std::vector<std::string> const & config_overrides = std::vector<std::string> ());
}
