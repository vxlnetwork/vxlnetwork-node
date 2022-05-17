#pragma once

#include <vxlnetwork/lib/ipc_client.hpp>
#include <vxlnetwork/lib/rpc_handler_interface.hpp>
#include <vxlnetwork/lib/rpcconfig.hpp>
#include <vxlnetwork/rpc/rpc.hpp>

#include <deque>

namespace vxlnetwork
{
struct ipc_connection
{
	ipc_connection (vxlnetwork::ipc::ipc_client && client_a, bool is_available_a) :
		client (std::move (client_a)), is_available (is_available_a)
	{
	}

	vxlnetwork::ipc::ipc_client client;
	bool is_available{ false };
};

struct rpc_request
{
	rpc_request (std::string const & action_a, std::string const & body_a, std::function<void (std::string const &)> response_a) :
		action (action_a), body (body_a), response (response_a)
	{
	}

	rpc_request (int rpc_api_version_a, std::string const & body_a, std::function<void (std::string const &)> response_a) :
		rpc_api_version (rpc_api_version_a), body (body_a), response (response_a)
	{
	}

	rpc_request (int rpc_api_version_a, std::string const & action_a, std::string const & body_a, std::function<void (std::string const &)> response_a) :
		rpc_api_version (rpc_api_version_a), action (action_a), body (body_a), response (response_a)
	{
	}

	int rpc_api_version{ 1 };
	std::string action;
	std::string body;
	std::function<void (std::string const &)> response;
};

class rpc_request_processor
{
public:
	rpc_request_processor (boost::asio::io_context & io_ctx, vxlnetwork::rpc_config & rpc_config);
	rpc_request_processor (boost::asio::io_context & io_ctx, vxlnetwork::rpc_config & rpc_config, std::uint16_t ipc_port_a);
	~rpc_request_processor ();
	void stop ();
	void add (std::shared_ptr<rpc_request> const & request);
	std::function<void ()> stop_callback;

private:
	void run ();
	void read_payload (std::shared_ptr<vxlnetwork::ipc_connection> const & connection, std::shared_ptr<std::vector<uint8_t>> const & res, std::shared_ptr<vxlnetwork::rpc_request> const & rpc_request);
	void try_reconnect_and_execute_request (std::shared_ptr<vxlnetwork::ipc_connection> const & connection, vxlnetwork::shared_const_buffer const & req, std::shared_ptr<std::vector<uint8_t>> const & res, std::shared_ptr<vxlnetwork::rpc_request> const & rpc_request);
	void make_available (vxlnetwork::ipc_connection & connection);

	std::vector<std::shared_ptr<vxlnetwork::ipc_connection>> connections;
	vxlnetwork::mutex request_mutex;
	vxlnetwork::mutex connections_mutex;
	bool stopped{ false };
	std::deque<std::shared_ptr<vxlnetwork::rpc_request>> requests;
	vxlnetwork::condition_variable condition;
	std::string const ipc_address;
	uint16_t const ipc_port;
	std::thread thread;
};

class ipc_rpc_processor final : public vxlnetwork::rpc_handler_interface
{
public:
	ipc_rpc_processor (boost::asio::io_context & io_ctx, vxlnetwork::rpc_config & rpc_config) :
		rpc_request_processor (io_ctx, rpc_config)
	{
	}
	ipc_rpc_processor (boost::asio::io_context & io_ctx, vxlnetwork::rpc_config & rpc_config, std::uint16_t ipc_port_a) :
		rpc_request_processor (io_ctx, rpc_config, ipc_port_a)
	{
	}

	void process_request (std::string const & action_a, std::string const & body_a, std::function<void (std::string const &)> response_a) override
	{
		rpc_request_processor.add (std::make_shared<vxlnetwork::rpc_request> (action_a, body_a, response_a));
	}

	void process_request_v2 (rpc_handler_request_params const & params_a, std::string const & body_a, std::function<void (std::shared_ptr<std::string> const &)> response_a) override
	{
		std::string body_l = params_a.json_envelope (body_a);
		rpc_request_processor.add (std::make_shared<vxlnetwork::rpc_request> (2 /* rpc version */, body_l, [response_a] (std::string const & resp) {
			auto resp_l (std::make_shared<std::string> (resp));
			response_a (resp_l);
		}));
	}

	void stop () override
	{
		rpc_request_processor.stop ();
	}

	void rpc_instance (vxlnetwork::rpc & rpc) override
	{
		rpc_request_processor.stop_callback = [&rpc] () {
			rpc.stop ();
		};
	}

private:
	vxlnetwork::rpc_request_processor rpc_request_processor;
};
}
