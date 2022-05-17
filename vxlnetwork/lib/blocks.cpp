#include <vxlnetwork/crypto_lib/random_pool.hpp>
#include <vxlnetwork/lib/blocks.hpp>
#include <vxlnetwork/lib/memory.hpp>
#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/lib/threading.hpp>
#include <vxlnetwork/secure/common.hpp>

#include <crypto/cryptopp/words.h>

#include <boost/endian/conversion.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <bitset>

/** Compare blocks, first by type, then content. This is an optimization over dynamic_cast, which is very slow on some platforms. */
namespace
{
template <typename T>
bool blocks_equal (T const & first, vxlnetwork::block const & second)
{
	static_assert (std::is_base_of<vxlnetwork::block, T>::value, "Input parameter is not a block type");
	return (first.type () == second.type ()) && (static_cast<T const &> (second)) == first;
}

template <typename block>
std::shared_ptr<block> deserialize_block (vxlnetwork::stream & stream_a)
{
	auto error (false);
	auto result = vxlnetwork::make_shared<block> (error, stream_a);
	if (error)
	{
		result = nullptr;
	}

	return result;
}
}

void vxlnetwork::block_memory_pool_purge ()
{
	vxlnetwork::purge_shared_ptr_singleton_pool_memory<vxlnetwork::open_block> ();
	vxlnetwork::purge_shared_ptr_singleton_pool_memory<vxlnetwork::state_block> ();
	vxlnetwork::purge_shared_ptr_singleton_pool_memory<vxlnetwork::send_block> ();
	vxlnetwork::purge_shared_ptr_singleton_pool_memory<vxlnetwork::change_block> ();
}

std::string vxlnetwork::block::to_json () const
{
	std::string result;
	serialize_json (result);
	return result;
}

size_t vxlnetwork::block::size (vxlnetwork::block_type type_a)
{
	size_t result (0);
	switch (type_a)
	{
		case vxlnetwork::block_type::invalid:
		case vxlnetwork::block_type::not_a_block:
			debug_assert (false);
			break;
		case vxlnetwork::block_type::send:
			result = vxlnetwork::send_block::size;
			break;
		case vxlnetwork::block_type::receive:
			result = vxlnetwork::receive_block::size;
			break;
		case vxlnetwork::block_type::change:
			result = vxlnetwork::change_block::size;
			break;
		case vxlnetwork::block_type::open:
			result = vxlnetwork::open_block::size;
			break;
		case vxlnetwork::block_type::state:
			result = vxlnetwork::state_block::size;
			break;
	}
	return result;
}

vxlnetwork::work_version vxlnetwork::block::work_version () const
{
	return vxlnetwork::work_version::work_1;
}

vxlnetwork::block_hash vxlnetwork::block::generate_hash () const
{
	vxlnetwork::block_hash result;
	blake2b_state hash_l;
	auto status (blake2b_init (&hash_l, sizeof (result.bytes)));
	debug_assert (status == 0);
	hash (hash_l);
	status = blake2b_final (&hash_l, result.bytes.data (), sizeof (result.bytes));
	debug_assert (status == 0);
	return result;
}

void vxlnetwork::block::refresh ()
{
	if (!cached_hash.is_zero ())
	{
		cached_hash = generate_hash ();
	}
}

vxlnetwork::block_hash const & vxlnetwork::block::hash () const
{
	if (!cached_hash.is_zero ())
	{
		// Once a block is created, it should not be modified (unless using refresh ())
		// This would invalidate the cache; check it hasn't changed.
		debug_assert (cached_hash == generate_hash ());
	}
	else
	{
		cached_hash = generate_hash ();
	}

	return cached_hash;
}

vxlnetwork::block_hash vxlnetwork::block::full_hash () const
{
	vxlnetwork::block_hash result;
	blake2b_state state;
	blake2b_init (&state, sizeof (result.bytes));
	blake2b_update (&state, hash ().bytes.data (), sizeof (hash ()));
	auto signature (block_signature ());
	blake2b_update (&state, signature.bytes.data (), sizeof (signature));
	auto work (block_work ());
	blake2b_update (&state, &work, sizeof (work));
	blake2b_final (&state, result.bytes.data (), sizeof (result.bytes));
	return result;
}

vxlnetwork::block_sideband const & vxlnetwork::block::sideband () const
{
	debug_assert (sideband_m.is_initialized ());
	return *sideband_m;
}

void vxlnetwork::block::sideband_set (vxlnetwork::block_sideband const & sideband_a)
{
	sideband_m = sideband_a;
}

bool vxlnetwork::block::has_sideband () const
{
	return sideband_m.is_initialized ();
}

vxlnetwork::account const & vxlnetwork::block::representative () const
{
	static vxlnetwork::account representative{};
	return representative;
}

vxlnetwork::block_hash const & vxlnetwork::block::source () const
{
	static vxlnetwork::block_hash source{ 0 };
	return source;
}

vxlnetwork::account const & vxlnetwork::block::destination () const
{
	static vxlnetwork::account destination{};
	return destination;
}

vxlnetwork::link const & vxlnetwork::block::link () const
{
	static vxlnetwork::link link{ 0 };
	return link;
}

vxlnetwork::account const & vxlnetwork::block::account () const
{
	static vxlnetwork::account account{};
	return account;
}

vxlnetwork::qualified_root vxlnetwork::block::qualified_root () const
{
	return vxlnetwork::qualified_root (root (), previous ());
}

vxlnetwork::amount const & vxlnetwork::block::balance () const
{
	static vxlnetwork::amount amount{ 0 };
	return amount;
}

void vxlnetwork::send_block::visit (vxlnetwork::block_visitor & visitor_a) const
{
	visitor_a.send_block (*this);
}

void vxlnetwork::send_block::visit (vxlnetwork::mutable_block_visitor & visitor_a)
{
	visitor_a.send_block (*this);
}

void vxlnetwork::send_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t vxlnetwork::send_block::block_work () const
{
	return work;
}

void vxlnetwork::send_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

vxlnetwork::send_hashables::send_hashables (vxlnetwork::block_hash const & previous_a, vxlnetwork::account const & destination_a, vxlnetwork::amount const & balance_a) :
	previous (previous_a),
	destination (destination_a),
	balance (balance_a)
{
}

vxlnetwork::send_hashables::send_hashables (bool & error_a, vxlnetwork::stream & stream_a)
{
	try
	{
		vxlnetwork::read (stream_a, previous.bytes);
		vxlnetwork::read (stream_a, destination.bytes);
		vxlnetwork::read (stream_a, balance.bytes);
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

vxlnetwork::send_hashables::send_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto destination_l (tree_a.get<std::string> ("destination"));
		auto balance_l (tree_a.get<std::string> ("balance"));
		error_a = previous.decode_hex (previous_l);
		if (!error_a)
		{
			error_a = destination.decode_account (destination_l);
			if (!error_a)
			{
				error_a = balance.decode_hex (balance_l);
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void vxlnetwork::send_hashables::hash (blake2b_state & hash_a) const
{
	auto status (blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes)));
	debug_assert (status == 0);
	status = blake2b_update (&hash_a, destination.bytes.data (), sizeof (destination.bytes));
	debug_assert (status == 0);
	status = blake2b_update (&hash_a, balance.bytes.data (), sizeof (balance.bytes));
	debug_assert (status == 0);
}

void vxlnetwork::send_block::serialize (vxlnetwork::stream & stream_a) const
{
	write (stream_a, hashables.previous.bytes);
	write (stream_a, hashables.destination.bytes);
	write (stream_a, hashables.balance.bytes);
	write (stream_a, signature.bytes);
	write (stream_a, work);
}

bool vxlnetwork::send_block::deserialize (vxlnetwork::stream & stream_a)
{
	auto error (false);
	try
	{
		read (stream_a, hashables.previous.bytes);
		read (stream_a, hashables.destination.bytes);
		read (stream_a, hashables.balance.bytes);
		read (stream_a, signature.bytes);
		read (stream_a, work);
	}
	catch (std::exception const &)
	{
		error = true;
	}

	return error;
}

void vxlnetwork::send_block::serialize_json (std::string & string_a, bool single_line) const
{
	boost::property_tree::ptree tree;
	serialize_json (tree);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree, !single_line);
	string_a = ostream.str ();
}

void vxlnetwork::send_block::serialize_json (boost::property_tree::ptree & tree) const
{
	tree.put ("type", "send");
	std::string previous;
	hashables.previous.encode_hex (previous);
	tree.put ("previous", previous);
	tree.put ("destination", hashables.destination.to_account ());
	std::string balance;
	hashables.balance.encode_hex (balance);
	tree.put ("balance", balance);
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("work", vxlnetwork::to_string_hex (work));
	tree.put ("signature", signature_l);
}

bool vxlnetwork::send_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		debug_assert (tree_a.get<std::string> ("type") == "send");
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto destination_l (tree_a.get<std::string> ("destination"));
		auto balance_l (tree_a.get<std::string> ("balance"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.previous.decode_hex (previous_l);
		if (!error)
		{
			error = hashables.destination.decode_account (destination_l);
			if (!error)
			{
				error = hashables.balance.decode_hex (balance_l);
				if (!error)
				{
					error = vxlnetwork::from_string_hex (work_l, work);
					if (!error)
					{
						error = signature.decode_hex (signature_l);
					}
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

vxlnetwork::send_block::send_block (vxlnetwork::block_hash const & previous_a, vxlnetwork::account const & destination_a, vxlnetwork::amount const & balance_a, vxlnetwork::raw_key const & prv_a, vxlnetwork::public_key const & pub_a, uint64_t work_a) :
	hashables (previous_a, destination_a, balance_a),
	signature (vxlnetwork::sign_message (prv_a, pub_a, hash ())),
	work (work_a)
{
	debug_assert (destination_a != nullptr);
	debug_assert (pub_a != nullptr);
}

vxlnetwork::send_block::send_block (bool & error_a, vxlnetwork::stream & stream_a) :
	hashables (error_a, stream_a)
{
	if (!error_a)
	{
		try
		{
			vxlnetwork::read (stream_a, signature.bytes);
			vxlnetwork::read (stream_a, work);
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

vxlnetwork::send_block::send_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
	hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto signature_l (tree_a.get<std::string> ("signature"));
			auto work_l (tree_a.get<std::string> ("work"));
			error_a = signature.decode_hex (signature_l);
			if (!error_a)
			{
				error_a = vxlnetwork::from_string_hex (work_l, work);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

bool vxlnetwork::send_block::operator== (vxlnetwork::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool vxlnetwork::send_block::valid_predecessor (vxlnetwork::block const & block_a) const
{
	bool result;
	switch (block_a.type ())
	{
		case vxlnetwork::block_type::send:
		case vxlnetwork::block_type::receive:
		case vxlnetwork::block_type::open:
		case vxlnetwork::block_type::change:
			result = true;
			break;
		default:
			result = false;
			break;
	}
	return result;
}

vxlnetwork::block_type vxlnetwork::send_block::type () const
{
	return vxlnetwork::block_type::send;
}

bool vxlnetwork::send_block::operator== (vxlnetwork::send_block const & other_a) const
{
	auto result (hashables.destination == other_a.hashables.destination && hashables.previous == other_a.hashables.previous && hashables.balance == other_a.hashables.balance && work == other_a.work && signature == other_a.signature);
	return result;
}

vxlnetwork::block_hash const & vxlnetwork::send_block::previous () const
{
	return hashables.previous;
}

vxlnetwork::account const & vxlnetwork::send_block::destination () const
{
	return hashables.destination;
}

vxlnetwork::root const & vxlnetwork::send_block::root () const
{
	return hashables.previous;
}

vxlnetwork::amount const & vxlnetwork::send_block::balance () const
{
	return hashables.balance;
}

vxlnetwork::signature const & vxlnetwork::send_block::block_signature () const
{
	return signature;
}

void vxlnetwork::send_block::signature_set (vxlnetwork::signature const & signature_a)
{
	signature = signature_a;
}

vxlnetwork::open_hashables::open_hashables (vxlnetwork::block_hash const & source_a, vxlnetwork::account const & representative_a, vxlnetwork::account const & account_a) :
	source (source_a),
	representative (representative_a),
	account (account_a)
{
}

vxlnetwork::open_hashables::open_hashables (bool & error_a, vxlnetwork::stream & stream_a)
{
	try
	{
		vxlnetwork::read (stream_a, source.bytes);
		vxlnetwork::read (stream_a, representative.bytes);
		vxlnetwork::read (stream_a, account.bytes);
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

vxlnetwork::open_hashables::open_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto source_l (tree_a.get<std::string> ("source"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto account_l (tree_a.get<std::string> ("account"));
		error_a = source.decode_hex (source_l);
		if (!error_a)
		{
			error_a = representative.decode_account (representative_l);
			if (!error_a)
			{
				error_a = account.decode_account (account_l);
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void vxlnetwork::open_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, source.bytes.data (), sizeof (source.bytes));
	blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
	blake2b_update (&hash_a, account.bytes.data (), sizeof (account.bytes));
}

vxlnetwork::open_block::open_block (vxlnetwork::block_hash const & source_a, vxlnetwork::account const & representative_a, vxlnetwork::account const & account_a, vxlnetwork::raw_key const & prv_a, vxlnetwork::public_key const & pub_a, uint64_t work_a) :
	hashables (source_a, representative_a, account_a),
	signature (vxlnetwork::sign_message (prv_a, pub_a, hash ())),
	work (work_a)
{
	debug_assert (representative_a != nullptr);
	debug_assert (account_a != nullptr);
	debug_assert (pub_a != nullptr);
}

vxlnetwork::open_block::open_block (vxlnetwork::block_hash const & source_a, vxlnetwork::account const & representative_a, vxlnetwork::account const & account_a, std::nullptr_t) :
	hashables (source_a, representative_a, account_a),
	work (0)
{
	debug_assert (representative_a != nullptr);
	debug_assert (account_a != nullptr);

	signature.clear ();
}

vxlnetwork::open_block::open_block (bool & error_a, vxlnetwork::stream & stream_a) :
	hashables (error_a, stream_a)
{
	if (!error_a)
	{
		try
		{
			vxlnetwork::read (stream_a, signature);
			vxlnetwork::read (stream_a, work);
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

vxlnetwork::open_block::open_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
	hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto work_l (tree_a.get<std::string> ("work"));
			auto signature_l (tree_a.get<std::string> ("signature"));
			error_a = vxlnetwork::from_string_hex (work_l, work);
			if (!error_a)
			{
				error_a = signature.decode_hex (signature_l);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void vxlnetwork::open_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t vxlnetwork::open_block::block_work () const
{
	return work;
}

void vxlnetwork::open_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

vxlnetwork::block_hash const & vxlnetwork::open_block::previous () const
{
	static vxlnetwork::block_hash result{ 0 };
	return result;
}

vxlnetwork::account const & vxlnetwork::open_block::account () const
{
	return hashables.account;
}

void vxlnetwork::open_block::serialize (vxlnetwork::stream & stream_a) const
{
	write (stream_a, hashables.source);
	write (stream_a, hashables.representative);
	write (stream_a, hashables.account);
	write (stream_a, signature);
	write (stream_a, work);
}

bool vxlnetwork::open_block::deserialize (vxlnetwork::stream & stream_a)
{
	auto error (false);
	try
	{
		read (stream_a, hashables.source);
		read (stream_a, hashables.representative);
		read (stream_a, hashables.account);
		read (stream_a, signature);
		read (stream_a, work);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

void vxlnetwork::open_block::serialize_json (std::string & string_a, bool single_line) const
{
	boost::property_tree::ptree tree;
	serialize_json (tree);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree, !single_line);
	string_a = ostream.str ();
}

void vxlnetwork::open_block::serialize_json (boost::property_tree::ptree & tree) const
{
	tree.put ("type", "open");
	tree.put ("source", hashables.source.to_string ());
	tree.put ("representative", representative ().to_account ());
	tree.put ("account", hashables.account.to_account ());
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("work", vxlnetwork::to_string_hex (work));
	tree.put ("signature", signature_l);
}

bool vxlnetwork::open_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		debug_assert (tree_a.get<std::string> ("type") == "open");
		auto source_l (tree_a.get<std::string> ("source"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto account_l (tree_a.get<std::string> ("account"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.source.decode_hex (source_l);
		if (!error)
		{
			error = hashables.representative.decode_hex (representative_l);
			if (!error)
			{
				error = hashables.account.decode_hex (account_l);
				if (!error)
				{
					error = vxlnetwork::from_string_hex (work_l, work);
					if (!error)
					{
						error = signature.decode_hex (signature_l);
					}
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void vxlnetwork::open_block::visit (vxlnetwork::block_visitor & visitor_a) const
{
	visitor_a.open_block (*this);
}

void vxlnetwork::open_block::visit (vxlnetwork::mutable_block_visitor & visitor_a)
{
	visitor_a.open_block (*this);
}

vxlnetwork::block_type vxlnetwork::open_block::type () const
{
	return vxlnetwork::block_type::open;
}

bool vxlnetwork::open_block::operator== (vxlnetwork::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool vxlnetwork::open_block::operator== (vxlnetwork::open_block const & other_a) const
{
	return hashables.source == other_a.hashables.source && hashables.representative == other_a.hashables.representative && hashables.account == other_a.hashables.account && work == other_a.work && signature == other_a.signature;
}

bool vxlnetwork::open_block::valid_predecessor (vxlnetwork::block const & block_a) const
{
	return false;
}

vxlnetwork::block_hash const & vxlnetwork::open_block::source () const
{
	return hashables.source;
}

vxlnetwork::root const & vxlnetwork::open_block::root () const
{
	return hashables.account;
}

vxlnetwork::account const & vxlnetwork::open_block::representative () const
{
	return hashables.representative;
}

vxlnetwork::signature const & vxlnetwork::open_block::block_signature () const
{
	return signature;
}

void vxlnetwork::open_block::signature_set (vxlnetwork::signature const & signature_a)
{
	signature = signature_a;
}

vxlnetwork::change_hashables::change_hashables (vxlnetwork::block_hash const & previous_a, vxlnetwork::account const & representative_a) :
	previous (previous_a),
	representative (representative_a)
{
}

vxlnetwork::change_hashables::change_hashables (bool & error_a, vxlnetwork::stream & stream_a)
{
	try
	{
		vxlnetwork::read (stream_a, previous);
		vxlnetwork::read (stream_a, representative);
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

vxlnetwork::change_hashables::change_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		error_a = previous.decode_hex (previous_l);
		if (!error_a)
		{
			error_a = representative.decode_account (representative_l);
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void vxlnetwork::change_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
	blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
}

vxlnetwork::change_block::change_block (vxlnetwork::block_hash const & previous_a, vxlnetwork::account const & representative_a, vxlnetwork::raw_key const & prv_a, vxlnetwork::public_key const & pub_a, uint64_t work_a) :
	hashables (previous_a, representative_a),
	signature (vxlnetwork::sign_message (prv_a, pub_a, hash ())),
	work (work_a)
{
	debug_assert (representative_a != nullptr);
	debug_assert (pub_a != nullptr);
}

vxlnetwork::change_block::change_block (bool & error_a, vxlnetwork::stream & stream_a) :
	hashables (error_a, stream_a)
{
	if (!error_a)
	{
		try
		{
			vxlnetwork::read (stream_a, signature);
			vxlnetwork::read (stream_a, work);
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

vxlnetwork::change_block::change_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
	hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto work_l (tree_a.get<std::string> ("work"));
			auto signature_l (tree_a.get<std::string> ("signature"));
			error_a = vxlnetwork::from_string_hex (work_l, work);
			if (!error_a)
			{
				error_a = signature.decode_hex (signature_l);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void vxlnetwork::change_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t vxlnetwork::change_block::block_work () const
{
	return work;
}

void vxlnetwork::change_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

vxlnetwork::block_hash const & vxlnetwork::change_block::previous () const
{
	return hashables.previous;
}

void vxlnetwork::change_block::serialize (vxlnetwork::stream & stream_a) const
{
	write (stream_a, hashables.previous);
	write (stream_a, hashables.representative);
	write (stream_a, signature);
	write (stream_a, work);
}

bool vxlnetwork::change_block::deserialize (vxlnetwork::stream & stream_a)
{
	auto error (false);
	try
	{
		read (stream_a, hashables.previous);
		read (stream_a, hashables.representative);
		read (stream_a, signature);
		read (stream_a, work);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

void vxlnetwork::change_block::serialize_json (std::string & string_a, bool single_line) const
{
	boost::property_tree::ptree tree;
	serialize_json (tree);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree, !single_line);
	string_a = ostream.str ();
}

void vxlnetwork::change_block::serialize_json (boost::property_tree::ptree & tree) const
{
	tree.put ("type", "change");
	tree.put ("previous", hashables.previous.to_string ());
	tree.put ("representative", representative ().to_account ());
	tree.put ("work", vxlnetwork::to_string_hex (work));
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("signature", signature_l);
}

bool vxlnetwork::change_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		debug_assert (tree_a.get<std::string> ("type") == "change");
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.previous.decode_hex (previous_l);
		if (!error)
		{
			error = hashables.representative.decode_hex (representative_l);
			if (!error)
			{
				error = vxlnetwork::from_string_hex (work_l, work);
				if (!error)
				{
					error = signature.decode_hex (signature_l);
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void vxlnetwork::change_block::visit (vxlnetwork::block_visitor & visitor_a) const
{
	visitor_a.change_block (*this);
}

void vxlnetwork::change_block::visit (vxlnetwork::mutable_block_visitor & visitor_a)
{
	visitor_a.change_block (*this);
}

vxlnetwork::block_type vxlnetwork::change_block::type () const
{
	return vxlnetwork::block_type::change;
}

bool vxlnetwork::change_block::operator== (vxlnetwork::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool vxlnetwork::change_block::operator== (vxlnetwork::change_block const & other_a) const
{
	return hashables.previous == other_a.hashables.previous && hashables.representative == other_a.hashables.representative && work == other_a.work && signature == other_a.signature;
}

bool vxlnetwork::change_block::valid_predecessor (vxlnetwork::block const & block_a) const
{
	bool result;
	switch (block_a.type ())
	{
		case vxlnetwork::block_type::send:
		case vxlnetwork::block_type::receive:
		case vxlnetwork::block_type::open:
		case vxlnetwork::block_type::change:
			result = true;
			break;
		default:
			result = false;
			break;
	}
	return result;
}

vxlnetwork::root const & vxlnetwork::change_block::root () const
{
	return hashables.previous;
}

vxlnetwork::account const & vxlnetwork::change_block::representative () const
{
	return hashables.representative;
}

vxlnetwork::signature const & vxlnetwork::change_block::block_signature () const
{
	return signature;
}

void vxlnetwork::change_block::signature_set (vxlnetwork::signature const & signature_a)
{
	signature = signature_a;
}

vxlnetwork::state_hashables::state_hashables (vxlnetwork::account const & account_a, vxlnetwork::block_hash const & previous_a, vxlnetwork::account const & representative_a, vxlnetwork::amount const & balance_a, vxlnetwork::link const & link_a) :
	account (account_a),
	previous (previous_a),
	representative (representative_a),
	balance (balance_a),
	link (link_a)
{
}

vxlnetwork::state_hashables::state_hashables (bool & error_a, vxlnetwork::stream & stream_a)
{
	try
	{
		vxlnetwork::read (stream_a, account);
		vxlnetwork::read (stream_a, previous);
		vxlnetwork::read (stream_a, representative);
		vxlnetwork::read (stream_a, balance);
		vxlnetwork::read (stream_a, link);
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

vxlnetwork::state_hashables::state_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto account_l (tree_a.get<std::string> ("account"));
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto balance_l (tree_a.get<std::string> ("balance"));
		auto link_l (tree_a.get<std::string> ("link"));
		error_a = account.decode_account (account_l);
		if (!error_a)
		{
			error_a = previous.decode_hex (previous_l);
			if (!error_a)
			{
				error_a = representative.decode_account (representative_l);
				if (!error_a)
				{
					error_a = balance.decode_dec (balance_l);
					if (!error_a)
					{
						error_a = link.decode_account (link_l) && link.decode_hex (link_l);
					}
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void vxlnetwork::state_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, account.bytes.data (), sizeof (account.bytes));
	blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
	blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
	blake2b_update (&hash_a, balance.bytes.data (), sizeof (balance.bytes));
	blake2b_update (&hash_a, link.bytes.data (), sizeof (link.bytes));
}

vxlnetwork::state_block::state_block (vxlnetwork::account const & account_a, vxlnetwork::block_hash const & previous_a, vxlnetwork::account const & representative_a, vxlnetwork::amount const & balance_a, vxlnetwork::link const & link_a, vxlnetwork::raw_key const & prv_a, vxlnetwork::public_key const & pub_a, uint64_t work_a) :
	hashables (account_a, previous_a, representative_a, balance_a, link_a),
	signature (vxlnetwork::sign_message (prv_a, pub_a, hash ())),
	work (work_a)
{
	debug_assert (account_a != nullptr);
	debug_assert (representative_a != nullptr);
	debug_assert (link_a.as_account () != nullptr);
	debug_assert (pub_a != nullptr);
}

vxlnetwork::state_block::state_block (bool & error_a, vxlnetwork::stream & stream_a) :
	hashables (error_a, stream_a)
{
	if (!error_a)
	{
		try
		{
			vxlnetwork::read (stream_a, signature);
			vxlnetwork::read (stream_a, work);
			boost::endian::big_to_native_inplace (work);
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

vxlnetwork::state_block::state_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
	hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto type_l (tree_a.get<std::string> ("type"));
			auto signature_l (tree_a.get<std::string> ("signature"));
			auto work_l (tree_a.get<std::string> ("work"));
			error_a = type_l != "state";
			if (!error_a)
			{
				error_a = vxlnetwork::from_string_hex (work_l, work);
				if (!error_a)
				{
					error_a = signature.decode_hex (signature_l);
				}
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void vxlnetwork::state_block::hash (blake2b_state & hash_a) const
{
	vxlnetwork::uint256_union preamble (static_cast<uint64_t> (vxlnetwork::block_type::state));
	blake2b_update (&hash_a, preamble.bytes.data (), preamble.bytes.size ());
	hashables.hash (hash_a);
}

uint64_t vxlnetwork::state_block::block_work () const
{
	return work;
}

void vxlnetwork::state_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

vxlnetwork::block_hash const & vxlnetwork::state_block::previous () const
{
	return hashables.previous;
}

vxlnetwork::account const & vxlnetwork::state_block::account () const
{
	return hashables.account;
}

void vxlnetwork::state_block::serialize (vxlnetwork::stream & stream_a) const
{
	write (stream_a, hashables.account);
	write (stream_a, hashables.previous);
	write (stream_a, hashables.representative);
	write (stream_a, hashables.balance);
	write (stream_a, hashables.link);
	write (stream_a, signature);
	write (stream_a, boost::endian::native_to_big (work));
}

bool vxlnetwork::state_block::deserialize (vxlnetwork::stream & stream_a)
{
	auto error (false);
	try
	{
		read (stream_a, hashables.account);
		read (stream_a, hashables.previous);
		read (stream_a, hashables.representative);
		read (stream_a, hashables.balance);
		read (stream_a, hashables.link);
		read (stream_a, signature);
		read (stream_a, work);
		boost::endian::big_to_native_inplace (work);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

void vxlnetwork::state_block::serialize_json (std::string & string_a, bool single_line) const
{
	boost::property_tree::ptree tree;
	serialize_json (tree);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree, !single_line);
	string_a = ostream.str ();
}

void vxlnetwork::state_block::serialize_json (boost::property_tree::ptree & tree) const
{
	tree.put ("type", "state");
	tree.put ("account", hashables.account.to_account ());
	tree.put ("previous", hashables.previous.to_string ());
	tree.put ("representative", representative ().to_account ());
	tree.put ("balance", hashables.balance.to_string_dec ());
	tree.put ("link", hashables.link.to_string ());
	tree.put ("link_as_account", hashables.link.to_account ());
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("signature", signature_l);
	tree.put ("work", vxlnetwork::to_string_hex (work));
}

bool vxlnetwork::state_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		debug_assert (tree_a.get<std::string> ("type") == "state");
		auto account_l (tree_a.get<std::string> ("account"));
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto balance_l (tree_a.get<std::string> ("balance"));
		auto link_l (tree_a.get<std::string> ("link"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.account.decode_account (account_l);
		if (!error)
		{
			error = hashables.previous.decode_hex (previous_l);
			if (!error)
			{
				error = hashables.representative.decode_account (representative_l);
				if (!error)
				{
					error = hashables.balance.decode_dec (balance_l);
					if (!error)
					{
						error = hashables.link.decode_account (link_l) && hashables.link.decode_hex (link_l);
						if (!error)
						{
							error = vxlnetwork::from_string_hex (work_l, work);
							if (!error)
							{
								error = signature.decode_hex (signature_l);
							}
						}
					}
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void vxlnetwork::state_block::visit (vxlnetwork::block_visitor & visitor_a) const
{
	visitor_a.state_block (*this);
}

void vxlnetwork::state_block::visit (vxlnetwork::mutable_block_visitor & visitor_a)
{
	visitor_a.state_block (*this);
}

vxlnetwork::block_type vxlnetwork::state_block::type () const
{
	return vxlnetwork::block_type::state;
}

bool vxlnetwork::state_block::operator== (vxlnetwork::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool vxlnetwork::state_block::operator== (vxlnetwork::state_block const & other_a) const
{
	return hashables.account == other_a.hashables.account && hashables.previous == other_a.hashables.previous && hashables.representative == other_a.hashables.representative && hashables.balance == other_a.hashables.balance && hashables.link == other_a.hashables.link && signature == other_a.signature && work == other_a.work;
}

bool vxlnetwork::state_block::valid_predecessor (vxlnetwork::block const & block_a) const
{
	return true;
}

vxlnetwork::root const & vxlnetwork::state_block::root () const
{
	if (!hashables.previous.is_zero ())
	{
		return hashables.previous;
	}
	else
	{
		return hashables.account;
	}
}

vxlnetwork::link const & vxlnetwork::state_block::link () const
{
	return hashables.link;
}

vxlnetwork::account const & vxlnetwork::state_block::representative () const
{
	return hashables.representative;
}

vxlnetwork::amount const & vxlnetwork::state_block::balance () const
{
	return hashables.balance;
}

vxlnetwork::signature const & vxlnetwork::state_block::block_signature () const
{
	return signature;
}

void vxlnetwork::state_block::signature_set (vxlnetwork::signature const & signature_a)
{
	signature = signature_a;
}

std::shared_ptr<vxlnetwork::block> vxlnetwork::deserialize_block_json (boost::property_tree::ptree const & tree_a, vxlnetwork::block_uniquer * uniquer_a)
{
	std::shared_ptr<vxlnetwork::block> result;
	try
	{
		auto type (tree_a.get<std::string> ("type"));
		bool error (false);
		std::unique_ptr<vxlnetwork::block> obj;
		if (type == "receive")
		{
			obj = std::make_unique<vxlnetwork::receive_block> (error, tree_a);
		}
		else if (type == "send")
		{
			obj = std::make_unique<vxlnetwork::send_block> (error, tree_a);
		}
		else if (type == "open")
		{
			obj = std::make_unique<vxlnetwork::open_block> (error, tree_a);
		}
		else if (type == "change")
		{
			obj = std::make_unique<vxlnetwork::change_block> (error, tree_a);
		}
		else if (type == "state")
		{
			obj = std::make_unique<vxlnetwork::state_block> (error, tree_a);
		}

		if (!error)
		{
			result = std::move (obj);
		}
	}
	catch (std::runtime_error const &)
	{
	}
	if (uniquer_a != nullptr)
	{
		result = uniquer_a->unique (result);
	}
	return result;
}

std::shared_ptr<vxlnetwork::block> vxlnetwork::deserialize_block (vxlnetwork::stream & stream_a)
{
	vxlnetwork::block_type type;
	auto error (try_read (stream_a, type));
	std::shared_ptr<vxlnetwork::block> result;
	if (!error)
	{
		result = vxlnetwork::deserialize_block (stream_a, type);
	}
	return result;
}

std::shared_ptr<vxlnetwork::block> vxlnetwork::deserialize_block (vxlnetwork::stream & stream_a, vxlnetwork::block_type type_a, vxlnetwork::block_uniquer * uniquer_a)
{
	std::shared_ptr<vxlnetwork::block> result;
	switch (type_a)
	{
		case vxlnetwork::block_type::receive:
		{
			result = ::deserialize_block<vxlnetwork::receive_block> (stream_a);
			break;
		}
		case vxlnetwork::block_type::send:
		{
			result = ::deserialize_block<vxlnetwork::send_block> (stream_a);
			break;
		}
		case vxlnetwork::block_type::open:
		{
			result = ::deserialize_block<vxlnetwork::open_block> (stream_a);
			break;
		}
		case vxlnetwork::block_type::change:
		{
			result = ::deserialize_block<vxlnetwork::change_block> (stream_a);
			break;
		}
		case vxlnetwork::block_type::state:
		{
			result = ::deserialize_block<vxlnetwork::state_block> (stream_a);
			break;
		}
		default:
#ifndef VXLNETWORK_FUZZER_TEST
			debug_assert (false);
#endif
			break;
	}
	if (uniquer_a != nullptr)
	{
		result = uniquer_a->unique (result);
	}
	return result;
}

void vxlnetwork::receive_block::visit (vxlnetwork::block_visitor & visitor_a) const
{
	visitor_a.receive_block (*this);
}

void vxlnetwork::receive_block::visit (vxlnetwork::mutable_block_visitor & visitor_a)
{
	visitor_a.receive_block (*this);
}

bool vxlnetwork::receive_block::operator== (vxlnetwork::receive_block const & other_a) const
{
	auto result (hashables.previous == other_a.hashables.previous && hashables.source == other_a.hashables.source && work == other_a.work && signature == other_a.signature);
	return result;
}

void vxlnetwork::receive_block::serialize (vxlnetwork::stream & stream_a) const
{
	write (stream_a, hashables.previous.bytes);
	write (stream_a, hashables.source.bytes);
	write (stream_a, signature.bytes);
	write (stream_a, work);
}

bool vxlnetwork::receive_block::deserialize (vxlnetwork::stream & stream_a)
{
	auto error (false);
	try
	{
		read (stream_a, hashables.previous.bytes);
		read (stream_a, hashables.source.bytes);
		read (stream_a, signature.bytes);
		read (stream_a, work);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

void vxlnetwork::receive_block::serialize_json (std::string & string_a, bool single_line) const
{
	boost::property_tree::ptree tree;
	serialize_json (tree);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree, !single_line);
	string_a = ostream.str ();
}

void vxlnetwork::receive_block::serialize_json (boost::property_tree::ptree & tree) const
{
	tree.put ("type", "receive");
	std::string previous;
	hashables.previous.encode_hex (previous);
	tree.put ("previous", previous);
	std::string source;
	hashables.source.encode_hex (source);
	tree.put ("source", source);
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("work", vxlnetwork::to_string_hex (work));
	tree.put ("signature", signature_l);
}

bool vxlnetwork::receive_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		debug_assert (tree_a.get<std::string> ("type") == "receive");
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto source_l (tree_a.get<std::string> ("source"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.previous.decode_hex (previous_l);
		if (!error)
		{
			error = hashables.source.decode_hex (source_l);
			if (!error)
			{
				error = vxlnetwork::from_string_hex (work_l, work);
				if (!error)
				{
					error = signature.decode_hex (signature_l);
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

vxlnetwork::receive_block::receive_block (vxlnetwork::block_hash const & previous_a, vxlnetwork::block_hash const & source_a, vxlnetwork::raw_key const & prv_a, vxlnetwork::public_key const & pub_a, uint64_t work_a) :
	hashables (previous_a, source_a),
	signature (vxlnetwork::sign_message (prv_a, pub_a, hash ())),
	work (work_a)
{
	debug_assert (pub_a != nullptr);
}

vxlnetwork::receive_block::receive_block (bool & error_a, vxlnetwork::stream & stream_a) :
	hashables (error_a, stream_a)
{
	if (!error_a)
	{
		try
		{
			vxlnetwork::read (stream_a, signature);
			vxlnetwork::read (stream_a, work);
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

vxlnetwork::receive_block::receive_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
	hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto signature_l (tree_a.get<std::string> ("signature"));
			auto work_l (tree_a.get<std::string> ("work"));
			error_a = signature.decode_hex (signature_l);
			if (!error_a)
			{
				error_a = vxlnetwork::from_string_hex (work_l, work);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void vxlnetwork::receive_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t vxlnetwork::receive_block::block_work () const
{
	return work;
}

void vxlnetwork::receive_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

bool vxlnetwork::receive_block::operator== (vxlnetwork::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool vxlnetwork::receive_block::valid_predecessor (vxlnetwork::block const & block_a) const
{
	bool result;
	switch (block_a.type ())
	{
		case vxlnetwork::block_type::send:
		case vxlnetwork::block_type::receive:
		case vxlnetwork::block_type::open:
		case vxlnetwork::block_type::change:
			result = true;
			break;
		default:
			result = false;
			break;
	}
	return result;
}

vxlnetwork::block_hash const & vxlnetwork::receive_block::previous () const
{
	return hashables.previous;
}

vxlnetwork::block_hash const & vxlnetwork::receive_block::source () const
{
	return hashables.source;
}

vxlnetwork::root const & vxlnetwork::receive_block::root () const
{
	return hashables.previous;
}

vxlnetwork::signature const & vxlnetwork::receive_block::block_signature () const
{
	return signature;
}

void vxlnetwork::receive_block::signature_set (vxlnetwork::signature const & signature_a)
{
	signature = signature_a;
}

vxlnetwork::block_type vxlnetwork::receive_block::type () const
{
	return vxlnetwork::block_type::receive;
}

vxlnetwork::receive_hashables::receive_hashables (vxlnetwork::block_hash const & previous_a, vxlnetwork::block_hash const & source_a) :
	previous (previous_a),
	source (source_a)
{
}

vxlnetwork::receive_hashables::receive_hashables (bool & error_a, vxlnetwork::stream & stream_a)
{
	try
	{
		vxlnetwork::read (stream_a, previous.bytes);
		vxlnetwork::read (stream_a, source.bytes);
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

vxlnetwork::receive_hashables::receive_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto source_l (tree_a.get<std::string> ("source"));
		error_a = previous.decode_hex (previous_l);
		if (!error_a)
		{
			error_a = source.decode_hex (source_l);
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void vxlnetwork::receive_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
	blake2b_update (&hash_a, source.bytes.data (), sizeof (source.bytes));
}

vxlnetwork::block_details::block_details (vxlnetwork::epoch const epoch_a, bool const is_send_a, bool const is_receive_a, bool const is_epoch_a) :
	epoch (epoch_a), is_send (is_send_a), is_receive (is_receive_a), is_epoch (is_epoch_a)
{
}

bool vxlnetwork::block_details::operator== (vxlnetwork::block_details const & other_a) const
{
	return epoch == other_a.epoch && is_send == other_a.is_send && is_receive == other_a.is_receive && is_epoch == other_a.is_epoch;
}

uint8_t vxlnetwork::block_details::packed () const
{
	std::bitset<8> result (static_cast<uint8_t> (epoch));
	result.set (7, is_send);
	result.set (6, is_receive);
	result.set (5, is_epoch);
	return static_cast<uint8_t> (result.to_ulong ());
}

void vxlnetwork::block_details::unpack (uint8_t details_a)
{
	constexpr std::bitset<8> epoch_mask{ 0b00011111 };
	auto as_bitset = static_cast<std::bitset<8>> (details_a);
	is_send = as_bitset.test (7);
	is_receive = as_bitset.test (6);
	is_epoch = as_bitset.test (5);
	epoch = static_cast<vxlnetwork::epoch> ((as_bitset & epoch_mask).to_ulong ());
}

void vxlnetwork::block_details::serialize (vxlnetwork::stream & stream_a) const
{
	vxlnetwork::write (stream_a, packed ());
}

bool vxlnetwork::block_details::deserialize (vxlnetwork::stream & stream_a)
{
	bool result (false);
	try
	{
		uint8_t packed{ 0 };
		vxlnetwork::read (stream_a, packed);
		unpack (packed);
	}
	catch (std::runtime_error &)
	{
		result = true;
	}

	return result;
}

std::string vxlnetwork::state_subtype (vxlnetwork::block_details const details_a)
{
	debug_assert (details_a.is_epoch + details_a.is_receive + details_a.is_send <= 1);
	if (details_a.is_send)
	{
		return "send";
	}
	else if (details_a.is_receive)
	{
		return "receive";
	}
	else if (details_a.is_epoch)
	{
		return "epoch";
	}
	else
	{
		return "change";
	}
}

vxlnetwork::block_sideband::block_sideband (vxlnetwork::account const & account_a, vxlnetwork::block_hash const & successor_a, vxlnetwork::amount const & balance_a, uint64_t const height_a, uint64_t const timestamp_a, vxlnetwork::block_details const & details_a, vxlnetwork::epoch const source_epoch_a) :
	successor (successor_a),
	account (account_a),
	balance (balance_a),
	height (height_a),
	timestamp (timestamp_a),
	details (details_a),
	source_epoch (source_epoch_a)
{
}

vxlnetwork::block_sideband::block_sideband (vxlnetwork::account const & account_a, vxlnetwork::block_hash const & successor_a, vxlnetwork::amount const & balance_a, uint64_t const height_a, uint64_t const timestamp_a, vxlnetwork::epoch const epoch_a, bool const is_send, bool const is_receive, bool const is_epoch, vxlnetwork::epoch const source_epoch_a) :
	successor (successor_a),
	account (account_a),
	balance (balance_a),
	height (height_a),
	timestamp (timestamp_a),
	details (epoch_a, is_send, is_receive, is_epoch),
	source_epoch (source_epoch_a)
{
}

size_t vxlnetwork::block_sideband::size (vxlnetwork::block_type type_a)
{
	size_t result (0);
	result += sizeof (successor);
	if (type_a != vxlnetwork::block_type::state && type_a != vxlnetwork::block_type::open)
	{
		result += sizeof (account);
	}
	if (type_a != vxlnetwork::block_type::open)
	{
		result += sizeof (height);
	}
	if (type_a == vxlnetwork::block_type::receive || type_a == vxlnetwork::block_type::change || type_a == vxlnetwork::block_type::open)
	{
		result += sizeof (balance);
	}
	result += sizeof (timestamp);
	if (type_a == vxlnetwork::block_type::state)
	{
		static_assert (sizeof (vxlnetwork::epoch) == vxlnetwork::block_details::size (), "block_details is larger than the epoch enum");
		result += vxlnetwork::block_details::size () + sizeof (vxlnetwork::epoch);
	}
	return result;
}

void vxlnetwork::block_sideband::serialize (vxlnetwork::stream & stream_a, vxlnetwork::block_type type_a) const
{
	vxlnetwork::write (stream_a, successor.bytes);
	if (type_a != vxlnetwork::block_type::state && type_a != vxlnetwork::block_type::open)
	{
		vxlnetwork::write (stream_a, account.bytes);
	}
	if (type_a != vxlnetwork::block_type::open)
	{
		vxlnetwork::write (stream_a, boost::endian::native_to_big (height));
	}
	if (type_a == vxlnetwork::block_type::receive || type_a == vxlnetwork::block_type::change || type_a == vxlnetwork::block_type::open)
	{
		vxlnetwork::write (stream_a, balance.bytes);
	}
	vxlnetwork::write (stream_a, boost::endian::native_to_big (timestamp));
	if (type_a == vxlnetwork::block_type::state)
	{
		details.serialize (stream_a);
		vxlnetwork::write (stream_a, static_cast<uint8_t> (source_epoch));
	}
}

bool vxlnetwork::block_sideband::deserialize (vxlnetwork::stream & stream_a, vxlnetwork::block_type type_a)
{
	bool result (false);
	try
	{
		vxlnetwork::read (stream_a, successor.bytes);
		if (type_a != vxlnetwork::block_type::state && type_a != vxlnetwork::block_type::open)
		{
			vxlnetwork::read (stream_a, account.bytes);
		}
		if (type_a != vxlnetwork::block_type::open)
		{
			vxlnetwork::read (stream_a, height);
			boost::endian::big_to_native_inplace (height);
		}
		else
		{
			height = 1;
		}
		if (type_a == vxlnetwork::block_type::receive || type_a == vxlnetwork::block_type::change || type_a == vxlnetwork::block_type::open)
		{
			vxlnetwork::read (stream_a, balance.bytes);
		}
		vxlnetwork::read (stream_a, timestamp);
		boost::endian::big_to_native_inplace (timestamp);
		if (type_a == vxlnetwork::block_type::state)
		{
			result = details.deserialize (stream_a);
			uint8_t source_epoch_uint8_t{ 0 };
			vxlnetwork::read (stream_a, source_epoch_uint8_t);
			source_epoch = static_cast<vxlnetwork::epoch> (source_epoch_uint8_t);
		}
	}
	catch (std::runtime_error &)
	{
		result = true;
	}

	return result;
}

std::shared_ptr<vxlnetwork::block> vxlnetwork::block_uniquer::unique (std::shared_ptr<vxlnetwork::block> const & block_a)
{
	auto result (block_a);
	if (result != nullptr)
	{
		vxlnetwork::uint256_union key (block_a->full_hash ());
		vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
		auto & existing (blocks[key]);
		if (auto block_l = existing.lock ())
		{
			result = block_l;
		}
		else
		{
			existing = block_a;
		}
		release_assert (std::numeric_limits<CryptoPP::word32>::max () > blocks.size ());
		for (auto i (0); i < cleanup_count && !blocks.empty (); ++i)
		{
			auto random_offset (vxlnetwork::random_pool::generate_word32 (0, static_cast<CryptoPP::word32> (blocks.size () - 1)));
			auto existing (std::next (blocks.begin (), random_offset));
			if (existing == blocks.end ())
			{
				existing = blocks.begin ();
			}
			if (existing != blocks.end ())
			{
				if (auto block_l = existing->second.lock ())
				{
					// Still live
				}
				else
				{
					blocks.erase (existing);
				}
			}
		}
	}
	return result;
}

size_t vxlnetwork::block_uniquer::size ()
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
	return blocks.size ();
}

std::unique_ptr<vxlnetwork::container_info_component> vxlnetwork::collect_container_info (block_uniquer & block_uniquer, std::string const & name)
{
	auto count = block_uniquer.size ();
	auto sizeof_element = sizeof (block_uniquer::value_type);
	auto composite = std::make_unique<container_info_composite> (name);
	composite->add_component (std::make_unique<container_info_leaf> (container_info{ "blocks", count, sizeof_element }));
	return composite;
}
