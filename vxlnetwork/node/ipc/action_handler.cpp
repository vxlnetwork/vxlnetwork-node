#include <vxlnetwork/ipc_flatbuffers_lib/generated/flatbuffers/vxlnetworkapi_generated.h>
#include <vxlnetwork/lib/errors.hpp>
#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/node/ipc/action_handler.hpp>
#include <vxlnetwork/node/ipc/ipc_server.hpp>
#include <vxlnetwork/node/node.hpp>

#include <iostream>

namespace
{
vxlnetwork::account parse_account (std::string const & account, bool & out_is_deprecated_format)
{
	vxlnetwork::account result{};
	if (account.empty ())
	{
		throw vxlnetwork::error (vxlnetwork::error_common::bad_account_number);
	}

	if (result.decode_account (account))
	{
		throw vxlnetwork::error (vxlnetwork::error_common::bad_account_number);
	}
	else if (account[3] == '-' || account[4] == '-')
	{
		out_is_deprecated_format = true;
	}

	return result;
}
/** Returns the message as a Flatbuffers ObjectAPI type, managed by a unique_ptr */
template <typename T>
auto get_message (vxlnetworkapi::Envelope const & envelope)
{
	auto raw (envelope.message_as<T> ()->UnPack ());
	return std::unique_ptr<typename T::NativeTableType> (raw);
}
}

/**
 * Mapping from message type to handler function.
 * @note This must be updated whenever a new message type is added to the Flatbuffers IDL.
 */
auto vxlnetwork::ipc::action_handler::handler_map () -> std::unordered_map<vxlnetworkapi::Message, std::function<void (vxlnetwork::ipc::action_handler *, vxlnetworkapi::Envelope const &)>, vxlnetwork::ipc::enum_hash>
{
	static std::unordered_map<vxlnetworkapi::Message, std::function<void (vxlnetwork::ipc::action_handler *, vxlnetworkapi::Envelope const &)>, vxlnetwork::ipc::enum_hash> handlers;
	if (handlers.empty ())
	{
		handlers.emplace (vxlnetworkapi::Message::Message_IsAlive, &vxlnetwork::ipc::action_handler::on_is_alive);
		handlers.emplace (vxlnetworkapi::Message::Message_TopicConfirmation, &vxlnetwork::ipc::action_handler::on_topic_confirmation);
		handlers.emplace (vxlnetworkapi::Message::Message_AccountWeight, &vxlnetwork::ipc::action_handler::on_account_weight);
		handlers.emplace (vxlnetworkapi::Message::Message_ServiceRegister, &vxlnetwork::ipc::action_handler::on_service_register);
		handlers.emplace (vxlnetworkapi::Message::Message_ServiceStop, &vxlnetwork::ipc::action_handler::on_service_stop);
		handlers.emplace (vxlnetworkapi::Message::Message_TopicServiceStop, &vxlnetwork::ipc::action_handler::on_topic_service_stop);
	}
	return handlers;
}

vxlnetwork::ipc::action_handler::action_handler (vxlnetwork::node & node_a, vxlnetwork::ipc::ipc_server & server_a, std::weak_ptr<vxlnetwork::ipc::subscriber> const & subscriber_a, std::shared_ptr<flatbuffers::FlatBufferBuilder> const & builder_a) :
	flatbuffer_producer (builder_a),
	node (node_a),
	ipc_server (server_a),
	subscriber (subscriber_a)
{
}

void vxlnetwork::ipc::action_handler::on_topic_confirmation (vxlnetworkapi::Envelope const & envelope_a)
{
	auto confirmationTopic (get_message<vxlnetworkapi::TopicConfirmation> (envelope_a));
	ipc_server.get_broker ()->subscribe (subscriber, std::move (confirmationTopic));
	vxlnetworkapi::EventAckT ack;
	create_response (ack);
}

void vxlnetwork::ipc::action_handler::on_service_register (vxlnetworkapi::Envelope const & envelope_a)
{
	require_oneof (envelope_a, { vxlnetwork::ipc::access_permission::api_service_register, vxlnetwork::ipc::access_permission::service });
	auto query (get_message<vxlnetworkapi::ServiceRegister> (envelope_a));
	ipc_server.get_broker ()->service_register (query->service_name, this->subscriber);
	vxlnetworkapi::SuccessT success;
	create_response (success);
}

void vxlnetwork::ipc::action_handler::on_service_stop (vxlnetworkapi::Envelope const & envelope_a)
{
	require_oneof (envelope_a, { vxlnetwork::ipc::access_permission::api_service_stop, vxlnetwork::ipc::access_permission::service });
	auto query (get_message<vxlnetworkapi::ServiceStop> (envelope_a));
	if (query->service_name == "node")
	{
		ipc_server.node.stop ();
	}
	else
	{
		ipc_server.get_broker ()->service_stop (query->service_name);
	}
	vxlnetworkapi::SuccessT success;
	create_response (success);
}

void vxlnetwork::ipc::action_handler::on_topic_service_stop (vxlnetworkapi::Envelope const & envelope_a)
{
	auto topic (get_message<vxlnetworkapi::TopicServiceStop> (envelope_a));
	ipc_server.get_broker ()->subscribe (subscriber, std::move (topic));
	vxlnetworkapi::EventAckT ack;
	create_response (ack);
}

void vxlnetwork::ipc::action_handler::on_account_weight (vxlnetworkapi::Envelope const & envelope_a)
{
	require_oneof (envelope_a, { vxlnetwork::ipc::access_permission::api_account_weight, vxlnetwork::ipc::access_permission::account_query });
	bool is_deprecated_format{ false };
	auto query (get_message<vxlnetworkapi::AccountWeight> (envelope_a));
	auto balance (node.weight (parse_account (query->account, is_deprecated_format)));

	vxlnetworkapi::AccountWeightResponseT response;
	response.voting_weight = balance.str ();
	create_response (response);
}

void vxlnetwork::ipc::action_handler::on_is_alive (vxlnetworkapi::Envelope const & envelope)
{
	vxlnetworkapi::IsAliveT alive;
	create_response (alive);
}

bool vxlnetwork::ipc::action_handler::has_access (vxlnetworkapi::Envelope const & envelope_a, vxlnetwork::ipc::access_permission permission_a) const noexcept
{
	// If credentials are missing in the envelope, the default user is used
	std::string credentials;
	if (envelope_a.credentials () != nullptr)
	{
		credentials = envelope_a.credentials ()->str ();
	}

	return ipc_server.get_access ().has_access (credentials, permission_a);
}

bool vxlnetwork::ipc::action_handler::has_access_to_all (vxlnetworkapi::Envelope const & envelope_a, std::initializer_list<vxlnetwork::ipc::access_permission> permissions_a) const noexcept
{
	// If credentials are missing in the envelope, the default user is used
	std::string credentials;
	if (envelope_a.credentials () != nullptr)
	{
		credentials = envelope_a.credentials ()->str ();
	}

	return ipc_server.get_access ().has_access_to_all (credentials, permissions_a);
}

bool vxlnetwork::ipc::action_handler::has_access_to_oneof (vxlnetworkapi::Envelope const & envelope_a, std::initializer_list<vxlnetwork::ipc::access_permission> permissions_a) const noexcept
{
	// If credentials are missing in the envelope, the default user is used
	std::string credentials;
	if (envelope_a.credentials () != nullptr)
	{
		credentials = envelope_a.credentials ()->str ();
	}

	return ipc_server.get_access ().has_access_to_oneof (credentials, permissions_a);
}

void vxlnetwork::ipc::action_handler::require (vxlnetworkapi::Envelope const & envelope_a, vxlnetwork::ipc::access_permission permission_a) const
{
	if (!has_access (envelope_a, permission_a))
	{
		throw vxlnetwork::error (vxlnetwork::error_common::access_denied);
	}
}

void vxlnetwork::ipc::action_handler::require_all (vxlnetworkapi::Envelope const & envelope_a, std::initializer_list<vxlnetwork::ipc::access_permission> permissions_a) const
{
	if (!has_access_to_all (envelope_a, permissions_a))
	{
		throw vxlnetwork::error (vxlnetwork::error_common::access_denied);
	}
}

void vxlnetwork::ipc::action_handler::require_oneof (vxlnetworkapi::Envelope const & envelope_a, std::initializer_list<vxlnetwork::ipc::access_permission> permissions_a) const
{
	if (!has_access_to_oneof (envelope_a, permissions_a))
	{
		throw vxlnetwork::error (vxlnetwork::error_common::access_denied);
	}
}
