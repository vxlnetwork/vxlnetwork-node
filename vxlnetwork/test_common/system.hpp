#pragma once

#include <vxlnetwork/lib/errors.hpp>
#include <vxlnetwork/node/node.hpp>

#include <chrono>

namespace vxlnetwork
{
/** Test-system related error codes */
enum class error_system
{
	generic = 1,
	deadline_expired
};
class system final
{
public:
	system ();
	system (uint16_t, vxlnetwork::transport::transport_type = vxlnetwork::transport::transport_type::tcp, vxlnetwork::node_flags = vxlnetwork::node_flags ());
	~system ();
	void ledger_initialization_set (std::vector<vxlnetwork::keypair> const & reps, vxlnetwork::amount const & reserve = 0);
	void generate_activity (vxlnetwork::node &, std::vector<vxlnetwork::account> &);
	void generate_mass_activity (uint32_t, vxlnetwork::node &);
	void generate_usage_traffic (uint32_t, uint32_t, size_t);
	void generate_usage_traffic (uint32_t, uint32_t);
	vxlnetwork::account get_random_account (std::vector<vxlnetwork::account> &);
	vxlnetwork::uint128_t get_random_amount (vxlnetwork::transaction const &, vxlnetwork::node &, vxlnetwork::account const &);
	void generate_rollback (vxlnetwork::node &, std::vector<vxlnetwork::account> &);
	void generate_change_known (vxlnetwork::node &, std::vector<vxlnetwork::account> &);
	void generate_change_unknown (vxlnetwork::node &, std::vector<vxlnetwork::account> &);
	void generate_receive (vxlnetwork::node &);
	void generate_send_new (vxlnetwork::node &, std::vector<vxlnetwork::account> &);
	void generate_send_existing (vxlnetwork::node &, std::vector<vxlnetwork::account> &);
	std::unique_ptr<vxlnetwork::state_block> upgrade_genesis_epoch (vxlnetwork::node &, vxlnetwork::epoch const);
	std::shared_ptr<vxlnetwork::wallet> wallet (size_t);
	vxlnetwork::account account (vxlnetwork::transaction const &, size_t);
	/** Generate work with difficulty between \p min_difficulty_a (inclusive) and \p max_difficulty_a (exclusive) */
	uint64_t work_generate_limited (vxlnetwork::block_hash const & root_a, uint64_t min_difficulty_a, uint64_t max_difficulty_a);
	/**
	 * Polls, sleep if there's no work to be done (default 50ms), then check the deadline
	 * @returns 0 or vxlnetwork::deadline_expired
	 */
	std::error_code poll (std::chrono::nanoseconds const & sleep_time = std::chrono::milliseconds (50));
	std::error_code poll_until_true (std::chrono::nanoseconds deadline, std::function<bool ()>);
	void delay_ms (std::chrono::milliseconds const & delay);
	void stop ();
	void deadline_set (std::chrono::duration<double, std::nano> const & delta);
	std::shared_ptr<vxlnetwork::node> add_node (vxlnetwork::node_flags = vxlnetwork::node_flags (), vxlnetwork::transport::transport_type = vxlnetwork::transport::transport_type::tcp);
	std::shared_ptr<vxlnetwork::node> add_node (vxlnetwork::node_config const &, vxlnetwork::node_flags = vxlnetwork::node_flags (), vxlnetwork::transport::transport_type = vxlnetwork::transport::transport_type::tcp);
	boost::asio::io_context io_ctx;
	std::vector<std::shared_ptr<vxlnetwork::node>> nodes;
	vxlnetwork::logging logging;
	vxlnetwork::work_pool work{ vxlnetwork::dev::network_params.network, std::max (std::thread::hardware_concurrency (), 1u) };
	std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> deadline{ std::chrono::steady_clock::time_point::max () };
	double deadline_scaling_factor{ 1.0 };
	unsigned node_sequence{ 0 };
	std::vector<std::shared_ptr<vxlnetwork::block>> initialization_blocks;
};
std::unique_ptr<vxlnetwork::state_block> upgrade_epoch (vxlnetwork::work_pool &, vxlnetwork::ledger &, vxlnetwork::epoch);
void blocks_confirm (vxlnetwork::node &, std::vector<std::shared_ptr<vxlnetwork::block>> const &, bool const = false);
uint16_t get_available_port ();
void cleanup_dev_directories_on_exit ();
}
REGISTER_ERROR_CODES (vxlnetwork, error_system);
