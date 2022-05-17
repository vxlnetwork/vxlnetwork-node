#pragma once

#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/secure/store.hpp>

#include <lmdb/libraries/liblmdb/lmdb.h>

namespace vxlnetwork
{
class wallet_value
{
public:
	wallet_value () = default;
	wallet_value (vxlnetwork::db_val<MDB_val> const &);
	wallet_value (vxlnetwork::raw_key const &, uint64_t);
	vxlnetwork::db_val<MDB_val> val () const;
	vxlnetwork::raw_key key;
	uint64_t work;
};
}
