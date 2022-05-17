#include <vxlnetwork/lib/stats.hpp>
#include <vxlnetwork/lib/work.hpp>
#include <vxlnetwork/secure/ledger.hpp>
#include <vxlnetwork/secure/store.hpp>
#include <vxlnetwork/secure/utility.hpp>
#include <vxlnetwork/test_common/testutil.hpp>

#include <gtest/gtest.h>

TEST (processor_service, bad_send_signature)
{
	vxlnetwork::logger_mt logger;
	auto store = vxlnetwork::make_store (logger, vxlnetwork::unique_path (), vxlnetwork::dev::constants);
	ASSERT_FALSE (store->init_error ());
	vxlnetwork::stat stats;
	vxlnetwork::ledger ledger (*store, stats, vxlnetwork::dev::constants);
	auto transaction (store->tx_begin_write ());
	store->initialize (transaction, ledger.cache);
	vxlnetwork::work_pool pool{ vxlnetwork::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	vxlnetwork::account_info info1;
	ASSERT_FALSE (store->account.get (transaction, vxlnetwork::dev::genesis_key.pub, info1));
	vxlnetwork::keypair key2;
	vxlnetwork::send_block send (info1.head, vxlnetwork::dev::genesis_key.pub, 50, vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub, *pool.generate (info1.head));
	send.signature.bytes[32] ^= 0x1;
	ASSERT_EQ (vxlnetwork::process_result::bad_signature, ledger.process (transaction, send).code);
}

TEST (processor_service, bad_receive_signature)
{
	vxlnetwork::logger_mt logger;
	auto store = vxlnetwork::make_store (logger, vxlnetwork::unique_path (), vxlnetwork::dev::constants);
	ASSERT_FALSE (store->init_error ());
	vxlnetwork::stat stats;
	vxlnetwork::ledger ledger (*store, stats, vxlnetwork::dev::constants);
	auto transaction (store->tx_begin_write ());
	store->initialize (transaction, ledger.cache);
	vxlnetwork::work_pool pool{ vxlnetwork::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	vxlnetwork::account_info info1;
	ASSERT_FALSE (store->account.get (transaction, vxlnetwork::dev::genesis_key.pub, info1));
	vxlnetwork::send_block send (info1.head, vxlnetwork::dev::genesis_key.pub, 50, vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub, *pool.generate (info1.head));
	vxlnetwork::block_hash hash1 (send.hash ());
	ASSERT_EQ (vxlnetwork::process_result::progress, ledger.process (transaction, send).code);
	vxlnetwork::account_info info2;
	ASSERT_FALSE (store->account.get (transaction, vxlnetwork::dev::genesis_key.pub, info2));
	vxlnetwork::receive_block receive (hash1, hash1, vxlnetwork::dev::genesis_key.prv, vxlnetwork::dev::genesis_key.pub, *pool.generate (hash1));
	receive.signature.bytes[32] ^= 0x1;
	ASSERT_EQ (vxlnetwork::process_result::bad_signature, ledger.process (transaction, receive).code);
}
