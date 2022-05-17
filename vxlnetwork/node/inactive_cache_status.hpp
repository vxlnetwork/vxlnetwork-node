#pragma once

#include <vxlnetwork/lib/numbers.hpp>

namespace vxlnetwork
{
class inactive_cache_status final
{
public:
	bool bootstrap_started{ false };

	/** Did item reach config threshold to start an impromptu election? */
	bool election_started{ false };

	/** Did item reach votes quorum? (minimum config value) */
	bool confirmed{ false };

	/** Last votes tally for block */
	vxlnetwork::uint128_t tally{ 0 };

	bool operator!= (inactive_cache_status const other) const;

	std::string to_string () const;
};

}
