#include <vxlnetwork/node/node_observers.hpp>

std::unique_ptr<vxlnetwork::container_info_component> vxlnetwork::collect_container_info (vxlnetwork::node_observers & node_observers, std::string const & name)
{
	auto composite = std::make_unique<vxlnetwork::container_info_composite> (name);
	composite->add_component (collect_container_info (node_observers.blocks, "blocks"));
	composite->add_component (collect_container_info (node_observers.wallet, "wallet"));
	composite->add_component (collect_container_info (node_observers.vote, "vote"));
	composite->add_component (collect_container_info (node_observers.active_stopped, "active_stopped"));
	composite->add_component (collect_container_info (node_observers.account_balance, "account_balance"));
	composite->add_component (collect_container_info (node_observers.endpoint, "endpoint"));
	composite->add_component (collect_container_info (node_observers.disconnect, "disconnect"));
	composite->add_component (collect_container_info (node_observers.work_cancel, "work_cancel"));
	return composite;
}
