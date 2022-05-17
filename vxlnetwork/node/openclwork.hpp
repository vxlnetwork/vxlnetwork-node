#pragma once

#include <vxlnetwork/lib/work.hpp>
#include <vxlnetwork/node/openclconfig.hpp>
#include <vxlnetwork/node/xorshift.hpp>

#include <boost/optional.hpp>

#include <atomic>
#include <mutex>
#include <vector>

#ifdef __APPLE__
#define CL_SILENCE_DEPRECATION
#include <OpenCL/opencl.h>
#else
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/cl.h>
#endif

namespace vxlnetwork
{
extern bool opencl_loaded;
class logger_mt;
class opencl_platform
{
public:
	cl_platform_id platform;
	std::vector<cl_device_id> devices;
};
class opencl_environment
{
public:
	opencl_environment (bool &);
	void dump (std::ostream & stream);
	std::vector<vxlnetwork::opencl_platform> platforms;
};
class root;
class work_pool;
class opencl_work
{
public:
	opencl_work (bool &, vxlnetwork::opencl_config const &, vxlnetwork::opencl_environment &, vxlnetwork::logger_mt &, vxlnetwork::work_thresholds & work);
	~opencl_work ();
	boost::optional<uint64_t> generate_work (vxlnetwork::work_version const, vxlnetwork::root const &, uint64_t const);
	boost::optional<uint64_t> generate_work (vxlnetwork::work_version const, vxlnetwork::root const &, uint64_t const, std::atomic<int> &);
	static std::unique_ptr<opencl_work> create (bool, vxlnetwork::opencl_config const &, vxlnetwork::logger_mt &, vxlnetwork::work_thresholds & work);
	vxlnetwork::opencl_config const & config;
	vxlnetwork::mutex mutex;
	cl_context context;
	cl_mem attempt_buffer;
	cl_mem result_buffer;
	cl_mem item_buffer;
	cl_mem difficulty_buffer;
	cl_program program;
	cl_kernel kernel;
	cl_command_queue queue;
	vxlnetwork::xorshift1024star rand;
	vxlnetwork::logger_mt & logger;
	vxlnetwork::work_thresholds & work;
};
}
