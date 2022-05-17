#include <vxlnetwork/lib/threading.hpp>

#include <pthread.h>

void vxlnetwork::thread_role::set_os_name (std::string const & thread_name)
{
	pthread_setname_np (thread_name.c_str ());
}
