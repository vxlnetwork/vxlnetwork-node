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
class account_store_partial : public account_store
{
private:
	vxlnetwork::store_partial<Val, Derived_Store> & store;

	friend void release_assert_success<Val, Derived_Store> (store_partial<Val, Derived_Store> const &, int const);

public:
	explicit account_store_partial (vxlnetwork::store_partial<Val, Derived_Store> & store_a) :
		store (store_a){};

	void put (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::account const & account_a, vxlnetwork::account_info const & info_a) override
	{
		// Check we are still in sync with other tables
		vxlnetwork::db_val<Val> info (info_a);
		auto status = store.put (transaction_a, tables::accounts, account_a, info);
		release_assert_success (store, status);
	}

	bool get (vxlnetwork::transaction const & transaction_a, vxlnetwork::account const & account_a, vxlnetwork::account_info & info_a) override
	{
		vxlnetwork::db_val<Val> value;
		vxlnetwork::db_val<Val> account (account_a);
		auto status1 (store.get (transaction_a, tables::accounts, account, value));
		release_assert (store.success (status1) || store.not_found (status1));
		bool result (true);
		if (store.success (status1))
		{
			vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
			result = info_a.deserialize (stream);
		}
		return result;
	}

	void del (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::account const & account_a) override
	{
		auto status = store.del (transaction_a, tables::accounts, account_a);
		release_assert_success (store, status);
	}

	bool exists (vxlnetwork::transaction const & transaction_a, vxlnetwork::account const & account_a) override
	{
		auto iterator (begin (transaction_a, account_a));
		return iterator != end () && vxlnetwork::account (iterator->first) == account_a;
	}

	size_t count (vxlnetwork::transaction const & transaction_a) override
	{
		return store.count (transaction_a, tables::accounts);
	}

	vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::account_info> begin (vxlnetwork::transaction const & transaction_a, vxlnetwork::account const & account_a) const override
	{
		return store.template make_iterator<vxlnetwork::account, vxlnetwork::account_info> (transaction_a, tables::accounts, vxlnetwork::db_val<Val> (account_a));
	}

	vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::account_info> begin (vxlnetwork::transaction const & transaction_a) const override
	{
		return store.template make_iterator<vxlnetwork::account, vxlnetwork::account_info> (transaction_a, tables::accounts);
	}

	vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::account_info> rbegin (vxlnetwork::transaction const & transaction_a) const override
	{
		return store.template make_iterator<vxlnetwork::account, vxlnetwork::account_info> (transaction_a, tables::accounts, false);
	}

	vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::account_info> end () const override
	{
		return vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::account_info> (nullptr);
	}

	void for_each_par (std::function<void (vxlnetwork::read_transaction const &, vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::account_info>, vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::account_info>)> const & action_a) const override
	{
		parallel_traversal<vxlnetwork::uint256_t> (
		[&action_a, this] (vxlnetwork::uint256_t const & start, vxlnetwork::uint256_t const & end, bool const is_last) {
			auto transaction (this->store.tx_begin_read ());
			action_a (transaction, this->begin (transaction, start), !is_last ? this->begin (transaction, end) : this->end ());
		});
	}
};

}
