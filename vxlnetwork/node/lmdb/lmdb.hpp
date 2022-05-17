#pragma once

#include <vxlnetwork/lib/diagnosticsconfig.hpp>
#include <vxlnetwork/lib/lmdbconfig.hpp>
#include <vxlnetwork/lib/logger_mt.hpp>
#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/node/lmdb/lmdb_env.hpp>
#include <vxlnetwork/node/lmdb/lmdb_iterator.hpp>
#include <vxlnetwork/node/lmdb/lmdb_txn.hpp>
#include <vxlnetwork/secure/common.hpp>
#include <vxlnetwork/secure/store/account_store_partial.hpp>
#include <vxlnetwork/secure/store/block_store_partial.hpp>
#include <vxlnetwork/secure/store/confirmation_height_store_partial.hpp>
#include <vxlnetwork/secure/store/final_vote_store_partial.hpp>
#include <vxlnetwork/secure/store/frontier_store_partial.hpp>
#include <vxlnetwork/secure/store/online_weight_partial.hpp>
#include <vxlnetwork/secure/store/peer_store_partial.hpp>
#include <vxlnetwork/secure/store/pending_store_partial.hpp>
#include <vxlnetwork/secure/store/pruned_store_partial.hpp>
#include <vxlnetwork/secure/store/unchecked_store_partial.hpp>
#include <vxlnetwork/secure/store/version_store_partial.hpp>
#include <vxlnetwork/secure/store_partial.hpp>
#include <vxlnetwork/secure/versioning.hpp>

#include <boost/optional.hpp>

#include <lmdb/libraries/liblmdb/lmdb.h>

namespace boost
{
namespace filesystem
{
	class path;
}
}

namespace vxlnetwork
{
using mdb_val = db_val<MDB_val>;

class logging_mt;
class mdb_store;

class unchecked_mdb_store : public unchecked_store_partial<MDB_val, mdb_store>
{
public:
	explicit unchecked_mdb_store (vxlnetwork::mdb_store &);
};

/**
 * mdb implementation of the block store
 */
class mdb_store : public store_partial<MDB_val, mdb_store>
{
private:
	vxlnetwork::block_store_partial<MDB_val, mdb_store> block_store_partial;
	vxlnetwork::frontier_store_partial<MDB_val, mdb_store> frontier_store_partial;
	vxlnetwork::account_store_partial<MDB_val, mdb_store> account_store_partial;
	vxlnetwork::pending_store_partial<MDB_val, mdb_store> pending_store_partial;
	vxlnetwork::unchecked_mdb_store unchecked_mdb_store;
	vxlnetwork::online_weight_store_partial<MDB_val, mdb_store> online_weight_store_partial;
	vxlnetwork::pruned_store_partial<MDB_val, mdb_store> pruned_store_partial;
	vxlnetwork::peer_store_partial<MDB_val, mdb_store> peer_store_partial;
	vxlnetwork::confirmation_height_store_partial<MDB_val, mdb_store> confirmation_height_store_partial;
	vxlnetwork::final_vote_store_partial<MDB_val, mdb_store> final_vote_store_partial;
	vxlnetwork::version_store_partial<MDB_val, mdb_store> version_store_partial;

	friend class vxlnetwork::unchecked_mdb_store;

public:
	mdb_store (vxlnetwork::logger_mt &, boost::filesystem::path const &, vxlnetwork::ledger_constants & constants, vxlnetwork::txn_tracking_config const & txn_tracking_config_a = vxlnetwork::txn_tracking_config{}, std::chrono::milliseconds block_processor_batch_max_time_a = std::chrono::milliseconds (5000), vxlnetwork::lmdb_config const & lmdb_config_a = vxlnetwork::lmdb_config{}, bool backup_before_upgrade = false);
	vxlnetwork::write_transaction tx_begin_write (std::vector<vxlnetwork::tables> const & tables_requiring_lock = {}, std::vector<vxlnetwork::tables> const & tables_no_lock = {}) override;
	vxlnetwork::read_transaction tx_begin_read () const override;

	std::string vendor_get () const override;

	void serialize_mdb_tracker (boost::property_tree::ptree &, std::chrono::milliseconds, std::chrono::milliseconds) override;

	static void create_backup_file (vxlnetwork::mdb_env &, boost::filesystem::path const &, vxlnetwork::logger_mt &);

	void serialize_memory_stats (boost::property_tree::ptree &) override;

	unsigned max_block_write_batch_num () const override;

private:
	vxlnetwork::logger_mt & logger;
	bool error{ false };

public:
	vxlnetwork::mdb_env env;

	/**
	 * Maps head block to owning account
	 * vxlnetwork::block_hash -> vxlnetwork::account
	 */
	MDB_dbi frontiers_handle{ 0 };

	/**
	 * Maps account v1 to account information, head, rep, open, balance, timestamp and block count. (Removed)
	 * vxlnetwork::account -> vxlnetwork::block_hash, vxlnetwork::block_hash, vxlnetwork::block_hash, vxlnetwork::amount, uint64_t, uint64_t
	 */
	MDB_dbi accounts_v0_handle{ 0 };

	/**
	 * Maps account v0 to account information, head, rep, open, balance, timestamp and block count. (Removed)
	 * vxlnetwork::account -> vxlnetwork::block_hash, vxlnetwork::block_hash, vxlnetwork::block_hash, vxlnetwork::amount, uint64_t, uint64_t
	 */
	MDB_dbi accounts_v1_handle{ 0 };

	/**
	 * Maps account v0 to account information, head, rep, open, balance, timestamp, block count and epoch
	 * vxlnetwork::account -> vxlnetwork::block_hash, vxlnetwork::block_hash, vxlnetwork::block_hash, vxlnetwork::amount, uint64_t, uint64_t, vxlnetwork::epoch
	 */
	MDB_dbi accounts_handle{ 0 };

	/**
	 * Maps block hash to send block. (Removed)
	 * vxlnetwork::block_hash -> vxlnetwork::send_block
	 */
	MDB_dbi send_blocks_handle{ 0 };

	/**
	 * Maps block hash to receive block. (Removed)
	 * vxlnetwork::block_hash -> vxlnetwork::receive_block
	 */
	MDB_dbi receive_blocks_handle{ 0 };

	/**
	 * Maps block hash to open block. (Removed)
	 * vxlnetwork::block_hash -> vxlnetwork::open_block
	 */
	MDB_dbi open_blocks_handle{ 0 };

	/**
	 * Maps block hash to change block. (Removed)
	 * vxlnetwork::block_hash -> vxlnetwork::change_block
	 */
	MDB_dbi change_blocks_handle{ 0 };

	/**
	 * Maps block hash to v0 state block. (Removed)
	 * vxlnetwork::block_hash -> vxlnetwork::state_block
	 */
	MDB_dbi state_blocks_v0_handle{ 0 };

	/**
	 * Maps block hash to v1 state block. (Removed)
	 * vxlnetwork::block_hash -> vxlnetwork::state_block
	 */
	MDB_dbi state_blocks_v1_handle{ 0 };

	/**
	 * Maps block hash to state block. (Removed)
	 * vxlnetwork::block_hash -> vxlnetwork::state_block
	 */
	MDB_dbi state_blocks_handle{ 0 };

	/**
	 * Maps min_version 0 (destination account, pending block) to (source account, amount). (Removed)
	 * vxlnetwork::account, vxlnetwork::block_hash -> vxlnetwork::account, vxlnetwork::amount
	 */
	MDB_dbi pending_v0_handle{ 0 };

	/**
	 * Maps min_version 1 (destination account, pending block) to (source account, amount). (Removed)
	 * vxlnetwork::account, vxlnetwork::block_hash -> vxlnetwork::account, vxlnetwork::amount
	 */
	MDB_dbi pending_v1_handle{ 0 };

	/**
	 * Maps (destination account, pending block) to (source account, amount, version). (Removed)
	 * vxlnetwork::account, vxlnetwork::block_hash -> vxlnetwork::account, vxlnetwork::amount, vxlnetwork::epoch
	 */
	MDB_dbi pending_handle{ 0 };

	/**
	 * Representative weights. (Removed)
	 * vxlnetwork::account -> vxlnetwork::uint128_t
	 */
	MDB_dbi representation_handle{ 0 };

	/**
	 * Unchecked bootstrap blocks info.
	 * vxlnetwork::block_hash -> vxlnetwork::unchecked_info
	 */
	MDB_dbi unchecked_handle{ 0 };

	/**
	 * Samples of online vote weight
	 * uint64_t -> vxlnetwork::amount
	 */
	MDB_dbi online_weight_handle{ 0 };

	/**
	 * Meta information about block store, such as versions.
	 * vxlnetwork::uint256_union (arbitrary key) -> blob
	 */
	MDB_dbi meta_handle{ 0 };

	/**
	 * Pruned blocks hashes
	 * vxlnetwork::block_hash -> none
	 */
	MDB_dbi pruned_handle{ 0 };

	/*
	 * Endpoints for peers
	 * vxlnetwork::endpoint_key -> no_value
	*/
	MDB_dbi peers_handle{ 0 };

	/*
	 * Confirmation height of an account, and the hash for the block at that height
	 * vxlnetwork::account -> uint64_t, vxlnetwork::block_hash
	 */
	MDB_dbi confirmation_height_handle{ 0 };

	/*
	 * Contains block_sideband and block for all block types (legacy send/change/open/receive & state blocks)
	 * vxlnetwork::block_hash -> vxlnetwork::block_sideband, vxlnetwork::block
	 */
	MDB_dbi blocks_handle{ 0 };

	/**
	 * Maps root to block hash for generated final votes.
	 * vxlnetwork::qualified_root -> vxlnetwork::block_hash
	 */
	MDB_dbi final_votes_handle{ 0 };

	bool exists (vxlnetwork::transaction const & transaction_a, tables table_a, vxlnetwork::mdb_val const & key_a) const;

	int get (vxlnetwork::transaction const & transaction_a, tables table_a, vxlnetwork::mdb_val const & key_a, vxlnetwork::mdb_val & value_a) const;
	int put (vxlnetwork::write_transaction const & transaction_a, tables table_a, vxlnetwork::mdb_val const & key_a, vxlnetwork::mdb_val const & value_a) const;
	int del (vxlnetwork::write_transaction const & transaction_a, tables table_a, vxlnetwork::mdb_val const & key_a) const;

	bool copy_db (boost::filesystem::path const & destination_file) override;
	void rebuild_db (vxlnetwork::write_transaction const & transaction_a) override;

	template <typename Key, typename Value>
	vxlnetwork::store_iterator<Key, Value> make_iterator (vxlnetwork::transaction const & transaction_a, tables table_a, bool const direction_asc) const
	{
		return vxlnetwork::store_iterator<Key, Value> (std::make_unique<vxlnetwork::mdb_iterator<Key, Value>> (transaction_a, table_to_dbi (table_a), vxlnetwork::mdb_val{}, direction_asc));
	}

	template <typename Key, typename Value>
	vxlnetwork::store_iterator<Key, Value> make_iterator (vxlnetwork::transaction const & transaction_a, tables table_a, vxlnetwork::mdb_val const & key) const
	{
		return vxlnetwork::store_iterator<Key, Value> (std::make_unique<vxlnetwork::mdb_iterator<Key, Value>> (transaction_a, table_to_dbi (table_a), key));
	}

	bool init_error () const override;

	uint64_t count (vxlnetwork::transaction const &, MDB_dbi) const;
	std::string error_string (int status) const override;

	// These are only use in the upgrade process.
	std::shared_ptr<vxlnetwork::block> block_get_v14 (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a, vxlnetwork::block_sideband_v14 * sideband_a = nullptr, bool * is_state_v1 = nullptr) const;
	std::size_t block_successor_offset_v14 (vxlnetwork::transaction const & transaction_a, std::size_t entry_size_a, vxlnetwork::block_type type_a) const;
	vxlnetwork::block_hash block_successor_v14 (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const;
	vxlnetwork::mdb_val block_raw_get_v14 (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a, vxlnetwork::block_type & type_a, bool * is_state_v1 = nullptr) const;
	boost::optional<vxlnetwork::mdb_val> block_raw_get_by_type_v14 (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a, vxlnetwork::block_type & type_a, bool * is_state_v1) const;

private:
	bool do_upgrades (vxlnetwork::write_transaction &, bool &);
	void upgrade_v14_to_v15 (vxlnetwork::write_transaction &);
	void upgrade_v15_to_v16 (vxlnetwork::write_transaction const &);
	void upgrade_v16_to_v17 (vxlnetwork::write_transaction const &);
	void upgrade_v17_to_v18 (vxlnetwork::write_transaction const &);
	void upgrade_v18_to_v19 (vxlnetwork::write_transaction const &);
	void upgrade_v19_to_v20 (vxlnetwork::write_transaction const &);
	void upgrade_v20_to_v21 (vxlnetwork::write_transaction const &);

	std::shared_ptr<vxlnetwork::block> block_get_v18 (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const;
	vxlnetwork::mdb_val block_raw_get_v18 (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a, vxlnetwork::block_type & type_a) const;
	boost::optional<vxlnetwork::mdb_val> block_raw_get_by_type_v18 (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a, vxlnetwork::block_type & type_a) const;
	vxlnetwork::uint128_t block_balance_v18 (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & hash_a) const;

	void open_databases (bool &, vxlnetwork::transaction const &, unsigned);

	int drop (vxlnetwork::write_transaction const & transaction_a, tables table_a) override;
	int clear (vxlnetwork::write_transaction const & transaction_a, MDB_dbi handle_a);

	bool not_found (int status) const override;
	bool success (int status) const override;
	int status_code_not_found () const override;

	MDB_dbi table_to_dbi (tables table_a) const;

	mutable vxlnetwork::mdb_txn_tracker mdb_txn_tracker;
	vxlnetwork::mdb_txn_callbacks create_txn_callbacks () const;
	bool txn_tracking_enabled;

	uint64_t count (vxlnetwork::transaction const & transaction_a, tables table_a) const override;

	bool vacuum_after_upgrade (boost::filesystem::path const & path_a, vxlnetwork::lmdb_config const & lmdb_config_a);

	class upgrade_counters
	{
	public:
		upgrade_counters (uint64_t count_before_v0, uint64_t count_before_v1);
		bool are_equal () const;

		uint64_t before_v0;
		uint64_t before_v1;
		uint64_t after_v0{ 0 };
		uint64_t after_v1{ 0 };
	};
};

template <>
void * mdb_val::data () const;
template <>
std::size_t mdb_val::size () const;
template <>
mdb_val::db_val (std::size_t size_a, void * data_a);
template <>
void mdb_val::convert_buffer_to_value ();

extern template class store_partial<MDB_val, mdb_store>;
}
