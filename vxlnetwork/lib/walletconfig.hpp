#pragma once

#include <vxlnetwork/lib/errors.hpp>
#include <vxlnetwork/lib/numbers.hpp>

#include <string>

namespace vxlnetwork
{
class tomlconfig;

/** Configuration options for the Qt wallet */
class wallet_config final
{
public:
	wallet_config ();
	/** Update this instance by parsing the given wallet and account */
	vxlnetwork::error parse (std::string const & wallet_a, std::string const & account_a);
	vxlnetwork::error serialize_toml (vxlnetwork::tomlconfig & toml_a) const;
	vxlnetwork::error deserialize_toml (vxlnetwork::tomlconfig & toml_a);
	vxlnetwork::wallet_id wallet;
	vxlnetwork::account account{};
};
}
