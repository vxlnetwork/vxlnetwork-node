#pragma once

#include <vxlnetwork/lib/blocks.hpp>
#include <vxlnetwork/node/state_block_signature_verification.hpp>
#include <vxlnetwork/secure/common.hpp>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>

#include <chrono>
#include <memory>
#include <thread>
#include <unordered_set>

namespace vxlnetwork
{
class node;
class read_transaction;
class transaction;
class write_transaction;
class write_database_queue;

enum class block_origin
{
	local,
	remote
};

class block_post_events final
{
public:
	explicit block_post_events (std::function<vxlnetwork::read_transaction ()> &&);
	~block_post_events ();
	std::deque<std::function<void (vxlnetwork::read_transaction const &)>> events;

private:
	std::function<vxlnetwork::read_transaction ()> get_transaction;
};

/**
 * Processing blocks is a potentially long IO operation.
 * This class isolates block insertion from other operations like servicing network operations
 */
class block_processor final
{
public:
	explicit block_processor (vxlnetwork::node &, vxlnetwork::write_database_queue &);
	~block_processor ();
	void stop ();
	void flush ();
	std::size_t size ();
	bool full ();
	bool half_full ();
	void add_local (vxlnetwork::unchecked_info const & info_a);
	void add (vxlnetwork::unchecked_info const &);
	void add (std::shared_ptr<vxlnetwork::block> const &);
	void force (std::shared_ptr<vxlnetwork::block> const &);
	void wait_write ();
	bool should_log ();
	bool have_blocks_ready ();
	bool have_blocks ();
	void process_blocks ();
	vxlnetwork::process_return process_one (vxlnetwork::write_transaction const &, block_post_events &, vxlnetwork::unchecked_info, bool const = false, vxlnetwork::block_origin const = vxlnetwork::block_origin::remote);
	vxlnetwork::process_return process_one (vxlnetwork::write_transaction const &, block_post_events &, std::shared_ptr<vxlnetwork::block> const &);
	std::atomic<bool> flushing{ false };
	// Delay required for average network propagartion before requesting confirmation
	static std::chrono::milliseconds constexpr confirmation_request_delay{ 1500 };

private:
	void queue_unchecked (vxlnetwork::write_transaction const &, vxlnetwork::hash_or_account const &);
	void process_batch (vxlnetwork::unique_lock<vxlnetwork::mutex> &);
	void process_live (vxlnetwork::transaction const &, vxlnetwork::block_hash const &, std::shared_ptr<vxlnetwork::block> const &, vxlnetwork::process_return const &, vxlnetwork::block_origin const = vxlnetwork::block_origin::remote);
	void requeue_invalid (vxlnetwork::block_hash const &, vxlnetwork::unchecked_info const &);
	void process_verified_state_blocks (std::deque<vxlnetwork::state_block_signature_verification::value_type> &, std::vector<int> const &, std::vector<vxlnetwork::block_hash> const &, std::vector<vxlnetwork::signature> const &);
	bool stopped{ false };
	bool active{ false };
	bool awaiting_write{ false };
	std::chrono::steady_clock::time_point next_log;
	std::deque<vxlnetwork::unchecked_info> blocks;
	std::deque<std::shared_ptr<vxlnetwork::block>> forced;
	vxlnetwork::condition_variable condition;
	vxlnetwork::node & node;
	vxlnetwork::write_database_queue & write_database_queue;
	vxlnetwork::mutex mutex{ mutex_identifier (mutexes::block_processor) };
	vxlnetwork::state_block_signature_verification state_block_signature_verification;
	std::thread processing_thread;

	friend std::unique_ptr<container_info_component> collect_container_info (block_processor & block_processor, std::string const & name);
};
std::unique_ptr<vxlnetwork::container_info_component> collect_container_info (block_processor & block_processor, std::string const & name);
}
