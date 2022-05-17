#pragma once

#include <vxlnetwork/lib/locks.hpp>
#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/secure/store.hpp>

#include <atomic>
#include <thread>
#include <unordered_map>

namespace vxlnetwork
{
class store;
class transaction;
class unchecked_info;
class unchecked_key;
class write_transaction;
class unchecked_map
{
public:
	using iterator = vxlnetwork::unchecked_store::iterator;

public:
	unchecked_map (vxlnetwork::store & store, bool const & do_delete);
	~unchecked_map ();
	void put (vxlnetwork::hash_or_account const & dependency, vxlnetwork::unchecked_info const & info);
	std::pair<iterator, iterator> equal_range (vxlnetwork::transaction const & transaction, vxlnetwork::block_hash const & dependency);
	std::pair<iterator, iterator> full_range (vxlnetwork::transaction const & transaction);
	std::vector<vxlnetwork::unchecked_info> get (vxlnetwork::transaction const &, vxlnetwork::block_hash const &);
	bool exists (vxlnetwork::transaction const & transaction, vxlnetwork::unchecked_key const & key) const;
	void del (vxlnetwork::write_transaction const & transaction, vxlnetwork::unchecked_key const & key);
	void clear (vxlnetwork::write_transaction const & transaction);
	size_t count (vxlnetwork::transaction const & transaction) const;
	void stop ();
	void flush ();

public: // Trigger requested dependencies
	void trigger (vxlnetwork::hash_or_account const & dependency);
	std::function<void (vxlnetwork::unchecked_info const &)> satisfied{ [] (vxlnetwork::unchecked_info const &) {} };

private:
	using insert = std::pair<vxlnetwork::hash_or_account, vxlnetwork::unchecked_info>;
	using query = vxlnetwork::hash_or_account;
	class item_visitor : boost::static_visitor<>
	{
	public:
		item_visitor (unchecked_map & unchecked, vxlnetwork::write_transaction const & transaction);
		void operator() (insert const & item);
		void operator() (query const & item);
		unchecked_map & unchecked;
		vxlnetwork::write_transaction const & transaction;
	};
	void run ();
	vxlnetwork::store & store;
	bool const & disable_delete;
	std::deque<boost::variant<insert, query>> buffer;
	std::deque<boost::variant<insert, query>> back_buffer;
	bool writing_back_buffer{ false };
	bool stopped{ false };
	vxlnetwork::condition_variable condition;
	vxlnetwork::mutex mutex;
	std::thread thread;
	void write_buffer (decltype (buffer) const & back_buffer);
};
}
