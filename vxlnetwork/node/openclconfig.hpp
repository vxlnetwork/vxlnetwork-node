#pragma once

#include <vxlnetwork/lib/errors.hpp>

namespace vxlnetwork
{
class tomlconfig;
class opencl_config
{
public:
	opencl_config () = default;
	opencl_config (unsigned, unsigned, unsigned);
	vxlnetwork::error serialize_toml (vxlnetwork::tomlconfig &) const;
	vxlnetwork::error deserialize_toml (vxlnetwork::tomlconfig &);
	unsigned platform{ 0 };
	unsigned device{ 0 };
	unsigned threads{ 1024 * 1024 };
};
}
