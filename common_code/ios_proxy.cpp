#include "ios_proxy.h"
#include "shared_data.h"
#include "mem_pool.h"
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/detail/strand_service.hpp>
#include <memory>

typedef boost::asio::detail::strand_service::strand_impl impl_type;
typedef boost::asio::basic_waitable_timer<boost::chrono::high_resolution_clock> timer_type;

ios_proxy::ios_proxy()
{
	_opend = false;
	_runLock = NULL;
	_runCount = 0;
	_priority = normal;
	_implPool = create_pool<impl_type>(256, [](void* p)
	{
		new(p)impl_type();
	});
	_timerPool = create_pool<timer_type>(4096, [this](void* p)
	{
		new(p)timer_type(_ios);
	});
}

ios_proxy::~ios_proxy()
{
	assert(!_opend);
	delete (mem_pool_base<impl_type>*)_implPool;
	delete (mem_pool_base<timer_type>*)_timerPool;
}

void ios_proxy::run(size_t threadNum)
{
	assert(threadNum >= 1);
	boost::lock_guard<boost::mutex> lg(_runMutex);
	if (!_opend)
	{
		_opend = true;
		_runCount = 0;
		_runLock = new boost::asio::io_service::work(_ios);
		_handleList.resize(threadNum);
		size_t rc = 0;
		std::shared_ptr<boost::mutex> blockMutex(new boost::mutex);
		std::shared_ptr<boost::condition_variable> blockConVar(new boost::condition_variable);
		std::weak_ptr<boost::mutex> weakMutex = blockMutex;
		std::weak_ptr<boost::condition_variable> weakConVar = blockConVar;
		boost::unique_lock<boost::mutex> ul(*blockMutex);
		for (size_t i = 0; i < threadNum; i++)
		{
			boost::thread* newThread = new boost::thread([&, i]()
			{
				try
				{
					{
						SetThreadPriority(GetCurrentThread(), _priority);
						DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &_handleList[i], 0, FALSE, DUPLICATE_SAME_ACCESS);
						auto blockMutex = weakMutex.lock();
						auto blockConVar = weakConVar.lock();
						boost::unique_lock<boost::mutex> ul(*blockMutex);
						if (threadNum == ++rc)
						{
							blockConVar->notify_all();
						}
						else
						{
							blockConVar->wait(ul);
						}
					}
					_runCount += _ios.run();
				}
				catch (msg_data::pool_memory_exception&)
				{
					MessageBoxA(NULL, "内存不足", NULL, NULL);
					exit(1);
				}
				catch (boost::exception&)
				{
					MessageBoxA(NULL, "未处理的BOOST异常", NULL, NULL);
					exit(2);
				}
				catch (std::exception&)
				{
					MessageBoxA(NULL, "未处理的STD异常", NULL, NULL);
					exit(3);
				}
				catch (std::shared_ptr<std::string> msg)
				{
					MessageBoxA(NULL, msg->c_str(), NULL, NULL);
					exit(4);
				}
				catch (...)
				{
					MessageBoxA(NULL, "未知异常", NULL, NULL);
					exit(-1);
				}
			});
			_threadIDs.insert(newThread->get_id());
			_runThreads.add_thread(newThread);
		}
		blockConVar->wait(ul);
	}
}

void ios_proxy::stop()
{
	boost::lock_guard<boost::mutex> lg(_runMutex);
	if (_opend)
	{
		assert(!runningInThisIos());
		delete _runLock;
		_runLock = NULL;
		_runThreads.join_all();
		_ios.reset();
		_threadIDs.clear();
		_ctrlMutex.lock();
		_handleList.clear();
		_ctrlMutex.unlock();
		_opend = false;
	}
}

void ios_proxy::suspend()
{
	boost::lock_guard<boost::mutex> lg(_ctrlMutex);
	for (auto it = _handleList.begin(); it != _handleList.end(); it++)
	{
		SuspendThread(*it);
	}
}

void ios_proxy::resume()
{
	boost::lock_guard<boost::mutex> lg(_ctrlMutex);
	for (auto it = _handleList.begin(); it != _handleList.end(); it++)
	{
		ResumeThread(*it);
	}
}

bool ios_proxy::runningInThisIos()
{
	assert(_opend);
	return _threadIDs.find(boost::this_thread::get_id()) != _threadIDs.end();
}

size_t ios_proxy::threadNumber()
{
	assert(_opend);
	return _threadIDs.size();
}

void ios_proxy::runPriority(priority pri)
{
	boost::lock_guard<boost::mutex> lg(_ctrlMutex);
	_priority = pri;
	for (auto it = _handleList.begin(); it != _handleList.end(); it++)
	{
		SetThreadPriority(*it, _priority);
	}
}

ios_proxy::priority ios_proxy::getPriority()
{
	return _priority;
}

long long ios_proxy::getRunCount()
{
	return _runCount;
}

unsigned ios_proxy::physicalConcurrency()
{
	return boost::thread::physical_concurrency();
}

unsigned ios_proxy::hardwareConcurrency()
{
	return boost::thread::hardware_concurrency();
}

void ios_proxy::cpuAffinity(unsigned mask)
{
	boost::lock_guard<boost::mutex> lg(_ctrlMutex);
	for (auto it = _handleList.begin(); it != _handleList.end(); it++)
	{
		SetThreadAffinityMask(*it, mask);
	}
}

ios_proxy::operator boost::asio::io_service&() const
{
	return (boost::asio::io_service&)_ios;
}

void* ios_proxy::getImpl()
{
	return ((mem_pool_base<impl_type>*)_implPool)->new_();
}

void ios_proxy::freeImpl(void* impl)
{
	((mem_pool_base<impl_type>*)_implPool)->delete_(impl);
}

void* ios_proxy::getTimer()
{
	return ((mem_pool_base<timer_type>*)_timerPool)->new_();
}

void ios_proxy::freeTimer(void* timer)
{
	((mem_pool_base<timer_type>*)_timerPool)->delete_(timer);
}