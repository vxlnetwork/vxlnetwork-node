#include <vxlnetwork/node/nodeconfig.hpp>
#include <vxlnetwork/node/online_reps.hpp>
#include <vxlnetwork/secure/ledger.hpp>
#include <vxlnetwork/secure/store.hpp>

vxlnetwork::online_reps::online_reps (vxlnetwork::ledger & ledger_a, vxlnetwork::node_config const & config_a) :
	ledger{ ledger_a },
	config{ config_a }
{
	if (!ledger.store.init_error ())
	{
		auto transaction (ledger.store.tx_begin_read ());
		trended_m = calculate_trend (transaction);
	}
}

void vxlnetwork::online_reps::observe (vxlnetwork::account const & rep_a)
{
	if (ledger.weight (rep_a) > 0)
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
		auto now = std::chrono::steady_clock::now ();
		auto new_insert = reps.get<tag_account> ().erase (rep_a) == 0;
		reps.insert ({ now, rep_a });
		auto cutoff = reps.get<tag_time> ().lower_bound (now - std::chrono::seconds (config.network_params.node.weight_period));
		auto trimmed = reps.get<tag_time> ().begin () != cutoff;
		reps.get<tag_time> ().erase (reps.get<tag_time> ().begin (), cutoff);
		if (new_insert || trimmed)
		{
			online_m = calculate_online ();
		}
	}
}

void vxlnetwork::online_reps::sample ()
{
	vxlnetwork::unique_lock<vxlnetwork::mutex> lock (mutex);
	vxlnetwork::uint128_t online_l = online_m;
	lock.unlock ();
	vxlnetwork::uint128_t trend_l;
	{
		auto transaction (ledger.store.tx_begin_write ({ tables::online_weight }));
		// Discard oldest entries
		while (ledger.store.online_weight.count (transaction) >= config.network_params.node.max_weight_samples)
		{
			auto oldest (ledger.store.online_weight.begin (transaction));
			debug_assert (oldest != ledger.store.online_weight.end ());
			ledger.store.online_weight.del (transaction, oldest->first);
		}
		ledger.store.online_weight.put (transaction, std::chrono::system_clock::now ().time_since_epoch ().count (), online_l);
		trend_l = calculate_trend (transaction);
	}
	lock.lock ();
	trended_m = trend_l;
}

vxlnetwork::uint128_t vxlnetwork::online_reps::calculate_online () const
{
	vxlnetwork::uint128_t current;
	for (auto & i : reps)
	{
		current += ledger.weight (i.account);
	}
	return current;
}

vxlnetwork::uint128_t vxlnetwork::online_reps::calculate_trend (vxlnetwork::transaction & transaction_a) const
{
	std::vector<vxlnetwork::uint128_t> items;
	items.reserve (config.network_params.node.max_weight_samples + 1);
	items.push_back (config.online_weight_minimum.number ());
	for (auto i (ledger.store.online_weight.begin (transaction_a)), n (ledger.store.online_weight.end ()); i != n; ++i)
	{
		items.push_back (i->second.number ());
	}
	vxlnetwork::uint128_t result;
	// Pick median value for our target vote weight
	auto median_idx = items.size () / 2;
	nth_element (items.begin (), items.begin () + median_idx, items.end ());
	result = items[median_idx];
	return result;
}

vxlnetwork::uint128_t vxlnetwork::online_reps::trended () const
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
	return trended_m;
}

vxlnetwork::uint128_t vxlnetwork::online_reps::online () const
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
	return online_m;
}

vxlnetwork::uint128_t vxlnetwork::online_reps::delta () const
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
	// Using a larger container to ensure maximum precision
	auto weight = static_cast<vxlnetwork::uint256_t> (std::max ({ online_m, trended_m, config.online_weight_minimum.number () }));
	return ((weight * online_weight_quorum) / 100).convert_to<vxlnetwork::uint128_t> ();
}

std::vector<vxlnetwork::account> vxlnetwork::online_reps::list ()
{
	std::vector<vxlnetwork::account> result;
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
	std::for_each (reps.begin (), reps.end (), [&result] (rep_info const & info_a) { result.push_back (info_a.account); });
	return result;
}

void vxlnetwork::online_reps::clear ()
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
	reps.clear ();
	online_m = 0;
}

std::unique_ptr<vxlnetwork::container_info_component> vxlnetwork::collect_container_info (online_reps & online_reps, std::string const & name)
{
	std::size_t count;
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> guard (online_reps.mutex);
		count = online_reps.reps.size ();
	}

	auto sizeof_element = sizeof (decltype (online_reps.reps)::value_type);
	auto composite = std::make_unique<container_info_composite> (name);
	composite->add_component (std::make_unique<container_info_leaf> (container_info{ "reps", count, sizeof_element }));
	return composite;
}
