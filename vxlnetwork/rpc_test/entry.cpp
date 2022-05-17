#include <vxlnetwork/lib/memory.hpp>
#include <vxlnetwork/node/common.hpp>

#include <gtest/gtest.h>
namespace vxlnetwork
{
void cleanup_dev_directories_on_exit ();
void force_vxlnetwork_dev_network ();
}

int main (int argc, char ** argv)
{
	vxlnetwork::force_vxlnetwork_dev_network ();
	vxlnetwork::set_use_memory_pools (false);
	vxlnetwork::node_singleton_memory_pool_purge_guard cleanup_guard;
	testing::InitGoogleTest (&argc, argv);
	auto res = RUN_ALL_TESTS ();
	vxlnetwork::cleanup_dev_directories_on_exit ();
	return res;
}
