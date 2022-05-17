#include <windows.h>
namespace vxlnetwork
{
void work_thread_reprioritize ()
{
	SetThreadPriority (GetCurrentThread (), THREAD_MODE_BACKGROUND_BEGIN);
}
}
