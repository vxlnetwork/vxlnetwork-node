#pragma once

#include <vxlnetwork/secure/store_partial.hpp>

namespace
{
template <typename T>
void parallel_traversal (std::function<void (T const &, T const &, bool const)> const & action);
}

namespace vxlnetwork
{
template <typename Val, typename Derived_Store>
class store_partial;

template <typename Val, typename Derived_Store>
void release_assert_success (store_partial<Val, Derived_Store> const & store, int const status);

template <typename Val, typename Derived_Store>
class frontier_store_partial : public frontier_store
{
private:
	vxlnetwork::store_partial<Val, Derived_Store> & store;

	friend void release_assert_success<Val, Derived_Store> (store_partial<Val, Derived_Store> const &, int const);

public:
	explicit frontier_store_partial (vxlnetwork::store_partial<Val, Derived_Store> & store_a) :
		store (store_a){};

	void put (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::block_hash const & block_a, vxlnetwork::account const & account_a) override
	{
		vxlnetwork::db_val<Val> account (account_a);
		auto status (store.put (transaction_a, tables::frontiers, block_a, account));
		release_assert_success (store, status);
	}

	vxlnetwork::account get (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & block_a) const override
	{
		vxlnetwork::db_val<Val> value;
		auto status (store.get (transaction_a, tables::frontiers, vxlnetwork::db_val<Val> (block_a), value));
		release_assert (store.success (status) || store.not_found (status));
		vxlnetwork::account result{};
		if (store.success (status))
		{
			result = static_cast<vxlnetwork::account> (value);
		}
		return result;
	}

	void del (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::block_hash const & block_a) override
	{
		auto status (store.del (transaction_a, tables::frontiers, block_a));
		release_assert_success (store, status);
	}

	vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::account> begin (vxlnetwork::transaction const & transaction_a) const override
	{
		return store.template make_iterator<vxlnetwork::block_hash, vxlnetwork::account> (transaction_a, tables::frontiers);
	}

	vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::account> begin (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const override
	{
		return store.template make_iterator<vxlnetwork::block_hash, vxlnetwork::account> (transaction_a, tables::frontiers, vxlnetwork::db_val<Val> (hash_a));
	}

	vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::account> end () const override
	{
		return vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::account> (nullptr);
	}

	void for_each_par (std::function<void (vxlnetwork::read_transaction const &, vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::account>, vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::account>)> const & action_a) const override
	{
		parallel_traversal<vxlnetwork::uint256_t> (
		[&action_a, this] (vxlnetwork::uint256_t const & start, vxlnetwork::uint256_t const & end, bool const is_last) {
			auto transaction (this->store.tx_begin_read ());
			action_a (transaction, this->begin (transaction, start), !is_last ? this->begin (transaction, end) : this->end ());
		});
	}
};

}
