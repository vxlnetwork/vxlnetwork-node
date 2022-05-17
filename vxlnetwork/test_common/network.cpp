#include <vxlnetwork/node/node.hpp>
#include <vxlnetwork/test_common/network.hpp>
#include <vxlnetwork/test_common/system.hpp>
#include <vxlnetwork/test_common/testutil.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <future>

using namespace std::chrono_literals;

std::shared_ptr<vxlnetwork::transport::channel_tcp> vxlnetwork::establish_tcp (vxlnetwork::system & system, vxlnetwork::node & node, vxlnetwork::endpoint const & endpoint)
{
	debug_assert (node.network.endpoint () != endpoint && "Establishing TCP to self is not allowed");

	std::shared_ptr<vxlnetwork::transport::channel_tcp> result;
	debug_assert (!node.flags.disable_tcp_realtime);
	node.network.tcp_channels.start_tcp (endpoint);
	auto error = system.poll_until_true (2s, [&result, &node, &endpoint] {
		result = node.network.tcp_channels.find_channel (vxlnetwork::transport::map_endpoint_to_tcp (endpoint));
		return result != nullptr;
	});
	return result;
}
