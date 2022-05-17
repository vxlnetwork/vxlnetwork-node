#include <vxlnetwork/node/common.hpp>
#include <vxlnetwork/secure/buffer.hpp>
#include <vxlnetwork/secure/common.hpp>
#include <vxlnetwork/secure/network_filter.hpp>
#include <vxlnetwork/test_common/testutil.hpp>

#include <gtest/gtest.h>

TEST (network_filter, unit)
{
	vxlnetwork::network_filter filter (1);
	auto one_block = [&filter] (std::shared_ptr<vxlnetwork::block> const & block_a, bool expect_duplicate_a) {
		vxlnetwork::publish message{ vxlnetwork::dev::network_params.network, block_a };
		auto bytes (message.to_bytes ());
		vxlnetwork::bufferstream stream (bytes->data (), bytes->size ());

		// First read the header
		bool error{ false };
		vxlnetwork::message_header header (error, stream);
		ASSERT_FALSE (error);

		// This validates vxlnetwork::message_header::size
		ASSERT_EQ (bytes->size (), block_a->size (block_a->type ()) + header.size);

		// Now filter the rest of the stream
		bool duplicate (filter.apply (bytes->data (), bytes->size () - header.size));
		ASSERT_EQ (expect_duplicate_a, duplicate);

		// Make sure the stream was rewinded correctly
		auto block (vxlnetwork::deserialize_block (stream, header.block_type ()));
		ASSERT_NE (nullptr, block);
		ASSERT_EQ (*block, *block_a);
	};
	one_block (vxlnetwork::dev::genesis, false);
	for (int i = 0; i < 10; ++i)
	{
		one_block (vxlnetwork::dev::genesis, true);
	}
	vxlnetwork::state_block_builder builder;
	auto new_block = builder
					 .account (vxlnetwork::dev::genesis_key.pub)
					 .previous (vxlnetwork::dev::genesis->hash ())
					 .representative (vxlnetwork::dev::genesis_key.pub)
					 .balance (vxlnetwork::dev::constants.genesis_amount - 10 * vxlnetwork::xrb_ratio)
					 .link (vxlnetwork::public_key ())
					 .sign (vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub)
					 .work (0)
					 .build_shared ();

	one_block (new_block, false);
	for (int i = 0; i < 10; ++i)
	{
		one_block (new_block, true);
	}
	for (int i = 0; i < 100; ++i)
	{
		one_block (vxlnetwork::dev::genesis, false);
		one_block (new_block, false);
	}
}

TEST (network_filter, many)
{
	vxlnetwork::network_filter filter (4);
	vxlnetwork::keypair key1;
	for (int i = 0; i < 100; ++i)
	{
		vxlnetwork::state_block_builder builder;
		auto block = builder
					 .account (vxlnetwork::dev::genesis_key.pub)
					 .previous (vxlnetwork::dev::genesis->hash ())
					 .representative (vxlnetwork::dev::genesis_key.pub)
					 .balance (vxlnetwork::dev::constants.genesis_amount - i * 10 * vxlnetwork::xrb_ratio)
					 .link (key1.pub)
					 .sign (vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub)
					 .work (0)
					 .build_shared ();

		vxlnetwork::publish message{ vxlnetwork::dev::network_params.network, block };
		auto bytes (message.to_bytes ());
		vxlnetwork::bufferstream stream (bytes->data (), bytes->size ());

		// First read the header
		bool error{ false };
		vxlnetwork::message_header header (error, stream);
		ASSERT_FALSE (error);

		// This validates vxlnetwork::message_header::size
		ASSERT_EQ (bytes->size (), block->size + header.size);

		// Now filter the rest of the stream
		// All blocks should pass through
		ASSERT_FALSE (filter.apply (bytes->data (), block->size));
		ASSERT_FALSE (error);

		// Make sure the stream was rewinded correctly
		auto deserialized_block (vxlnetwork::deserialize_block (stream, header.block_type ()));
		ASSERT_NE (nullptr, deserialized_block);
		ASSERT_EQ (*block, *deserialized_block);
	}
}

TEST (network_filter, clear)
{
	vxlnetwork::network_filter filter (1);
	std::vector<uint8_t> bytes1{ 1, 2, 3 };
	std::vector<uint8_t> bytes2{ 1 };
	ASSERT_FALSE (filter.apply (bytes1.data (), bytes1.size ()));
	ASSERT_TRUE (filter.apply (bytes1.data (), bytes1.size ()));
	filter.clear (bytes1.data (), bytes1.size ());
	ASSERT_FALSE (filter.apply (bytes1.data (), bytes1.size ()));
	ASSERT_TRUE (filter.apply (bytes1.data (), bytes1.size ()));
	filter.clear (bytes2.data (), bytes2.size ());
	ASSERT_TRUE (filter.apply (bytes1.data (), bytes1.size ()));
	ASSERT_FALSE (filter.apply (bytes2.data (), bytes2.size ()));
}

TEST (network_filter, optional_digest)
{
	vxlnetwork::network_filter filter (1);
	std::vector<uint8_t> bytes1{ 1, 2, 3 };
	vxlnetwork::uint128_t digest{ 0 };
	ASSERT_FALSE (filter.apply (bytes1.data (), bytes1.size (), &digest));
	ASSERT_NE (0, digest);
	ASSERT_TRUE (filter.apply (bytes1.data (), bytes1.size ()));
	filter.clear (digest);
	ASSERT_FALSE (filter.apply (bytes1.data (), bytes1.size ()));
}
