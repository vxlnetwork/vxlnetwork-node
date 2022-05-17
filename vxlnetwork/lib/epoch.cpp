#include <vxlnetwork/lib/epoch.hpp>
#include <vxlnetwork/lib/utility.hpp>

vxlnetwork::link const & vxlnetwork::epochs::link (vxlnetwork::epoch epoch_a) const
{
	return epochs_m.at (epoch_a).link;
}

bool vxlnetwork::epochs::is_epoch_link (vxlnetwork::link const & link_a) const
{
	return std::any_of (epochs_m.begin (), epochs_m.end (), [&link_a] (auto const & item_a) { return item_a.second.link == link_a; });
}

vxlnetwork::public_key const & vxlnetwork::epochs::signer (vxlnetwork::epoch epoch_a) const
{
	return epochs_m.at (epoch_a).signer;
}

vxlnetwork::epoch vxlnetwork::epochs::epoch (vxlnetwork::link const & link_a) const
{
	auto existing (std::find_if (epochs_m.begin (), epochs_m.end (), [&link_a] (auto const & item_a) { return item_a.second.link == link_a; }));
	debug_assert (existing != epochs_m.end ());
	return existing->first;
}

void vxlnetwork::epochs::add (vxlnetwork::epoch epoch_a, vxlnetwork::public_key const & signer_a, vxlnetwork::link const & link_a)
{
	debug_assert (epochs_m.find (epoch_a) == epochs_m.end ());
	epochs_m[epoch_a] = { signer_a, link_a };
}

bool vxlnetwork::epochs::is_sequential (vxlnetwork::epoch epoch_a, vxlnetwork::epoch new_epoch_a)
{
	auto head_epoch = std::underlying_type_t<vxlnetwork::epoch> (epoch_a);
	bool is_valid_epoch (head_epoch >= std::underlying_type_t<vxlnetwork::epoch> (vxlnetwork::epoch::epoch_0));
	return is_valid_epoch && (std::underlying_type_t<vxlnetwork::epoch> (new_epoch_a) == (head_epoch + 1));
}

std::underlying_type_t<vxlnetwork::epoch> vxlnetwork::normalized_epoch (vxlnetwork::epoch epoch_a)
{
	// Currently assumes that the epoch versions in the enum are sequential.
	auto start = std::underlying_type_t<vxlnetwork::epoch> (vxlnetwork::epoch::epoch_0);
	auto end = std::underlying_type_t<vxlnetwork::epoch> (epoch_a);
	debug_assert (end >= start);
	return end - start;
}
