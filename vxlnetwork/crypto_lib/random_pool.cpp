#include <vxlnetwork/crypto_lib/random_pool.hpp>

#include <crypto/cryptopp/osrng.h>

void vxlnetwork::random_pool::generate_block (unsigned char * output, size_t size)
{
	auto & pool = get_pool ();
	pool.GenerateBlock (output, size);
}

unsigned vxlnetwork::random_pool::generate_word32 (unsigned min, unsigned max)
{
	auto & pool = get_pool ();
	return pool.GenerateWord32 (min, max);
}

unsigned char vxlnetwork::random_pool::generate_byte ()
{
	auto & pool = get_pool ();
	return pool.GenerateByte ();
}

CryptoPP::AutoSeededRandomPool & vxlnetwork::random_pool::get_pool ()
{
	static thread_local CryptoPP::AutoSeededRandomPool pool;
	return pool;
}
