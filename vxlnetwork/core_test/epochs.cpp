#include <vxlnetwork/lib/epoch.hpp>
#include <vxlnetwork/secure/common.hpp>

#include <gtest/gtest.h>

TEST (epochs, is_epoch_link)
{
	vxlnetwork::epochs epochs;
	// Test epoch 1
	vxlnetwork::keypair key1;
	auto link1 = 42;
	auto link2 = 43;
	ASSERT_FALSE (epochs.is_epoch_link (link1));
	ASSERT_FALSE (epochs.is_epoch_link (link2));
	epochs.add (vxlnetwork::epoch::epoch_1, key1.pub, link1);
	ASSERT_TRUE (epochs.is_epoch_link (link1));
	ASSERT_FALSE (epochs.is_epoch_link (link2));
	ASSERT_EQ (key1.pub, epochs.signer (vxlnetwork::epoch::epoch_1));
	ASSERT_EQ (epochs.epoch (link1), vxlnetwork::epoch::epoch_1);

	// Test epoch 2
	vxlnetwork::keypair key2;
	epochs.add (vxlnetwork::epoch::epoch_2, key2.pub, link2);
	ASSERT_TRUE (epochs.is_epoch_link (link2));
	ASSERT_EQ (key2.pub, epochs.signer (vxlnetwork::epoch::epoch_2));
	ASSERT_EQ (vxlnetwork::uint256_union (link1), epochs.link (vxlnetwork::epoch::epoch_1));
	ASSERT_EQ (vxlnetwork::uint256_union (link2), epochs.link (vxlnetwork::epoch::epoch_2));
	ASSERT_EQ (epochs.epoch (link2), vxlnetwork::epoch::epoch_2);
}

TEST (epochs, is_sequential)
{
	ASSERT_TRUE (vxlnetwork::epochs::is_sequential (vxlnetwork::epoch::epoch_0, vxlnetwork::epoch::epoch_1));
	ASSERT_TRUE (vxlnetwork::epochs::is_sequential (vxlnetwork::epoch::epoch_1, vxlnetwork::epoch::epoch_2));

	ASSERT_FALSE (vxlnetwork::epochs::is_sequential (vxlnetwork::epoch::epoch_0, vxlnetwork::epoch::epoch_2));
	ASSERT_FALSE (vxlnetwork::epochs::is_sequential (vxlnetwork::epoch::epoch_0, vxlnetwork::epoch::invalid));
	ASSERT_FALSE (vxlnetwork::epochs::is_sequential (vxlnetwork::epoch::unspecified, vxlnetwork::epoch::epoch_1));
	ASSERT_FALSE (vxlnetwork::epochs::is_sequential (vxlnetwork::epoch::epoch_1, vxlnetwork::epoch::epoch_0));
	ASSERT_FALSE (vxlnetwork::epochs::is_sequential (vxlnetwork::epoch::epoch_2, vxlnetwork::epoch::epoch_0));
	ASSERT_FALSE (vxlnetwork::epochs::is_sequential (vxlnetwork::epoch::epoch_2, vxlnetwork::epoch::epoch_2));
}
