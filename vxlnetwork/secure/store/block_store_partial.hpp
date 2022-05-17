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
class block_predecessor_set;

template <typename Val, typename Derived_Store>
void release_assert_success (store_partial<Val, Derived_Store> const &, int const);

template <typename Val, typename Derived_Store>
class block_store_partial : public block_store
{
protected:
	vxlnetwork::store_partial<Val, Derived_Store> & store;

	friend class vxlnetwork::block_predecessor_set<Val, Derived_Store>;

public:
	explicit block_store_partial (vxlnetwork::store_partial<Val, Derived_Store> & store_a) :
		store (store_a){};

	void put (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::block_hash const & hash_a, vxlnetwork::block const & block_a) override
	{
		debug_assert (block_a.sideband ().successor.is_zero () || exists (transaction_a, block_a.sideband ().successor));
		std::vector<uint8_t> vector;
		{
			vxlnetwork::vectorstream stream (vector);
			vxlnetwork::serialize_block (stream, block_a);
			block_a.sideband ().serialize (stream, block_a.type ());
		}
		raw_put (transaction_a, vector, hash_a);
		vxlnetwork::block_predecessor_set<Val, Derived_Store> predecessor (transaction_a, *this);
		block_a.visit (predecessor);
		debug_assert (block_a.previous ().is_zero () || successor (transaction_a, block_a.previous ()) == hash_a);
	}

	void raw_put (vxlnetwork::write_transaction const & transaction_a, std::vector<uint8_t> const & data, vxlnetwork::block_hash const & hash_a) override
	{
		vxlnetwork::db_val<Val> value{ data.size (), (void *)data.data () };
		auto status = store.put (transaction_a, tables::blocks, hash_a, value);
		release_assert_success (store, status);
	}

	vxlnetwork::block_hash successor (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const override
	{
		auto value (block_raw_get (transaction_a, hash_a));
		vxlnetwork::block_hash result;
		if (value.size () != 0)
		{
			debug_assert (value.size () >= result.bytes.size ());
			auto type = block_type_from_raw (value.data ());
			vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()) + block_successor_offset (transaction_a, value.size (), type), result.bytes.size ());
			auto error (vxlnetwork::try_read (stream, result.bytes));
			(void)error;
			debug_assert (!error);
		}
		else
		{
			result.clear ();
		}
		return result;
	}

	void successor_clear (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) override
	{
		auto value (block_raw_get (transaction_a, hash_a));
		debug_assert (value.size () != 0);
		auto type = block_type_from_raw (value.data ());
		std::vector<uint8_t> data (static_cast<uint8_t *> (value.data ()), static_cast<uint8_t *> (value.data ()) + value.size ());
		std::fill_n (data.begin () + block_successor_offset (transaction_a, value.size (), type), sizeof (vxlnetwork::block_hash), uint8_t{ 0 });
		raw_put (transaction_a, data, hash_a);
	}

	std::shared_ptr<vxlnetwork::block> get (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const override
	{
		auto value (block_raw_get (transaction_a, hash_a));
		std::shared_ptr<vxlnetwork::block> result;
		if (value.size () != 0)
		{
			vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
			vxlnetwork::block_type type;
			auto error (try_read (stream, type));
			release_assert (!error);
			result = vxlnetwork::deserialize_block (stream, type);
			release_assert (result != nullptr);
			vxlnetwork::block_sideband sideband;
			error = (sideband.deserialize (stream, type));
			release_assert (!error);
			result->sideband_set (sideband);
		}
		return result;
	}

	std::shared_ptr<vxlnetwork::block> get_no_sideband (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const override
	{
		auto value (block_raw_get (transaction_a, hash_a));
		std::shared_ptr<vxlnetwork::block> result;
		if (value.size () != 0)
		{
			vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
			result = vxlnetwork::deserialize_block (stream);
			debug_assert (result != nullptr);
		}
		return result;
	}

	std::shared_ptr<vxlnetwork::block> random (vxlnetwork::transaction const & transaction_a) override
	{
		vxlnetwork::block_hash hash;
		vxlnetwork::random_pool::generate_block (hash.bytes.data (), hash.bytes.size ());
		auto existing = store.template make_iterator<vxlnetwork::block_hash, std::shared_ptr<vxlnetwork::block>> (transaction_a, tables::blocks, vxlnetwork::db_val<Val> (hash));
		auto end (vxlnetwork::store_iterator<vxlnetwork::block_hash, std::shared_ptr<vxlnetwork::block>> (nullptr));
		if (existing == end)
		{
			existing = store.template make_iterator<vxlnetwork::block_hash, std::shared_ptr<vxlnetwork::block>> (transaction_a, tables::blocks);
		}
		debug_assert (existing != end);
		return existing->second;
	}

	void del (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) override
	{
		auto status = store.del (transaction_a, tables::blocks, hash_a);
		release_assert_success (store, status);
	}

	bool exists (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) override
	{
		auto junk = block_raw_get (transaction_a, hash_a);
		return junk.size () != 0;
	}

	uint64_t count (vxlnetwork::transaction const & transaction_a) override
	{
		return store.count (transaction_a, tables::blocks);
	}

	vxlnetwork::account account (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const override
	{
		auto block (get (transaction_a, hash_a));
		debug_assert (block != nullptr);
		return account_calculated (*block);
	}

	vxlnetwork::account account_calculated (vxlnetwork::block const & block_a) const override
	{
		debug_assert (block_a.has_sideband ());
		vxlnetwork::account result (block_a.account ());
		if (result.is_zero ())
		{
			result = block_a.sideband ().account;
		}
		debug_assert (!result.is_zero ());
		return result;
	}

	vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::block_w_sideband> begin (vxlnetwork::transaction const & transaction_a) const override
	{
		return store.template make_iterator<vxlnetwork::block_hash, vxlnetwork::block_w_sideband> (transaction_a, tables::blocks);
	}

	vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::block_w_sideband> begin (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const override
	{
		return store.template make_iterator<vxlnetwork::block_hash, vxlnetwork::block_w_sideband> (transaction_a, tables::blocks, vxlnetwork::db_val<Val> (hash_a));
	}

	vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::block_w_sideband> end () const override
	{
		return vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::block_w_sideband> (nullptr);
	}

	vxlnetwork::uint128_t balance (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) override
	{
		auto block (get (transaction_a, hash_a));
		release_assert (block);
		vxlnetwork::uint128_t result (balance_calculated (block));
		return result;
	}

	vxlnetwork::uint128_t balance_calculated (std::shared_ptr<vxlnetwork::block> const & block_a) const override
	{
		vxlnetwork::uint128_t result;
		switch (block_a->type ())
		{
			case vxlnetwork::block_type::open:
			case vxlnetwork::block_type::receive:
			case vxlnetwork::block_type::change:
				result = block_a->sideband ().balance.number ();
				break;
			case vxlnetwork::block_type::send:
				result = boost::polymorphic_downcast<vxlnetwork::send_block *> (block_a.get ())->hashables.balance.number ();
				break;
			case vxlnetwork::block_type::state:
				result = boost::polymorphic_downcast<vxlnetwork::state_block *> (block_a.get ())->hashables.balance.number ();
				break;
			case vxlnetwork::block_type::invalid:
			case vxlnetwork::block_type::not_a_block:
				release_assert (false);
				break;
		}
		return result;
	}

	vxlnetwork::epoch version (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) override
	{
		auto block = get (transaction_a, hash_a);
		if (block && block->type () == vxlnetwork::block_type::state)
		{
			return block->sideband ().details.epoch;
		}

		return vxlnetwork::epoch::epoch_0;
	}

	void for_each_par (std::function<void (vxlnetwork::read_transaction const &, vxlnetwork::store_iterator<vxlnetwork::block_hash, block_w_sideband>, vxlnetwork::store_iterator<vxlnetwork::block_hash, block_w_sideband>)> const & action_a) const override
	{
		parallel_traversal<vxlnetwork::uint256_t> (
		[&action_a, this] (vxlnetwork::uint256_t const & start, vxlnetwork::uint256_t const & end, bool const is_last) {
			auto transaction (this->store.tx_begin_read ());
			action_a (transaction, this->begin (transaction, start), !is_last ? this->begin (transaction, end) : this->end ());
		});
	}

	// Converts a block hash to a block height
	uint64_t account_height (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const override
	{
		auto block = get (transaction_a, hash_a);
		return block->sideband ().height;
	}

protected:
	vxlnetwork::db_val<Val> block_raw_get (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const
	{
		vxlnetwork::db_val<Val> result;
		auto status = store.get (transaction_a, tables::blocks, hash_a, result);
		release_assert (store.success (status) || store.not_found (status));
		return result;
	}

	size_t block_successor_offset (vxlnetwork::transaction const & transaction_a, size_t entry_size_a, vxlnetwork::block_type type_a) const
	{
		return entry_size_a - vxlnetwork::block_sideband::size (type_a);
	}

	static vxlnetwork::block_type block_type_from_raw (void * data_a)
	{
		// The block type is the first byte
		return static_cast<vxlnetwork::block_type> ((reinterpret_cast<uint8_t const *> (data_a))[0]);
	}
};

/**
 * Fill in our predecessors
 */
template <typename Val, typename Derived_Store>
class block_predecessor_set : public vxlnetwork::block_visitor
{
public:
	block_predecessor_set (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::block_store_partial<Val, Derived_Store> & block_store_a) :
		transaction (transaction_a),
		block_store (block_store_a)
	{
	}
	virtual ~block_predecessor_set () = default;
	void fill_value (vxlnetwork::block const & block_a)
	{
		auto hash (block_a.hash ());
		auto value (block_store.block_raw_get (transaction, block_a.previous ()));
		debug_assert (value.size () != 0);
		auto type = block_store.block_type_from_raw (value.data ());
		std::vector<uint8_t> data (static_cast<uint8_t *> (value.data ()), static_cast<uint8_t *> (value.data ()) + value.size ());
		std::copy (hash.bytes.begin (), hash.bytes.end (), data.begin () + block_store.block_successor_offset (transaction, value.size (), type));
		block_store.raw_put (transaction, data, block_a.previous ());
	}
	void send_block (vxlnetwork::send_block const & block_a) override
	{
		fill_value (block_a);
	}
	void receive_block (vxlnetwork::receive_block const & block_a) override
	{
		fill_value (block_a);
	}
	void open_block (vxlnetwork::open_block const & block_a) override
	{
		// Open blocks don't have a predecessor
	}
	void change_block (vxlnetwork::change_block const & block_a) override
	{
		fill_value (block_a);
	}
	void state_block (vxlnetwork::state_block const & block_a) override
	{
		if (!block_a.previous ().is_zero ())
		{
			fill_value (block_a);
		}
	}
	vxlnetwork::write_transaction const & transaction;
	vxlnetwork::block_store_partial<Val, Derived_Store> & block_store;
};
}
