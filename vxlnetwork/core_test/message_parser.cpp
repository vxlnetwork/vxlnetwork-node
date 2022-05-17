#include <vxlnetwork/test_common/system.hpp>
#include <vxlnetwork/test_common/testutil.hpp>

#include <gtest/gtest.h>

namespace
{
class dev_visitor : public vxlnetwork::message_visitor
{
public:
	void keepalive (vxlnetwork::keepalive const &) override
	{
		++keepalive_count;
	}
	void publish (vxlnetwork::publish const &) override
	{
		++publish_count;
	}
	void confirm_req (vxlnetwork::confirm_req const &) override
	{
		++confirm_req_count;
	}
	void confirm_ack (vxlnetwork::confirm_ack const &) override
	{
		++confirm_ack_count;
	}
	void bulk_pull (vxlnetwork::bulk_pull const &) override
	{
		ASSERT_FALSE (true);
	}
	void bulk_pull_account (vxlnetwork::bulk_pull_account const &) override
	{
		ASSERT_FALSE (true);
	}
	void bulk_push (vxlnetwork::bulk_push const &) override
	{
		ASSERT_FALSE (true);
	}
	void frontier_req (vxlnetwork::frontier_req const &) override
	{
		ASSERT_FALSE (true);
	}
	void node_id_handshake (vxlnetwork::node_id_handshake const &) override
	{
		ASSERT_FALSE (true);
	}
	void telemetry_req (vxlnetwork::telemetry_req const &) override
	{
		ASSERT_FALSE (true);
	}
	void telemetry_ack (vxlnetwork::telemetry_ack const &) override
	{
		ASSERT_FALSE (true);
	}

	uint64_t keepalive_count{ 0 };
	uint64_t publish_count{ 0 };
	uint64_t confirm_req_count{ 0 };
	uint64_t confirm_ack_count{ 0 };
};
}

TEST (message_parser, exact_confirm_ack_size)
{
	vxlnetwork::system system (1);
	dev_visitor visitor;
	vxlnetwork::network_filter filter (1);
	vxlnetwork::block_uniquer block_uniquer;
	vxlnetwork::vote_uniquer vote_uniquer (block_uniquer);
	vxlnetwork::message_parser parser (filter, block_uniquer, vote_uniquer, visitor, system.work, vxlnetwork::dev::network_params.network);
	auto block (std::make_shared<vxlnetwork::send_block> (1, 1, 2, vxlnetwork::keypair ().prv, 4, *system.work.generate (vxlnetwork::root (1))));
	auto vote (std::make_shared<vxlnetwork::vote> (0, vxlnetwork::keypair ().prv, 0, 0, std::move (block)));
	vxlnetwork::confirm_ack message{ vxlnetwork::dev::network_params.network, vote };
	std::vector<uint8_t> bytes;
	{
		vxlnetwork::vectorstream stream (bytes);
		message.serialize (stream);
	}
	ASSERT_EQ (0, visitor.confirm_ack_count);
	ASSERT_EQ (parser.status, vxlnetwork::message_parser::parse_status::success);
	auto error (false);
	vxlnetwork::bufferstream stream1 (bytes.data (), bytes.size ());
	vxlnetwork::message_header header1 (error, stream1);
	ASSERT_FALSE (error);
	parser.deserialize_confirm_ack (stream1, header1);
	ASSERT_EQ (1, visitor.confirm_ack_count);
	ASSERT_EQ (parser.status, vxlnetwork::message_parser::parse_status::success);
	bytes.push_back (0);
	vxlnetwork::bufferstream stream2 (bytes.data (), bytes.size ());
	vxlnetwork::message_header header2 (error, stream2);
	ASSERT_FALSE (error);
	parser.deserialize_confirm_ack (stream2, header2);
	ASSERT_EQ (1, visitor.confirm_ack_count);
	ASSERT_NE (parser.status, vxlnetwork::message_parser::parse_status::success);
}

TEST (message_parser, exact_confirm_req_size)
{
	vxlnetwork::system system (1);
	dev_visitor visitor;
	vxlnetwork::network_filter filter (1);
	vxlnetwork::block_uniquer block_uniquer;
	vxlnetwork::vote_uniquer vote_uniquer (block_uniquer);
	vxlnetwork::message_parser parser (filter, block_uniquer, vote_uniquer, visitor, system.work, vxlnetwork::dev::network_params.network);
	auto block (std::make_shared<vxlnetwork::send_block> (1, 1, 2, vxlnetwork::keypair ().prv, 4, *system.work.generate (vxlnetwork::root (1))));
	vxlnetwork::confirm_req message{ vxlnetwork::dev::network_params.network, block };
	std::vector<uint8_t> bytes;
	{
		vxlnetwork::vectorstream stream (bytes);
		message.serialize (stream);
	}
	ASSERT_EQ (0, visitor.confirm_req_count);
	ASSERT_EQ (parser.status, vxlnetwork::message_parser::parse_status::success);
	auto error (false);
	vxlnetwork::bufferstream stream1 (bytes.data (), bytes.size ());
	vxlnetwork::message_header header1 (error, stream1);
	ASSERT_FALSE (error);
	parser.deserialize_confirm_req (stream1, header1);
	ASSERT_EQ (1, visitor.confirm_req_count);
	ASSERT_EQ (parser.status, vxlnetwork::message_parser::parse_status::success);
	bytes.push_back (0);
	vxlnetwork::bufferstream stream2 (bytes.data (), bytes.size ());
	vxlnetwork::message_header header2 (error, stream2);
	ASSERT_FALSE (error);
	parser.deserialize_confirm_req (stream2, header2);
	ASSERT_EQ (1, visitor.confirm_req_count);
	ASSERT_NE (parser.status, vxlnetwork::message_parser::parse_status::success);
}

TEST (message_parser, exact_confirm_req_hash_size)
{
	vxlnetwork::system system (1);
	dev_visitor visitor;
	vxlnetwork::network_filter filter (1);
	vxlnetwork::block_uniquer block_uniquer;
	vxlnetwork::vote_uniquer vote_uniquer (block_uniquer);
	vxlnetwork::message_parser parser (filter, block_uniquer, vote_uniquer, visitor, system.work, vxlnetwork::dev::network_params.network);
	vxlnetwork::send_block block (1, 1, 2, vxlnetwork::keypair ().prv, 4, *system.work.generate (vxlnetwork::root (1)));
	vxlnetwork::confirm_req message{ vxlnetwork::dev::network_params.network, block.hash (), block.root () };
	std::vector<uint8_t> bytes;
	{
		vxlnetwork::vectorstream stream (bytes);
		message.serialize (stream);
	}
	ASSERT_EQ (0, visitor.confirm_req_count);
	ASSERT_EQ (parser.status, vxlnetwork::message_parser::parse_status::success);
	auto error (false);
	vxlnetwork::bufferstream stream1 (bytes.data (), bytes.size ());
	vxlnetwork::message_header header1 (error, stream1);
	ASSERT_FALSE (error);
	parser.deserialize_confirm_req (stream1, header1);
	ASSERT_EQ (1, visitor.confirm_req_count);
	ASSERT_EQ (parser.status, vxlnetwork::message_parser::parse_status::success);
	bytes.push_back (0);
	vxlnetwork::bufferstream stream2 (bytes.data (), bytes.size ());
	vxlnetwork::message_header header2 (error, stream2);
	ASSERT_FALSE (error);
	parser.deserialize_confirm_req (stream2, header2);
	ASSERT_EQ (1, visitor.confirm_req_count);
	ASSERT_NE (parser.status, vxlnetwork::message_parser::parse_status::success);
}

TEST (message_parser, exact_publish_size)
{
	vxlnetwork::system system (1);
	dev_visitor visitor;
	vxlnetwork::network_filter filter (1);
	vxlnetwork::block_uniquer block_uniquer;
	vxlnetwork::vote_uniquer vote_uniquer (block_uniquer);
	vxlnetwork::message_parser parser (filter, block_uniquer, vote_uniquer, visitor, system.work, vxlnetwork::dev::network_params.network);
	auto block (std::make_shared<vxlnetwork::send_block> (1, 1, 2, vxlnetwork::keypair ().prv, 4, *system.work.generate (vxlnetwork::root (1))));
	vxlnetwork::publish message{ vxlnetwork::dev::network_params.network, block };
	std::vector<uint8_t> bytes;
	{
		vxlnetwork::vectorstream stream (bytes);
		message.serialize (stream);
	}
	ASSERT_EQ (0, visitor.publish_count);
	ASSERT_EQ (parser.status, vxlnetwork::message_parser::parse_status::success);
	auto error (false);
	vxlnetwork::bufferstream stream1 (bytes.data (), bytes.size ());
	vxlnetwork::message_header header1 (error, stream1);
	ASSERT_FALSE (error);
	parser.deserialize_publish (stream1, header1);
	ASSERT_EQ (1, visitor.publish_count);
	ASSERT_EQ (parser.status, vxlnetwork::message_parser::parse_status::success);
	bytes.push_back (0);
	vxlnetwork::bufferstream stream2 (bytes.data (), bytes.size ());
	vxlnetwork::message_header header2 (error, stream2);
	ASSERT_FALSE (error);
	parser.deserialize_publish (stream2, header2);
	ASSERT_EQ (1, visitor.publish_count);
	ASSERT_NE (parser.status, vxlnetwork::message_parser::parse_status::success);
}

TEST (message_parser, exact_keepalive_size)
{
	vxlnetwork::system system (1);
	dev_visitor visitor;
	vxlnetwork::network_filter filter (1);
	vxlnetwork::block_uniquer block_uniquer;
	vxlnetwork::vote_uniquer vote_uniquer (block_uniquer);
	vxlnetwork::message_parser parser (filter, block_uniquer, vote_uniquer, visitor, system.work, vxlnetwork::dev::network_params.network);
	vxlnetwork::keepalive message{ vxlnetwork::dev::network_params.network };
	std::vector<uint8_t> bytes;
	{
		vxlnetwork::vectorstream stream (bytes);
		message.serialize (stream);
	}
	ASSERT_EQ (0, visitor.keepalive_count);
	ASSERT_EQ (parser.status, vxlnetwork::message_parser::parse_status::success);
	auto error (false);
	vxlnetwork::bufferstream stream1 (bytes.data (), bytes.size ());
	vxlnetwork::message_header header1 (error, stream1);
	ASSERT_FALSE (error);
	parser.deserialize_keepalive (stream1, header1);
	ASSERT_EQ (1, visitor.keepalive_count);
	ASSERT_EQ (parser.status, vxlnetwork::message_parser::parse_status::success);
	bytes.push_back (0);
	vxlnetwork::bufferstream stream2 (bytes.data (), bytes.size ());
	vxlnetwork::message_header header2 (error, stream2);
	ASSERT_FALSE (error);
	parser.deserialize_keepalive (stream2, header2);
	ASSERT_EQ (1, visitor.keepalive_count);
	ASSERT_NE (parser.status, vxlnetwork::message_parser::parse_status::success);
}
