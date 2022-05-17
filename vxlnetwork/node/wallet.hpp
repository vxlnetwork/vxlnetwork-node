#pragma once

#include <vxlnetwork/lib/lmdbconfig.hpp>
#include <vxlnetwork/lib/locks.hpp>
#include <vxlnetwork/lib/work.hpp>
#include <vxlnetwork/node/lmdb/lmdb.hpp>
#include <vxlnetwork/node/lmdb/wallet_value.hpp>
#include <vxlnetwork/node/openclwork.hpp>
#include <vxlnetwork/secure/common.hpp>
#include <vxlnetwork/secure/store.hpp>

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_set>
namespace vxlnetwork
{
class node;
class node_config;
class wallets;
// The fan spreads a key out over the heap to decrease the likelihood of it being recovered by memory inspection
class fan final
{
public:
	fan (vxlnetwork::raw_key const &, std::size_t);
	void value (vxlnetwork::raw_key &);
	void value_set (vxlnetwork::raw_key const &);
	std::vector<std::unique_ptr<vxlnetwork::raw_key>> values;

private:
	vxlnetwork::mutex mutex;
	void value_get (vxlnetwork::raw_key &);
};
class kdf final
{
public:
	kdf (unsigned & kdf_work) :
		kdf_work{ kdf_work }
	{
	}
	void phs (vxlnetwork::raw_key &, std::string const &, vxlnetwork::uint256_union const &);
	vxlnetwork::mutex mutex;
	unsigned & kdf_work;
};
enum class key_type
{
	not_a_type,
	unknown,
	adhoc,
	deterministic
};
class wallet_store final
{
public:
	wallet_store (bool &, vxlnetwork::kdf &, vxlnetwork::transaction &, vxlnetwork::account, unsigned, std::string const &);
	wallet_store (bool &, vxlnetwork::kdf &, vxlnetwork::transaction &, vxlnetwork::account, unsigned, std::string const &, std::string const &);
	std::vector<vxlnetwork::account> accounts (vxlnetwork::transaction const &);
	void initialize (vxlnetwork::transaction const &, bool &, std::string const &);
	vxlnetwork::uint256_union check (vxlnetwork::transaction const &);
	bool rekey (vxlnetwork::transaction const &, std::string const &);
	bool valid_password (vxlnetwork::transaction const &);
	bool valid_public_key (vxlnetwork::public_key const &);
	bool attempt_password (vxlnetwork::transaction const &, std::string const &);
	void wallet_key (vxlnetwork::raw_key &, vxlnetwork::transaction const &);
	void seed (vxlnetwork::raw_key &, vxlnetwork::transaction const &);
	void seed_set (vxlnetwork::transaction const &, vxlnetwork::raw_key const &);
	vxlnetwork::key_type key_type (vxlnetwork::wallet_value const &);
	vxlnetwork::public_key deterministic_insert (vxlnetwork::transaction const &);
	vxlnetwork::public_key deterministic_insert (vxlnetwork::transaction const &, uint32_t const);
	vxlnetwork::raw_key deterministic_key (vxlnetwork::transaction const &, uint32_t);
	uint32_t deterministic_index_get (vxlnetwork::transaction const &);
	void deterministic_index_set (vxlnetwork::transaction const &, uint32_t);
	void deterministic_clear (vxlnetwork::transaction const &);
	vxlnetwork::uint256_union salt (vxlnetwork::transaction const &);
	bool is_representative (vxlnetwork::transaction const &);
	vxlnetwork::account representative (vxlnetwork::transaction const &);
	void representative_set (vxlnetwork::transaction const &, vxlnetwork::account const &);
	vxlnetwork::public_key insert_adhoc (vxlnetwork::transaction const &, vxlnetwork::raw_key const &);
	bool insert_watch (vxlnetwork::transaction const &, vxlnetwork::account const &);
	void erase (vxlnetwork::transaction const &, vxlnetwork::account const &);
	vxlnetwork::wallet_value entry_get_raw (vxlnetwork::transaction const &, vxlnetwork::account const &);
	void entry_put_raw (vxlnetwork::transaction const &, vxlnetwork::account const &, vxlnetwork::wallet_value const &);
	bool fetch (vxlnetwork::transaction const &, vxlnetwork::account const &, vxlnetwork::raw_key &);
	bool exists (vxlnetwork::transaction const &, vxlnetwork::account const &);
	void destroy (vxlnetwork::transaction const &);
	vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::wallet_value> find (vxlnetwork::transaction const &, vxlnetwork::account const &);
	vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::wallet_value> begin (vxlnetwork::transaction const &, vxlnetwork::account const &);
	vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::wallet_value> begin (vxlnetwork::transaction const &);
	vxlnetwork::store_iterator<vxlnetwork::account, vxlnetwork::wallet_value> end ();
	void derive_key (vxlnetwork::raw_key &, vxlnetwork::transaction const &, std::string const &);
	void serialize_json (vxlnetwork::transaction const &, std::string &);
	void write_backup (vxlnetwork::transaction const &, boost::filesystem::path const &);
	bool move (vxlnetwork::transaction const &, vxlnetwork::wallet_store &, std::vector<vxlnetwork::public_key> const &);
	bool import (vxlnetwork::transaction const &, vxlnetwork::wallet_store &);
	bool work_get (vxlnetwork::transaction const &, vxlnetwork::public_key const &, uint64_t &);
	void work_put (vxlnetwork::transaction const &, vxlnetwork::public_key const &, uint64_t);
	unsigned version (vxlnetwork::transaction const &);
	void version_put (vxlnetwork::transaction const &, unsigned);
	vxlnetwork::fan password;
	vxlnetwork::fan wallet_key_mem;
	static unsigned const version_1 = 1;
	static unsigned const version_2 = 2;
	static unsigned const version_3 = 3;
	static unsigned const version_4 = 4;
	static unsigned constexpr version_current = version_4;
	static vxlnetwork::account const version_special;
	static vxlnetwork::account const wallet_key_special;
	static vxlnetwork::account const salt_special;
	static vxlnetwork::account const check_special;
	static vxlnetwork::account const representative_special;
	static vxlnetwork::account const seed_special;
	static vxlnetwork::account const deterministic_index_special;
	static std::size_t const check_iv_index;
	static std::size_t const seed_iv_index;
	static int const special_count;
	vxlnetwork::kdf & kdf;
	std::atomic<MDB_dbi> handle{ 0 };
	std::recursive_mutex mutex;

private:
	MDB_txn * tx (vxlnetwork::transaction const &) const;
};
// A wallet is a set of account keys encrypted by a common encryption key
class wallet final : public std::enable_shared_from_this<vxlnetwork::wallet>
{
public:
	std::shared_ptr<vxlnetwork::block> change_action (vxlnetwork::account const &, vxlnetwork::account const &, uint64_t = 0, bool = true);
	std::shared_ptr<vxlnetwork::block> receive_action (vxlnetwork::block_hash const &, vxlnetwork::account const &, vxlnetwork::uint128_union const &, vxlnetwork::account const &, uint64_t = 0, bool = true);
	std::shared_ptr<vxlnetwork::block> send_action (vxlnetwork::account const &, vxlnetwork::account const &, vxlnetwork::uint128_t const &, uint64_t = 0, bool = true, boost::optional<std::string> = {});
	bool action_complete (std::shared_ptr<vxlnetwork::block> const &, vxlnetwork::account const &, bool const, vxlnetwork::block_details const &);
	wallet (bool &, vxlnetwork::transaction &, vxlnetwork::wallets &, std::string const &);
	wallet (bool &, vxlnetwork::transaction &, vxlnetwork::wallets &, std::string const &, std::string const &);
	void enter_initial_password ();
	bool enter_password (vxlnetwork::transaction const &, std::string const &);
	vxlnetwork::public_key insert_adhoc (vxlnetwork::raw_key const &, bool = true);
	bool insert_watch (vxlnetwork::transaction const &, vxlnetwork::public_key const &);
	vxlnetwork::public_key deterministic_insert (vxlnetwork::transaction const &, bool = true);
	vxlnetwork::public_key deterministic_insert (uint32_t, bool = true);
	vxlnetwork::public_key deterministic_insert (bool = true);
	bool exists (vxlnetwork::public_key const &);
	bool import (std::string const &, std::string const &);
	void serialize (std::string &);
	bool change_sync (vxlnetwork::account const &, vxlnetwork::account const &);
	void change_async (vxlnetwork::account const &, vxlnetwork::account const &, std::function<void (std::shared_ptr<vxlnetwork::block> const &)> const &, uint64_t = 0, bool = true);
	bool receive_sync (std::shared_ptr<vxlnetwork::block> const &, vxlnetwork::account const &, vxlnetwork::uint128_t const &);
	void receive_async (vxlnetwork::block_hash const &, vxlnetwork::account const &, vxlnetwork::uint128_t const &, vxlnetwork::account const &, std::function<void (std::shared_ptr<vxlnetwork::block> const &)> const &, uint64_t = 0, bool = true);
	vxlnetwork::block_hash send_sync (vxlnetwork::account const &, vxlnetwork::account const &, vxlnetwork::uint128_t const &);
	void send_async (vxlnetwork::account const &, vxlnetwork::account const &, vxlnetwork::uint128_t const &, std::function<void (std::shared_ptr<vxlnetwork::block> const &)> const &, uint64_t = 0, bool = true, boost::optional<std::string> = {});
	void work_cache_blocking (vxlnetwork::account const &, vxlnetwork::root const &);
	void work_update (vxlnetwork::transaction const &, vxlnetwork::account const &, vxlnetwork::root const &, uint64_t);
	// Schedule work generation after a few seconds
	void work_ensure (vxlnetwork::account const &, vxlnetwork::root const &);
	bool search_receivable (vxlnetwork::transaction const &);
	void init_free_accounts (vxlnetwork::transaction const &);
	uint32_t deterministic_check (vxlnetwork::transaction const & transaction_a, uint32_t index);
	/** Changes the wallet seed and returns the first account */
	vxlnetwork::public_key change_seed (vxlnetwork::transaction const & transaction_a, vxlnetwork::raw_key const & prv_a, uint32_t count = 0);
	void deterministic_restore (vxlnetwork::transaction const & transaction_a);
	bool live ();
	std::unordered_set<vxlnetwork::account> free_accounts;
	std::function<void (bool, bool)> lock_observer;
	vxlnetwork::wallet_store store;
	vxlnetwork::wallets & wallets;
	vxlnetwork::mutex representatives_mutex;
	std::unordered_set<vxlnetwork::account> representatives;
};

class wallet_representatives
{
public:
	uint64_t voting{ 0 }; // Number of representatives with at least the configured minimum voting weight
	bool half_principal{ false }; // has representatives with at least 50% of principal representative requirements
	std::unordered_set<vxlnetwork::account> accounts; // Representatives with at least the configured minimum voting weight
	bool have_half_rep () const
	{
		return half_principal;
	}
	bool exists (vxlnetwork::account const & rep_a) const
	{
		return accounts.count (rep_a) > 0;
	}
	void clear ()
	{
		voting = 0;
		half_principal = false;
		accounts.clear ();
	}
};

/**
 * The wallets set is all the wallets a node controls.
 * A node may contain multiple wallets independently encrypted and operated.
 */
class wallets final
{
public:
	wallets (bool, vxlnetwork::node &);
	~wallets ();
	std::shared_ptr<vxlnetwork::wallet> open (vxlnetwork::wallet_id const &);
	std::shared_ptr<vxlnetwork::wallet> create (vxlnetwork::wallet_id const &);
	bool search_receivable (vxlnetwork::wallet_id const &);
	void search_receivable_all ();
	void destroy (vxlnetwork::wallet_id const &);
	void reload ();
	void do_wallet_actions ();
	void queue_wallet_action (vxlnetwork::uint128_t const &, std::shared_ptr<vxlnetwork::wallet> const &, std::function<void (vxlnetwork::wallet &)>);
	void foreach_representative (std::function<void (vxlnetwork::public_key const &, vxlnetwork::raw_key const &)> const &);
	bool exists (vxlnetwork::transaction const &, vxlnetwork::account const &);
	void start ();
	void stop ();
	void clear_send_ids (vxlnetwork::transaction const &);
	vxlnetwork::wallet_representatives reps () const;
	bool check_rep (vxlnetwork::account const &, vxlnetwork::uint128_t const &, bool const = true);
	void compute_reps ();
	void ongoing_compute_reps ();
	void split_if_needed (vxlnetwork::transaction &, vxlnetwork::store &);
	void move_table (std::string const &, MDB_txn *, MDB_txn *);
	std::unordered_map<vxlnetwork::wallet_id, std::shared_ptr<vxlnetwork::wallet>> get_wallets ();
	vxlnetwork::network_params & network_params;
	std::function<void (bool)> observer;
	std::unordered_map<vxlnetwork::wallet_id, std::shared_ptr<vxlnetwork::wallet>> items;
	std::multimap<vxlnetwork::uint128_t, std::pair<std::shared_ptr<vxlnetwork::wallet>, std::function<void (vxlnetwork::wallet &)>>, std::greater<vxlnetwork::uint128_t>> actions;
	vxlnetwork::locked<std::unordered_map<vxlnetwork::account, vxlnetwork::root>> delayed_work;
	vxlnetwork::mutex mutex;
	vxlnetwork::mutex action_mutex;
	vxlnetwork::condition_variable condition;
	vxlnetwork::kdf kdf;
	MDB_dbi handle;
	MDB_dbi send_action_ids;
	vxlnetwork::node & node;
	vxlnetwork::mdb_env & env;
	std::atomic<bool> stopped;
	std::thread thread;
	static vxlnetwork::uint128_t const generate_priority;
	static vxlnetwork::uint128_t const high_priority;
	/** Start read-write transaction */
	vxlnetwork::write_transaction tx_begin_write ();

	/** Start read-only transaction */
	vxlnetwork::read_transaction tx_begin_read ();

private:
	mutable vxlnetwork::mutex reps_cache_mutex;
	vxlnetwork::wallet_representatives representatives;
};

std::unique_ptr<container_info_component> collect_container_info (wallets & wallets, std::string const & name);

class wallets_store
{
public:
	virtual ~wallets_store () = default;
	virtual bool init_error () const = 0;
};
class mdb_wallets_store final : public wallets_store
{
public:
	mdb_wallets_store (boost::filesystem::path const &, vxlnetwork::lmdb_config const & lmdb_config_a = vxlnetwork::lmdb_config{});
	vxlnetwork::mdb_env environment;
	bool init_error () const override;
	bool error{ false };
};
}
