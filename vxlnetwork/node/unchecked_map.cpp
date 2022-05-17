#include <vxlnetwork/lib/locks.hpp>
#include <vxlnetwork/lib/threading.hpp>
#include <vxlnetwork/lib/timer.hpp>
#include <vxlnetwork/node/unchecked_map.hpp>
#include <vxlnetwork/secure/store.hpp>

#include <boost/range/join.hpp>

vxlnetwork::unchecked_map::unchecked_map (vxlnetwork::store & store, bool const & disable_delete) :
	store{ store },
	disable_delete{ disable_delete },
	thread{ [this] () { run (); } }
{
}

vxlnetwork::unchecked_map::~unchecked_map ()
{
	stop ();
	thread.join ();
}

void vxlnetwork::unchecked_map::put (vxlnetwork::hash_or_account const & dependency, vxlnetwork::unchecked_info const & info)
{
	vxlnetwork::unique_lock<vxlnetwork::mutex> lock{ mutex };
	buffer.push_back (std::make_pair (dependency, info));
	lock.unlock ();
	condition.notify_all (); // Notify run ()
}

auto vxlnetwork::unchecked_map::equal_range (vxlnetwork::transaction const & transaction, vxlnetwork::block_hash const & dependency) -> std::pair<iterator, iterator>
{
	return store.unchecked.equal_range (transaction, dependency);
}

auto vxlnetwork::unchecked_map::full_range (vxlnetwork::transaction const & transaction) -> std::pair<iterator, iterator>
{
	return store.unchecked.full_range (transaction);
}

std::vector<vxlnetwork::unchecked_info> vxlnetwork::unchecked_map::get (vxlnetwork::transaction const & transaction, vxlnetwork::block_hash const & hash)
{
	return store.unchecked.get (transaction, hash);
}

bool vxlnetwork::unchecked_map::exists (vxlnetwork::transaction const & transaction, vxlnetwork::unchecked_key const & key) const
{
	return store.unchecked.exists (transaction, key);
}

void vxlnetwork::unchecked_map::del (vxlnetwork::write_transaction const & transaction, vxlnetwork::unchecked_key const & key)
{
	store.unchecked.del (transaction, key);
}

void vxlnetwork::unchecked_map::clear (vxlnetwork::write_transaction const & transaction)
{
	store.unchecked.clear (transaction);
}

size_t vxlnetwork::unchecked_map::count (vxlnetwork::transaction const & transaction) const
{
	return store.unchecked.count (transaction);
}

void vxlnetwork::unchecked_map::stop ()
{
	vxlnetwork::unique_lock<vxlnetwork::mutex> lock{ mutex };
	if (!stopped)
	{
		stopped = true;
		condition.notify_all (); // Notify flush (), run ()
	}
}

void vxlnetwork::unchecked_map::flush ()
{
	vxlnetwork::unique_lock<vxlnetwork::mutex> lock{ mutex };
	condition.wait (lock, [this] () {
		return stopped || (buffer.empty () && back_buffer.empty () && !writing_back_buffer);
	});
}

void vxlnetwork::unchecked_map::trigger (vxlnetwork::hash_or_account const & dependency)
{
	vxlnetwork::unique_lock<vxlnetwork::mutex> lock{ mutex };
	buffer.push_back (dependency);
	debug_assert (buffer.back ().which () == 1); // which stands for "query".
	lock.unlock ();
	condition.notify_all (); // Notify run ()
}

vxlnetwork::unchecked_map::item_visitor::item_visitor (unchecked_map & unchecked, vxlnetwork::write_transaction const & transaction) :
	unchecked{ unchecked },
	transaction{ transaction }
{
}
void vxlnetwork::unchecked_map::item_visitor::operator() (insert const & item)
{
	auto const & [dependency, info] = item;
	unchecked.store.unchecked.put (transaction, dependency, { info.block, info.account, info.verified });
}

void vxlnetwork::unchecked_map::item_visitor::operator() (query const & item)
{
	auto [i, n] = unchecked.store.unchecked.equal_range (transaction, item.hash);
	std::deque<vxlnetwork::unchecked_key> delete_queue;
	for (; i != n; ++i)
	{
		auto const & key = i->first;
		auto const & info = i->second;
		delete_queue.push_back (key);
		unchecked.satisfied (info);
	}
	if (!unchecked.disable_delete)
	{
		for (auto const & key : delete_queue)
		{
			unchecked.del (transaction, key);
		}
	}
}

void vxlnetwork::unchecked_map::write_buffer (decltype (buffer) const & back_buffer)
{
	auto transaction = store.tx_begin_write ();
	item_visitor visitor{ *this, transaction };
	for (auto const & item : back_buffer)
	{
		boost::apply_visitor (visitor, item);
	}
}

void vxlnetwork::unchecked_map::run ()
{
	vxlnetwork::thread_role::set (vxlnetwork::thread_role::name::unchecked);
	vxlnetwork::unique_lock<vxlnetwork::mutex> lock{ mutex };
	while (!stopped)
	{
		if (!buffer.empty ())
		{
			back_buffer.swap (buffer);
			writing_back_buffer = true;
			lock.unlock ();
			write_buffer (back_buffer);
			lock.lock ();
			writing_back_buffer = false;
			back_buffer.clear ();
		}
		else
		{
			condition.notify_all (); // Notify flush ()
			condition.wait (lock, [this] () {
				return stopped || !buffer.empty ();
			});
		}
	}
}
