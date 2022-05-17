#pragma once

#include <vxlnetwork/crypto/blake2/blake2.h>
#include <vxlnetwork/lib/epoch.hpp>
#include <vxlnetwork/lib/errors.hpp>
#include <vxlnetwork/lib/numbers.hpp>
#include <vxlnetwork/lib/optional_ptr.hpp>
#include <vxlnetwork/lib/stream.hpp>
#include <vxlnetwork/lib/utility.hpp>
#include <vxlnetwork/lib/work.hpp>

#include <boost/property_tree/ptree_fwd.hpp>

#include <unordered_map>

namespace vxlnetwork
{
class block_visitor;
class mutable_block_visitor;
enum class block_type : uint8_t
{
	invalid = 0,
	not_a_block = 1,
	send = 2,
	receive = 3,
	open = 4,
	change = 5,
	state = 6
};
class block_details
{
	static_assert (std::is_same<std::underlying_type<vxlnetwork::epoch>::type, uint8_t> (), "Epoch enum is not the proper type");
	static_assert (static_cast<uint8_t> (vxlnetwork::epoch::max) < (1 << 5), "Epoch max is too large for the sideband");

public:
	block_details () = default;
	block_details (vxlnetwork::epoch const epoch_a, bool const is_send_a, bool const is_receive_a, bool const is_epoch_a);
	static constexpr size_t size ()
	{
		return 1;
	}
	bool operator== (block_details const & other_a) const;
	void serialize (vxlnetwork::stream &) const;
	bool deserialize (vxlnetwork::stream &);
	vxlnetwork::epoch epoch{ vxlnetwork::epoch::epoch_0 };
	bool is_send{ false };
	bool is_receive{ false };
	bool is_epoch{ false };

private:
	uint8_t packed () const;
	void unpack (uint8_t);
};

std::string state_subtype (vxlnetwork::block_details const);

class block_sideband final
{
public:
	block_sideband () = default;
	block_sideband (vxlnetwork::account const &, vxlnetwork::block_hash const &, vxlnetwork::amount const &, uint64_t const, uint64_t const, vxlnetwork::block_details const &, vxlnetwork::epoch const source_epoch_a);
	block_sideband (vxlnetwork::account const &, vxlnetwork::block_hash const &, vxlnetwork::amount const &, uint64_t const, uint64_t const, vxlnetwork::epoch const epoch_a, bool const is_send, bool const is_receive, bool const is_epoch, vxlnetwork::epoch const source_epoch_a);
	void serialize (vxlnetwork::stream &, vxlnetwork::block_type) const;
	bool deserialize (vxlnetwork::stream &, vxlnetwork::block_type);
	static size_t size (vxlnetwork::block_type);
	vxlnetwork::block_hash successor{ 0 };
	vxlnetwork::account account{};
	vxlnetwork::amount balance{ 0 };
	uint64_t height{ 0 };
	uint64_t timestamp{ 0 };
	vxlnetwork::block_details details;
	vxlnetwork::epoch source_epoch{ vxlnetwork::epoch::epoch_0 };
};
class block
{
public:
	// Return a digest of the hashables in this block.
	vxlnetwork::block_hash const & hash () const;
	// Return a digest of hashables and non-hashables in this block.
	vxlnetwork::block_hash full_hash () const;
	vxlnetwork::block_sideband const & sideband () const;
	void sideband_set (vxlnetwork::block_sideband const &);
	bool has_sideband () const;
	std::string to_json () const;
	virtual void hash (blake2b_state &) const = 0;
	virtual uint64_t block_work () const = 0;
	virtual void block_work_set (uint64_t) = 0;
	virtual vxlnetwork::account const & account () const;
	// Previous block in account's chain, zero for open block
	virtual vxlnetwork::block_hash const & previous () const = 0;
	// Source block for open/receive blocks, zero otherwise.
	virtual vxlnetwork::block_hash const & source () const;
	// Destination account for send blocks, zero otherwise.
	virtual vxlnetwork::account const & destination () const;
	// Previous block or account number for open blocks
	virtual vxlnetwork::root const & root () const = 0;
	// Qualified root value based on previous() and root()
	virtual vxlnetwork::qualified_root qualified_root () const;
	// Link field for state blocks, zero otherwise.
	virtual vxlnetwork::link const & link () const;
	virtual vxlnetwork::account const & representative () const;
	virtual vxlnetwork::amount const & balance () const;
	virtual void serialize (vxlnetwork::stream &) const = 0;
	virtual void serialize_json (std::string &, bool = false) const = 0;
	virtual void serialize_json (boost::property_tree::ptree &) const = 0;
	virtual void visit (vxlnetwork::block_visitor &) const = 0;
	virtual void visit (vxlnetwork::mutable_block_visitor &) = 0;
	virtual bool operator== (vxlnetwork::block const &) const = 0;
	virtual vxlnetwork::block_type type () const = 0;
	virtual vxlnetwork::signature const & block_signature () const = 0;
	virtual void signature_set (vxlnetwork::signature const &) = 0;
	virtual ~block () = default;
	virtual bool valid_predecessor (vxlnetwork::block const &) const = 0;
	static size_t size (vxlnetwork::block_type);
	virtual vxlnetwork::work_version work_version () const;
	// If there are any changes to the hashables, call this to update the cached hash
	void refresh ();

protected:
	mutable vxlnetwork::block_hash cached_hash{ 0 };
	/**
	 * Contextual details about a block, some fields may or may not be set depending on block type.
	 * This field is set via sideband_set in ledger processing or deserializing blocks from the database.
	 * Otherwise it may be null (for example, an old block or fork).
	 */
	vxlnetwork::optional_ptr<vxlnetwork::block_sideband> sideband_m;

private:
	vxlnetwork::block_hash generate_hash () const;
};
class send_hashables
{
public:
	send_hashables () = default;
	send_hashables (vxlnetwork::block_hash const &, vxlnetwork::account const &, vxlnetwork::amount const &);
	send_hashables (bool &, vxlnetwork::stream &);
	send_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	vxlnetwork::block_hash previous;
	vxlnetwork::account destination;
	vxlnetwork::amount balance;
	static std::size_t constexpr size = sizeof (previous) + sizeof (destination) + sizeof (balance);
};
class send_block : public vxlnetwork::block
{
public:
	send_block () = default;
	send_block (vxlnetwork::block_hash const &, vxlnetwork::account const &, vxlnetwork::amount const &, vxlnetwork::raw_key const &, vxlnetwork::public_key const &, uint64_t);
	send_block (bool &, vxlnetwork::stream &);
	send_block (bool &, boost::property_tree::ptree const &);
	virtual ~send_block () = default;
	using vxlnetwork::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	vxlnetwork::block_hash const & previous () const override;
	vxlnetwork::account const & destination () const override;
	vxlnetwork::root const & root () const override;
	vxlnetwork::amount const & balance () const override;
	void serialize (vxlnetwork::stream &) const override;
	bool deserialize (vxlnetwork::stream &);
	void serialize_json (std::string &, bool = false) const override;
	void serialize_json (boost::property_tree::ptree &) const override;
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (vxlnetwork::block_visitor &) const override;
	void visit (vxlnetwork::mutable_block_visitor &) override;
	vxlnetwork::block_type type () const override;
	vxlnetwork::signature const & block_signature () const override;
	void signature_set (vxlnetwork::signature const &) override;
	bool operator== (vxlnetwork::block const &) const override;
	bool operator== (vxlnetwork::send_block const &) const;
	bool valid_predecessor (vxlnetwork::block const &) const override;
	send_hashables hashables;
	vxlnetwork::signature signature;
	uint64_t work;
	static std::size_t constexpr size = vxlnetwork::send_hashables::size + sizeof (signature) + sizeof (work);
};
class receive_hashables
{
public:
	receive_hashables () = default;
	receive_hashables (vxlnetwork::block_hash const &, vxlnetwork::block_hash const &);
	receive_hashables (bool &, vxlnetwork::stream &);
	receive_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	vxlnetwork::block_hash previous;
	vxlnetwork::block_hash source;
	static std::size_t constexpr size = sizeof (previous) + sizeof (source);
};
class receive_block : public vxlnetwork::block
{
public:
	receive_block () = default;
	receive_block (vxlnetwork::block_hash const &, vxlnetwork::block_hash const &, vxlnetwork::raw_key const &, vxlnetwork::public_key const &, uint64_t);
	receive_block (bool &, vxlnetwork::stream &);
	receive_block (bool &, boost::property_tree::ptree const &);
	virtual ~receive_block () = default;
	using vxlnetwork::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	vxlnetwork::block_hash const & previous () const override;
	vxlnetwork::block_hash const & source () const override;
	vxlnetwork::root const & root () const override;
	void serialize (vxlnetwork::stream &) const override;
	bool deserialize (vxlnetwork::stream &);
	void serialize_json (std::string &, bool = false) const override;
	void serialize_json (boost::property_tree::ptree &) const override;
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (vxlnetwork::block_visitor &) const override;
	void visit (vxlnetwork::mutable_block_visitor &) override;
	vxlnetwork::block_type type () const override;
	vxlnetwork::signature const & block_signature () const override;
	void signature_set (vxlnetwork::signature const &) override;
	bool operator== (vxlnetwork::block const &) const override;
	bool operator== (vxlnetwork::receive_block const &) const;
	bool valid_predecessor (vxlnetwork::block const &) const override;
	receive_hashables hashables;
	vxlnetwork::signature signature;
	uint64_t work;
	static std::size_t constexpr size = vxlnetwork::receive_hashables::size + sizeof (signature) + sizeof (work);
};
class open_hashables
{
public:
	open_hashables () = default;
	open_hashables (vxlnetwork::block_hash const &, vxlnetwork::account const &, vxlnetwork::account const &);
	open_hashables (bool &, vxlnetwork::stream &);
	open_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	vxlnetwork::block_hash source;
	vxlnetwork::account representative;
	vxlnetwork::account account;
	static std::size_t constexpr size = sizeof (source) + sizeof (representative) + sizeof (account);
};
class open_block : public vxlnetwork::block
{
public:
	open_block () = default;
	open_block (vxlnetwork::block_hash const &, vxlnetwork::account const &, vxlnetwork::account const &, vxlnetwork::raw_key const &, vxlnetwork::public_key const &, uint64_t);
	open_block (vxlnetwork::block_hash const &, vxlnetwork::account const &, vxlnetwork::account const &, std::nullptr_t);
	open_block (bool &, vxlnetwork::stream &);
	open_block (bool &, boost::property_tree::ptree const &);
	virtual ~open_block () = default;
	using vxlnetwork::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	vxlnetwork::block_hash const & previous () const override;
	vxlnetwork::account const & account () const override;
	vxlnetwork::block_hash const & source () const override;
	vxlnetwork::root const & root () const override;
	vxlnetwork::account const & representative () const override;
	void serialize (vxlnetwork::stream &) const override;
	bool deserialize (vxlnetwork::stream &);
	void serialize_json (std::string &, bool = false) const override;
	void serialize_json (boost::property_tree::ptree &) const override;
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (vxlnetwork::block_visitor &) const override;
	void visit (vxlnetwork::mutable_block_visitor &) override;
	vxlnetwork::block_type type () const override;
	vxlnetwork::signature const & block_signature () const override;
	void signature_set (vxlnetwork::signature const &) override;
	bool operator== (vxlnetwork::block const &) const override;
	bool operator== (vxlnetwork::open_block const &) const;
	bool valid_predecessor (vxlnetwork::block const &) const override;
	vxlnetwork::open_hashables hashables;
	vxlnetwork::signature signature;
	uint64_t work;
	static std::size_t constexpr size = vxlnetwork::open_hashables::size + sizeof (signature) + sizeof (work);
};
class change_hashables
{
public:
	change_hashables () = default;
	change_hashables (vxlnetwork::block_hash const &, vxlnetwork::account const &);
	change_hashables (bool &, vxlnetwork::stream &);
	change_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	vxlnetwork::block_hash previous;
	vxlnetwork::account representative;
	static std::size_t constexpr size = sizeof (previous) + sizeof (representative);
};
class change_block : public vxlnetwork::block
{
public:
	change_block () = default;
	change_block (vxlnetwork::block_hash const &, vxlnetwork::account const &, vxlnetwork::raw_key const &, vxlnetwork::public_key const &, uint64_t);
	change_block (bool &, vxlnetwork::stream &);
	change_block (bool &, boost::property_tree::ptree const &);
	virtual ~change_block () = default;
	using vxlnetwork::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	vxlnetwork::block_hash const & previous () const override;
	vxlnetwork::root const & root () const override;
	vxlnetwork::account const & representative () const override;
	void serialize (vxlnetwork::stream &) const override;
	bool deserialize (vxlnetwork::stream &);
	void serialize_json (std::string &, bool = false) const override;
	void serialize_json (boost::property_tree::ptree &) const override;
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (vxlnetwork::block_visitor &) const override;
	void visit (vxlnetwork::mutable_block_visitor &) override;
	vxlnetwork::block_type type () const override;
	vxlnetwork::signature const & block_signature () const override;
	void signature_set (vxlnetwork::signature const &) override;
	bool operator== (vxlnetwork::block const &) const override;
	bool operator== (vxlnetwork::change_block const &) const;
	bool valid_predecessor (vxlnetwork::block const &) const override;
	vxlnetwork::change_hashables hashables;
	vxlnetwork::signature signature;
	uint64_t work;
	static std::size_t constexpr size = vxlnetwork::change_hashables::size + sizeof (signature) + sizeof (work);
};
class state_hashables
{
public:
	state_hashables () = default;
	state_hashables (vxlnetwork::account const &, vxlnetwork::block_hash const &, vxlnetwork::account const &, vxlnetwork::amount const &, vxlnetwork::link const &);
	state_hashables (bool &, vxlnetwork::stream &);
	state_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	// Account# / public key that operates this account
	// Uses:
	// Bulk signature validation in advance of further ledger processing
	// Arranging uncomitted transactions by account
	vxlnetwork::account account;
	// Previous transaction in this chain
	vxlnetwork::block_hash previous;
	// Representative of this account
	vxlnetwork::account representative;
	// Current balance of this account
	// Allows lookup of account balance simply by looking at the head block
	vxlnetwork::amount balance;
	// Link field contains source block_hash if receiving, destination account if sending
	vxlnetwork::link link;
	// Serialized size
	static std::size_t constexpr size = sizeof (account) + sizeof (previous) + sizeof (representative) + sizeof (balance) + sizeof (link);
};
class state_block : public vxlnetwork::block
{
public:
	state_block () = default;
	state_block (vxlnetwork::account const &, vxlnetwork::block_hash const &, vxlnetwork::account const &, vxlnetwork::amount const &, vxlnetwork::link const &, vxlnetwork::raw_key const &, vxlnetwork::public_key const &, uint64_t);
	state_block (bool &, vxlnetwork::stream &);
	state_block (bool &, boost::property_tree::ptree const &);
	virtual ~state_block () = default;
	using vxlnetwork::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	vxlnetwork::block_hash const & previous () const override;
	vxlnetwork::account const & account () const override;
	vxlnetwork::root const & root () const override;
	vxlnetwork::link const & link () const override;
	vxlnetwork::account const & representative () const override;
	vxlnetwork::amount const & balance () const override;
	void serialize (vxlnetwork::stream &) const override;
	bool deserialize (vxlnetwork::stream &);
	void serialize_json (std::string &, bool = false) const override;
	void serialize_json (boost::property_tree::ptree &) const override;
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (vxlnetwork::block_visitor &) const override;
	void visit (vxlnetwork::mutable_block_visitor &) override;
	vxlnetwork::block_type type () const override;
	vxlnetwork::signature const & block_signature () const override;
	void signature_set (vxlnetwork::signature const &) override;
	bool operator== (vxlnetwork::block const &) const override;
	bool operator== (vxlnetwork::state_block const &) const;
	bool valid_predecessor (vxlnetwork::block const &) const override;
	vxlnetwork::state_hashables hashables;
	vxlnetwork::signature signature;
	uint64_t work;
	static std::size_t constexpr size = vxlnetwork::state_hashables::size + sizeof (signature) + sizeof (work);
};
class block_visitor
{
public:
	virtual void send_block (vxlnetwork::send_block const &) = 0;
	virtual void receive_block (vxlnetwork::receive_block const &) = 0;
	virtual void open_block (vxlnetwork::open_block const &) = 0;
	virtual void change_block (vxlnetwork::change_block const &) = 0;
	virtual void state_block (vxlnetwork::state_block const &) = 0;
	virtual ~block_visitor () = default;
};
class mutable_block_visitor
{
public:
	virtual void send_block (vxlnetwork::send_block &) = 0;
	virtual void receive_block (vxlnetwork::receive_block &) = 0;
	virtual void open_block (vxlnetwork::open_block &) = 0;
	virtual void change_block (vxlnetwork::change_block &) = 0;
	virtual void state_block (vxlnetwork::state_block &) = 0;
	virtual ~mutable_block_visitor () = default;
};
/**
 * This class serves to find and return unique variants of a block in order to minimize memory usage
 */
class block_uniquer
{
public:
	using value_type = std::pair<vxlnetwork::uint256_union const, std::weak_ptr<vxlnetwork::block>>;

	std::shared_ptr<vxlnetwork::block> unique (std::shared_ptr<vxlnetwork::block> const &);
	size_t size ();

private:
	vxlnetwork::mutex mutex{ mutex_identifier (mutexes::block_uniquer) };
	std::unordered_map<std::remove_const_t<value_type::first_type>, value_type::second_type> blocks;
	static unsigned constexpr cleanup_count = 2;
};

std::unique_ptr<container_info_component> collect_container_info (block_uniquer & block_uniquer, std::string const & name);

std::shared_ptr<vxlnetwork::block> deserialize_block (vxlnetwork::stream &);
std::shared_ptr<vxlnetwork::block> deserialize_block (vxlnetwork::stream &, vxlnetwork::block_type, vxlnetwork::block_uniquer * = nullptr);
std::shared_ptr<vxlnetwork::block> deserialize_block_json (boost::property_tree::ptree const &, vxlnetwork::block_uniquer * = nullptr);
void serialize_block (vxlnetwork::stream &, vxlnetwork::block const &);
void block_memory_pool_purge ();
}
