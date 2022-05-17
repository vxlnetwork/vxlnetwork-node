#include <vxlnetwork/lib/jsonconfig.hpp>
#include <vxlnetwork/lib/tomlconfig.hpp>
#include <vxlnetwork/node/openclconfig.hpp>

vxlnetwork::opencl_config::opencl_config (unsigned platform_a, unsigned device_a, unsigned threads_a) :
	platform (platform_a),
	device (device_a),
	threads (threads_a)
{
}

vxlnetwork::error vxlnetwork::opencl_config::serialize_toml (vxlnetwork::tomlconfig & toml) const
{
	toml.put ("platform", platform);
	toml.put ("device", device);
	toml.put ("threads", threads);

	// Add documentation
	toml.doc ("platform", "OpenCL platform identifier");
	toml.doc ("device", "OpenCL device identifier");
	toml.doc ("threads", "OpenCL thread count");

	return toml.get_error ();
}

vxlnetwork::error vxlnetwork::opencl_config::deserialize_toml (vxlnetwork::tomlconfig & toml)
{
	toml.get_optional<unsigned> ("platform", platform);
	toml.get_optional<unsigned> ("device", device);
	toml.get_optional<unsigned> ("threads", threads);
	return toml.get_error ();
}
