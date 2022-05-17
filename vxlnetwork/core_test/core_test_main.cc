#include "gtest/gtest.h"

#include <vxlnetwork/node/common.hpp>
#include <vxlnetwork/node/logging.hpp>
#include <vxlnetwork/secure/utility.hpp>

#include <boost/filesystem/path.hpp>

namespace vxlnetwork
{
void cleanup_dev_directories_on_exit ();
void force_vxlnetwork_dev_network ();
}

GTEST_API_ int main (int argc, char ** argv)
{
	printf ("Running main() from core_test_main.cc\n");
	vxlnetwork::force_vxlnetwork_dev_network ();
	vxlnetwork::node_singleton_memory_pool_purge_guard memory_pool_cleanup_guard;
	// Setting up logging so that there aren't any piped to standard output.
	vxlnetwork::logging logging;
	logging.init (vxlnetwork::unique_path ());
	testing::InitGoogleTest (&argc, argv);
	auto res = RUN_ALL_TESTS ();
	vxlnetwork::cleanup_dev_directories_on_exit ();
	return res;
}
