#include <vxlnetwork/lib/cli.hpp>
#include <vxlnetwork/lib/errors.hpp>
#include <vxlnetwork/lib/signal_manager.hpp>
#include <vxlnetwork/lib/threading.hpp>
#include <vxlnetwork/lib/tlsconfig.hpp>
#include <vxlnetwork/lib/utility.hpp>
#include <vxlnetwork/node/cli.hpp>
#include <vxlnetwork/node/ipc/ipc_server.hpp>
#include <vxlnetwork/rpc/rpc.hpp>
#include <vxlnetwork/rpc/rpc_request_processor.hpp>
#include <vxlnetwork/secure/utility.hpp>

#include <boost/filesystem.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/program_options.hpp>

namespace
{
void logging_init (boost::filesystem::path const & application_path_a)
{
	static std::atomic_flag logging_already_added = ATOMIC_FLAG_INIT;
	if (!logging_already_added.test_and_set ())
	{
		boost::log::add_common_attributes ();
		auto path = application_path_a / "log";

		uintmax_t max_size{ 128 * 1024 * 1024 };
		uintmax_t rotation_size{ 4 * 1024 * 1024 };
		bool flush{ true };
		boost::log::add_file_log (boost::log::keywords::target = path, boost::log::keywords::file_name = path / "rpc_log_%Y-%m-%d_%H-%M-%S.%N.log", boost::log::keywords::rotation_size = rotation_size, boost::log::keywords::auto_flush = flush, boost::log::keywords::scan_method = boost::log::sinks::file::scan_method::scan_matching, boost::log::keywords::max_size = max_size, boost::log::keywords::format = "[%TimeStamp%]: %Message%");
	}
}

volatile sig_atomic_t sig_int_or_term = 0;

void run (boost::filesystem::path const & data_path, std::vector<std::string> const & config_overrides)
{
	boost::filesystem::create_directories (data_path);
	boost::system::error_code error_chmod;
	vxlnetwork::set_secure_perm_directory (data_path, error_chmod);
	std::unique_ptr<vxlnetwork::thread_runner> runner;

	vxlnetwork::network_params network_params{ vxlnetwork::network_constants::active_network };
	vxlnetwork::rpc_config rpc_config{ network_params.network };
	auto error = vxlnetwork::read_rpc_config_toml (data_path, rpc_config, config_overrides);
	if (!error)
	{
		logging_init (data_path);
		vxlnetwork::logger_mt logger;

		auto tls_config (std::make_shared<vxlnetwork::tls_config> ());
		error = vxlnetwork::read_tls_config_toml (data_path, *tls_config, logger);
		if (error)
		{
			std::cerr << error.get_message () << std::endl;
			std::exit (1);
		}
		else
		{
			rpc_config.tls_config = tls_config;
		}

		boost::asio::io_context io_ctx;
		vxlnetwork::signal_manager sigman;
		try
		{
			vxlnetwork::ipc_rpc_processor ipc_rpc_processor (io_ctx, rpc_config);
			auto rpc = vxlnetwork::get_rpc (io_ctx, rpc_config, ipc_rpc_processor);
			rpc->start ();

			debug_assert (!vxlnetwork::signal_handler_impl);
			vxlnetwork::signal_handler_impl = [&io_ctx] () {
				io_ctx.stop ();
				sig_int_or_term = 1;
			};

			sigman.register_signal_handler (SIGINT, &vxlnetwork::signal_handler, true);
			sigman.register_signal_handler (SIGTERM, &vxlnetwork::signal_handler, false);

			runner = std::make_unique<vxlnetwork::thread_runner> (io_ctx, rpc_config.rpc_process.io_threads);
			runner->join ();

			if (sig_int_or_term == 1)
			{
				rpc->stop ();
			}
		}
		catch (std::runtime_error const & e)
		{
			std::cerr << "Error while running rpc (" << e.what () << ")\n";
		}
	}
	else
	{
		std::cerr << "Error deserializing config: " << error.get_message () << std::endl;
	}
}
}

int main (int argc, char * const * argv)
{
	vxlnetwork::set_umask ();

	boost::program_options::options_description description ("Command line options");

	// clang-format off
	description.add_options ()
		("help", "Print out options")
		("config", boost::program_options::value<std::vector<vxlnetwork::config_key_value_pair>>()->multitoken(), "Pass RPC configuration values. This takes precedence over any values in the configuration file. This option can be repeated multiple times.")
		("daemon", "Start RPC daemon")
		("data_path", boost::program_options::value<std::string> (), "Use the supplied path as the data directory")
		("network", boost::program_options::value<std::string> (), "Use the supplied network (live, test, beta or dev)")
		("version", "Prints out version");
	// clang-format on

	boost::program_options::variables_map vm;
	try
	{
		boost::program_options::store (boost::program_options::parse_command_line (argc, argv, description), vm);
	}
	catch (boost::program_options::error const & err)
	{
		std::cerr << err.what () << std::endl;
		return 1;
	}
	boost::program_options::notify (vm);

	auto network (vm.find ("network"));
	if (network != vm.end ())
	{
		auto err (vxlnetwork::network_constants::set_active_network (network->second.as<std::string> ()));
		if (err)
		{
			std::cerr << vxlnetwork::network_constants::active_network_err_msg << std::endl;
			std::exit (1);
		}
	}

	auto data_path_it = vm.find ("data_path");
	boost::filesystem::path data_path ((data_path_it != vm.end ()) ? data_path_it->second.as<std::string> () : vxlnetwork::working_path ());
	if (vm.count ("daemon") > 0)
	{
		std::vector<std::string> config_overrides;
		auto config (vm.find ("config"));
		if (config != vm.end ())
		{
			config_overrides = vxlnetwork::config_overrides (config->second.as<std::vector<vxlnetwork::config_key_value_pair>> ());
		}
		run (data_path, config_overrides);
	}
	else if (vm.count ("version"))
	{
		std::cout << "Version " << VXLNETWORK_VERSION_STRING << "\n"
				  << "Build Info " << BUILD_INFO << std::endl;
	}
	else
	{
		// Issue #3748
		// Regardless how the options were added, output the options in alphabetical order so they are easy to find.
		boost::program_options::options_description sorted_description ("Command line options");
		vxlnetwork::sort_options_description (description, sorted_description);
		std::cout << sorted_description << std::endl;
	}

	return 1;
}
