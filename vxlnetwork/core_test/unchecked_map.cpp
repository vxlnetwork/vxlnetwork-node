#include <vxlnetwork/lib/blockbuilders.hpp>
#include <vxlnetwork/lib/logger_mt.hpp>
#include <vxlnetwork/node/unchecked_map.hpp>
#include <vxlnetwork/secure/store.hpp>
#include <vxlnetwork/secure/utility.hpp>
#include <vxlnetwork/test_common/system.hpp>
#include <vxlnetwork/test_common/testutil.hpp>

#include <gtest/gtest.h>

#include <memory>

using namespace std::chrono_literals;

namespace
{
class context
{
public:
	context () :
		store{ vxlnetwork::make_store (logger, vxlnetwork::unique_path (), vxlnetwork::dev::constants) },
		unchecked{ *store, false }
	{
	}
	vxlnetwork::logger_mt logger;
	std::unique_ptr<vxlnetwork::store> store;
	vxlnetwork::unchecked_map unchecked;
};
std::shared_ptr<vxlnetwork::block> block ()
{
	vxlnetwork::block_builder builder;
	return builder.state ()
	.account (vxlnetwork::dev::genesis_key.pub)
	.previous (vxlnetwork::dev::genesis->hash ())
	.representative (vxlnetwork::dev::genesis_key.pub)
	.balance (vxlnetwork::dev::constants.genesis_amount - 1)
	.link (vxlnetwork::dev::genesis_key.pub)
	.sign (vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub)
	.work (0)
	.build_shared ();
}
}

TEST (unchecked_map, construction)
{
	context context;
}

TEST (unchecked_map, put_one)
{
	context context;
	vxlnetwork::unchecked_info info{ block (), vxlnetwork::dev::genesis_key.pub };
	context.unchecked.put (info.block->previous (), info);
}

TEST (block_store, one_bootstrap)
{
	vxlnetwork::system system{};
	vxlnetwork::logger_mt logger{};
	auto store = vxlnetwork::make_store (logger, vxlnetwork::unique_path (), vxlnetwork::dev::constants);
	vxlnetwork::unchecked_map unchecked{ *store, false };
	ASSERT_TRUE (!store->init_error ());
	auto block1 = std::make_shared<vxlnetwork::send_block> (0, 1, 2, vxlnetwork::keypair ().prv, 4, 5);
	unchecked.put (block1->hash (), vxlnetwork::unchecked_info{ block1 });
	auto check_block_is_listed = [&] (vxlnetwork::transaction const & transaction_a, vxlnetwork::block_hash const & block_hash_a) {
		return unchecked.get (transaction_a, block_hash_a).size () > 0;
	};
	// Waits for the block1 to get saved in the database
	ASSERT_TIMELY (10s, check_block_is_listed (store->tx_begin_read (), block1->hash ()));
	auto transaction = store->tx_begin_read ();
	auto [begin, end] = unchecked.full_range (transaction);
	ASSERT_NE (end, begin);
	auto hash1 = begin->first.key ();
	ASSERT_EQ (block1->hash (), hash1);
	auto blocks = unchecked.get (transaction, hash1);
	ASSERT_EQ (1, blocks.size ());
	auto block2 = blocks[0].block;
	ASSERT_EQ (*block1, *block2);
	++begin;
	ASSERT_EQ (end, begin);
}
