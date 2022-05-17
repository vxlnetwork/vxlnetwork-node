#include <vxlnetwork/lib/threading.hpp>
#include <vxlnetwork/secure/store.hpp>

vxlnetwork::representative_visitor::representative_visitor (vxlnetwork::transaction const & transaction_a, vxlnetwork::store & store_a) :
	transaction (transaction_a),
	store (store_a),
	result (0)
{
}

void vxlnetwork::representative_visitor::compute (vxlnetwork::block_hash const & hash_a)
{
	current = hash_a;
	while (result.is_zero ())
	{
		auto block (store.block.get (transaction, current));
		debug_assert (block != nullptr);
		block->visit (*this);
	}
}

void vxlnetwork::representative_visitor::send_block (vxlnetwork::send_block const & block_a)
{
	current = block_a.previous ();
}

void vxlnetwork::representative_visitor::receive_block (vxlnetwork::receive_block const & block_a)
{
	current = block_a.previous ();
}

void vxlnetwork::representative_visitor::open_block (vxlnetwork::open_block const & block_a)
{
	result = block_a.hash ();
}

void vxlnetwork::representative_visitor::change_block (vxlnetwork::change_block const & block_a)
{
	result = block_a.hash ();
}

void vxlnetwork::representative_visitor::state_block (vxlnetwork::state_block const & block_a)
{
	result = block_a.hash ();
}

vxlnetwork::read_transaction::read_transaction (std::unique_ptr<vxlnetwork::read_transaction_impl> read_transaction_impl) :
	impl (std::move (read_transaction_impl))
{
}

void * vxlnetwork::read_transaction::get_handle () const
{
	return impl->get_handle ();
}

void vxlnetwork::read_transaction::reset () const
{
	impl->reset ();
}

void vxlnetwork::read_transaction::renew () const
{
	impl->renew ();
}

void vxlnetwork::read_transaction::refresh () const
{
	reset ();
	renew ();
}

vxlnetwork::write_transaction::write_transaction (std::unique_ptr<vxlnetwork::write_transaction_impl> write_transaction_impl) :
	impl (std::move (write_transaction_impl))
{
	/*
	 * For IO threads, we do not want them to block on creating write transactions.
	 */
	debug_assert (vxlnetwork::thread_role::get () != vxlnetwork::thread_role::name::io);
}

void * vxlnetwork::write_transaction::get_handle () const
{
	return impl->get_handle ();
}

void vxlnetwork::write_transaction::commit ()
{
	impl->commit ();
}

void vxlnetwork::write_transaction::renew ()
{
	impl->renew ();
}

void vxlnetwork::write_transaction::refresh ()
{
	impl->commit ();
	impl->renew ();
}

bool vxlnetwork::write_transaction::contains (vxlnetwork::tables table_a) const
{
	return impl->contains (table_a);
}

// clang-format off
vxlnetwork::store::store (
	vxlnetwork::block_store & block_store_a,
	vxlnetwork::frontier_store & frontier_store_a,
	vxlnetwork::account_store & account_store_a,
	vxlnetwork::pending_store & pending_store_a,
	vxlnetwork::unchecked_store & unchecked_store_a,
	vxlnetwork::online_weight_store & online_weight_store_a,
	vxlnetwork::pruned_store & pruned_store_a,
	vxlnetwork::peer_store & peer_store_a,
	vxlnetwork::confirmation_height_store & confirmation_height_store_a,
	vxlnetwork::final_vote_store & final_vote_store_a,
	vxlnetwork::version_store & version_store_a
) :
	block (block_store_a),
	frontier (frontier_store_a),
	account (account_store_a),
	pending (pending_store_a),
	unchecked (unchecked_store_a),
	online_weight (online_weight_store_a),
	pruned (pruned_store_a),
	peer (peer_store_a),
	confirmation_height (confirmation_height_store_a),
	final_vote (final_vote_store_a),
	version (version_store_a)
{
}
// clang-format on

auto vxlnetwork::unchecked_store::equal_range (vxlnetwork::transaction const & transaction, vxlnetwork::block_hash const & dependency) -> std::pair<iterator, iterator>
{
	vxlnetwork::unchecked_key begin_l{ dependency, 0 };
	vxlnetwork::unchecked_key end_l{ vxlnetwork::block_hash{ dependency.number () + 1 }, 0 };
	// Adjust for edge case where number () + 1 wraps around.
	auto end_iter = begin_l.previous < end_l.previous ? lower_bound (transaction, end_l) : end ();
	return std::make_pair (lower_bound (transaction, begin_l), std::move (end_iter));
}

auto vxlnetwork::unchecked_store::full_range (vxlnetwork::transaction const & transaction) -> std::pair<iterator, iterator>
{
	return std::make_pair (begin (transaction), end ());
}

std::vector<vxlnetwork::unchecked_info> vxlnetwork::unchecked_store::get (vxlnetwork::transaction const & transaction, vxlnetwork::block_hash const & dependency)
{
	auto range = equal_range (transaction, dependency);
	std::vector<vxlnetwork::unchecked_info> result;
	auto & i = range.first;
	auto & n = range.second;
	for (; i != n; ++i)
	{
		auto const & key = i->first;
		auto const & value = i->second;
		debug_assert (key.hash == value.block->hash ());
		result.push_back (value);
	}
	return result;
}
