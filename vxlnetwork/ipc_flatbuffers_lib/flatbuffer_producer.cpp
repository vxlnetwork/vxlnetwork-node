#include <vxlnetwork/ipc_flatbuffers_lib/flatbuffer_producer.hpp>

vxlnetwork::ipc::flatbuffer_producer::flatbuffer_producer ()
{
	fbb = std::make_shared<flatbuffers::FlatBufferBuilder> ();
}

vxlnetwork::ipc::flatbuffer_producer::flatbuffer_producer (std::shared_ptr<flatbuffers::FlatBufferBuilder> const & builder_a) :
	fbb (builder_a)
{
}

void vxlnetwork::ipc::flatbuffer_producer::make_error (int code, std::string const & message)
{
	auto msg = fbb->CreateString (message);
	vxlnetworkapi::ErrorBuilder builder (*fbb);
	builder.add_code (code);
	builder.add_message (msg);
	create_builder_response (builder);
}

void vxlnetwork::ipc::flatbuffer_producer::set_correlation_id (std::string const & correlation_id_a)
{
	correlation_id = correlation_id_a;
}

void vxlnetwork::ipc::flatbuffer_producer::set_credentials (std::string const & credentials_a)
{
	credentials = credentials_a;
}

std::shared_ptr<flatbuffers::FlatBufferBuilder> vxlnetwork::ipc::flatbuffer_producer::get_shared_flatbuffer () const
{
	return fbb;
}
