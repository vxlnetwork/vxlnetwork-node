#include <vxlnetwork/crypto_lib/random_pool.hpp>
#include <vxlnetwork/lib/locks.hpp>
#include <vxlnetwork/secure/buffer.hpp>
#include <vxlnetwork/secure/common.hpp>
#include <vxlnetwork/secure/network_filter.hpp>

vxlnetwork::network_filter::network_filter (size_t size_a) :
	items (size_a, vxlnetwork::uint128_t{ 0 })
{
	vxlnetwork::random_pool::generate_block (key, key.size ());
}

bool vxlnetwork::network_filter::apply (uint8_t const * bytes_a, size_t count_a, vxlnetwork::uint128_t * digest_a)
{
	// Get hash before locking
	auto digest (hash (bytes_a, count_a));

	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
	auto & element (get_element (digest));
	bool existed (element == digest);
	if (!existed)
	{
		// Replace likely old element with a new one
		element = digest;
	}
	if (digest_a)
	{
		*digest_a = digest;
	}
	return existed;
}

void vxlnetwork::network_filter::clear (vxlnetwork::uint128_t const & digest_a)
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
	auto & element (get_element (digest_a));
	if (element == digest_a)
	{
		element = vxlnetwork::uint128_t{ 0 };
	}
}

void vxlnetwork::network_filter::clear (std::vector<vxlnetwork::uint128_t> const & digests_a)
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
	for (auto const & digest : digests_a)
	{
		auto & element (get_element (digest));
		if (element == digest)
		{
			element = vxlnetwork::uint128_t{ 0 };
		}
	}
}

void vxlnetwork::network_filter::clear (uint8_t const * bytes_a, size_t count_a)
{
	clear (hash (bytes_a, count_a));
}

template <typename OBJECT>
void vxlnetwork::network_filter::clear (OBJECT const & object_a)
{
	clear (hash (object_a));
}

void vxlnetwork::network_filter::clear ()
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> lock (mutex);
	items.assign (items.size (), vxlnetwork::uint128_t{ 0 });
}

template <typename OBJECT>
vxlnetwork::uint128_t vxlnetwork::network_filter::hash (OBJECT const & object_a) const
{
	std::vector<uint8_t> bytes;
	{
		vxlnetwork::vectorstream stream (bytes);
		object_a->serialize (stream);
	}
	return hash (bytes.data (), bytes.size ());
}

vxlnetwork::uint128_t & vxlnetwork::network_filter::get_element (vxlnetwork::uint128_t const & hash_a)
{
	debug_assert (!mutex.try_lock ());
	debug_assert (items.size () > 0);
	size_t index (hash_a % items.size ());
	return items[index];
}

vxlnetwork::uint128_t vxlnetwork::network_filter::hash (uint8_t const * bytes_a, size_t count_a) const
{
	vxlnetwork::uint128_union digest{ 0 };
	siphash_t siphash (key, static_cast<unsigned int> (key.size ()));
	siphash.CalculateDigest (digest.bytes.data (), bytes_a, count_a);
	return digest.number ();
}

// Explicitly instantiate
template vxlnetwork::uint128_t vxlnetwork::network_filter::hash (std::shared_ptr<vxlnetwork::block> const &) const;
template void vxlnetwork::network_filter::clear (std::shared_ptr<vxlnetwork::block> const &);
