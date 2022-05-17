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
	vxlnetwork::node_singleton_memory_pool_purge_guard memory_pool_cleanup_guard;
	testing::InitGoogleTest (&argc, argv);
	auto res = RUN_ALL_TESTS ();
	vxlnetwork::cleanup_dev_directories_on_exit ();
	return res;
}
