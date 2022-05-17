#pragma once

#include <vxlnetwork/lib/numbers.hpp>

#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace vxlnetwork
{
class container_info_component;
class distributed_work;
class node;
class root;
struct work_request;

class distributed_work_factory final
{
public:
	distributed_work_factory (vxlnetwork::node &);
	~distributed_work_factory ();
	bool make (vxlnetwork::work_version const, vxlnetwork::root const &, std::vector<std::pair<std::string, uint16_t>> const &, uint64_t, std::function<void (boost::optional<uint64_t>)> const &, boost::optional<vxlnetwork::account> const & = boost::none);
	bool make (std::chrono::seconds const &, vxlnetwork::work_request const &);
	void cancel (vxlnetwork::root const &);
	void cleanup_finished ();
	void stop ();
	std::size_t size () const;

private:
	std::unordered_multimap<vxlnetwork::root, std::weak_ptr<vxlnetwork::distributed_work>> items;

	vxlnetwork::node & node;
	mutable vxlnetwork::mutex mutex;
	std::atomic<bool> stopped{ false };

	friend std::unique_ptr<container_info_component> collect_container_info (distributed_work_factory &, std::string const &);
};

std::unique_ptr<container_info_component> collect_container_info (distributed_work_factory & distributed_work, std::string const & name);
}
