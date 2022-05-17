#pragma once

#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/node/active_transactions.hpp>
#include <vxlnetwork/node/prioritization.hpp>

#include <boost/optional.hpp>

#include <condition_variable>
#include <deque>
#include <memory>
#include <thread>

namespace vxlnetwork
{
class block;
class node;
class election_scheduler final
{
public:
	election_scheduler (vxlnetwork::node & node);
	~election_scheduler ();
	// Manualy start an election for a block
	// Call action with confirmed block, may be different than what we started with
	void manual (std::shared_ptr<vxlnetwork::block> const &, boost::optional<vxlnetwork::uint128_t> const & = boost::none, vxlnetwork::election_behavior = vxlnetwork::election_behavior::normal, std::function<void (std::shared_ptr<vxlnetwork::block> const &)> const & = nullptr);
	// Activates the first unconfirmed block of \p account_a
	void activate (vxlnetwork::account const &, vxlnetwork::transaction const &);
	void stop ();
	// Blocks until no more elections can be activated or there are no more elections to activate
	void flush ();
	void notify ();
	std::size_t size () const;
	bool empty () const;
	std::size_t priority_queue_size () const;
	std::unique_ptr<container_info_component> collect_container_info (std::string const &);

private:
	void run ();
	bool empty_locked () const;
	bool priority_queue_predicate () const;
	bool manual_queue_predicate () const;
	bool overfill_predicate () const;
	vxlnetwork::prioritization priority;
	std::deque<std::tuple<std::shared_ptr<vxlnetwork::block>, boost::optional<vxlnetwork::uint128_t>, vxlnetwork::election_behavior, std::function<void (std::shared_ptr<vxlnetwork::block>)>>> manual_queue;
	vxlnetwork::node & node;
	bool stopped;
	vxlnetwork::condition_variable condition;
	mutable vxlnetwork::mutex mutex;
	std::thread thread;
};
}