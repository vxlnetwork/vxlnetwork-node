namespace boost
{
namespace filesystem
{
	class path;
}
}

namespace vxlnetwork
{
class node_flags;
}
namespace vxlnetwork_daemon
{
class daemon
{
public:
	void run (boost::filesystem::path const &, vxlnetwork::node_flags const & flags);
};
}
