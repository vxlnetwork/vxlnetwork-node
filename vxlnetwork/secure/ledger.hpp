#pragma once

#include <vxlnetwork/lib/rep_weights.hpp>
#include <vxlnetwork/lib/timer.hpp>
#include <vxlnetwork/secure/common.hpp>

#include <map>

namespace vxlnetwork
{
class store;
class stat;
class write_transaction;

// map of vote weight per block, ordered greater first
using tally_t = std::map<vxlnetwork::uint128_t, std::shared_ptr<vxlnetwork::block>, std::greater<vxlnetwork::uint128_t>>;

class uncemented_info
{
public:
	uncemented_info (vxlnetwork::block_hash const & cemented_frontier, vxlnetwork::block_hash const & frontier, vxlnetwork::account const & account);
	vxlnetwork::block_hash cemented_frontier;
	vxlnetwork::block_hash frontier;
	vxlnetwork::account account;
};

class ledger final
{
public:
	ledger (vxlnetwork::store &, vxlnetwork::stat &, vxlnetwork::ledger_constants & constants, vxlnetwork::generate_cache const & = vxlnetwork::generate_cache ());
	vxlnetwork::account account (vxlnetwork::transaction const &, vxlnetwork::block_hash const &) const;
	vxlnetwork::account account_safe (vxlnetwork::transaction const &, vxlnetwork::block_hash const &, bool &) const;
	vxlnetwork::uint128_t amount (vxlnetwork::transaction const &, vxlnetwork::account const &);
	vxlnetwork::uint128_t amount (vxlnetwork::transaction const &, vxlnetwork::block_hash const &);
	/** Safe for previous block, but block hash_a must exist */
	vxlnetwork::uint128_t amount_safe (vxlnetwork::transaction const &, vxlnetwork::block_hash const & hash_a, bool &) const;
	vxlnetwork::uint128_t balance (vxlnetwork::transaction const &, vxlnetwork::block_hash const &) const;
	vxlnetwork::uint128_t balance_safe (vxlnetwork::transaction const &, vxlnetwork::block_hash const &, bool &) const;
	vxlnetwork::uint128_t account_balance (vxlnetwork::transaction const &, vxlnetwork::account const &, bool = false);
	vxlnetwork::uint128_t account_receivable (vxlnetwork::transaction const &, vxlnetwork::account const &, bool = false);
	vxlnetwork::uint128_t weight (vxlnetwork::account const &);
	std::shared_ptr<vxlnetwork::block> successor (vxlnetwork::transaction const &, vxlnetwork::qualified_root const &);
	std::shared_ptr<vxlnetwork::block> forked_block (vxlnetwork::transaction const &, vxlnetwork::block const &);
	bool block_confirmed (vxlnetwork::transaction const &, vxlnetwork::block_hash const &) const;
	vxlnetwork::block_hash latest (vxlnetwork::transaction const &, vxlnetwork::account const &);
	vxlnetwork::root latest_root (vxlnetwork::transaction const &, vxlnetwork::account const &);
	vxlnetwork::block_hash representative (vxlnetwork::transaction const &, vxlnetwork::block_hash const &);
	vxlnetwork::block_hash representative_calculated (vxlnetwork::transaction const &, vxlnetwork::block_hash const &);
	bool block_or_pruned_exists (vxlnetwork::block_hash const &) const;
	bool block_or_pruned_exists (vxlnetwork::transaction const &, vxlnetwork::block_hash const &) const;
	std::string block_text (char const *);
	std::string block_text (vxlnetwork::block_hash const &);
	bool is_send (vxlnetwork::transaction const &, vxlnetwork::state_block const &) const;
	vxlnetwork::account const & block_destination (vxlnetwork::transaction const &, vxlnetwork::block const &);
	vxlnetwork::block_hash block_source (vxlnetwork::transaction const &, vxlnetwork::block const &);
	std::pair<vxlnetwork::block_hash, vxlnetwork::block_hash> hash_root_random (vxlnetwork::transaction const &) const;
	vxlnetwork::process_return process (vxlnetwork::write_transaction const &, vxlnetwork::block &, vxlnetwork::signature_verification = vxlnetwork::signature_verification::unknown);
	bool rollback (vxlnetwork::write_transaction const &, vxlnetwork::block_hash const &, std::vector<std::shared_ptr<vxlnetwork::block>> &);
	bool rollback (vxlnetwork::write_transaction const &, vxlnetwork::block_hash const &);
	void update_account (vxlnetwork::write_transaction const &, vxlnetwork::account const &, vxlnetwork::account_info const &, vxlnetwork::account_info const &);
	uint64_t pruning_action (vxlnetwork::write_transaction &, vxlnetwork::block_hash const &, uint64_t const);
	void dump_account_chain (vxlnetwork::account const &, std::ostream & = std::cout);
	bool could_fit (vxlnetwork::transaction const &, vxlnetwork::block const &) const;
	bool dependents_confirmed (vxlnetwork::transaction const &, vxlnetwork::block const &) const;
	bool is_epoch_link (vxlnetwork::link const &) const;
	std::array<vxlnetwork::block_hash, 2> dependent_blocks (vxlnetwork::transaction const &, vxlnetwork::block const &) const;
	std::shared_ptr<vxlnetwork::block> find_receive_block_by_send_hash (vxlnetwork::transaction const & transaction, vxlnetwork::account const & destination, vxlnetwork::block_hash const & send_block_hash);
	vxlnetwork::account const & epoch_signer (vxlnetwork::link const &) const;
	vxlnetwork::link const & epoch_link (vxlnetwork::epoch) const;
	std::multimap<uint64_t, uncemented_info, std::greater<>> unconfirmed_frontiers () const;
	bool migrate_lmdb_to_rocksdb (boost::filesystem::path const &) const;
	static vxlnetwork::uint128_t const unit;
	vxlnetwork::ledger_constants & constants;
	vxlnetwork::store & store;
	vxlnetwork::ledger_cache cache;
	vxlnetwork::stat & stats;
	std::unordered_map<vxlnetwork::account, vxlnetwork::uint128_t> bootstrap_weights;
	std::atomic<size_t> bootstrap_weights_size{ 0 };
	uint64_t bootstrap_weight_max_blocks{ 1 };
	std::atomic<bool> check_bootstrap_weights;
	bool pruning{ false };

private:
	void initialize (vxlnetwork::generate_cache const &);
};

std::unique_ptr<container_info_component> collect_container_info (ledger & ledger, std::string const & name);
}
