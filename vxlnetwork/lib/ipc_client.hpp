#pragma once

#include <vxlnetwork/ipc_flatbuffers_lib/flatbuffer_producer.hpp>
#include <vxlnetwork/lib/asio.hpp>
#include <vxlnetwork/lib/errors.hpp>
#include <vxlnetwork/lib/ipc.hpp>
#include <vxlnetwork/lib/utility.hpp>

#include <chrono>
#include <string>
#include <vector>

#include <flatbuffers/flatbuffers.h>

namespace vxlnetwork
{
class shared_const_buffer;
namespace ipc
{
	class ipc_client_impl
	{
	public:
		virtual ~ipc_client_impl () = default;
	};

	/** IPC client */
	class ipc_client
	{
	public:
		ipc_client (boost::asio::io_context & io_ctx_a);
		ipc_client (ipc_client && ipc_client) = default;
		virtual ~ipc_client () = default;

		/** Connect to a domain socket */
		vxlnetwork::error connect (std::string const & path);

		/** Connect to a tcp socket synchronously */
		vxlnetwork::error connect (std::string const & host, uint16_t port);

		/** Connect to a tcp socket asynchronously */
		void async_connect (std::string const & host, uint16_t port, std::function<void (vxlnetwork::error)> callback);

		/** Write buffer asynchronously */
		void async_write (vxlnetwork::shared_const_buffer const & buffer_a, std::function<void (vxlnetwork::error, size_t)> callback_a);

		/** Read \p size_a bytes asynchronously */
		void async_read (std::shared_ptr<std::vector<uint8_t>> const & buffer_a, size_t size_a, std::function<void (vxlnetwork::error, size_t)> callback_a);

		/**
		 * Read a length-prefixed message asynchronously using the given timeout. This is suitable for full duplex scenarios where it may
		 * take an arbitrarily long time for the node to send messages for a given subscription.
		 * Received length must be a big endian 32-bit unsigned integer.
		 * @param buffer_a Receives the payload
		 * @param timeout_a How long to await message data. In some scenarios, such as waiting for data on subscriptions, specifying std::chrono::seconds::max() makes sense.
		 * @param callback_a If called without errors, the payload buffer is successfully populated
		 */
		void async_read_message (std::shared_ptr<std::vector<uint8_t>> const & buffer_a, std::chrono::seconds timeout_a, std::function<void (vxlnetwork::error, size_t)> callback_a);

	private:
		boost::asio::io_context & io_ctx;

		// PIMPL pattern to hide implementation details
		std::unique_ptr<ipc_client_impl> impl;
	};

	/** Convenience function for making synchronous IPC calls. The client must be connected */
	std::string request (vxlnetwork::ipc::payload_encoding encoding_a, vxlnetwork::ipc::ipc_client & ipc_client, std::string const & rpc_action_a);

	/**
	 * Returns a buffer with an IPC preamble for the given \p encoding_a
	 */
	std::vector<uint8_t> get_preamble (vxlnetwork::ipc::payload_encoding encoding_a);

	/**
	 * Returns a buffer with an IPC preamble, followed by 32-bit BE lenght, followed by payload
	 */
	vxlnetwork::shared_const_buffer prepare_flatbuffers_request (std::shared_ptr<flatbuffers::FlatBufferBuilder> const & flatbuffer_a);

	template <typename T>
	vxlnetwork::shared_const_buffer shared_buffer_from (T & object_a, std::string const & correlation_id_a = {}, std::string const & credentials_a = {})
	{
		auto buffer_l (vxlnetwork::ipc::flatbuffer_producer::make_buffer (object_a, correlation_id_a, credentials_a));
		return vxlnetwork::ipc::prepare_flatbuffers_request (buffer_l);
	}

	/**
  	 * Returns a buffer with an IPC preamble for the given \p encoding_a followed by the payload. Depending on encoding,
	 * the buffer may contain a payload length or end sentinel.
	 */
	vxlnetwork::shared_const_buffer prepare_request (vxlnetwork::ipc::payload_encoding encoding_a, std::string const & payload_a);
}
}
