#include <vxlnetwork/lib/rep_weights.hpp>
#include <vxlnetwork/secure/store.hpp>

void vxlnetwork::rep_weights::representation_add (vxlnetwork::account const & source_rep_a, vxlnetwork::uint128_t const & amount_a)
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> guard (mutex);
	auto source_previous (get (source_rep_a));
	put (source_rep_a, source_previous + amount_a);
}

void vxlnetwork::rep_weights::representation_add_dual (vxlnetwork::account const & source_rep_1, vxlnetwork::uint128_t const & amount_1, vxlnetwork::account const & source_rep_2, vxlnetwork::uint128_t const & amount_2)
{
	if (source_rep_1 != source_rep_2)
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> guard (mutex);
		auto source_previous_1 (get (source_rep_1));
		put (source_rep_1, source_previous_1 + amount_1);
		auto source_previous_2 (get (source_rep_2));
		put (source_rep_2, source_previous_2 + amount_2);
	}
	else
	{
		representation_add (source_rep_1, amount_1 + amount_2);
	}
}

void vxlnetwork::rep_weights::representation_put (vxlnetwork::account const & account_a, vxlnetwork::uint128_union const & representation_a)
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> guard (mutex);
	put (account_a, representation_a);
}

vxlnetwork::uint128_t vxlnetwork::rep_weights::representation_get (vxlnetwork::account const & account_a) const
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> lk (mutex);
	return get (account_a);
}

/** Makes a copy */
std::unordered_map<vxlnetwork::account, vxlnetwork::uint128_t> vxlnetwork::rep_weights::get_rep_amounts () const
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> guard (mutex);
	return rep_amounts;
}

void vxlnetwork::rep_weights::copy_from (vxlnetwork::rep_weights & other_a)
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> guard_this (mutex);
	vxlnetwork::lock_guard<vxlnetwork::mutex> guard_other (other_a.mutex);
	for (auto const & entry : other_a.rep_amounts)
	{
		auto prev_amount (get (entry.first));
		put (entry.first, prev_amount + entry.second);
	}
}

void vxlnetwork::rep_weights::put (vxlnetwork::account const & account_a, vxlnetwork::uint128_union const & representation_a)
{
	auto it = rep_amounts.find (account_a);
	auto amount = representation_a.number ();
	if (it != rep_amounts.end ())
	{
		it->second = amount;
	}
	else
	{
		rep_amounts.emplace (account_a, amount);
	}
}

vxlnetwork::uint128_t vxlnetwork::rep_weights::get (vxlnetwork::account const & account_a) const
{
	auto it = rep_amounts.find (account_a);
	if (it != rep_amounts.end ())
	{
		return it->second;
	}
	else
	{
		return vxlnetwork::uint128_t{ 0 };
	}
}

std::unique_ptr<vxlnetwork::container_info_component> vxlnetwork::collect_container_info (vxlnetwork::rep_weights const & rep_weights, std::string const & name)
{
	size_t rep_amounts_count;

	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> guard (rep_weights.mutex);
		rep_amounts_count = rep_weights.rep_amounts.size ();
	}
	auto sizeof_element = sizeof (decltype (rep_weights.rep_amounts)::value_type);
	auto composite = std::make_unique<vxlnetwork::container_info_composite> (name);
	composite->add_component (std::make_unique<vxlnetwork::container_info_leaf> (container_info{ "rep_amounts", rep_amounts_count, sizeof_element }));
	return composite;
}
