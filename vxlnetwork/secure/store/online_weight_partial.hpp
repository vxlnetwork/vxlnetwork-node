#pragma once

#include <vxlnetwork/secure/store_partial.hpp>

namespace vxlnetwork
{
template <typename Val, typename Derived_Store>
class store_partial;

template <typename Val, typename Derived_Store>
void release_assert_success (store_partial<Val, Derived_Store> const &, int const);

template <typename Val, typename Derived_Store>
class online_weight_store_partial : public online_weight_store
{
private:
	vxlnetwork::store_partial<Val, Derived_Store> & store;

	friend void release_assert_success<Val, Derived_Store> (store_partial<Val, Derived_Store> const &, int const);

public:
	explicit online_weight_store_partial (vxlnetwork::store_partial<Val, Derived_Store> & store_a) :
		store (store_a){};

	void put (vxlnetwork::write_transaction const & transaction_a, uint64_t time_a, vxlnetwork::amount const & amount_a) override
	{
		vxlnetwork::db_val<Val> value (amount_a);
		auto status (store.put (transaction_a, tables::online_weight, time_a, value));
		release_assert_success (store, status);
	}

	void del (vxlnetwork::write_transaction const & transaction_a, uint64_t time_a) override
	{
		auto status (store.del (transaction_a, tables::online_weight, time_a));
		release_assert_success (store, status);
	}

	vxlnetwork::store_iterator<uint64_t, vxlnetwork::amount> begin (vxlnetwork::transaction const & transaction_a) const override
	{
		return store.template make_iterator<uint64_t, vxlnetwork::amount> (transaction_a, tables::online_weight);
	}

	vxlnetwork::store_iterator<uint64_t, vxlnetwork::amount> rbegin (vxlnetwork::transaction const & transaction_a) const override
	{
		return store.template make_iterator<uint64_t, vxlnetwork::amount> (transaction_a, tables::online_weight, false);
	}

	vxlnetwork::store_iterator<uint64_t, vxlnetwork::amount> end () const override
	{
		return vxlnetwork::store_iterator<uint64_t, vxlnetwork::amount> (nullptr);
	}

	size_t count (vxlnetwork::transaction const & transaction_a) const override
	{
		return store.count (transaction_a, tables::online_weight);
	}

	void clear (vxlnetwork::write_transaction const & transaction_a) override
	{
		auto status (store.drop (transaction_a, tables::online_weight));
		release_assert_success (store, status);
	}
};

}
