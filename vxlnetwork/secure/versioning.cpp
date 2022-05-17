#include <vxlnetwork/secure/versioning.hpp>

#include <boost/endian/conversion.hpp>

#include <lmdb/libraries/liblmdb/lmdb.h>

vxlnetwork::pending_info_v14::pending_info_v14 (vxlnetwork::account const & source_a, vxlnetwork::amount const & amount_a, vxlnetwork::epoch epoch_a) :
	source (source_a),
	amount (amount_a),
	epoch (epoch_a)
{
}

bool vxlnetwork::pending_info_v14::deserialize (vxlnetwork::stream & stream_a)
{
	auto error (false);
	try
	{
		vxlnetwork::read (stream_a, source.bytes);
		vxlnetwork::read (stream_a, amount.bytes);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

size_t vxlnetwork::pending_info_v14::db_size () const
{
	return sizeof (source) + sizeof (amount);
}

bool vxlnetwork::pending_info_v14::operator== (vxlnetwork::pending_info_v14 const & other_a) const
{
	return source == other_a.source && amount == other_a.amount && epoch == other_a.epoch;
}

vxlnetwork::account_info_v14::account_info_v14 (vxlnetwork::block_hash const & head_a, vxlnetwork::block_hash const & rep_block_a, vxlnetwork::block_hash const & open_block_a, vxlnetwork::amount const & balance_a, uint64_t modified_a, uint64_t block_count_a, uint64_t confirmation_height_a, vxlnetwork::epoch epoch_a) :
	head (head_a),
	rep_block (rep_block_a),
	open_block (open_block_a),
	balance (balance_a),
	modified (modified_a),
	block_count (block_count_a),
	confirmation_height (confirmation_height_a),
	epoch (epoch_a)
{
}

size_t vxlnetwork::account_info_v14::db_size () const
{
	debug_assert (reinterpret_cast<uint8_t const *> (this) == reinterpret_cast<uint8_t const *> (&head));
	debug_assert (reinterpret_cast<uint8_t const *> (&head) + sizeof (head) == reinterpret_cast<uint8_t const *> (&rep_block));
	debug_assert (reinterpret_cast<uint8_t const *> (&rep_block) + sizeof (rep_block) == reinterpret_cast<uint8_t const *> (&open_block));
	debug_assert (reinterpret_cast<uint8_t const *> (&open_block) + sizeof (open_block) == reinterpret_cast<uint8_t const *> (&balance));
	debug_assert (reinterpret_cast<uint8_t const *> (&balance) + sizeof (balance) == reinterpret_cast<uint8_t const *> (&modified));
	debug_assert (reinterpret_cast<uint8_t const *> (&modified) + sizeof (modified) == reinterpret_cast<uint8_t const *> (&block_count));
	debug_assert (reinterpret_cast<uint8_t const *> (&block_count) + sizeof (block_count) == reinterpret_cast<uint8_t const *> (&confirmation_height));
	return sizeof (head) + sizeof (rep_block) + sizeof (open_block) + sizeof (balance) + sizeof (modified) + sizeof (block_count) + sizeof (confirmation_height);
}

vxlnetwork::block_sideband_v14::block_sideband_v14 (vxlnetwork::block_type type_a, vxlnetwork::account const & account_a, vxlnetwork::block_hash const & successor_a, vxlnetwork::amount const & balance_a, uint64_t height_a, uint64_t timestamp_a) :
	type (type_a),
	successor (successor_a),
	account (account_a),
	balance (balance_a),
	height (height_a),
	timestamp (timestamp_a)
{
}

size_t vxlnetwork::block_sideband_v14::size (vxlnetwork::block_type type_a)
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
	return result;
}

void vxlnetwork::block_sideband_v14::serialize (vxlnetwork::stream & stream_a) const
{
	vxlnetwork::write (stream_a, successor.bytes);
	if (type != vxlnetwork::block_type::state && type != vxlnetwork::block_type::open)
	{
		vxlnetwork::write (stream_a, account.bytes);
	}
	if (type != vxlnetwork::block_type::open)
	{
		vxlnetwork::write (stream_a, boost::endian::native_to_big (height));
	}
	if (type == vxlnetwork::block_type::receive || type == vxlnetwork::block_type::change || type == vxlnetwork::block_type::open)
	{
		vxlnetwork::write (stream_a, balance.bytes);
	}
	vxlnetwork::write (stream_a, boost::endian::native_to_big (timestamp));
}

bool vxlnetwork::block_sideband_v14::deserialize (vxlnetwork::stream & stream_a)
{
	bool result (false);
	try
	{
		vxlnetwork::read (stream_a, successor.bytes);
		if (type != vxlnetwork::block_type::state && type != vxlnetwork::block_type::open)
		{
			vxlnetwork::read (stream_a, account.bytes);
		}
		if (type != vxlnetwork::block_type::open)
		{
			vxlnetwork::read (stream_a, height);
			boost::endian::big_to_native_inplace (height);
		}
		else
		{
			height = 1;
		}
		if (type == vxlnetwork::block_type::receive || type == vxlnetwork::block_type::change || type == vxlnetwork::block_type::open)
		{
			vxlnetwork::read (stream_a, balance.bytes);
		}
		vxlnetwork::read (stream_a, timestamp);
		boost::endian::big_to_native_inplace (timestamp);
	}
	catch (std::runtime_error &)
	{
		result = true;
	}

	return result;
}

vxlnetwork::block_sideband_v18::block_sideband_v18 (vxlnetwork::account const & account_a, vxlnetwork::block_hash const & successor_a, vxlnetwork::amount const & balance_a, uint64_t height_a, uint64_t timestamp_a, vxlnetwork::block_details const & details_a) :
	successor (successor_a),
	account (account_a),
	balance (balance_a),
	height (height_a),
	timestamp (timestamp_a),
	details (details_a)
{
}

vxlnetwork::block_sideband_v18::block_sideband_v18 (vxlnetwork::account const & account_a, vxlnetwork::block_hash const & successor_a, vxlnetwork::amount const & balance_a, uint64_t height_a, uint64_t timestamp_a, vxlnetwork::epoch epoch_a, bool is_send, bool is_receive, bool is_epoch) :
	successor (successor_a),
	account (account_a),
	balance (balance_a),
	height (height_a),
	timestamp (timestamp_a),
	details (epoch_a, is_send, is_receive, is_epoch)
{
}

size_t vxlnetwork::block_sideband_v18::size (vxlnetwork::block_type type_a)
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
		static_assert (sizeof (vxlnetwork::epoch) == vxlnetwork::block_details::size (), "block_details_v18 is larger than the epoch enum");
		result += vxlnetwork::block_details::size ();
	}
	return result;
}

void vxlnetwork::block_sideband_v18::serialize (vxlnetwork::stream & stream_a, vxlnetwork::block_type type_a) const
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
	}
}

bool vxlnetwork::block_sideband_v18::deserialize (vxlnetwork::stream & stream_a, vxlnetwork::block_type type_a)
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
		}
	}
	catch (std::runtime_error &)
	{
		result = true;
	}

	return result;
}
