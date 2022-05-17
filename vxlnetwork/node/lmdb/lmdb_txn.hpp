#pragma once

#include <vxlnetwork/lib/diagnosticsconfig.hpp>
#include <vxlnetwork/lib/timer.hpp>
#include <vxlnetwork/secure/store.hpp>

#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/stacktrace/stacktrace_fwd.hpp>

#include <mutex>

#include <lmdb/libraries/liblmdb/lmdb.h>

namespace vxlnetwork
{
class transaction_impl;
class logger_mt;
class mdb_env;

class mdb_txn_callbacks
{
public:
	std::function<void (vxlnetwork::transaction_impl const *)> txn_start{ [] (vxlnetwork::transaction_impl const *) {} };
	std::function<void (vxlnetwork::transaction_impl const *)> txn_end{ [] (vxlnetwork::transaction_impl const *) {} };
};

class read_mdb_txn final : public read_transaction_impl
{
public:
	read_mdb_txn (vxlnetwork::mdb_env const &, mdb_txn_callbacks mdb_txn_callbacks);
	~read_mdb_txn ();
	void reset () override;
	void renew () override;
	void * get_handle () const override;
	MDB_txn * handle;
	mdb_txn_callbacks txn_callbacks;
};

class write_mdb_txn final : public write_transaction_impl
{
public:
	write_mdb_txn (vxlnetwork::mdb_env const &, mdb_txn_callbacks mdb_txn_callbacks);
	~write_mdb_txn ();
	void commit () override;
	void renew () override;
	void * get_handle () const override;
	bool contains (vxlnetwork::tables table_a) const override;
	MDB_txn * handle;
	vxlnetwork::mdb_env const & env;
	mdb_txn_callbacks txn_callbacks;
	bool active{ true };
};

class mdb_txn_stats
{
public:
	mdb_txn_stats (vxlnetwork::transaction_impl const * transaction_impl_a);
	bool is_write () const;
	vxlnetwork::timer<std::chrono::milliseconds> timer;
	vxlnetwork::transaction_impl const * transaction_impl;
	std::string thread_name;

	// Smart pointer so that we don't need the full definition which causes min/max issues on Windows
	std::shared_ptr<boost::stacktrace::stacktrace> stacktrace;
};

class mdb_txn_tracker
{
public:
	mdb_txn_tracker (vxlnetwork::logger_mt & logger_a, vxlnetwork::txn_tracking_config const & txn_tracking_config_a, std::chrono::milliseconds block_processor_batch_max_time_a);
	void serialize_json (boost::property_tree::ptree & json, std::chrono::milliseconds min_read_time, std::chrono::milliseconds min_write_time);
	void add (vxlnetwork::transaction_impl const * transaction_impl);
	void erase (vxlnetwork::transaction_impl const * transaction_impl);

private:
	vxlnetwork::mutex mutex;
	std::vector<mdb_txn_stats> stats;
	vxlnetwork::logger_mt & logger;
	vxlnetwork::txn_tracking_config txn_tracking_config;
	std::chrono::milliseconds block_processor_batch_max_time;

	void log_if_held_long_enough (vxlnetwork::mdb_txn_stats const & mdb_txn_stats) const;
};
}
