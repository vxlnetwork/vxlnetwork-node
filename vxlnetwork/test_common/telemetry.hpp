#pragma once

#include <cstdint>

namespace vxlnetwork
{
class keypair;
class network_params;
class telemetry_data;

void compare_default_telemetry_response_data_excluding_signature (vxlnetwork::telemetry_data const & telemetry_data_a, vxlnetwork::network_params const & network_params_a, uint64_t bandwidth_limit_a, uint64_t active_difficulty_a);
void compare_default_telemetry_response_data (vxlnetwork::telemetry_data const & telemetry_data_a, vxlnetwork::network_params const & network_params_a, uint64_t bandwidth_limit_a, uint64_t active_difficulty_a, vxlnetwork::keypair const & node_id_a);
}