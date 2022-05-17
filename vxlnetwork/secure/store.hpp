#pragma once

#include <vxlnetwork/crypto_lib/random_pool.hpp>
#include <vxlnetwork/lib/diagnosticsconfig.hpp>
#include <vxlnetwork/lib/lmdbconfig.hpp>
#include <vxlnetwork/lib/logger_mt.hpp>
#include <vxlnetwork/lib/memory.hpp>
#include <vxlnetwork/lib/rocksdbconfig.hpp>
#include <vxlnetwork/secure/buffer.hpp>
#include <vxlnetwork/secure/common.hpp>
#include <vxlnetwork/secure/versioning.hpp>

#include <boost/endian/conversion.hpp>
#include <boost/polymorphic_cast.hpp>

#include <stack>

namespace vxlnetwork
{
// Move to versioning with a specific version if required for a future upgrade
template <typename T>
class block_w_sideband_v18
{
public:
	std::shared_ptr<T> block;
	vxlnetwork::block_sideband_v18 sideband;
};

class block_w_sideband
{
public:
	std::shared_ptr<vxlnetwork::block> block;
	vxlnetwork::block_sideband sideband;
};

/**
 * Encapsulates database specific container
 */
template <typename Val>
class db_val
{
public:
	db_val (Val const & value_a) :
		value (value_a)
	{
	}

	db_val () :
		db_val (0, nullptr)
	{
	}

	db_val (std::nullptr_t) :
		db_val (0, this)
	{
	}

	db_val (vxlnetwork::uint128_union const & val_a) :
		db_val (sizeof (val_a), const_cast<vxlnetwork::uint128_union *> (&val_a))
	{
	}

	db_val (vxlnetwork::uint256_union const & val_a) :
		db_val (sizeof (val_a), const_cast<vxlnetwork::uint256_union *> (&val_a))
	{
	}

	db_val (vxlnetwork::uint512_union const & val_a) :
		db_val (sizeof (val_a), const_cast<vxlnetwork::uint512_union *> (&val_a))
	{
	}

	db_val (vxlnetwork::qualified_root const & val_a) :
		db_val (sizeof (val_a), const_cast<vxlnetwork::qualified_root *> (&val_a))
	{
	}

	db_val (vxlnetwork::account_info const & val_a) :
		db_val (val_a.db_size (), const_cast<vxlnetwork::account_info *> (&val_a))
	{
	}

	db_val (vxlnetwork::account_info_v14 const & val_a) :
		db_val (val_a.db_size (), const_cast<vxlnetwork::account_info_v14 *> (&val_a))
	{
	}

	db_val (vxlnetwork::pending_info const & val_a) :
		db_val (val_a.db_size (), const_cast<vxlnetwork::pending_info *> (&val_a))
	{
		static_assert (std::is_standard_layout<vxlnetwork::pending_info>::value, "Standard layout is required");
	}

	db_val (vxlnetwork::pending_info_v14 const & val_a) :
		db_val (val_a.db_size (), const_cast<vxlnetwork::pending_info_v14 *> (&val_a))
	{
		static_assert (std::is_standard_layout<vxlnetwork::pending_info_v14>::value, "Standard layout is required");
	}

	db_val (vxlnetwork::pending_key const & val_a) :
		db_val (sizeof (val_a), const_cast<vxlnetwork::pending_key *> (&val_a))
	{
		static_assert (std::is_standard_layout<vxlnetwork::pending_key>::value, "Standard layout is required");
	}

	db_val (vxlnetwork::unchecked_info const & val_a) :
		buffer (std::make_shared<std::vector<uint8_t>> ())
	{
		{
			vxlnetwork::vectorstream stream (*buffer);
			val_a.serialize (stream);
		}
		convert_buffer_to_value ();
	}

	db_val (vxlnetwork::unchecked_key const & val_a) :
		db_val (sizeof (val_a), const_cast<vxlnetwork::unchecked_key *> (&val_a))
	{
		static_assert (std::is_standard_layout<vxlnetwork::unchecked_key>::value, "Standard layout is required");
	}

	db_val (vxlnetwork::confirmation_height_info const & val_a) :
		buffer (std::make_shared<std::vector<uint8_t>> ())
	{
		{
			vxlnetwork::vectorstream stream (*buffer);
			val_a.serialize (stream);
		}
		convert_buffer_to_value ();
	}

	db_val (vxlnetwork::block_info const & val_a) :
		db_val (sizeof (val_a), const_cast<vxlnetwork::block_info *> (&val_a))
	{
		static_assert (std::is_standard_layout<vxlnetwork::block_info>::value, "Standard layout is required");
	}

	db_val (vxlnetwork::endpoint_key const & val_a) :
		db_val (sizeof (val_a), const_cast<vxlnetwork::endpoint_key *> (&val_a))
	{
		static_assert (std::is_standard_layout<vxlnetwork::endpoint_key>::value, "Standard layout is required");
	}

	db_val (std::shared_ptr<vxlnetwork::block> const & val_a) :
		buffer (std::make_shared<std::vector<uint8_t>> ())
	{
		{
			vxlnetwork::vectorstream stream (*buffer);
			vxlnetwork::serialize_block (stream, *val_a);
		}
		convert_buffer_to_value ();
	}

	db_val (uint64_t val_a) :
		buffer (std::make_shared<std::vector<uint8_t>> ())
	{
		{
			boost::endian::native_to_big_inplace (val_a);
			vxlnetwork::vectorstream stream (*buffer);
			vxlnetwork::write (stream, val_a);
		}
		convert_buffer_to_value ();
	}

	explicit operator vxlnetwork::account_info () const
	{
		vxlnetwork::account_info result;
		debug_assert (size () == result.db_size ());
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + result.db_size (), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator vxlnetwork::account_info_v14 () const
	{
		vxlnetwork::account_info_v14 result;
		debug_assert (size () == result.db_size ());
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + result.db_size (), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator vxlnetwork::block_info () const
	{
		vxlnetwork::block_info result;
		debug_assert (size () == sizeof (result));
		static_assert (sizeof (vxlnetwork::block_info::account) + sizeof (vxlnetwork::block_info::balance) == sizeof (result), "Packed class");
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator vxlnetwork::pending_info_v14 () const
	{
		vxlnetwork::pending_info_v14 result;
		debug_assert (size () == result.db_size ());
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + result.db_size (), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator vxlnetwork::pending_info () const
	{
		vxlnetwork::pending_info result;
		debug_assert (size () == result.db_size ());
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + result.db_size (), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator vxlnetwork::pending_key () const
	{
		vxlnetwork::pending_key result;
		debug_assert (size () == sizeof (result));
		static_assert (sizeof (vxlnetwork::pending_key::account) + sizeof (vxlnetwork::pending_key::hash) == sizeof (result), "Packed class");
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator vxlnetwork::confirmation_height_info () const
	{
		vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		vxlnetwork::confirmation_height_info result;
		bool error (result.deserialize (stream));
		(void)error;
		debug_assert (!error);
		return result;
	}

	explicit operator vxlnetwork::unchecked_info () const
	{
		vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		vxlnetwork::unchecked_info result;
		bool error (result.deserialize (stream));
		(void)error;
		debug_assert (!error);
		return result;
	}

	explicit operator vxlnetwork::unchecked_key () const
	{
		vxlnetwork::unchecked_key result;
		debug_assert (size () == sizeof (result));
		static_assert (sizeof (vxlnetwork::unchecked_key::previous) + sizeof (vxlnetwork::pending_key::hash) == sizeof (result), "Packed class");
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator vxlnetwork::uint128_union () const
	{
		return convert<vxlnetwork::uint128_union> ();
	}

	explicit operator vxlnetwork::amount () const
	{
		return convert<vxlnetwork::amount> ();
	}

	explicit operator vxlnetwork::block_hash () const
	{
		return convert<vxlnetwork::block_hash> ();
	}

	explicit operator vxlnetwork::public_key () const
	{
		return convert<vxlnetwork::public_key> ();
	}

	explicit operator vxlnetwork::qualified_root () const
	{
		return convert<vxlnetwork::qualified_root> ();
	}

	explicit operator vxlnetwork::uint256_union () const
	{
		return convert<vxlnetwork::uint256_union> ();
	}

	explicit operator vxlnetwork::uint512_union () const
	{
		return convert<vxlnetwork::uint512_union> ();
	}

	explicit operator std::array<char, 64> () const
	{
		vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		std::array<char, 64> result;
		auto error = vxlnetwork::try_read (stream, result);
		(void)error;
		debug_assert (!error);
		return result;
	}

	explicit operator vxlnetwork::endpoint_key () const
	{
		vxlnetwork::endpoint_key result;
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	template <class Block>
	explicit operator block_w_sideband_v18<Block> () const
	{
		vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		auto error (false);
		block_w_sideband_v18<Block> block_w_sideband;
		block_w_sideband.block = std::make_shared<Block> (error, stream);
		release_assert (!error);

		error = block_w_sideband.sideband.deserialize (stream, block_w_sideband.block->type ());
		release_assert (!error);

		return block_w_sideband;
	}

	explicit operator block_w_sideband () const
	{
		vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		vxlnetwork::block_w_sideband block_w_sideband;
		block_w_sideband.block = (vxlnetwork::deserialize_block (stream));
		auto error = block_w_sideband.sideband.deserialize (stream, block_w_sideband.block->type ());
		release_assert (!error);
		block_w_sideband.block->sideband_set (block_w_sideband.sideband);
		return block_w_sideband;
	}

	explicit operator state_block_w_sideband_v14 () const
	{
		vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		auto error (false);
		vxlnetwork::state_block_w_sideband_v14 block_w_sideband;
		block_w_sideband.state_block = std::make_shared<vxlnetwork::state_block> (error, stream);
		debug_assert (!error);

		block_w_sideband.sideband.type = vxlnetwork::block_type::state;
		error = block_w_sideband.sideband.deserialize (stream);
		debug_assert (!error);

		return block_w_sideband;
	}

	explicit operator std::nullptr_t () const
	{
		return nullptr;
	}

	explicit operator vxlnetwork::no_value () const
	{
		return no_value::dummy;
	}

	explicit operator std::shared_ptr<vxlnetwork::block> () const
	{
		vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		std::shared_ptr<vxlnetwork::block> result (vxlnetwork::deserialize_block (stream));
		return result;
	}

	template <typename Block>
	std::shared_ptr<Block> convert_to_block () const
	{
		vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		auto error (false);
		auto result (std::make_shared<Block> (error, stream));
		debug_assert (!error);
		return result;
	}

	explicit operator std::shared_ptr<vxlnetwork::send_block> () const
	{
		return convert_to_block<vxlnetwork::send_block> ();
	}

	explicit operator std::shared_ptr<vxlnetwork::receive_block> () const
	{
		return convert_to_block<vxlnetwork::receive_block> ();
	}

	explicit operator std::shared_ptr<vxlnetwork::open_block> () const
	{
		return convert_to_block<vxlnetwork::open_block> ();
	}

	explicit operator std::shared_ptr<vxlnetwork::change_block> () const
	{
		return convert_to_block<vxlnetwork::change_block> ();
	}

	explicit operator std::shared_ptr<vxlnetwork::state_block> () const
	{
		return convert_to_block<vxlnetwork::state_block> ();
	}

	explicit operator std::shared_ptr<vxlnetwork::vote> () const
	{
		vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		auto error (false);
		auto result (vxlnetwork::make_shared<vxlnetwork::vote> (error, stream));
		debug_assert (!error);
		return result;
	}

	explicit operator uint64_t () const
	{
		uint64_t result;
		vxlnetwork::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		auto error (vxlnetwork::try_read (stream, result));
		(void)error;
		debug_assert (!error);
		boost::endian::big_to_native_inplace (result);
		return result;
	}

	operator Val * () const
	{
		// Allow passing a temporary to a non-c++ function which doesn't have constness
		return const_cast<Val *> (&value);
	}

	operator Val const & () const
	{
		return value;
	}

	// Must be specialized
	void * data () const;
	size_t size () const;
	db_val (size_t size_a, void * data_a);
	void convert_buffer_to_value ();

	Val value;
	std::shared_ptr<std::vector<uint8_t>> buffer;

private:
	template <typename T>
	T convert () const
	{
		T result;
		debug_assert (size () == sizeof (result));
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), result.bytes.data ());
		return result;
	}
};

class transaction;
class store;

/**
 * Determine the representative for this block
 */
class representative_visitor final : public vxlnetwork::block_visitor
{
public:
	representative_visitor (vxlnetwork::transaction const & transaction_a, vxlnetwork::store & store_a);
	~representative_visitor () = default;
	void compute (vxlnetwork::block_hash const & hash_a);
	void send_block (vxlnetwork::send_block const & block_a) override;
	void receive_block (vxlnetwork::receive_block const & block_a) override;
	void open_block (vxlnetwork::open_block const & block_a) override;
	void change_block (vxlnetwork::change_block const & block_a) override;
	void state_block (vxlnetwork::state_block const & block_a) override;
	vxlnetwork::transaction const & transaction;
	vxlnetwork::store & store;
	vxlnetwork::block_hash current;
	vxlnetwork::block_hash result;
};
template <typename T, typename U>
class store_iterator_impl
{
public:
	virtual ~store_iterator_impl () = default;
	virtual vxlnetwork::store_iterator_impl<T, U> & operator++ () = 0;
	virtual vxlnetwork::store_iterator_impl<T, U> & operator-- () = 0;
	virtual bool operator== (vxlnetwork::store_iterator_impl<T, U> const & other_a) const = 0;
	virtual bool is_end_sentinal () const = 0;
	virtual void fill (std::pair<T, U> &) const = 0;
	vxlnetwork::store_iterator_impl<T, U> & operator= (vxlnetwork::store_iterator_impl<T, U> const &) = delete;
	bool operator== (vxlnetwork::store_iterator_impl<T, U> const * other_a) const
	{
		return (other_a != nullptr && *this == *other_a) || (other_a == nullptr && is_end_sentinal ());
	}
	bool operator!= (vxlnetwork::store_iterator_impl<T, U> const & other_a) const
	{
		return !(*this == other_a);
	}
};
/**
 * Iterates the key/value pairs of a transaction
 */
template <typename T, typename U>
class store_iterator final
{
public:
	store_iterator (std::nullptr_t)
	{
	}
	store_iterator (std::unique_ptr<vxlnetwork::store_iterator_impl<T, U>> impl_a) :
		impl (std::move (impl_a))
	{
		impl->fill (current);
	}
	store_iterator (vxlnetwork::store_iterator<T, U> && other_a) :
		current (std::move (other_a.current)),
		impl (std::move (other_a.impl))
	{
	}
	vxlnetwork::store_iterator<T, U> & operator++ ()
	{
		++*impl;
		impl->fill (current);
		return *this;
	}
	vxlnetwork::store_iterator<T, U> & operator-- ()
	{
		--*impl;
		impl->fill (current);
		return *this;
	}
	vxlnetwork::store_iterator<T, U> & operator= (vxlnetwork::store_iterator<T, U> && other_a) noexcept
	{
		impl = std::move (other_a.impl);
		current = std::move (other_a.current);
		return *this;
	}
	vxlnetwork::store_iterator<T, U> & operator= (vxlnetwork::store_iterator<T, U> const &) = delete;
	std::pair<T, U> * operator-> ()
	{
		return &current;
	}
	bool operator== (vxlnetwork::store_iterator<T, U> const & other_a) const
	{
		return (impl == nullptr && other_a.impl == nullptr) || (impl != nullptr && *impl == other_a.impl.get ()) || (other_a.impl != nullptr && *other_a.impl == impl.get ());
	}
	bool operator!= (vxlnetwork::store_iterator<T, U> const & other_a) const
	{
		return !(*this == other_a);
	}

private:
	std::pair<T, U> current;
	std::unique_ptr<vxlnetwork::store_iterator_impl<T, U>> impl;
};

// Keep this in alphabetical order
enum class tables
{
	accounts,
	blocks,
	confirmation_height,
	default_unused, // RocksDB only
	final_votes,
	frontiers,
	meta,
	online_weight,
	peers,
	pending,
	pruned,
	unchecked,
	vote
};

class transaction_impl
{
public:
	virtual ~transaction_impl () = default;
	virtual void * get_handle () const = 0;
};

class read_transaction_impl : public transaction_impl
{
public:
	virtual void reset () = 0;
	virtual void renew () = 0;
};

class write_transaction_impl : public transaction_impl
{
public:
	virtual void commit () = 0;
	virtual void renew () = 0;
	virtual bool contains (vxlnetwork::tables table_a) const = 0;
};

class transaction
{
public:
	virtual ~transaction () = default;
	virtual void * get_handle () const = 0;
};

/**
 * RAII wrapper of a read MDB_txn where the constructor starts the transaction
 * and the destructor aborts it.
 */
class read_transaction final : public transaction
{
public:
	explicit read_transaction (std::unique_ptr<vxlnetwork::read_transaction_impl> read_transaction_impl);
	void * get_handle () const override;
	void reset () const;
	void renew () const;
	void refresh () const;

private:
	std::unique_ptr<vxlnetwork::read_transaction_impl> impl;
};

/**
 * RAII wrapper of a read-write MDB_txn where the constructor starts the transaction
 * and the destructor commits it.
 */
class write_transaction final : public transaction
{
public:
	explicit write_transaction (std::unique_ptr<vxlnetwork::write_transaction_impl> write_transaction_impl);
	void * get_handle () const override;
	void commit ();
	void renew ();
	void refresh ();
	bool contains (vxlnetwork::tables table_a) const;

private:
	std::unique_ptr<vxlnetwork::write_transaction_impl> impl;
};

class ledger_cache;

/**
 * Manages frontier storage and iteration
 */
class frontier_store
{
public:
	virtual void put (vxlnetwork::write_transaction const &, vxlnetwork::block_hash const &, vxlnetwork::account const &) = 0;
	virtual vxlnetwork::account get (vxlnetwork::transaction const &, vxlnetwork::block_hash const &) const = 0;
	virtual void del (vxlnetwork::write_transaction const &, vxlnetwork::block_hash const &) = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::account> begin (vxlnetwork::transaction const &) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::account> begin (vxlnetwork::transaction const &, vxlnetwork::block_hash const &) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::account> end () const = 0;
	virtual void for_each_par (std::function<void (vxlnetwork::read_transaction const &, vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::account>, vxlnetwork::store_iterator<vxlnetwork::block_hash, vxlnetwork::account>)> const & action_a) const = 0;
};

/**
 * Manages account storage and iteration
 */
class account_store
{
public:
	virtual void put (vxlnetwork::write_transaction const &, vxlnetwork::account const &, vxlnetwork::account_info const &) = 0;
	virtual bool get (vxlnetwork::transaction const &, vxlnetwork::account const &, vxlnetwork::account_info &) = 0;
	virtual void del (vxlnetwork::write_transaction const &, vxlnetwork::account const &) = 0;
	virtual bool exists (vxlnetwork::transaction const &, vxlnetwork::account const &) = 0;
	virtual size_t count (vxlnetwork::transaction const &) = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::account_info> begin (vxlnetwork::transaction const &, vxlnetwork::account const &) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::account_info> begin (vxlnetwork::transaction const &) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::account_info> rbegin (vxlnetwork::transaction const &) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::account_info> end () const = 0;
	virtual void for_each_par (std::function<void (vxlnetwork::read_transaction const &, vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::account_info>, vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::account_info>)> const &) const = 0;
};

/**
 * Manages pending storage and iteration
 */
class pending_store
{
public:
	virtual void put (vxlnetwork::write_transaction const &, vxlnetwork::pending_key const &, vxlnetwork::pending_info const &) = 0;
	virtual void del (vxlnetwork::write_transaction const &, vxlnetwork::pending_key const &) = 0;
	virtual bool get (vxlnetwork::transaction const &, vxlnetwork::pending_key const &, vxlnetwork::pending_info &) = 0;
	virtual bool exists (vxlnetwork::transaction const &, vxlnetwork::pending_key const &) = 0;
	virtual bool any (vxlnetwork::transaction const &, vxlnetwork::account const &) = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::pending_key, vxlnetwork::pending_info> begin (vxlnetwork::transaction const &, vxlnetwork::pending_key const &) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::pending_key, vxlnetwork::pending_info> begin (vxlnetwork::transaction const &) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::pending_key, vxlnetwork::pending_info> end () const = 0;
	virtual void for_each_par (std::function<void (vxlnetwork::read_transaction const &, vxlnetwork::store_iterator<vxlnetwork::pending_key, vxlnetwork::pending_info>, vxlnetwork::store_iterator<vxlnetwork::pending_key, vxlnetwork::pending_info>)> const & action_a) const = 0;
};

/**
 * Manages peer storage and iteration
 */
class peer_store
{
public:
	virtual void put (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::endpoint_key const & endpoint_a) = 0;
	virtual void del (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::endpoint_key const & endpoint_a) = 0;
	virtual bool exists (vxlnetwork::transaction const & transaction_a, vxlnetwork::endpoint_key const & endpoint_a) const = 0;
	virtual size_t count (vxlnetwork::transaction const & transaction_a) const = 0;
	virtual void clear (vxlnetwork::write_transaction const & transaction_a) = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::endpoint_key, vxlnetwork::no_value> begin (vxlnetwork::transaction const & transaction_a) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::endpoint_key, vxlnetwork::no_value> end () const = 0;
};

/**
 * Manages online weight storage and iteration
 */
class online_weight_store
{
public:
	virtual void put (vxlnetwork::write_transaction const &, uint64_t, vxlnetwork::amount const &) = 0;
	virtual void del (vxlnetwork::write_transaction const &, uint64_t) = 0;
	virtual vxlnetwork::store_iterator<uint64_t, vxlnetwork::amount> begin (vxlnetwork::transaction const &) const = 0;
	virtual vxlnetwork::store_iterator<uint64_t, vxlnetwork::amount> rbegin (vxlnetwork::transaction const &) const = 0;
	virtual vxlnetwork::store_iterator<uint64_t, vxlnetwork::amount> end () const = 0;
	virtual size_t count (vxlnetwork::transaction const &) const = 0;
	virtual void clear (vxlnetwork::write_transaction const &) = 0;
};

/**
 * Manages pruned storage and iteration
 */
class pruned_store
{
public:
	virtual void put (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) = 0;
	virtual void del (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) = 0;
	virtual bool exists (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const = 0;
	virtual vxlnetwork::block_hash random (vxlnetwork::transaction const & transaction_a) = 0;
	virtual size_t count (vxlnetwork::transaction const & transaction_a) const = 0;
	virtual void clear (vxlnetwork::write_transaction const &) = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::block_hash, std::nullptr_t> begin (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::block_hash, std::nullptr_t> begin (vxlnetwork::transaction const & transaction_a) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::block_hash, std::nullptr_t> end () const = 0;
	virtual void for_each_par (std::function<void (vxlnetwork::read_transaction const &, vxlnetwork::store_iterator<vxlnetwork::block_hash, std::nullptr_t>, vxlnetwork::store_iterator<vxlnetwork::block_hash, std::nullptr_t>)> const & action_a) const = 0;
};

/**
 * Manages confirmation height storage and iteration
 */
class confirmation_height_store
{
public:
	virtual void put (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::account const & account_a, vxlnetwork::confirmation_height_info const & confirmation_height_info_a) = 0;

	/** Retrieves confirmation height info relating to an account.
	 *  The parameter confirmation_height_info_a is always written.
	 *  On error, the confirmation height and frontier hash are set to 0.
	 *  Ruturns true on error, false on success.
	 */
	virtual bool get (vxlnetwork::transaction const & transaction_a, vxlnetwork::account const & account_a, vxlnetwork::confirmation_height_info & confirmation_height_info_a) = 0;

	virtual bool exists (vxlnetwork::transaction const & transaction_a, vxlnetwork::account const & account_a) const = 0;
	virtual void del (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::account const & account_a) = 0;
	virtual uint64_t count (vxlnetwork::transaction const & transaction_a) = 0;
	virtual void clear (vxlnetwork::write_transaction const &, vxlnetwork::account const &) = 0;
	virtual void clear (vxlnetwork::write_transaction const &) = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::confirmation_height_info> begin (vxlnetwork::transaction const & transaction_a, vxlnetwork::account const & account_a) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::confirmation_height_info> begin (vxlnetwork::transaction const & transaction_a) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::confirmation_height_info> end () const = 0;
	virtual void for_each_par (std::function<void (vxlnetwork::read_transaction const &, vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::confirmation_height_info>, vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::confirmation_height_info>)> const &) const = 0;
};

/**
 * Manages unchecked storage and iteration
 */
class unchecked_store
{
public:
	using iterator = vxlnetwork::store_iterator<vxlnetwork::unchecked_key, vxlnetwork::unchecked_info>;

	virtual void clear (vxlnetwork::write_transaction const &) = 0;
	virtual void put (vxlnetwork::write_transaction const &, vxlnetwork::hash_or_account const & dependency, vxlnetwork::unchecked_info const &) = 0;
	std::pair<iterator, iterator> equal_range (vxlnetwork::transaction const & transaction, vxlnetwork::block_hash const & dependency);
	std::pair<iterator, iterator> full_range (vxlnetwork::transaction const & transaction);
	std::vector<vxlnetwork::unchecked_info> get (vxlnetwork::transaction const &, vxlnetwork::block_hash const &);
	virtual bool exists (vxlnetwork::transaction const & transaction_a, vxlnetwork::unchecked_key const & unchecked_key_a) = 0;
	virtual void del (vxlnetwork::write_transaction const &, vxlnetwork::unchecked_key const &) = 0;
	virtual iterator begin (vxlnetwork::transaction const &) const = 0;
	virtual iterator lower_bound (vxlnetwork::transaction const &, vxlnetwork::unchecked_key const &) const = 0;
	virtual iterator end () const = 0;
	virtual size_t count (vxlnetwork::transaction const &) = 0;
	virtual void for_each_par (std::function<void (vxlnetwork::read_transaction const &, iterator, iterator)> const & action_a) const = 0;
};

/**
 * Manages final vote storage and iteration
 */
class final_vote_store
{
public:
	virtual bool put (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::qualified_root const & root_a, vxlnetwork::block_hash const & hash_a) = 0;
	virtual std::vector<vxlnetwork::block_hash> get (vxlnetwork::transaction const & transaction_a, vxlnetwork::root const & root_a) = 0;
	virtual void del (vxlnetwork::write_transaction const & transaction_a, vxlnetwork::root const & root_a) = 0;
	virtual size_t count (vxlnetwork::transaction const & transaction_a) const = 0;
	virtual void clear (vxlnetwork::write_transaction const &, vxlnetwork::root const &) = 0;
	virtual void clear (vxlnetwork::write_transaction const &) = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::qualified_root, vxlnetwork::block_hash> begin (vxlnetwork::transaction const & transaction_a, vxlnetwork::qualified_root const & root_a) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::qualified_root, vxlnetwork::block_hash> begin (vxlnetwork::transaction const & transaction_a) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::qualified_root, vxlnetwork::block_hash> end () const = 0;
	virtual void for_each_par (std::function<void (vxlnetwork::read_transaction const &, vxlnetwork::store_iterator<vxlnetwork::qualified_root, vxlnetwork::block_hash>, vxlnetwork::store_iterator<vxlnetwork::qualified_root, vxlnetwork::block_hash>)> const & action_a) const = 0;
};

/**
 * Manages version storage
 */
class version_store
{
public:
	virtual void put (vxlnetwork::write_transaction const &, int) = 0;
	virtual int get (vxlnetwork::transaction const &) const = 0;
};

/**
 * Manages block storage and iteration
 */
class block_store
{
public:
	virtual void put (vxlnetwork::write_transaction const &, vxlnetwork::block_hash const &, vxlnetwork::block const &) = 0;
	virtual void raw_put (vxlnetwork::write_transaction const &, std::vector<uint8_t> const &, vxlnetwork::block_hash const &) = 0;
	virtual vxlnetwork::block_hash successor (vxlnetwork::transaction const &, vxlnetwork::block_hash const &) const = 0;
	virtual void successor_clear (vxlnetwork::write_transaction const &, vxlnetwork::block_hash const &) = 0;
	virtual std::shared_ptr<vxlnetwork::block> get (vxlnetwork::transaction const &, vxlnetwork::block_hash const &) const = 0;
	virtual std::shared_ptr<vxlnetwork::block> get_no_sideband (vxlnetwork::transaction const &, vxlnetwork::block_hash const &) const = 0;
	virtual std::shared_ptr<vxlnetwork::block> random (vxlnetwork::transaction const &) = 0;
	virtual void del (vxlnetwork::write_transaction const &, vxlnetwork::block_hash const &) = 0;
	virtual bool exists (vxlnetwork::transaction const &, vxlnetwork::block_hash const &) = 0;
	virtual uint64_t count (vxlnetwork::transaction const &) = 0;
	virtual vxlnetwork::account account (vxlnetwork::transaction const &, vxlnetwork::block_hash const &) const = 0;
	virtual vxlnetwork::account account_calculated (vxlnetwork::block const &) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::block_hash, block_w_sideband> begin (vxlnetwork::transaction const &, vxlnetwork::block_hash const &) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::block_hash, block_w_sideband> begin (vxlnetwork::transaction const &) const = 0;
	virtual vxlnetwork::store_iterator<vxlnetwork::block_hash, block_w_sideband> end () const = 0;
	virtual vxlnetwork::uint128_t balance (vxlnetwork::transaction const &, vxlnetwork::block_hash const &) = 0;
	virtual vxlnetwork::uint128_t balance_calculated (std::shared_ptr<vxlnetwork::block> const &) const = 0;
	virtual vxlnetwork::epoch version (vxlnetwork::transaction const &, vxlnetwork::block_hash const &) = 0;
	virtual void for_each_par (std::function<void (vxlnetwork::read_transaction const &, vxlnetwork::store_iterator<vxlnetwork::block_hash, block_w_sideband>, vxlnetwork::store_iterator<vxlnetwork::block_hash, block_w_sideband>)> const & action_a) const = 0;
	virtual uint64_t account_height (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const = 0;
};

class unchecked_map;
/**
 * Store manager
 */
class store
{
public:
	// clang-format off
	explicit store (
		vxlnetwork::block_store &,
		vxlnetwork::frontier_store &,
		vxlnetwork::account_store &,
		vxlnetwork::pending_store &,
		vxlnetwork::unchecked_store &,
		vxlnetwork::online_weight_store &,
		vxlnetwork::pruned_store &,
		vxlnetwork::peer_store &,
		vxlnetwork::confirmation_height_store &,
		vxlnetwork::final_vote_store &,
		vxlnetwork::version_store &
	);
	// clang-format on
	virtual ~store () = default;
	virtual void initialize (vxlnetwork::write_transaction const &, vxlnetwork::ledger_cache &) = 0;
	virtual bool root_exists (vxlnetwork::transaction const &, vxlnetwork::root const &) = 0;

	block_store & block;
	frontier_store & frontier;
	account_store & account;
	pending_store & pending;

private:
	unchecked_store & unchecked;

public:
	online_weight_store & online_weight;
	pruned_store & pruned;
	peer_store & peer;
	confirmation_height_store & confirmation_height;
	final_vote_store & final_vote;
	version_store & version;

	virtual unsigned max_block_write_batch_num () const = 0;

	virtual bool copy_db (boost::filesystem::path const & destination) = 0;
	virtual void rebuild_db (vxlnetwork::write_transaction const & transaction_a) = 0;

	/** Not applicable to all sub-classes */
	virtual void serialize_mdb_tracker (boost::property_tree::ptree &, std::chrono::milliseconds, std::chrono::milliseconds){};
	virtual void serialize_memory_stats (boost::property_tree::ptree &) = 0;

	virtual bool init_error () const = 0;

	/** Start read-write transaction */
	virtual vxlnetwork::write_transaction tx_begin_write (std::vector<vxlnetwork::tables> const & tables_to_lock = {}, std::vector<vxlnetwork::tables> const & tables_no_lock = {}) = 0;

	/** Start read-only transaction */
	virtual vxlnetwork::read_transaction tx_begin_read () const = 0;

	virtual std::string vendor_get () const = 0;

	friend class unchecked_map;
};

std::unique_ptr<vxlnetwork::store> make_store (vxlnetwork::logger_mt & logger, boost::filesystem::path const & path, vxlnetwork::ledger_constants & constants, bool open_read_only = false, bool add_db_postfix = false, vxlnetwork::rocksdb_config const & rocksdb_config = vxlnetwork::rocksdb_config{}, vxlnetwork::txn_tracking_config const & txn_tracking_config_a = vxlnetwork::txn_tracking_config{}, std::chrono::milliseconds block_processor_batch_max_time_a = std::chrono::milliseconds (5000), vxlnetwork::lmdb_config const & lmdb_config_a = vxlnetwork::lmdb_config{}, bool backup_before_upgrade = false);
}

namespace std
{
template <>
struct hash<::vxlnetwork::tables>
{
	size_t operator() (::vxlnetwork::tables const & table_a) const
	{
		return static_cast<size_t> (table_a);
	}
};
}
