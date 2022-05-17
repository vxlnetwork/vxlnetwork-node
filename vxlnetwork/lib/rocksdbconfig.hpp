#pragma once

#include <vxlnetwork/lib/errors.hpp>

#include <thread>

namespace vxlnetwork
{
class tomlconfig;

/** Configuration options for RocksDB */
class rocksdb_config final
{
public:
	rocksdb_config () :
		enable{ using_rocksdb_in_tests () }
	{
	}
	vxlnetwork::error serialize_toml (vxlnetwork::tomlconfig & toml_a) const;
	vxlnetwork::error deserialize_toml (vxlnetwork::tomlconfig & toml_a);

	/** To use RocksDB in tests make sure the environment variable TEST_USE_ROCKSDB=1 is set */
	static bool using_rocksdb_in_tests ();

	bool enable{ false };
	uint8_t memory_multiplier{ 2 };
	unsigned io_threads{ std::thread::hardware_concurrency () };
};
}
