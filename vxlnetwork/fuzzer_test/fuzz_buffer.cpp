#include <vxlnetwork/node/common.hpp>
#include <vxlnetwork/node/testing.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace vxlnetwork
{
void force_vxlnetwork_dev_network ();
}
namespace
{
std::shared_ptr<vxlnetwork::system> system0;
std::shared_ptr<vxlnetwork::node> node0;

class fuzz_visitor : public vxlnetwork::message_visitor
{
public:
	virtual void keepalive (vxlnetwork::keepalive const &) override
	{
	}
	virtual void publish (vxlnetwork::publish const &) override
	{
	}
	virtual void confirm_req (vxlnetwork::confirm_req const &) override
	{
	}
	virtual void confirm_ack (vxlnetwork::confirm_ack const &) override
	{
	}
	virtual void bulk_pull (vxlnetwork::bulk_pull const &) override
	{
	}
	virtual void bulk_pull_account (vxlnetwork::bulk_pull_account const &) override
	{
	}
	virtual void bulk_push (vxlnetwork::bulk_push const &) override
	{
	}
	virtual void frontier_req (vxlnetwork::frontier_req const &) override
	{
	}
	virtual void node_id_handshake (vxlnetwork::node_id_handshake const &) override
	{
	}
	virtual void telemetry_req (vxlnetwork::telemetry_req const &) override
	{
	}
	virtual void telemetry_ack (vxlnetwork::telemetry_ack const &) override
	{
	}
};
}

/** Fuzz live message parsing. This covers parsing and block/vote uniquing. */
void fuzz_message_parser (uint8_t const * Data, size_t Size)
{
	static bool initialized = false;
	if (!initialized)
	{
		vxlnetwork::force_vxlnetwork_dev_network ();
		initialized = true;
		system0 = std::make_shared<vxlnetwork::system> (1);
		node0 = system0->nodes[0];
	}

	fuzz_visitor visitor;
	vxlnetwork::message_parser parser (node0->network.publish_filter, node0->block_uniquer, node0->vote_uniquer, visitor, node0->work);
	parser.deserialize_buffer (Data, Size);
}

/** Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput (uint8_t const * Data, size_t Size)
{
	fuzz_message_parser (Data, Size);
	return 0;
}
