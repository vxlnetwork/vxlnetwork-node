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
class peer_store_partial : public peer_store
{
private:
	vxlnetwork::store_partial<Val, Derived_Store> & store;

	friend void release_assert_success<Val, Derived_Store> (store_partial<Val, Derived_Store> const &, int const);

public:
	explicit peer_store_partial (vxlnetwork::store_partial<Val, Derived_Store> & store_a) :
		store (store_a){};

	void put (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::endpoint_key const & endpoint_a) override
	{
		auto status = store.put_key (transaction_a, tables::peers, endpoint_a);
		release_assert_success (store, status);
	}

	void del (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::endpoint_key const & endpoint_a) override
	{
		auto status (store.del (transaction_a, tables::peers, endpoint_a));
		release_assert_success (store, status);
	}

	bool exists (vxlnetwork::transaction const & transaction_a, vxlnetwork::endpoint_key const & endpoint_a) const override
	{
		return store.exists (transaction_a, tables::peers, vxlnetwork::db_val<Val> (endpoint_a));
	}

	size_t count (vxlnetwork::transaction const & transaction_a) const override
	{
		return store.count (transaction_a, tables::peers);
	}

	void clear (vxlnetwork::write_transaction const & transaction_a) override
	{
		auto status = store.drop (transaction_a, tables::peers);
		release_assert_success (store, status);
	}

	vxlnetwork::store_iterator<vxlnetwork::endpoint_key, vxlnetwork::no_value> begin (vxlnetwork::transaction const & transaction_a) const override
	{
		return store.template make_iterator<vxlnetwork::endpoint_key, vxlnetwork::no_value> (transaction_a, tables::peers);
	}

	vxlnetwork::store_iterator<vxlnetwork::endpoint_key, vxlnetwork::no_value> end () const override
	{
		return vxlnetwork::store_iterator<vxlnetwork::endpoint_key, vxlnetwork::no_value> (nullptr);
	}
};

}
