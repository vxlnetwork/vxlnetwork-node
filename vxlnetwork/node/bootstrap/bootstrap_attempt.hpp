#pragma once

#include <vxlnetwork/node/bootstrap/bootstrap.hpp>

#include <atomic>
#include <future>

namespace vxlnetwork
{
class node;

class frontier_req_client;
class bulk_push_client;

/**
 * Polymorphic base class for bootstrap sessions.
 */
class bootstrap_attempt : public std::enable_shared_from_this<bootstrap_attempt>
{
public:
	explicit bootstrap_attempt (std::shared_ptr<vxlnetwork::node> const & node_a, vxlnetwork::bootstrap_mode mode_a, uint64_t incremental_id_a, std::string id_a);
	virtual ~bootstrap_attempt ();
	virtual void run () = 0;
	virtual void stop ();
	bool still_pulling ();
	void pull_started ();
	void pull_finished ();
	bool should_log ();
	std::string mode_text ();
	virtual void add_frontier (vxlnetwork::pull_info const &);
	virtual void add_bulk_push_target (vxlnetwork::block_hash const &, vxlnetwork::block_hash const &);
	virtual bool request_bulk_push_target (std::pair<vxlnetwork::block_hash, vxlnetwork::block_hash> &);
	virtual void set_start_account (vxlnetwork::account const &);
	virtual bool lazy_start (vxlnetwork::hash_or_account const &, bool confirmed = true);
	virtual void lazy_add (vxlnetwork::pull_info const &);
	virtual void lazy_requeue (vxlnetwork::block_hash const &, vxlnetwork::block_hash const &);
	virtual uint32_t lazy_batch_size ();
	virtual bool lazy_has_expired () const;
	virtual bool lazy_processed_or_exists (vxlnetwork::block_hash const &);
	virtual bool process_block (std::shared_ptr<vxlnetwork::block> const &, vxlnetwork::account const &, uint64_t, vxlnetwork::bulk_pull::count_t, bool, unsigned);
	virtual void requeue_pending (vxlnetwork::account const &);
	virtual void wallet_start (std::deque<vxlnetwork::account> &);
	virtual std::size_t wallet_size ();
	virtual void get_information (boost::property_tree::ptree &) = 0;
	vxlnetwork::mutex next_log_mutex;
	std::chrono::steady_clock::time_point next_log{ std::chrono::steady_clock::now () };
	std::atomic<unsigned> pulling{ 0 };
	std::shared_ptr<vxlnetwork::node> node;
	std::atomic<uint64_t> total_blocks{ 0 };
	std::atomic<unsigned> requeued_pulls{ 0 };
	std::atomic<bool> started{ false };
	std::atomic<bool> stopped{ false };
	uint64_t incremental_id{ 0 };
	std::string id;
	std::chrono::steady_clock::time_point attempt_start{ std::chrono::steady_clock::now () };
	std::atomic<bool> frontiers_received{ false };
	vxlnetwork::bootstrap_mode mode;
	vxlnetwork::mutex mutex;
	vxlnetwork::condition_variable condition;
};
}
