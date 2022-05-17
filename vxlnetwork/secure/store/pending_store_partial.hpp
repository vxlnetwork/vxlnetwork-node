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
class pending_store_partial : public pending_store
{
private:
	vxlnetwork::store_partial<Val, Derived_Store> & store;

	friend void release_assert_success<Val, Derived_Store> (store_partial<Val, Derived_Store> const &, int const);

public:
	explicit pending_store_partial (vxlnetwork::store_partial<Val, Derived_Store> & store_a) :
		store (store_a){};

	void put (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::pending_key const & key_a, vxlnetwork::pending_info const & pending_info_a) override
	{
		vxlnetwork::db_val<Val> pending (pending_info_a);
		auto status = store.put (transaction_a, tables::pending, key_a, pending);
		release_assert_success (store, status);
	}

	void del (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::pending_key const & key_a) override
	{
		auto status = store.del (transaction_a, tables::pending, key_a);
		release_assert_success (store, status);
	}

	bool get (vxlnetwork::transaction const & transaction_a, vxlnetwork::pending_key const & key_a, vxlnetwork::pending_info & pending_a) override
	{
		vxlnetwork::db_val<Val> value;
		vxlnetwork::db_val<Val> key (key_a);
		auto status1 = store.get (transaction_a, tables::pending, key, value);
		release_assert (store.success (status1) || store.not_found (status1));
		bool result (true);
		if (store.success (status1))
		{
			vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
			result = pending_a.deserialize (stream);
		}
		return result;
	}

	bool exists (vxlnetwork::transaction const & transaction_a, vxlnetwork::pending_key const & key_a) override
	{
		auto iterator (begin (transaction_a, key_a));
		return iterator != end () && vxlnetwork::pending_key (iterator->first) == key_a;
	}

	bool any (vxlnetwork::transaction const & transaction_a, vxlnetwork::account const & account_a) override
	{
		auto iterator (begin (transaction_a, vxlnetwork::pending_key (account_a, 0)));
		return iterator != end () && vxlnetwork::pending_key (iterator->first).account == account_a;
	}

	vxlnetwork::store_iterator<vxlnetwork::pending_key, vxlnetwork::pending_info> begin (vxlnetwork::transaction const & transaction_a, vxlnetwork::pending_key const & key_a) const override
	{
		return store.template make_iterator<vxlnetwork::pending_key, vxlnetwork::pending_info> (transaction_a, tables::pending, vxlnetwork::db_val<Val> (key_a));
	}

	vxlnetwork::store_iterator<vxlnetwork::pending_key, vxlnetwork::pending_info> begin (vxlnetwork::transaction const & transaction_a) const override
	{
		return store.template make_iterator<vxlnetwork::pending_key, vxlnetwork::pending_info> (transaction_a, tables::pending);
	}

	vxlnetwork::store_iterator<vxlnetwork::pending_key, vxlnetwork::pending_info> end () const override
	{
		return vxlnetwork::store_iterator<vxlnetwork::pending_key, vxlnetwork::pending_info> (nullptr);
	}

	void for_each_par (std::function<void (vxlnetwork::read_transaction const &, vxlnetwork::store_iterator<vxlnetwork::pending_key, vxlnetwork::pending_info>, vxlnetwork::store_iterator<vxlnetwork::pending_key, vxlnetwork::pending_info>)> const & action_a) const override
	{
		parallel_traversal<vxlnetwork::uint512_t> (
		[&action_a, this] (vxlnetwork::uint512_t const & start, vxlnetwork::uint512_t const & end, bool const is_last) {
			vxlnetwork::uint512_union union_start (start);
			vxlnetwork::uint512_union union_end (end);
			vxlnetwork::pending_key key_start (union_start.uint256s[0].number (), union_start.uint256s[1].number ());
			vxlnetwork::pending_key key_end (union_end.uint256s[0].number (), union_end.uint256s[1].number ());
			auto transaction (this->store.tx_begin_read ());
			action_a (transaction, this->begin (transaction, key_start), !is_last ? this->begin (transaction, key_end) : this->end ());
		});
	}
};

}
