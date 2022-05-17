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
void release_assert_success (store_partial<Val, Derived_Store> const &, int const);

template <typename Val, typename Derived_Store>
class unchecked_store_partial : public unchecked_store
{
private:
	vxlnetwork::store_partial<Val, Derived_Store> & store;

	friend void release_assert_success<Val, Derived_Store> (store_partial<Val, Derived_Store> const &, int const);

public:
	unchecked_store_partial (vxlnetwork::store_partial<Val, Derived_Store> & store_a) :
		store (store_a){};

	void clear (vxlnetwork::write_transaction const & transaction_a) override
	{
		auto status = store.drop (transaction_a, tables::unchecked);
		release_assert_success (store, status);
	}

	void put (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::hash_or_account const & dependency, vxlnetwork::unchecked_info const & info_a) override
	{
		vxlnetwork::db_val<Val> info (info_a);
		auto status (store.put (transaction_a, tables::unchecked, vxlnetwork::unchecked_key{ dependency, info_a.block->hash () }, info));
		release_assert_success (store, status);
	}

	bool exists (vxlnetwork::transaction const & transaction_a, vxlnetwork::unchecked_key const & unchecked_key_a) override
	{
		vxlnetwork::db_val<Val> value;
		auto status (store.get (transaction_a, tables::unchecked, vxlnetwork::db_val<Val> (unchecked_key_a), value));
		release_assert (store.success (status) || store.not_found (status));
		return (store.success (status));
	}

	void del (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::unchecked_key const & key_a) override
	{
		auto status (store.del (transaction_a, tables::unchecked, key_a));
		release_assert_success (store, status);
	}

	vxlnetwork::store_iterator<vxlnetwork::unchecked_key, vxlnetwork::unchecked_info> end () const override
	{
		return vxlnetwork::store_iterator<vxlnetwork::unchecked_key, vxlnetwork::unchecked_info> (nullptr);
	}

	vxlnetwork::store_iterator<vxlnetwork::unchecked_key, vxlnetwork::unchecked_info> begin (vxlnetwork::transaction const & transaction_a) const override
	{
		return store.template make_iterator<vxlnetwork::unchecked_key, vxlnetwork::unchecked_info> (transaction_a, tables::unchecked);
	}

	vxlnetwork::store_iterator<vxlnetwork::unchecked_key, vxlnetwork::unchecked_info> lower_bound (vxlnetwork::transaction const & transaction_a, vxlnetwork::unchecked_key const & key_a) const override
	{
		return store.template make_iterator<vxlnetwork::unchecked_key, vxlnetwork::unchecked_info> (transaction_a, tables::unchecked, vxlnetwork::db_val<Val> (key_a));
	}

	size_t count (vxlnetwork::transaction const & transaction_a) override
	{
		return store.count (transaction_a, tables::unchecked);
	}

	void for_each_par (std::function<void (vxlnetwork::read_transaction const &, vxlnetwork::store_iterator<vxlnetwork::unchecked_key, vxlnetwork::unchecked_info>, vxlnetwork::store_iterator<vxlnetwork::unchecked_key, vxlnetwork::unchecked_info>)> const & action_a) const override
	{
		parallel_traversal<vxlnetwork::uint512_t> (
		[&action_a, this] (vxlnetwork::uint512_t const & start, vxlnetwork::uint512_t const & end, bool const is_last) {
			vxlnetwork::unchecked_key key_start (start);
			vxlnetwork::unchecked_key key_end (end);
			auto transaction (this->store.tx_begin_read ());
			action_a (transaction, this->lower_bound (transaction, key_start), !is_last ? this->lower_bound (transaction, key_end) : this->end ());
		});
	}
};

}
