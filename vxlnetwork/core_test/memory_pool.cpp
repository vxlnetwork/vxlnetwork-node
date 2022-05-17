#include <vxlnetwork/lib/memory.hpp>
#include <vxlnetwork/node/active_transactions.hpp>
#include <vxlnetwork/secure/common.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace
{
/** This allocator records the size of all allocations that happen */
template <class T>
class record_allocations_new_delete_allocator
{
public:
	using value_type = T;

	explicit record_allocations_new_delete_allocator (std::vector<size_t> * allocated) :
		allocated (allocated)
	{
	}

	template <typename U>
	record_allocations_new_delete_allocator (const record_allocations_new_delete_allocator<U> & a)
	{
		allocated = a.allocated;
	}

	template <typename U>
	record_allocations_new_delete_allocator & operator= (const record_allocations_new_delete_allocator<U> &) = delete;

	T * allocate (size_t num_to_allocate)
	{
		auto size_allocated = (sizeof (T) * num_to_allocate);
		allocated->push_back (size_allocated);
		return static_cast<T *> (::operator new (size_allocated));
	}

	void deallocate (T * p, size_t num_to_deallocate)
	{
		::operator delete (p);
	}

	std::vector<size_t> * allocated;
};

template <typename T>
size_t get_allocated_size ()
{
	std::vector<size_t> allocated;
	record_allocations_new_delete_allocator<T> alloc (&allocated);
	(void)std::allocate_shared<T, record_allocations_new_delete_allocator<T>> (alloc);
	debug_assert (allocated.size () == 1);
	return allocated.front ();
}
}

TEST (memory_pool, validate_cleanup)
{
	// This might be turned off, e.g on Mac for instance, so don't do this test
	if (!vxlnetwork::get_use_memory_pools ())
	{
		return;
	}

	vxlnetwork::make_shared<vxlnetwork::open_block> ();
	vxlnetwork::make_shared<vxlnetwork::receive_block> ();
	vxlnetwork::make_shared<vxlnetwork::send_block> ();
	vxlnetwork::make_shared<vxlnetwork::change_block> ();
	vxlnetwork::make_shared<vxlnetwork::state_block> ();
	vxlnetwork::make_shared<vxlnetwork::vote> ();

	ASSERT_TRUE (vxlnetwork::purge_shared_ptr_singleton_pool_memory<vxlnetwork::open_block> ());
	ASSERT_TRUE (vxlnetwork::purge_shared_ptr_singleton_pool_memory<vxlnetwork::receive_block> ());
	ASSERT_TRUE (vxlnetwork::purge_shared_ptr_singleton_pool_memory<vxlnetwork::send_block> ());
	ASSERT_TRUE (vxlnetwork::purge_shared_ptr_singleton_pool_memory<vxlnetwork::state_block> ());
	ASSERT_TRUE (vxlnetwork::purge_shared_ptr_singleton_pool_memory<vxlnetwork::vote> ());

	// Change blocks have the same size as open_block so won't deallocate any memory
	ASSERT_FALSE (vxlnetwork::purge_shared_ptr_singleton_pool_memory<vxlnetwork::change_block> ());

	ASSERT_EQ (vxlnetwork::determine_shared_ptr_pool_size<vxlnetwork::open_block> (), get_allocated_size<vxlnetwork::open_block> () - sizeof (size_t));
	ASSERT_EQ (vxlnetwork::determine_shared_ptr_pool_size<vxlnetwork::receive_block> (), get_allocated_size<vxlnetwork::receive_block> () - sizeof (size_t));
	ASSERT_EQ (vxlnetwork::determine_shared_ptr_pool_size<vxlnetwork::send_block> (), get_allocated_size<vxlnetwork::send_block> () - sizeof (size_t));
	ASSERT_EQ (vxlnetwork::determine_shared_ptr_pool_size<vxlnetwork::change_block> (), get_allocated_size<vxlnetwork::change_block> () - sizeof (size_t));
	ASSERT_EQ (vxlnetwork::determine_shared_ptr_pool_size<vxlnetwork::state_block> (), get_allocated_size<vxlnetwork::state_block> () - sizeof (size_t));
	ASSERT_EQ (vxlnetwork::determine_shared_ptr_pool_size<vxlnetwork::vote> (), get_allocated_size<vxlnetwork::vote> () - sizeof (size_t));

	{
		vxlnetwork::active_transactions::ordered_cache inactive_votes_cache;
		vxlnetwork::account representative{ 1 };
		vxlnetwork::block_hash hash{ 1 };
		uint64_t timestamp{ 1 };
		vxlnetwork::inactive_cache_status default_status{};
		inactive_votes_cache.emplace (std::chrono::steady_clock::now (), hash, representative, timestamp, default_status);
	}

	ASSERT_TRUE (vxlnetwork::purge_singleton_inactive_votes_cache_pool_memory ());
}
