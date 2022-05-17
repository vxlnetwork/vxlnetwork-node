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
class pruned_store_partial : public pruned_store
{
private:
	vxlnetwork::store_partial<Val, Derived_Store> & store;

	friend void release_assert_success<Val, Derived_Store> (store_partial<Val, Derived_Store> const &, int const);

public:
	explicit pruned_store_partial (vxlnetwork::store_partial<Val, Derived_Store> & store_a) :
		store (store_a){};

	void put (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) override
	{
		auto status = store.put_key (transaction_a, tables::pruned, hash_a);
		release_assert_success (store, status);
	}

	void del (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) override
	{
		auto status = store.del (transaction_a, tables::pruned, hash_a);
		release_assert_success (store, status);
	}

	bool exists (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const override
	{
		return store.exists (transaction_a, tables::pruned, vxlnetwork::db_val<Val> (hash_a));
	}

	vxlnetwork::block_hash random (vxlnetwork::transaction const & transaction_a) override
	{
		vxlnetwork::block_hash random_hash;
		vxlnetwork::random_pool::generate_block (random_hash.bytes.data (), random_hash.bytes.size ());
		auto existing = store.template make_iterator<vxlnetwork::block_hash, vxlnetwork::db_val<Val>> (transaction_a, tables::pruned, vxlnetwork::db_val<Val> (random_hash));
		auto end (vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::db_val<Val>> (nullptr));
		if (existing == end)
		{
			existing = store.template make_iterator<vxlnetwork::block_hash, vxlnetwork::db_val<Val>> (transaction_a, tables::pruned);
		}
		return existing != end ? existing->first : 0;
	}

	size_t count (vxlnetwork::transaction const & transaction_a) const override
	{
		return store.count (transaction_a, tables::pruned);
	}

	void clear (vxlnetwork::write_transaction const & transaction_a) override
	{
		auto status = store.drop (transaction_a, tables::pruned);
		release_assert_success (store, status);
	}

	vxlnetwork::store_iterator<vxlnetwork::block_hash, std::nullptr_t> begin (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const override
	{
		return store.template make_iterator<vxlnetwork::block_hash, std::nullptr_t> (transaction_a, tables::pruned, vxlnetwork::db_val<Val> (hash_a));
	}

	vxlnetwork::store_iterator<vxlnetwork::block_hash, std::nullptr_t> begin (vxlnetwork::transaction const & transaction_a) const override
	{
		return store.template make_iterator<vxlnetwork::block_hash, std::nullptr_t> (transaction_a, tables::pruned);
	}

	vxlnetwork::store_iterator<vxlnetwork::block_hash, std::nullptr_t> end () const override
	{
		return vxlnetwork::store_iterator<vxlnetwork::block_hash, std::nullptr_t> (nullptr);
	}

	void for_each_par (std::function<void (vxlnetwork::read_transaction const &, vxlnetwork::store_iterator<vxlnetwork::block_hash, std::nullptr_t>, vxlnetwork::store_iterator<vxlnetwork::block_hash, std::nullptr_t>)> const & action_a) const override
	{
		parallel_traversal<vxlnetwork::uint256_t> (
		[&action_a, this] (vxlnetwork::uint256_t const & start, vxlnetwork::uint256_t const & end, bool const is_last) {
			auto transaction (this->store.tx_begin_read ());
			action_a (transaction, this->begin (transaction, start), !is_last ? this->begin (transaction, end) : this->end ());
		});
	}
};

}
