#pragma once

#include <vxlnetwork/lib/rpcconfig.hpp>

#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/filesystem.hpp>

#include <string>

namespace vxlnetwork
{
class tomlconfig;

inline std::string get_default_pow_server_filepath ()
{
	boost::system::error_code err;
	auto running_executable_filepath = boost::dll::program_location (err);

	// Construct the vxlnetwork_pow_server executable file path based on where the currently running executable is found.
	auto pow_server_filepath = running_executable_filepath.parent_path () / "vxlnetwork_pow_server";
	if (running_executable_filepath.has_extension ())
	{
		pow_server_filepath.replace_extension (running_executable_filepath.extension ());
	}

	return pow_server_filepath.string ();
}

class node_pow_server_config final
{
public:
	vxlnetwork::error serialize_toml (vxlnetwork::tomlconfig & toml) const;
	vxlnetwork::error deserialize_toml (vxlnetwork::tomlconfig & toml);

	bool enable{ false };
	std::string pow_server_path{ vxlnetwork::get_default_pow_server_filepath () };
};
}
