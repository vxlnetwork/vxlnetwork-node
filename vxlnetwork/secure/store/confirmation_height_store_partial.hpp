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
class confirmation_height_store_partial : public confirmation_height_store
{
private:
	vxlnetwork::store_partial<Val, Derived_Store> & store;

	friend void release_assert_success<Val, Derived_Store> (store_partial<Val, Derived_Store> const &, int const);

public:
	explicit confirmation_height_store_partial (vxlnetwork::store_partial<Val, Derived_Store> & store_a) :
		store (store_a){};

	void put (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::account const & account_a, vxlnetwork::confirmation_height_info const & confirmation_height_info_a) override
	{
		vxlnetwork::db_val<Val> confirmation_height_info (confirmation_height_info_a);
		auto status = store.put (transaction_a, tables::confirmation_height, account_a, confirmation_height_info);
		release_assert_success (store, status);
	}

	bool get (vxlnetwork::transaction const & transaction_a, vxlnetwork::account const & account_a, vxlnetwork::confirmation_height_info & confirmation_height_info_a) override
	{
		vxlnetwork::db_val<Val> value;
		auto status = store.get (transaction_a, tables::confirmation_height, vxlnetwork::db_val<Val> (account_a), value);
		release_assert (store.success (status) || store.not_found (status));
		bool result (true);
		if (store.success (status))
		{
			vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
			result = confirmation_height_info_a.deserialize (stream);
		}
		if (result)
		{
			confirmation_height_info_a.height = 0;
			confirmation_height_info_a.frontier = 0;
		}

		return result;
	}

	bool exists (vxlnetwork::transaction const & transaction_a, vxlnetwork::account const & account_a) const override
	{
		return store.exists (transaction_a, tables::confirmation_height, vxlnetwork::db_val<Val> (account_a));
	}

	void del (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::account const & account_a) override
	{
		auto status (store.del (transaction_a, tables::confirmation_height, vxlnetwork::db_val<Val> (account_a)));
		release_assert_success (store, status);
	}

	uint64_t count (vxlnetwork::transaction const & transaction_a) override
	{
		return store.count (transaction_a, tables::confirmation_height);
	}

	void clear (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::account const & account_a) override
	{
		del (transaction_a, account_a);
	}

	void clear (vxlnetwork::write_transaction const & transaction_a) override
	{
		store.drop (transaction_a, vxlnetwork::tables::confirmation_height);
	}

	vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::confirmation_height_info> begin (vxlnetwork::transaction const & transaction_a, vxlnetwork::account const & account_a) const override
	{
		return store.template make_iterator<vxlnetwork::account, vxlnetwork::confirmation_height_info> (transaction_a, tables::confirmation_height, vxlnetwork::db_val<Val> (account_a));
	}

	vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::confirmation_height_info> begin (vxlnetwork::transaction const & transaction_a) const override
	{
		return store.template make_iterator<vxlnetwork::account, vxlnetwork::confirmation_height_info> (transaction_a, tables::confirmation_height);
	}

	vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::confirmation_height_info> end () const override
	{
		return vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::confirmation_height_info> (nullptr);
	}

	void for_each_par (std::function<void (vxlnetwork::read_transaction const &, vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::confirmation_height_info>, vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::confirmation_height_info>)> const & action_a) const override
	{
		parallel_traversal<vxlnetwork::uint256_t> (
		[&action_a, this] (vxlnetwork::uint256_t const & start, vxlnetwork::uint256_t const & end, bool const is_last) {
			auto transaction (this->store.tx_begin_read ());
			action_a (transaction, this->begin (transaction, start), !is_last ? this->begin (transaction, end) : this->end ());
		});
	}
};

}
