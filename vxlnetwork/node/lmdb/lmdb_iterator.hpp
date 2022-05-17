#pragma once

#include <vxlnetwork/secure/store.hpp>

#include <lmdb/libraries/liblmdb/lmdb.h>

namespace vxlnetwork
{
template <typename T, typename U>
class mdb_iterator : public store_iterator_impl<T, U>
{
public:
	mdb_iterator (vxlnetwork::transaction const & transaction_a, MDB_dbi db_a, MDB_val const & val_a = MDB_val{}, bool const direction_asc = true)
	{
		auto status (mdb_cursor_open (tx (transaction_a), db_a, &cursor));
		release_assert (status == 0);
		auto operation (MDB_SET_RANGE);
		if (val_a.mv_size != 0)
		{
			current.first = val_a;
		}
		else
		{
			operation = direction_asc ? MDB_FIRST : MDB_LAST;
		}
		auto status2 (mdb_cursor_get (cursor, &current.first.value, &current.second.value, operation));
		release_assert (status2 == 0 || status2 == MDB_NOTFOUND);
		if (status2 != MDB_NOTFOUND)
		{
			auto status3 (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_GET_CURRENT));
			release_assert (status3 == 0 || status3 == MDB_NOTFOUND);
			if (current.first.size () != sizeof (T))
			{
				clear ();
			}
		}
		else
		{
			clear ();
		}
	}

	mdb_iterator () = default;

	mdb_iterator (vxlnetwork::mdb_iterator<T, U> && other_a)
	{
		cursor = other_a.cursor;
		other_a.cursor = nullptr;
		current = other_a.current;
	}

	mdb_iterator (vxlnetwork::mdb_iterator<T, U> const &) = delete;

	~mdb_iterator ()
	{
		if (cursor != nullptr)
		{
			mdb_cursor_close (cursor);
		}
	}

	vxlnetwork::store_iterator_impl<T, U> & operator++ () override
	{
		debug_assert (cursor != nullptr);
		auto status (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_NEXT));
		release_assert (status == 0 || status == MDB_NOTFOUND);
		if (status == MDB_NOTFOUND)
		{
			clear ();
		}
		if (current.first.size () != sizeof (T))
		{
			clear ();
		}
		return *this;
	}

	vxlnetwork::store_iterator_impl<T, U> & operator-- () override
	{
		debug_assert (cursor != nullptr);
		auto status (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_PREV));
		release_assert (status == 0 || status == MDB_NOTFOUND);
		if (status == MDB_NOTFOUND)
		{
			clear ();
		}
		if (current.first.size () != sizeof (T))
		{
			clear ();
		}
		return *this;
	}

	std::pair<vxlnetwork::db_val<MDB_val>, vxlnetwork::db_val<MDB_val>> * operator-> ()
	{
		return &current;
	}

	bool operator== (vxlnetwork::store_iterator_impl<T, U> const & base_a) const override
	{
		auto const other_a (boost::polymorphic_downcast<vxlnetwork::mdb_iterator<T, U> const *> (&base_a));
		auto result (current.first.data () == other_a->current.first.data ());
		debug_assert (!result || (current.first.size () == other_a->current.first.size ()));
		debug_assert (!result || (current.second.data () == other_a->current.second.data ()));
		debug_assert (!result || (current.second.size () == other_a->current.second.size ()));
		return result;
	}

	bool is_end_sentinal () const override
	{
		return current.first.size () == 0;
	}
	void fill (std::pair<T, U> & value_a) const override
	{
		if (current.first.size () != 0)
		{
			value_a.first = static_cast<T> (current.first);
		}
		else
		{
			value_a.first = T ();
		}
		if (current.second.size () != 0)
		{
			value_a.second = static_cast<U> (current.second);
		}
		else
		{
			value_a.second = U ();
		}
	}
	void clear ()
	{
		current.first = vxlnetwork::db_val<MDB_val> ();
		current.second = vxlnetwork::db_val<MDB_val> ();
		debug_assert (is_end_sentinal ());
	}

	vxlnetwork::mdb_iterator<T, U> & operator= (vxlnetwork::mdb_iterator<T, U> && other_a)
	{
		if (cursor != nullptr)
		{
			mdb_cursor_close (cursor);
		}
		cursor = other_a.cursor;
		other_a.cursor = nullptr;
		current = other_a.current;
		other_a.clear ();
		return *this;
	}

	vxlnetwork::store_iterator_impl<T, U> & operator= (vxlnetwork::store_iterator_impl<T, U> const &) = delete;
	MDB_cursor * cursor{ nullptr };
	std::pair<vxlnetwork::db_val<MDB_val>, vxlnetwork::db_val<MDB_val>> current;

private:
	MDB_txn * tx (vxlnetwork::transaction const & transaction_a) const
	{
		return static_cast<MDB_txn *> (transaction_a.get_handle ());
	}
};

/**
 * Iterates the key/value pairs of two stores merged together
 */
template <typename T, typename U>
class mdb_merge_iterator : public store_iterator_impl<T, U>
{
public:
	mdb_merge_iterator (vxlnetwork::transaction const & transaction_a, MDB_dbi db1_a, MDB_dbi db2_a) :
		impl1 (std::make_unique<vxlnetwork::mdb_iterator<T, U>> (transaction_a, db1_a)),
		impl2 (std::make_unique<vxlnetwork::mdb_iterator<T, U>> (transaction_a, db2_a))
	{
	}

	mdb_merge_iterator () :
		impl1 (std::make_unique<vxlnetwork::mdb_iterator<T, U>> ()),
		impl2 (std::make_unique<vxlnetwork::mdb_iterator<T, U>> ())
	{
	}

	mdb_merge_iterator (vxlnetwork::transaction const & transaction_a, MDB_dbi db1_a, MDB_dbi db2_a, MDB_val const & val_a) :
		impl1 (std::make_unique<vxlnetwork::mdb_iterator<T, U>> (transaction_a, db1_a, val_a)),
		impl2 (std::make_unique<vxlnetwork::mdb_iterator<T, U>> (transaction_a, db2_a, val_a))
	{
	}

	mdb_merge_iterator (vxlnetwork::mdb_merge_iterator<T, U> && other_a)
	{
		impl1 = std::move (other_a.impl1);
		impl2 = std::move (other_a.impl2);
	}

	mdb_merge_iterator (vxlnetwork::mdb_merge_iterator<T, U> const &) = delete;

	vxlnetwork::store_iterator_impl<T, U> & operator++ () override
	{
		++least_iterator ();
		return *this;
	}

	vxlnetwork::store_iterator_impl<T, U> & operator-- () override
	{
		--least_iterator ();
		return *this;
	}

	std::pair<vxlnetwork::db_val<MDB_val>, vxlnetwork::db_val<MDB_val>> * operator-> ()
	{
		return least_iterator ().operator-> ();
	}

	bool operator== (vxlnetwork::store_iterator_impl<T, U> const & base_a) const override
	{
		debug_assert ((dynamic_cast<vxlnetwork::mdb_merge_iterator<T, U> const *> (&base_a) != nullptr) && "Incompatible iterator comparison");
		auto & other (static_cast<vxlnetwork::mdb_merge_iterator<T, U> const &> (base_a));
		return *impl1 == *other.impl1 && *impl2 == *other.impl2;
	}

	bool is_end_sentinal () const override
	{
		return least_iterator ().is_end_sentinal ();
	}

	void fill (std::pair<T, U> & value_a) const override
	{
		auto & current (least_iterator ());
		if (current->first.size () != 0)
		{
			value_a.first = static_cast<T> (current->first);
		}
		else
		{
			value_a.first = T ();
		}
		if (current->second.size () != 0)
		{
			value_a.second = static_cast<U> (current->second);
		}
		else
		{
			value_a.second = U ();
		}
	}
	vxlnetwork::mdb_merge_iterator<T, U> & operator= (vxlnetwork::mdb_merge_iterator<T, U> &&) = default;
	vxlnetwork::mdb_merge_iterator<T, U> & operator= (vxlnetwork::mdb_merge_iterator<T, U> const &) = delete;

	mutable bool from_first_database{ false };

private:
	vxlnetwork::mdb_iterator<T, U> & least_iterator () const
	{
		vxlnetwork::mdb_iterator<T, U> * result;
		if (impl1->is_end_sentinal ())
		{
			result = impl2.get ();
			from_first_database = false;
		}
		else if (impl2->is_end_sentinal ())
		{
			result = impl1.get ();
			from_first_database = true;
		}
		else
		{
			auto key_cmp (mdb_cmp (mdb_cursor_txn (impl1->cursor), mdb_cursor_dbi (impl1->cursor), impl1->current.first, impl2->current.first));

			if (key_cmp < 0)
			{
				result = impl1.get ();
				from_first_database = true;
			}
			else if (key_cmp > 0)
			{
				result = impl2.get ();
				from_first_database = false;
			}
			else
			{
				auto val_cmp (mdb_cmp (mdb_cursor_txn (impl1->cursor), mdb_cursor_dbi (impl1->cursor), impl1->current.second, impl2->current.second));
				result = val_cmp < 0 ? impl1.get () : impl2.get ();
				from_first_database = (result == impl1.get ());
			}
		}
		return *result;
	}

	std::unique_ptr<vxlnetwork::mdb_iterator<T, U>> impl1;
	std::unique_ptr<vxlnetwork::mdb_iterator<T, U>> impl2;
};
}
