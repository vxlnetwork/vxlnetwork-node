#include <vxlnetwork/lib/config.hpp>
#include <vxlnetwork/lib/utility.hpp>
#include <vxlnetwork/node/write_database_queue.hpp>

#include <algorithm>

vxlnetwork::write_guard::write_guard (std::function<void ()> guard_finish_callback_a) :
	guard_finish_callback (guard_finish_callback_a)
{
}

vxlnetwork::write_guard::write_guard (vxlnetwork::write_guard && write_guard_a) noexcept :
	guard_finish_callback (std::move (write_guard_a.guard_finish_callback)),
	owns (write_guard_a.owns)
{
	write_guard_a.owns = false;
	write_guard_a.guard_finish_callback = nullptr;
}

vxlnetwork::write_guard & vxlnetwork::write_guard::operator= (vxlnetwork::write_guard && write_guard_a) noexcept
{
	owns = write_guard_a.owns;
	guard_finish_callback = std::move (write_guard_a.guard_finish_callback);

	write_guard_a.owns = false;
	write_guard_a.guard_finish_callback = nullptr;
	return *this;
}

vxlnetwork::write_guard::~write_guard ()
{
	if (owns)
	{
		guard_finish_callback ();
	}
}

bool vxlnetwork::write_guard::is_owned () const
{
	return owns;
}

void vxlnetwork::write_guard::release ()
{
	debug_assert (owns);
	if (owns)
	{
		guard_finish_callback ();
	}
	owns = false;
}

vxlnetwork::write_database_queue::write_database_queue (bool use_noops_a) :
	guard_finish_callback ([use_noops_a, &queue = queue, &mutex = mutex, &cv = cv] () {
		if (!use_noops_a)
		{
			{
				vxlnetwork::lock_guard<vxlnetwork::mutex> guard (mutex);
				queue.pop_front ();
			}
			cv.notify_all ();
		}
	}),
	use_noops (use_noops_a)
{
}

vxlnetwork::write_guard vxlnetwork::write_database_queue::wait (vxlnetwork::writer writer)
{
	if (use_noops)
	{
		return write_guard ([] {});
	}

	vxlnetwork::unique_lock<vxlnetwork::mutex> lk (mutex);
	// Add writer to the end of the queue if it's not already waiting
	auto exists = std::find (queue.cbegin (), queue.cend (), writer) != queue.cend ();
	if (!exists)
	{
		queue.push_back (writer);
	}

	while (queue.front () != writer)
	{
		cv.wait (lk);
	}

	return write_guard (guard_finish_callback);
}

bool vxlnetwork::write_database_queue::contains (vxlnetwork::writer writer)
{
	debug_assert (!use_noops);
	vxlnetwork::lock_guard<vxlnetwork::mutex> guard (mutex);
	return std::find (queue.cbegin (), queue.cend (), writer) != queue.cend ();
}

bool vxlnetwork::write_database_queue::process (vxlnetwork::writer writer)
{
	if (use_noops)
	{
		return true;
	}

	auto result = false;
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> guard (mutex);
		// Add writer to the end of the queue if it's not already waiting
		auto exists = std::find (queue.cbegin (), queue.cend (), writer) != queue.cend ();
		if (!exists)
		{
			queue.push_back (writer);
		}

		result = (queue.front () == writer);
	}

	if (!result)
	{
		cv.notify_all ();
	}

	return result;
}

vxlnetwork::write_guard vxlnetwork::write_database_queue::pop ()
{
	return write_guard (guard_finish_callback);
}
