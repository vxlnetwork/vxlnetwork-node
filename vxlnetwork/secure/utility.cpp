#include <vxlnetwork/lib/config.hpp>
#include <vxlnetwork/secure/utility.hpp>
#include <vxlnetwork/secure/working.hpp>

#include <boost/filesystem.hpp>

static std::vector<boost::filesystem::path> all_unique_paths;

boost::filesystem::path vxlnetwork::working_path (vxlnetwork::networks network)
{
	auto result (vxlnetwork::app_path ());
	switch (network)
	{
		case vxlnetwork::networks::invalid:
			release_assert (false);
			break;
		case vxlnetwork::networks::vxlnetwork_dev_network:
			result /= "VxlnetworkDev";
			break;
		case vxlnetwork::networks::vxlnetwork_beta_network:
			result /= "VxlnetworkBeta";
			break;
		case vxlnetwork::networks::vxlnetwork_live_network:
			result /= "Vxlnetwork";
			break;
		case vxlnetwork::networks::vxlnetwork_test_network:
			result /= "VxlnetworkTest";
			break;
	}
	return result;
}

boost::filesystem::path vxlnetwork::unique_path (vxlnetwork::networks network)
{
	auto result (working_path (network) / boost::filesystem::unique_path ());
	all_unique_paths.push_back (result);
	return result;
}

void vxlnetwork::remove_temporary_directories ()
{
	for (auto & path : all_unique_paths)
	{
		boost::system::error_code ec;
		boost::filesystem::remove_all (path, ec);
		if (ec)
		{
			std::cerr << "Could not remove temporary directory: " << ec.message () << std::endl;
		}

		// lmdb creates a -lock suffixed file for its MDB_NOSUBDIR databases
		auto lockfile = path;
		lockfile += "-lock";
		boost::filesystem::remove (lockfile, ec);
		if (ec)
		{
			std::cerr << "Could not remove temporary lock file: " << ec.message () << std::endl;
		}
	}
}

namespace vxlnetwork
{
/** A wrapper for handling signals */
std::function<void ()> signal_handler_impl;
void signal_handler (int sig)
{
	if (signal_handler_impl != nullptr)
	{
		signal_handler_impl ();
	}
}
}
