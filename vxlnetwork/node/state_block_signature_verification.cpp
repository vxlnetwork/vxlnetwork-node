#include <vxlnetwork/lib/logger_mt.hpp>
#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/lib/threading.hpp>
#include <vxlnetwork/lib/timer.hpp>
#include <vxlnetwork/node/nodeconfig.hpp>
#include <vxlnetwork/node/signatures.hpp>
#include <vxlnetwork/node/state_block_signature_verification.hpp>
#include <vxlnetwork/secure/common.hpp>

#include <boost/format.hpp>

vxlnetwork::state_block_signature_verification::state_block_signature_verification (vxlnetwork::signature_checker & signature_checker, vxlnetwork::epochs & epochs, vxlnetwork::node_config & node_config, vxlnetwork::logger_mt & logger, uint64_t state_block_signature_verification_size) :
	signature_checker (signature_checker),
	epochs (epochs),
	node_config (node_config),
	logger (logger),
	thread ([this, state_block_signature_verification_size] () {
		vxlnetwork::thread_role::set (vxlnetwork::thread_role::name::state_block_signature_verification);
		this->run (state_block_signature_verification_size);
	})
{
}

vxlnetwork::state_block_signature_verification::~state_block_signature_verification ()
{
	stop ();
}

void vxlnetwork::state_block_signature_verification::stop ()
{
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> guard (mutex);
		stopped = true;
	}

	if (thread.joinable ())
	{
		condition.notify_one ();
		thread.join ();
	}
}

void vxlnetwork::state_block_signature_verification::run (uint64_t state_block_signature_verification_size)
{
	vxlnetwork::unique_lock<vxlnetwork::mutex> lk (mutex);
	while (!stopped)
	{
		if (!state_blocks.empty ())
		{
			std::size_t const max_verification_batch (state_block_signature_verification_size != 0 ? state_block_signature_verification_size : vxlnetwork::signature_checker::batch_size * (node_config.signature_checker_threads + 1));
			active = true;
			while (!state_blocks.empty () && !stopped)
			{
				auto items = setup_items (max_verification_batch);
				lk.unlock ();
				verify_state_blocks (items);
				lk.lock ();
			}
			active = false;
			lk.unlock ();
			transition_inactive_callback ();
			lk.lock ();
		}
		else
		{
			condition.wait (lk);
		}
	}
}

bool vxlnetwork::state_block_signature_verification::is_active ()
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> guard (mutex);
	return active;
}

void vxlnetwork::state_block_signature_verification::add (value_type const & item)
{
	{
		vxlnetwork::lock_guard<vxlnetwork::mutex> guard (mutex);
		state_blocks.emplace_back (item);
	}
	condition.notify_one ();
}

std::size_t vxlnetwork::state_block_signature_verification::size ()
{
	vxlnetwork::lock_guard<vxlnetwork::mutex> guard (mutex);
	return state_blocks.size ();
}

auto vxlnetwork::state_block_signature_verification::setup_items (std::size_t max_count) -> std::deque<value_type>
{
	std::deque<value_type> items;
	if (state_blocks.size () <= max_count)
	{
		items.swap (state_blocks);
	}
	else
	{
		for (auto i (0); i < max_count; ++i)
		{
			items.push_back (state_blocks.front ());
			state_blocks.pop_front ();
		}
		debug_assert (!state_blocks.empty ());
	}
	return items;
}

void vxlnetwork::state_block_signature_verification::verify_state_blocks (std::deque<value_type> & items)
{
	if (!items.empty ())
	{
		vxlnetwork::timer<> timer_l;
		timer_l.start ();
		auto size (items.size ());
		std::vector<vxlnetwork::block_hash> hashes;
		hashes.reserve (size);
		std::vector<unsigned char const *> messages;
		messages.reserve (size);
		std::vector<std::size_t> lengths;
		lengths.reserve (size);
		std::vector<vxlnetwork::account> accounts;
		accounts.reserve (size);
		std::vector<unsigned char const *> pub_keys;
		pub_keys.reserve (size);
		std::vector<vxlnetwork::signature> blocks_signatures;
		blocks_signatures.reserve (size);
		std::vector<unsigned char const *> signatures;
		signatures.reserve (size);
		std::vector<int> verifications;
		verifications.resize (size, 0);
		for (auto const & [block, account, unused] : items)
		{
			hashes.push_back (block->hash ());
			messages.push_back (hashes.back ().bytes.data ());
			lengths.push_back (sizeof (decltype (hashes)::value_type));
			vxlnetwork::account account_l = block->account ();
			if (!block->link ().is_zero () && epochs.is_epoch_link (block->link ()))
			{
				account_l = epochs.signer (epochs.epoch (block->link ()));
			}
			else if (!account.is_zero ())
			{
				account_l = account;
			}
			accounts.push_back (account_l);
			pub_keys.push_back (accounts.back ().bytes.data ());
			blocks_signatures.push_back (block->block_signature ());
			signatures.push_back (blocks_signatures.back ().bytes.data ());
		}
		vxlnetwork::signature_check_set check = { size, messages.data (), lengths.data (), pub_keys.data (), signatures.data (), verifications.data () };
		signature_checker.verify (check);
		if (node_config.logging.timing_logging () && timer_l.stop () > std::chrono::milliseconds (10))
		{
			logger.try_log (boost::str (boost::format ("Batch verified %1% state blocks in %2% %3%") % size % timer_l.value ().count () % timer_l.unit ()));
		}
		blocks_verified_callback (items, verifications, hashes, blocks_signatures);
	}
}

std::unique_ptr<vxlnetwork::container_info_component> vxlnetwork::collect_container_info (state_block_signature_verification & state_block_signature_verification, std::string const & name)
{
	auto composite = std::make_unique<container_info_composite> (name);
	composite->add_component (std::make_unique<container_info_leaf> (container_info{ "state_blocks", state_block_signature_verification.size (), sizeof (state_block_signature_verification::value_type) }));
	return composite;
}
