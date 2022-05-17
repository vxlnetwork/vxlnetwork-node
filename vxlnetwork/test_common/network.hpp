#pragma once

#include <vxlnetwork/node/common.hpp>

namespace vxlnetwork
{
class node;
class system;

namespace transport
{
	class channel;
	class channel_tcp;
}

/** Waits until a TCP connection is established and returns the TCP channel on success*/
std::shared_ptr<vxlnetwork::transport::channel_tcp> establish_tcp (vxlnetwork::system &, vxlnetwork::node &, vxlnetwork::endpoint const &);
}
