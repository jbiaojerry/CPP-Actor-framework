/*!
 @header     actor_framework.h
 @abstract   并发逻辑控制框架(Actor Model)，使用"协程(coroutine)"技术，依赖boost_1.55或更新;
 @discussion 一个Actor对象(actor_handle)依赖一个shared_strand(二级调度器，本身依赖于io_service)，多个Actor可以共同依赖同一个shared_strand;
             支持强制结束、挂起/恢复、延时、多子任务(并发控制);
             在Actor中或所依赖的io_service中进行长时间阻塞的操作或重量级运算，会严重影响依赖同一个io_service的Actor响应速度;
             默认Actor栈空间64k字节，远比线程栈小，注意局部变量占用的空间以及调用层次(注意递归).
 @copyright  Copyright (c) 2015 HAM, E-Mail:591170887@qq.com
 */

#ifndef __ACTOR_FRAMEWORK_H
#define __ACTOR_FRAMEWORK_H

#include <boost/circular_buffer.hpp>
#include <list>
#include <xutility>
#include <functional>
#include "ios_proxy.h"
#include "shared_strand.h"
#include "ref_ex.h"
#include "function_type.h"
#include "msg_queue.h"
#include "actor_mutex.h"

class my_actor;
typedef std::shared_ptr<my_actor> actor_handle;//Actor句柄

using namespace std;

//此函数会进入Actor中断标记，使用时注意逻辑的“连续性”可能会被打破
#define __yield_interrupt

/*!
@brief 用于检测在Actor内调用的函数是否触发了强制退出
*/
#ifdef _DEBUG
#define BEGIN_CHECK_FORCE_QUIT try {
#define END_CHECK_FORCE_QUIT } catch (my_actor::force_quit_exception&) {assert(false);}
#else
#define BEGIN_CHECK_FORCE_QUIT
#define END_CHECK_FORCE_QUIT
#endif

// Actor内使用，在使用了Actor函数的异常捕捉 catch (...) 之前用于过滤Actor退出异常并继续抛出，不然可能导致程序崩溃
#define CATCH_ACTOR_QUIT()\
catch (my_actor::force_quit_exception& e)\
{\
	throw e;\
}

#ifdef _DEBUG
#define DEBUG_OPERATION(__exp__)	__exp__
#else
#define DEBUG_OPERATION(__exp__)
#endif

//默认堆栈大小64k
#define kB	*1024
#define DEFAULT_STACKSIZE	64 kB

template <typename T0, typename T1 = void, typename T2 = void, typename T3 = void>
struct msg_param
{
	typedef ref_ex<T0, T1, T2, T3> ref_type;
	typedef const_ref_ex<T0, T1, T2, T3> const_ref_type;

	msg_param()
	{

	}

	msg_param(const T0& p0, const T1& p1, const T2& p2, const T3& p3)
		:_res0(p0), _res1(p1), _res2(p2), _res3(p3)
	{

	}

	msg_param(const const_ref_type& rp)
		:_res0(rp._p0), _res1(rp._p1), _res2(rp._p2), _res3(rp._p3)
	{

	}

	msg_param(ref_type& s)
	{
		move_from(s);
	}

	msg_param(msg_param&& s)
	{
		_res0 = std::move(s._res0);
		_res1 = std::move(s._res1);
		_res2 = std::move(s._res2);
		_res3 = std::move(s._res3);
	}

	void operator =(msg_param&& s)
	{
		_res0 = std::move(s._res0);
		_res1 = std::move(s._res1);
		_res2 = std::move(s._res2);
		_res3 = std::move(s._res3);
	}

	void move_out(ref_type& dst)
	{
		dst._p0 = std::move(_res0);
		dst._p1 = std::move(_res1);
		dst._p2 = std::move(_res2);
		dst._p3 = std::move(_res3);
	}

	void save_from(const const_ref_type& rp)
	{
		_res0 = rp._p0;
		_res1 = rp._p1;
		_res2 = rp._p2;
		_res3 = rp._p3;
	}

	void move_from(ref_type& src)
	{
		_res0 = std::move(src._p0);
		_res1 = std::move(src._p1);
		_res2 = std::move(src._p2);
		_res3 = std::move(src._p3);
	}

	T0 _res0;
	T1 _res1;
	T2 _res2;
	T3 _res3;
};

template <typename T0, typename T1, typename T2>
struct msg_param<T0, T1, T2, void>
{
	typedef ref_ex<T0, T1, T2> ref_type;
	typedef const_ref_ex<T0, T1, T2> const_ref_type;

	msg_param()
	{

	}

	msg_param(const T0& p0, const T1& p1, const T2& p2)
		:_res0(p0), _res1(p1), _res2(p2)
	{

	}

	msg_param(const const_ref_type& rp)
		:_res0(rp._p0), _res1(rp._p1), _res2(rp._p2)
	{

	}

	msg_param(msg_param&& s)
	{
		_res0 = std::move(s._res0);
		_res1 = std::move(s._res1);
		_res2 = std::move(s._res2);
	}

	msg_param(ref_type& s)
	{
		move_from(s);
	}

	void operator =(msg_param&& s)
	{
		_res0 = std::move(s._res0);
		_res1 = std::move(s._res1);
		_res2 = std::move(s._res2);
	}

	void move_out(ref_type& dst)
	{
		dst._p0 = std::move(_res0);
		dst._p1 = std::move(_res1);
		dst._p2 = std::move(_res2);
	}

	void save_from(const const_ref_type& rp)
	{
		_res0 = rp._p0;
		_res1 = rp._p1;
		_res2 = rp._p2;
	}

	void move_from(ref_type& src)
	{
		_res0 = std::move(src._p0);
		_res1 = std::move(src._p1);
		_res2 = std::move(src._p2);
	}

	T0 _res0;
	T1 _res1;
	T2 _res2;
};

template <typename T0, typename T1>
struct msg_param<T0, T1, void, void>
{
	typedef ref_ex<T0, T1> ref_type;
	typedef const_ref_ex<T0, T1> const_ref_type;

	msg_param()
	{

	}

	msg_param(const T0& p0, const T1& p1)
		:_res0(p0), _res1(p1)
	{

	}

	msg_param(const const_ref_type& rp)
		:_res0(rp._p0), _res1(rp._p1)
	{

	}

	msg_param(msg_param&& s)
	{
		_res0 = std::move(s._res0);
		_res1 = std::move(s._res1);
	}

	msg_param(ref_type& s)
	{
		move_from(s);
	}

	void operator =(msg_param&& s)
	{
		_res0 = std::move(s._res0);
		_res1 = std::move(s._res1);
	}

	void move_out(ref_type& dst)
	{
		dst._p0 = std::move(_res0);
		dst._p1 = std::move(_res1);
	}

	void save_from(const const_ref_type& rp)
	{
		_res0 = rp._p0;
		_res1 = rp._p1;
	}

	void move_from(ref_type& src)
	{
		_res0 = std::move(src._p0);
		_res1 = std::move(src._p1);
	}

	T0 _res0;
	T1 _res1;
};

template <typename T0>
struct msg_param<T0, void, void, void>
{
	typedef ref_ex<T0> ref_type;
	typedef const_ref_ex<T0> const_ref_type;

	msg_param()
	{

	}

	msg_param(const T0& p0)
		:_res0(p0)
	{

	}

	msg_param(const const_ref_type& rp)
		:_res0(rp._p0)
	{

	}

	msg_param(msg_param&& s)
	{
		_res0 = std::move(s._res0);
	}

	msg_param(ref_type& s)
	{
		move_from(s);
	}

	void operator =(msg_param&& s)
	{
		_res0 = std::move(s._res0);
	}

	void move_out(ref_type& dst)
	{
		dst._p0 = std::move(_res0);
	}

	void save_from(const const_ref_type& rp)
	{
		_res0 = rp._p0;
	}

	void move_from(ref_type& src)
	{
		_res0 = std::move(src._p0);
	}

	T0 _res0;
};
//////////////////////////////////////////////////////////////////////////

class actor_msg_handle_base
{
public:
	actor_msg_handle_base();
	virtual ~actor_msg_handle_base(){};
public:
	virtual void close() = 0;
protected:
	void run_one();
	void set_actor(const actor_handle& hostActor);
protected:
	bool _waiting;
	shared_strand _strand;
	actor_handle _hostActor;
	std::shared_ptr<bool> _closed;
};

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class actor_msg_handle;

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class actor_trig_handle;

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class actor_msg_notifer
{
	typedef actor_msg_handle<T0, T1, T2, T3> msg_handle;

	friend msg_handle;
public:
	actor_msg_notifer()
		:_msgHandle(NULL){}
private:
	actor_msg_notifer(msg_handle* msgHandle)
		:_msgHandle(msgHandle), _strand(msgHandle->_strand), _hostActor(msgHandle->_hostActor), _closed(msgHandle->_closed) {}
public:
	template <typename PT0, typename PT1, typename PT2, typename PT3>
	void operator()(const PT0& p0, const PT1& p1, const PT2& p2, const PT3& p3) const
	{
		auto& msgHandle_ = _msgHandle;
		auto& hostActor_ = _hostActor;
		auto& closed_ = _closed;
		_strand->post([=]()
		{
			if (!hostActor_->is_quited() && !(*closed_))
			{
				msgHandle_->push_msg(ref_ex<PT0, PT1, PT2, PT3>((PT0&)p0, (PT1&)p1, (PT2&)p2, (PT3&)p3));
			}
		});
	}

	template <typename PT0, typename PT1, typename PT2>
	void operator()(const PT0& p0, const PT1& p1, const PT2& p2) const
	{
		auto& msgHandle_ = _msgHandle;
		auto& hostActor_ = _hostActor;
		auto& closed_ = _closed;
		_strand->post([=]()
		{
			if (!hostActor_->is_quited() && !(*closed_))
			{
				msgHandle_->push_msg(ref_ex<PT0, PT1, PT2>((PT0&)p0, (PT1&)p1, (PT2&)p2));
			}
		});
	}

	template <typename PT0, typename PT1>
	void operator()(const PT0& p0, const PT1& p1) const
	{
		auto& msgHandle_ = _msgHandle;
		auto& hostActor_ = _hostActor;
		auto& closed_ = _closed;
		_strand->post([=]()
		{
			if (!hostActor_->is_quited() && !(*closed_))
			{
				msgHandle_->push_msg(ref_ex<PT0, PT1>((PT0&)p0, (PT1&)p1));
			}
		});
	}

	template <typename PT0>
	void operator()(const PT0& p0) const
	{
		auto& msgHandle_ = _msgHandle;
		auto& hostActor_ = _hostActor;
		auto& closed_ = _closed;
		_strand->post([=]()
		{
			if (!hostActor_->is_quited() && !(*closed_))
			{
				msgHandle_->push_msg(ref_ex<PT0>((PT0&)p0));
			}
		});
	}

	void operator()() const
	{
		auto& msgHandle_ = _msgHandle;
		auto& hostActor_ = _hostActor;
		auto& closed_ = _closed;
		_strand->post([=]()
		{
			if (!hostActor_->is_quited() && !(*closed_))
			{
				msgHandle_->push_msg();
			}
		});
	}

	actor_handle host_actor() const
	{
		return _hostActor;
	}

	bool empty() const
	{
		return !_msgHandle;
	}

	void clear()
	{
		_msgHandle = NULL;
		_strand.reset();
		_hostActor.reset();
		_closed.reset();
	}

	operator bool() const
	{
		return !empty();
	}
private:
	msg_handle* _msgHandle;
	shared_strand _strand;
	actor_handle _hostActor;
	std::shared_ptr<bool> _closed;
};

template <typename T0, typename T1, typename T2, typename T3>
class actor_msg_handle: public actor_msg_handle_base
{
	typedef msg_param<T0, T1, T2, T3> msg_type;
	typedef ref_ex<T0, T1, T2, T3> ref_type;
	typedef actor_msg_notifer<T0, T1, T2, T3> msg_notifer;

	friend msg_notifer;
	friend my_actor;
public:
	actor_msg_handle(size_t fixedSize = 16)
		:_msgBuff(fixedSize), _dstRef(NULL) {}

	~actor_msg_handle()
	{
		close();
	}
private:
	msg_notifer make_notifer(const actor_handle& hostActor)
	{
		close();
		set_actor(hostActor);
		_closed = std::shared_ptr<bool>(new bool(false));
		_waiting = false;
		return msg_notifer(this);
	}

	void push_msg(ref_type& msg)
	{
		assert(_strand->running_in_this_thread());
		if (_waiting)
		{
			_waiting = false;
			assert(_msgBuff.empty());
			assert(_dstRef);
			_dstRef->move_from(msg);
			_dstRef = NULL;
			run_one();
			return;
		}
		_msgBuff.push_back(std::move(msg_type(msg)));
	}

	bool read_msg(ref_type& dst)
	{
		assert(_strand->running_in_this_thread());
		if (!_msgBuff.empty())
		{
			_msgBuff.front().move_out(dst);
			_msgBuff.pop_front();
			return true;
		}
		_dstRef = &dst;
		_waiting = true;
		return false;
	}

	void close()
	{
		if (_closed)
		{
			*_closed = true;
			assert(_strand->running_in_this_thread());
		}
		_dstRef = NULL;
		_waiting = false;
		_msgBuff.clear();
		_hostActor.reset();
	}

	size_t size()
	{
		assert(_strand->running_in_this_thread());
		return _msgBuff.size();
	}
private:
	ref_type* _dstRef;
	msg_queue<msg_type> _msgBuff;
};


template <>
class actor_msg_handle<void, void, void, void> : public actor_msg_handle_base
{
	typedef actor_msg_notifer<> msg_notifer;

	friend msg_notifer;
	friend my_actor;
public:
	~actor_msg_handle()
	{
		close();
	}
private:
	msg_notifer make_notifer(const actor_handle& hostActor)
	{
		close();
		set_actor(hostActor);
		_closed = std::shared_ptr<bool>(new bool(false));
		_waiting = false;
		return msg_notifer(this);
	}

	void push_msg()
	{
		assert(_strand->running_in_this_thread());
		if (_waiting)
		{
			_waiting = false;
			run_one();
			return;
		}
		_msgCount++;
	}

	bool read_msg()
	{
		assert(_strand->running_in_this_thread());
		if (_msgCount)
		{
			_msgCount--;
			return true;
		}
		_waiting = true;
		return false;
	}

	void close()
	{
		if (_closed)
		{
			*_closed = true;
			assert(_strand->running_in_this_thread());
		}
		_msgCount = 0;
		_waiting = false;
		_hostActor.reset();
	}

	size_t size()
	{
		assert(_strand->running_in_this_thread());
		return _msgCount;
	}
private:
	size_t _msgCount;
};
//////////////////////////////////////////////////////////////////////////

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class actor_trig_notifer
{
	typedef actor_trig_handle<T0, T1, T2, T3> trig_handle;

	friend trig_handle;
public:
	actor_trig_notifer()
		:_trigHandle(NULL){}
private:
	actor_trig_notifer(trig_handle* trigHandle)
		:_trigHandle(trigHandle), _strand(trigHandle->_strand), _hostActor(trigHandle->_hostActor), _closed(trigHandle->_closed) {}
public:
	template <typename PT0, typename PT1, typename PT2, typename PT3>
	void operator()(const PT0& p0, const PT1& p1, const PT2& p2, const PT3& p3) const
	{
		auto& trigHandle_ = _trigHandle;
		auto& hostActor_ = _hostActor;
		auto& closed_ = _closed;
		_strand->post([=]()
		{
			if (!hostActor_->is_quited() && !(*closed_))
			{
				trigHandle_->push_msg(ref_ex<PT0, PT1, PT2, PT3>((PT0&)p0, (PT1&)p1, (PT2&)p2, (PT3&)p3));
			}
		});
	}

	template <typename PT0, typename PT1, typename PT2>
	void operator()(const PT0& p0, const PT1& p1, const PT2& p2) const
	{
		auto& trigHandle_ = _trigHandle;
		auto& hostActor_ = _hostActor;
		auto& closed_ = _closed;
		_strand->post([=]()
		{
			if (!hostActor_->is_quited() && !(*closed_))
			{
				trigHandle_->push_msg(ref_ex<PT0, PT1, PT2>((PT0&)p0, (PT1&)p1, (PT2&)p2));
			}
		});
	}

	template <typename PT0, typename PT1>
	void operator()(const PT0& p0, const PT1& p1) const
	{
		auto& trigHandle_ = _trigHandle;
		auto& hostActor_ = _hostActor;
		auto& closed_ = _closed;
		_strand->post([=]()
		{
			if (!hostActor_->is_quited() && !(*closed_))
			{
				trigHandle_->push_msg(ref_ex<PT0, PT1>((PT0&)p0, (PT1&)p1));
			}
		});
	}

	template <typename PT0>
	void operator()(const PT0& p0) const
	{
		auto& trigHandle_ = _trigHandle;
		auto& hostActor_ = _hostActor;
		auto& closed_ = _closed;
		_strand->post([=]()
		{
			if (!hostActor_->is_quited() && !(*closed_))
			{
				trigHandle_->push_msg(ref_ex<PT0>((PT0&)p0));
			}
		});
	}

	void operator()() const
	{
		auto& trigHandle_ = _trigHandle;
		auto& hostActor_ = _hostActor;
		auto& closed_ = _closed;
		_strand->post([=]()
		{
			if (!hostActor_->is_quited() && !(*closed_))
			{
				trigHandle_->push_msg();
			}
		});
	}

	actor_handle host_actor() const
	{
		return _hostActor;
	}

	bool empty() const
	{
		return !_trigHandle;
	}

	void clear()
	{
		_trigHandle = NULL;
		_strand.reset();
		_hostActor.reset();
		_closed.reset();
	}

	operator bool() const
	{
		return !empty();
	}
private:
	trig_handle* _trigHandle;
	shared_strand _strand;
	actor_handle _hostActor;
	std::shared_ptr<bool> _closed;
};

template <typename T0, typename T1, typename T2, typename T3>
class actor_trig_handle : public actor_msg_handle_base
{
	typedef msg_param<T0, T1, T2, T3> msg_type;
	typedef ref_ex<T0, T1, T2, T3> ref_type;
	typedef actor_trig_notifer<T0, T1, T2, T3> msg_notifer;

	friend msg_notifer;
	friend my_actor;
public:
	actor_trig_handle()
		:_hasMsg(false), _dstRef(NULL) {}

	~actor_trig_handle()
	{
		close();
	}
private:
	msg_notifer make_notifer(const actor_handle& hostActor)
	{
		close();
		set_actor(hostActor);
		_closed = std::shared_ptr<bool>(new bool(false));
		_waiting = false;
		_hasMsg = false;
		return msg_notifer(this);
	}

	void push_msg(ref_type& msg)
	{
		assert(_strand->running_in_this_thread());
		*_closed = true;
		if (_waiting)
		{
			_waiting = false;
			assert(_dstRef);
			_dstRef->move_from(msg);
			_dstRef = NULL;
			run_one();
			return;
		}
		_hasMsg = true;
		new (_msgBuff)msg_type(msg);
	}

	bool read_msg(ref_type& dst)
	{
		assert(_strand->running_in_this_thread());
		if (_hasMsg)
		{
			_hasMsg = false;
			((msg_type*)_msgBuff)->move_out(dst);
			((msg_type*)_msgBuff)->~msg_type();
			return true;
		}
		_dstRef = &dst;
		_waiting = true;
		return false;
	}

	void close()
	{
		if (_closed)
		{
			*_closed = true;
			assert(_strand->running_in_this_thread());
		}
		if (_hasMsg)
		{
			_hasMsg = false;
			((msg_type*)_msgBuff)->~msg_type();
		}
		_dstRef = NULL;
		_waiting = false;
		_hostActor.reset();
	}
public:
	bool has()
	{
		return _hasMsg;
	}
private:
	ref_type* _dstRef;
	bool _hasMsg;
	BYTE _msgBuff[sizeof(msg_type)];
};

template <>
class actor_trig_handle<void, void, void, void> : public actor_msg_handle_base
{
	typedef actor_trig_notifer<> msg_notifer;

	friend msg_notifer;
	friend my_actor;
public:
	actor_trig_handle()
		:_hasMsg(false){}

	~actor_trig_handle()
	{
		close();
	}
private:
	msg_notifer make_notifer(const actor_handle& hostActor)
	{
		close();
		set_actor(hostActor);
		_closed = std::shared_ptr<bool>(new bool(false));
		_waiting = false;
		_hasMsg = false;
		return msg_notifer(this);
	}

	void push_msg()
	{
		assert(_strand->running_in_this_thread());
		*_closed = true;
		if (_waiting)
		{
			_waiting = false;
			run_one();
			return;
		}
		_hasMsg = true;
	}

	bool read_msg()
	{
		assert(_strand->running_in_this_thread());
		if (_hasMsg)
		{
			_hasMsg = false;
			return true;
		}
		_waiting = true;
		return false;
	}

	void close()
	{
		if (_closed)
		{
			*_closed = true;
			assert(_strand->running_in_this_thread());
		}
		_hasMsg = false;
		_waiting = false;
		_hostActor.reset();
	}
public:
	bool has()
	{
		return _hasMsg;
	}
private:
	bool _hasMsg;
};

//////////////////////////////////////////////////////////////////////////
template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class msg_pump;

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class msg_pool;

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class post_actor_msg;

class msg_pump_base
{
	friend my_actor;
public:
	virtual ~msg_pump_base() {};
	virtual void clear() = 0;
	virtual void close() = 0;
protected:
	void run_one();
protected:
	actor_handle _hostActor;
};

class msg_pool_base
{
	friend msg_pump<>;
	friend my_actor;
public:
	virtual ~msg_pool_base() {};
};

template <typename T0, typename T1, typename T2, typename T3>
class msg_pump : public msg_pump_base
{
	typedef msg_param<T0, T1, T2, T3> msg_type;
	typedef const_ref_ex<T0, T1, T2, T3> const_ref_type;
	typedef ref_ex<T0, T1, T2, T3> ref_type;
	typedef msg_pool<T0, T1, T2, T3> msg_pool_type;
	typedef typename msg_pool_type::pump_handler pump_handler;

	friend my_actor;
	friend msg_pool<T0, T1, T2, T3>;
public:
	typedef msg_pump* handle;
private:
	msg_pump(){}
public:
	~msg_pump(){}
private:
	static std::shared_ptr<msg_pump> make(const actor_handle& hostActor)
	{
		std::shared_ptr<msg_pump> res(new msg_pump());
		res->_weakThis = res;
		res->_hasMsg = false;
		res->_waiting = false;
		res->_checkDis = false;
		res->_pumpCount = 0;
		res->_dstRef = NULL;
		res->_hostActor = hostActor;
		res->_strand = hostActor->self_strand();
		return res;
	}

	void receiver(msg_type&& msg)
	{
		if (_hostActor)
		{
			assert(!_hasMsg);
			_pumpCount++;
			if (_dstRef)
			{
				msg.move_out(*_dstRef);
				_dstRef = NULL;
				if (_waiting)
				{
					_waiting = false;
					_checkDis = false;
					run_one();
				}
			}
			else
			{//pump_msg超时结束后才接受到消息
				assert(!_waiting);
				_hasMsg = true;
				new (_msgSpace)msg_type(std::move(msg));
			}
		}
	}

	void receive_msg_post(msg_type&& msg)
	{
		auto shared_this = _weakThis.lock();
		_strand->post([=]()
		{
			shared_this->receiver(std::move((msg_type&)msg));
		});
	}

	void receive_msg(msg_type&& msg)
	{
		if (_strand->running_in_this_thread())
		{
			receiver(std::move((msg_type&)msg));
		} 
		else
		{
			receive_msg_post(std::move(msg));
		}
	}

	bool read_msg(ref_type& dst)
	{
		assert(_strand->running_in_this_thread());
		assert(!_dstRef);
		assert(!_waiting);
		if (_hasMsg)
		{
			_hasMsg = false;
			((msg_type*)_msgSpace)->move_out(dst);
			((msg_type*)_msgSpace)->~msg_type();
			return true;
		}
		_dstRef = &dst;
		if (!_pumpHandler.empty())
		{
			_pumpHandler(_pumpCount);
			_waiting = !!_dstRef;
			return !_dstRef;
		}
		_waiting = true;
		return false;
	}

	void connect(const pump_handler& pumpHandler)
	{
		assert(_strand->running_in_this_thread());
		if (_hostActor)
		{
			_pumpHandler = pumpHandler;
			_pumpCount = 0;
			if (_waiting)
			{
				_pumpHandler.post_pump(_pumpCount);
			}
		}
	}

	void clear()
	{
		assert(_strand->running_in_this_thread());
		assert(_hostActor);
		_pumpHandler.clear();
		if (_checkDis)
		{
			assert(_waiting);
			_waiting = false;
			_dstRef = NULL;
			run_one();
		}
	}

	void close()
	{
		if (_hasMsg)
		{
			((msg_type*)_msgSpace)->~msg_type();
		}
		_hasMsg = false;
		_dstRef = NULL;
		_pumpCount = 0;
		_waiting = false;
		_checkDis = false;
		_pumpHandler.clear();
		_hostActor.reset();
	}

	bool isDisconnected()
	{
		return _pumpHandler.empty();
	}
private:
	std::weak_ptr<msg_pump> _weakThis;
	BYTE _msgSpace[sizeof(msg_type)];
	pump_handler _pumpHandler;
	shared_strand _strand;
	ref_type* _dstRef;
	BYTE _pumpCount;
	bool _hasMsg;
	bool _waiting;
	bool _checkDis;
};

template <typename T0, typename T1, typename T2, typename T3>
class msg_pool : public msg_pool_base
{
	typedef msg_param<T0, T1, T2, T3> msg_type;
	typedef const_ref_ex<T0, T1, T2, T3> const_ref_type;
	typedef ref_ex<T0, T1, T2, T3> ref_type;
	typedef msg_pump<T0, T1, T2, T3> msg_pump_type;
	typedef post_actor_msg<T0, T1, T2, T3> post_type;

	struct pump_handler
	{
		void operator()(BYTE pumpID)
		{
			assert(_thisPool);
			if (_thisPool->_strand->running_in_this_thread())
			{
				if (_msgPump == _thisPool->_msgPump)
				{
					auto& msgBuff = _thisPool->_msgBuff;
					if (pumpID == _thisPool->_sendCount)
					{
						if (!msgBuff.empty())
						{
							msg_type mt_ = std::move(msgBuff.front());
							msgBuff.pop_front();
							_thisPool->_sendCount++;
							_thisPool->_msgPump->receive_msg(std::move(mt_));
						}
						else
						{
							_thisPool->_waiting = true;
						}
					}
					else
					{//上次消息没取到，重新取，但实际中间已经post出去了
						assert(!_thisPool->_waiting);
						assert(pumpID + 1 == _thisPool->_sendCount);
					}
				}
			}
			else
			{
				auto& refThis_ = *this;
				_thisPool->_strand->post([refThis_, pumpID]()
				{
					((pump_handler&)refThis_)(pumpID);
				});
			}
		}

		void post_pump(BYTE pumpID)
		{
			auto& refThis_ = *this;
			_thisPool->_strand->post([refThis_, pumpID]()
			{
				((pump_handler&)refThis_)(pumpID);
			});
		}

		bool empty()
		{
			return !_thisPool;
		}

		void clear()
		{
			_thisPool.reset();
			_msgPump.reset();
		}

		std::shared_ptr<msg_pool> _thisPool;
		std::shared_ptr<msg_pump_type> _msgPump;
	};

	friend my_actor;
	friend msg_pump_type;
	friend post_type;
public:
	msg_pool(size_t fixedSize)
		:_msgBuff(fixedSize)
	{

	}
	~msg_pool()
	{

	}
private:
	static std::shared_ptr<msg_pool> make(shared_strand strand, size_t fixedSize)
	{
		std::shared_ptr<msg_pool> res(new msg_pool(fixedSize));
		res->_weakThis = res;
		res->_strand = strand;
		res->_waiting = false;
		res->_sendCount = 0;
		return res;
	}

	void send_msg(msg_type&& mt, bool post)
	{
		if (_waiting)
		{
			_waiting = false;
			assert(_msgPump);
			_sendCount++;
			if (_msgBuff.empty())
			{
				if (post)
				{
					_msgPump->receive_msg_post(std::move(mt));
				}
				else
				{
					_msgPump->receive_msg(std::move(mt));
				}
			}
			else
			{
				if (post)
				{
					_msgBuff.push_back(std::move(mt));
					_msgPump->receive_msg_post(std::move(_msgBuff.front()));
					_msgBuff.pop_front();
				}
				else
				{
					_msgBuff.push_back(std::move(mt));
					msg_type mt_ = std::move(_msgBuff.front());
					_msgBuff.pop_front();
					_msgPump->receive_msg(std::move(mt_));
				}
			}
		}
		else
		{
			if (post)
			{
				_msgBuff.push_back(mt);
			}
			else
			{
				_msgBuff.push_back(std::move(mt));
			}
		}
	}

	void push_msg(msg_type&& mt)
	{
		if (_strand->running_in_this_thread())
		{
			send_msg(std::move(mt), true);
		}
		else
		{
			auto shared_this = _weakThis.lock();
			_strand->post([=]()
			{
				shared_this->send_msg(std::move((msg_type&)mt), false);
			});
		}
	}

	pump_handler connect_pump(const std::shared_ptr<msg_pump_type>& msgPump)
	{
		assert(msgPump);
		assert(_strand->running_in_this_thread());
		_msgPump = msgPump;
		pump_handler compHandler;
		compHandler._thisPool = _weakThis.lock();
		compHandler._msgPump = msgPump;
		_sendCount = 0;
		_waiting = false;
		return compHandler;
	}

	void disconnect()
	{
		assert(_strand->running_in_this_thread());
		_msgPump.reset();
		_waiting = false;
	}

	void expand_fixed(size_t fixedSize)
	{
		assert(_strand->running_in_this_thread());
		_msgBuff.expand_fixed(fixedSize);
	}
private:
	std::weak_ptr<msg_pool> _weakThis;
	std::shared_ptr<msg_pump_type> _msgPump;
	msg_queue<msg_type> _msgBuff;
	shared_strand _strand;
	BYTE _sendCount;
	bool _waiting;
};

class msg_pump_void;

class msg_pool_void : public msg_pool_base
{
	typedef post_actor_msg<> post_type;
	typedef msg_pump_void msg_pump_type;

	struct pump_handler
	{
		void operator()(BYTE pumpID);
		void post_pump(BYTE pumpID);
		bool empty();
		bool same_strand();
		void clear();

		std::shared_ptr<msg_pool_void> _thisPool;
		std::shared_ptr<msg_pump_type> _msgPump;
	};

	friend my_actor;
	friend msg_pump_void;
	friend post_type;
protected:
	msg_pool_void(shared_strand strand);
public:
	virtual ~msg_pool_void();
protected:
	void send_msg(bool post);
	void push_msg();
	pump_handler connect_pump(const std::shared_ptr<msg_pump_type>& msgPump);
	void disconnect();
	void expand_fixed(size_t fixedSize){};
protected:
	std::weak_ptr<msg_pool_void> _weakThis;
	std::shared_ptr<msg_pump_type> _msgPump;
	size_t _msgBuff;
	shared_strand _strand;
	BYTE _sendCount;
	bool _waiting;
};

class msg_pump_void : public msg_pump_base
{
	typedef msg_pool_void msg_pool_type;
	typedef msg_pool_void::pump_handler pump_handler;

	friend my_actor;
	friend msg_pool_void;
protected:
	msg_pump_void(const actor_handle& hostActor);
public:
	virtual ~msg_pump_void();
protected:
	void receiver();
	void receive_msg_post();
	void receive_msg();
	bool read_msg();
	void connect(const pump_handler& pumpHandler);
	void clear();
	void close();
	bool isDisconnected();
protected:
	std::weak_ptr<msg_pump_void> _weakThis;
	pump_handler _pumpHandler;
	shared_strand _strand;
	BYTE _pumpCount;
	bool _waiting;
	bool _hasMsg;
	bool _checkDis;
};

template <>
class msg_pool<void, void, void, void> : public msg_pool_void
{
	friend my_actor;
private:
	typedef std::shared_ptr<msg_pool> handle;

	msg_pool(shared_strand strand)
		:msg_pool_void(strand)
	{

	}

	static handle make(shared_strand strand, size_t fixedSize)
	{
		handle res(new msg_pool(strand));
		res->_weakThis = res;
		return res;
	}
};

template <>
class msg_pump<void, void, void, void> : public msg_pump_void
{
	friend my_actor;
public:
	typedef msg_pump* handle;
private:
	msg_pump(const actor_handle& hostActor)
		:msg_pump_void(hostActor)
	{

	}

	static std::shared_ptr<msg_pump> make(const actor_handle& hostActor)
	{
		std::shared_ptr<msg_pump> res(new msg_pump(hostActor));
		res->_weakThis = res;
		return res;
	}
};

template <typename T0, typename T1, typename T2, typename T3>
class post_actor_msg
{
	typedef msg_pool<T0, T1, T2, T3> msg_pool_type;
public:
	post_actor_msg(){}
	post_actor_msg(const std::shared_ptr<msg_pool_type>& msgPool)
		:_msgPool(msgPool){}
public:
	template <typename PT0, typename PT1, typename PT2, typename PT3>
	void operator()(const PT0& p0, const PT1& p1, const PT2& p2, const PT3& p3) const
	{
		_msgPool->push_msg(std::move(msg_param<PT0, PT1, PT2, PT3>(p0, p1, p2, p3)));
	}

	template <typename PT0, typename PT1, typename PT2>
	void operator()(const PT0& p0, const PT1& p1, const PT2& p2) const
	{
		_msgPool->push_msg(std::move(msg_param<PT0, PT1, PT2>(p0, p1, p2)));
	}

	template <typename PT0, typename PT1>
	void operator()(const PT0& p0, const PT1& p1) const
	{
		_msgPool->push_msg(std::move(msg_param<PT0, PT1>(p0, p1)));
	}

	template <typename PT0>
	void operator()(const PT0& p0) const
	{
		_msgPool->push_msg(std::move(msg_param<PT0>(p0)));
	}

	void operator()() const
	{
		_msgPool->push_msg();
	}

	bool empty() const
	{
		return !_msgPool;
	}

	void clear()
	{
		_msgPool.reset();
	}

	operator bool() const
	{
		return !empty();
	}
private:
	std::shared_ptr<msg_pool_type> _msgPool;
};
//////////////////////////////////////////////////////////////////////////

class trig_once_base
{
protected:
	DEBUG_OPERATION(trig_once_base() :_pIsTrig(new boost::atomic<bool>(false)){})
public:
	virtual ~trig_once_base(){};
protected:
	template <typename DST /*ref_ex*/, typename SRC /*msg_param*/>
	void _trig_handler(DST& dstRef, SRC&& src) const
	{
#ifdef _DEBUG
		if (!_pIsTrig->exchange(true))
		{
			assert(_hostActor);
			_hostActor->_trig_handler(dstRef, std::move(src));
		}
		else
		{
			assert(false);
		}
#else
		assert(_hostActor);
		_hostActor->_trig_handler(dstRef, std::move(src));
#endif
	}

	void trig_handler() const;
protected:
	actor_handle _hostActor;
	DEBUG_OPERATION(std::shared_ptr<boost::atomic<bool> > _pIsTrig);
};

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class trig_once_notifer: public trig_once_base
{
	typedef ref_ex<T0, T1, T2, T3> ref_type;

	friend my_actor;
public:
	trig_once_notifer():_dstRef(0) {};
private:
	trig_once_notifer(const actor_handle& hostActor, ref_type* dstRef)
		:_dstRef(dstRef) {_hostActor = hostActor;}
public:
	template <typename PT0, typename PT1, typename PT2, typename PT3>
	void operator()(const PT0& p0, const PT1& p1, const PT2& p2, const PT3& p3) const
	{
		_trig_handler(*_dstRef, std::move(msg_param<PT0, PT1, PT2, PT3>(p0, p1, p2, p3)));
	}

	template <typename PT0, typename PT1, typename PT2>
	void operator()(const PT0& p0, const PT1& p1, const PT2& p2) const
	{
		_trig_handler(*_dstRef, std::move(msg_param<PT0, PT1, PT2>(p0, p1, p2)));
	}

	template <typename PT0, typename PT1>
	void operator()(const PT0& p0, const PT1& p1) const
	{
		_trig_handler(*_dstRef, std::move(msg_param<PT0, PT1>(p0, p1)));
	}

	template <typename PT0>
	void operator()(const PT0& p0) const
	{
		_trig_handler(*_dstRef, std::move(msg_param<PT0>(p0)));
	}

	void operator()() const
	{
		trig_handler();
	}
private:
	ref_type* _dstRef;
};

//////////////////////////////////////////////////////////////////////////
/*!
@brief 子Actor句柄，不可拷贝
*/
class child_actor_handle 
{
public:
	typedef std::shared_ptr<child_actor_handle> ptr;
private:
	friend my_actor;
	/*!
	@brief 子Actor句柄参数，child_actor_handle内使用
	*/
	struct child_actor_param
	{
#ifdef _DEBUG
		child_actor_param();
		child_actor_param(child_actor_param& s);
		~child_actor_param();
		child_actor_param& operator =(child_actor_param& s);
		bool _isCopy;
#endif
		actor_handle _actor;///<本Actor
		list<actor_handle>::iterator _actorIt;///<保存在父Actor集合中的节点
	};
private:
	child_actor_handle(child_actor_handle&);
	child_actor_handle& operator =(child_actor_handle&);
public:
	child_actor_handle();
	child_actor_handle(child_actor_param& s);
	~child_actor_handle();
	child_actor_handle& operator =(child_actor_param& s);
	actor_handle get_actor();
	static ptr make_ptr();
	bool empty();
private:
	actor_handle peel();
	void* operator new(size_t s);
public:
	void operator delete(void* p);
private:
	DEBUG_OPERATION(list<std::function<void ()> >::iterator _qh);
	bool _norQuit;///<是否正常退出
	bool _quited;///<检测是否已经关闭
	child_actor_param _param;
};
//////////////////////////////////////////////////////////////////////////

class my_actor
{
	struct suspend_resume_option 
	{
		bool _isSuspend;
		std::function<void ()> _h;
	};

	struct msg_pool_status 
	{
		struct pck_base 
		{
			pck_base(shared_strand strand)
			:_strand(strand), _amutex(strand), _isHead(true){}

			virtual ~pck_base(){};

			virtual void close() = 0;

			void lock(my_actor* self)
			{
				_amutex.lock(self);
			}

			void unlock(my_actor* self)
			{
				_amutex.unlock(self);
			}

			shared_strand _strand;
			actor_mutex _amutex;
			bool _isHead;
		};

		template <typename T0, typename T1, typename T2, typename T3>
		struct pck: public pck_base
		{
			pck(shared_strand strand)
			:pck_base(strand){}

			void close()
			{
				if (_msgPump)
				{
					_msgPump->close();
				}
			}

			std::shared_ptr<msg_pool<T0, T1, T2, T3> > _msgPool;
			std::shared_ptr<msg_pump<T0, T1, T2, T3> > _msgPump;
			std::shared_ptr<pck> _next;
		};

		void clear()
		{
			for (int i = 0; i < 5; i++)
			{
				for (auto it = _msgPumpList[i].begin(); it != _msgPumpList[i].end(); it++)
				{
					(*it)->close();
				}
			}
		}

		list<std::shared_ptr<pck_base> > _msgPumpList[5];
	};

	struct timer_pck;
	class boost_actor_run;
	friend boost_actor_run;
	friend child_actor_handle;
	friend msg_pump_base;
	friend actor_msg_handle_base;
	friend trig_once_base;
public:
	/*!
	@brief 在{}一定范围内锁定当前Actor不被强制退出
	*/
	class quit_guard
	{
	public:
		quit_guard(my_actor* self)
		:_self(self)
		{
			_self->lock_quit();
		}

		~quit_guard()
		{
			_self->unlock_quit();
		}
	private:
		quit_guard(const quit_guard&){};
		void operator=(const quit_guard&){};
		my_actor* _self;
	};

	/*!
	@brief Actor被强制退出的异常类型
	*/
	struct force_quit_exception { };

	/*!
	@brief 消息泵被断开
	*/
	struct pump_disconnected_exception { };

	/*!
	@brief Actor入口函数体
	*/
	typedef std::function<void (my_actor*)> main_func;
private:
	my_actor();
	my_actor(const my_actor&);
	my_actor& operator =(const my_actor&);
public:
	~my_actor();
public:
	/*!
	@brief 创建一个Actor
	@param actorStrand Actor所依赖的strand
	@param mainFunc Actor执行入口
	@param stackSize Actor栈大小，默认64k字节，必须是4k的整数倍，最小4k，最大1M
	*/
	static actor_handle create(shared_strand actorStrand, const main_func& mainFunc, size_t stackSize = DEFAULT_STACKSIZE);

	/*!
	@brief 同上，带完成Actor后的回调通知
	@param cb Actor完成后的触发函数，false强制结束的，true正常结束
	*/
	static actor_handle create(shared_strand actorStrand, const main_func& mainFunc, 
		const std::function<void (bool)>& cb, size_t stackSize = DEFAULT_STACKSIZE);

	/*!
	@brief 异步创建一个Actor，创建成功后通过回调函数通知
	*/
	static void async_create(shared_strand actorStrand, const main_func& mainFunc, 
		const std::function<void (actor_handle)>& ch, size_t stackSize = DEFAULT_STACKSIZE);

	/*!
	@brief 同上，带完成Actor后的回调通知
	*/
	static void async_create(shared_strand actorStrand, const main_func& mainFunc, 
		const std::function<void (actor_handle)>& ch, const std::function<void (bool)>& cb, size_t stackSize = DEFAULT_STACKSIZE);

	/*!
	@brief 启用堆栈内存池
	*/
	static void enable_stack_pool();

	/*!
	@brief 禁用创建Actor时自动构造定时器
	*/
	static void disable_auto_make_timer();
public:
	/*!
	@brief 创建一个子Actor，父Actor终止时，子Actor也终止（在子Actor都完全退出后，父Actor才结束）
	@param actorStrand 子Actor依赖的strand
	@param mainFunc 子Actor入口函数
	@param stackSize Actor栈大小，4k的整数倍（最大1MB）
	@return 子Actor句柄，使用 child_actor_handle 接收返回值
	*/
	child_actor_handle::child_actor_param create_child_actor(shared_strand actorStrand, const main_func& mainFunc, size_t stackSize = DEFAULT_STACKSIZE);
	child_actor_handle::child_actor_param create_child_actor(const main_func& mainFunc, size_t stackSize = DEFAULT_STACKSIZE);

	/*!
	@brief 开始运行子Actor，只能调用一次
	*/
	void child_actor_run(child_actor_handle& actorHandle);
	void child_actor_run(const list<child_actor_handle::ptr>& actorHandles);

	/*!
	@brief 强制终止一个子Actor
	*/
	__yield_interrupt bool child_actor_force_quit(child_actor_handle& actorHandle);
	__yield_interrupt void child_actors_force_quit(const list<child_actor_handle::ptr>& actorHandles);

	/*!
	@brief 等待一个子Actor完成后返回
	@return 正常退出的返回true，否则false
	*/
	__yield_interrupt bool child_actor_wait_quit(child_actor_handle& actorHandle);

	/*!
	@brief 等待一组子Actor完成后返回
	@return 都正常退出的返回true，否则false
	*/
	__yield_interrupt void child_actors_wait_quit(const list<child_actor_handle::ptr>& actorHandles);

	/*!
	@brief 挂起子Actor
	*/
	__yield_interrupt void child_actor_suspend(child_actor_handle& actorHandle);
	
	/*!
	@brief 挂起一组子Actor
	*/
	__yield_interrupt void child_actors_suspend(const list<child_actor_handle::ptr>& actorHandles);

	/*!
	@brief 恢复子Actor
	*/
	__yield_interrupt void child_actor_resume(child_actor_handle& actorHandle);
	
	/*!
	@brief 恢复一组子Actor
	*/
	__yield_interrupt void child_actors_resume(const list<child_actor_handle::ptr>& actorHandles);

	/*!
	@brief 创建另一个Actor，Actor执行完成后返回
	*/
	__yield_interrupt bool run_child_actor_complete(shared_strand actorStrand, const main_func& h, size_t stackSize = DEFAULT_STACKSIZE);
	__yield_interrupt bool run_child_actor_complete(const main_func& h, size_t stackSize = DEFAULT_STACKSIZE);

	/*!
	@brief 延时等待，Actor内部禁止使用操作系统API Sleep()
	@param ms 等待毫秒数，等于0时暂时放弃Actor执行，直到下次被调度器触发
	*/
	__yield_interrupt void sleep(int ms);

	/*!
	@brief 调用disable_auto_make_timer后，使用这个打开当前Actor定时器
	*/
	void open_timer();

	/*!
	@brief 关闭内部定时器
	*/
	void close_timer();

	/*!
	@brief 获取父Actor
	*/
	actor_handle parent_actor();

	/*!
	@brief 获取子Actor
	*/
	const list<actor_handle>& child_actors();
public:
	typedef list<std::function<void ()> >::iterator quit_iterator;

	/*!
	@brief 注册一个资源释放函数，在强制退出Actor时调用
	*/
	quit_iterator regist_quit_handler(const std::function<void ()>& quitHandler);

	/*!
	@brief 注销资源释放函数
	*/
	void cancel_quit_handler(quit_iterator qh);
public:
	/*!
	@brief 使用内部定时器延时触发某个函数，在触发完成之前不能多次调用
	@param ms 触发延时(毫秒)
	@param h 触发函数
	*/
	template <typename H>
	void delay_trig(int ms, const H& h)
	{
		assert_enter();
		if (ms > 0)
		{
			assert(_timer);
			time_out(ms, h);
		} 
		else if (0 == ms)
		{
			_strand->post(h);
		}
		else
		{
			assert(false);
		}
	}

	/*!
	@brief 取消内部定时器触发
	*/
	void cancel_delay_trig();
public:
	/*!
	@brief 发送一个异步函数到shared_strand中执行，完成后返回
	*/
	template <typename H>
	__yield_interrupt void send(shared_strand exeStrand, const H& h)
	{
		assert_enter();
		if (exeStrand != _strand)
		{
			actor_handle shared_this = shared_from_this();
			exeStrand->asyncInvokeVoid(h, [shared_this](){shared_this->trig_handler(); });
			push_yield();
			return;
		}
		h();
	}

	template <typename T0, typename H>
	__yield_interrupt T0 send(shared_strand exeStrand, const H& h)
	{
		assert_enter();
		if (exeStrand != _strand)
		{
			T0 r0;
			ref_ex<T0> dstRef(r0);
			actor_handle shared_this = shared_from_this();
			exeStrand->asyncInvoke(h, [shared_this, &dstRef](const T0& p0){shared_this->_trig_handler(dstRef, std::move(msg_param<T0>(p0))); });
			push_yield();
			return r0;
		} 
		return h();
	}

	template <typename H>
	__yield_interrupt void async_send(shared_strand exeStrand, const H& h)
	{
		assert_enter();
		actor_handle shared_this = shared_from_this();
		exeStrand->asyncInvokeVoid(h, [shared_this](){shared_this->trig_handler(); });
		push_yield();
	}

	template <typename T0, typename H>
	__yield_interrupt T0 async_send(shared_strand exeStrand, const H& h)
	{
		assert_enter();
		T0 r0;
		ref_ex<T0> dstRef(r0);
		actor_handle shared_this = shared_from_this();
		exeStrand->asyncInvoke(h, [shared_this, &dstRef](const T0& p0){shared_this->_trig_handler(dstRef, std::move(msg_param<T0>(p0))); });
		push_yield();
		return r0;
	}

	/*!
	@brief 调用一个异步函数，异步回调完成后返回
	*/
	template <typename H>
	__yield_interrupt void trig(const H& h)
	{
		assert_enter();
		h(trig_once_notifer<>(shared_from_this(), NULL));
		push_yield();
	}

	template <typename T0, typename H>
	__yield_interrupt void trig(__out T0& r0, const H& h)
	{
		assert_enter();
		ref_ex<T0> dstRef(r0);
		h(trig_once_notifer<T0>(shared_from_this(), &dstRef));
		push_yield();
	}

	template <typename T0, typename H>
	__yield_interrupt T0 trig(const H& h)
	{
		T0 r0;
		trig(r0, h);
		return r0;
	}

	template <typename T0, typename T1, typename H>
	__yield_interrupt void trig(__out T0& r0, __out T1& r1, const H& h)
	{
		assert_enter();
		ref_ex<T0, T1> dstRef(r0, r1);
		h(trig_once_notifer<T0, T1>(shared_from_this(), &dstRef));
		push_yield();
	}

	template <typename T0, typename T1, typename T2, typename H>
	__yield_interrupt void trig(__out T0& r0, __out T1& r1, __out T2& r2, const H& h)
	{
		assert_enter();
		ref_ex<T0, T1, T2> dstRef(r0, r1, r2);
		h(trig_once_notifer<T0, T1, T2>(shared_from_this(), &dstRef));
		push_yield();
	}

	template <typename T0, typename T1, typename T2, typename T3, typename H>
	__yield_interrupt void trig(__out T0& r0, __out T1& r1, __out T2& r2, __out T3& r3, const H& h)
	{
		assert_enter();
		ref_ex<T0, T1, T2, T3> dstRef(r0, r1, r2, r3);
		h(trig_once_notifer<T0, T1, T2, T3>(shared_from_this(), &dstRef));
		push_yield();
	}
private:
	void trig_handler();

	template <typename DST /*ref_ex*/, typename SRC /*msg_param*/>
	void _trig_handler(DST& dstRef, SRC&& src)
	{
		if (_strand->running_in_this_thread())
		{
			if (!_quited)
			{
				src.move_out(dstRef);
				trig_handler();
			}
		} 
		else
		{
			actor_handle shared_this = shared_from_this();
			_strand->post([=, &dstRef]()
			{
				if (!shared_this->_quited)
				{
					((SRC&)src).move_out(dstRef);
					shared_this->pull_yield();
				}
			});
		}
	}
public:
	/*!
	@brief 创建一个消息通知函数
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	actor_msg_notifer<T0, T1, T2, T3> make_msg_notifer(actor_msg_handle<T0, T1, T2, T3>& amh)
	{
		return amh.make_notifer(shared_from_this());
	}

	template <typename T0, typename T1, typename T2>
	actor_msg_notifer<T0, T1, T2> make_msg_notifer(actor_msg_handle<T0, T1, T2>& amh)
	{
		return amh.make_notifer(shared_from_this());
	}

	template <typename T0, typename T1>
	actor_msg_notifer<T0, T1> make_msg_notifer(actor_msg_handle<T0, T1>& amh)
	{
		return amh.make_notifer(shared_from_this());
	}

	template <typename T0>
	actor_msg_notifer<T0> make_msg_notifer(actor_msg_handle<T0>& amh)
	{
		return amh.make_notifer(shared_from_this());
	}

	actor_msg_notifer<> make_msg_notifer(actor_msg_handle<>& amh);

	/*!
	@brief 关闭消息通知句柄
	*/
	void close_msg_notifer(actor_msg_handle_base& amh);
public:
	/*!
	@brief 使用内部定时器延时触发某个句柄，之前必须调用过make_trig_notifer
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	void delay_trig(int ms, actor_trig_handle<T0, T1, T2, T3>& ath, const T0& p0, const T1& p1, const T2& p2, const T3& p3)
	{
		assert_enter();
		assert(ath._hostActor && ath._hostActor->self_id() == self_id());
		assert(ath._closed && !(*ath._closed));
		auto& closed_ = ath._closed;
		delay_trig(ms, [=, &ath]()
		{
			if (!*(closed_))
			{
				ath.push_msg(ref_ex<T0, T1, T2, T3>((T0&)p0, (T1&)p1, (T2&)p2, (T3&)p3));
			}
		});
	}

	template <typename T0, typename T1, typename T2>
	void delay_trig(int ms, actor_trig_handle<T0, T1, T2>& ath, const T0& p0, const T1& p1, const T2& p2)
	{
		assert_enter();
		assert(ath._hostActor && ath._hostActor->self_id() == self_id());
		assert(ath._closed && !(*ath._closed));
		auto& closed_ = ath._closed;
		delay_trig(ms, [=, &ath]()
		{
			if (!*(closed_))
			{
				ath.push_msg(ref_ex<T0, T1, T2>((T0&)p0, (T1&)p1, (T2&)p2));
			}
		});
	}

	template <typename T0, typename T1>
	void delay_trig(int ms, actor_trig_handle<T0, T1>& ath, const T0& p0, const T1& p1)
	{
		assert_enter();
		assert(ath._hostActor && ath._hostActor->self_id() == self_id());
		assert(ath._closed && !(*ath._closed));
		auto& closed_ = ath._closed;
		delay_trig(ms, [=, &ath]()
		{
			if (!*(closed_))
			{
				ath.push_msg(ref_ex<T0, T1>((T0&)p0, (T1&)p1));
			}
		});
	}

	template <typename T0>
	void delay_trig(int ms, actor_trig_handle<T0>& ath, const T0& p0)
	{
		assert_enter();
		assert(ath._hostActor && ath._hostActor->self_id() == self_id());
		assert(ath._closed && !(*ath._closed));
		auto& closed_ = ath._closed;
		delay_trig(ms, [=, &ath]()
		{
			if (!*(closed_))
			{
				ath.push_msg(ref_ex<T0>((T0&)p0));
			}
		});
	}
private:
	template <typename AMH, typename DST>
	bool _timed_wait_msg(AMH& amh, DST& dstRef, int tm)
	{
		assert(amh._hostActor && amh._hostActor->self_id() == self_id());
		if (!amh.read_msg(dstRef))
		{
			bool timeout = false;
			if (tm >= 0)
			{
				delay_trig(tm, [this, &timeout]()
				{
					timeout = true;
					run_one();
				});
			}
			push_yield();
			if (!timeout)
			{
				if (tm >= 0)
				{
					cancel_delay_trig();
				}
				return true;
			}
			amh._dstRef = NULL;
			amh._waiting = false;
			return false;
		}
		return true;
	}
public:
	/*!
	@brief 从消息句柄中提取消息
	@param tm 超时时间
	@return 超时完成返回false，成功提取消息返回true
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<T0, T1, T2, T3>& amh, T0& r0, T1& r1, T2& r2, T3& r3)
	{
		assert_enter();
		assert(amh._closed && !(*amh._closed));
		ref_ex<T0, T1, T2, T3> dstRef(r0, r1, r2, r3);
		return _timed_wait_msg(amh, dstRef, tm);
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<T0, T1, T2>& amh, T0& r0, T1& r1, T2& r2)
	{
		assert_enter();
		assert(amh._closed && !(*amh._closed));
		ref_ex<T0, T1, T2> dstRef(r0, r1, r2);
		return _timed_wait_msg(amh, dstRef, tm);
	}

	template <typename T0, typename T1>
	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<T0, T1>& amh, T0& r0, T1& r1)
	{
		assert_enter();
		assert(amh._closed && !(*amh._closed));
		ref_ex<T0, T1> dstRef(r0, r1);
		return _timed_wait_msg(amh, dstRef, tm);
	}

	template <typename T0>
	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<T0>& amh, T0& r0)
	{
		assert_enter();
		assert(amh._closed && !(*amh._closed));
		ref_ex<T0> dstRef(r0);
		return _timed_wait_msg(amh, dstRef, tm);
	}

	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<>& amh);

	/*!
	@brief 从消息句柄中提取消息
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt void wait_msg(actor_msg_handle<T0, T1, T2, T3>& amh, T0& r0, T1& r1, T2& r2, T3& r3)
	{
		timed_wait_msg(-1, amh, r0, r1, r2, r3);
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt void wait_msg(actor_msg_handle<T0, T1, T2>& amh, T0& r0, T1& r1, T2& r2)
	{
		timed_wait_msg(-1, amh, r0, r1, r2);
	}

	template <typename T0, typename T1>
	__yield_interrupt void wait_msg(actor_msg_handle<T0, T1>& amh, T0& r0, T1& r1)
	{
		timed_wait_msg(-1, amh, r0, r1);
	}

	template <typename T0>
	__yield_interrupt void wait_msg(actor_msg_handle<T0>& amh, T0& r0)
	{
		timed_wait_msg(-1, amh, r0);
	}

	template <typename T0>
	__yield_interrupt T0 wait_msg(actor_msg_handle<T0>& amh)
	{
		T0 r0;
		timed_wait_msg(-1, amh, r0);
		return r0;
	}

	__yield_interrupt void wait_msg(actor_msg_handle<>& amh);
public:
	/*!
	@brief 创建一个消息触发函数，只有一次触发有效
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	actor_trig_notifer<T0, T1, T2, T3> make_trig_notifer(actor_trig_handle<T0, T1, T2, T3>& ath)
	{
		return ath.make_notifer(shared_from_this());
	}

	template <typename T0, typename T1, typename T2>
	actor_trig_notifer<T0, T1, T2> make_trig_notifer(actor_trig_handle<T0, T1, T2>& ath)
	{
		return ath.make_notifer(shared_from_this());
	}

	template <typename T0, typename T1>
	actor_trig_notifer<T0, T1> make_trig_notifer(actor_trig_handle<T0, T1>& ath)
	{
		return ath.make_notifer(shared_from_this());
	}

	template <typename T0>
	actor_trig_notifer<T0> make_trig_notifer(actor_trig_handle<T0>& ath)
	{
		return ath.make_notifer(shared_from_this());
	}

	actor_trig_notifer<> make_trig_notifer(actor_trig_handle<>& ath);

	/*!
	@brief 关闭消息触发句柄
	*/
	void close_trig_notifer(actor_msg_handle_base& ath);
public:
	/*!
	@brief 从触发句柄中提取消息
	@param tm 超时时间
	@return 超时完成返回false，成功提取消息返回true
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<T0, T1, T2, T3>& ath, T0& r0, T1& r1, T2& r2, T3& r3)
	{
		assert_enter();
		assert(ath._closed && !(*ath._closed));
		ref_ex<T0, T1, T2, T3> dstRef(r0, r1, r2, r3);
		return _timed_wait_msg(ath, dstRef, tm);
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<T0, T1, T2>& ath, T0& r0, T1& r1, T2& r2)
	{
		assert_enter();
		assert(ath._closed && !(*ath._closed));
		ref_ex<T0, T1, T2> dstRef(r0, r1, r2);
		return _timed_wait_msg(ath, dstRef, tm);
	}

	template <typename T0, typename T1>
	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<T0, T1>& ath, T0& r0, T1& r1)
	{
		assert_enter();
		assert(ath._closed && !(*ath._closed));
		ref_ex<T0, T1> dstRef(r0, r1);
		return _timed_wait_msg(ath, dstRef, tm);
	}

	template <typename T0>
	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<T0>& ath, T0& r0)
	{
		assert_enter();
		assert(ath._closed && !(*ath._closed));
		ref_ex<T0> dstRef(r0);
		return _timed_wait_msg(ath, dstRef, tm);
	}

	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<>& ath);

	/*!
	@brief 从触发句柄中提取消息
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt void wait_trig(actor_trig_handle<T0, T1, T2, T3>& ath, T0& r0, T1& r1, T2& r2, T3& r3)
	{
		timed_wait_trig(-1, ath, r0, r1, r2, r3);
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt void wait_trig(actor_trig_handle<T0, T1, T2>& ath, T0& r0, T1& r1, T2& r2)
	{
		timed_wait_trig(-1, ath, r0, r1, r2);
	}

	template <typename T0, typename T1>
	__yield_interrupt void wait_trig(actor_trig_handle<T0, T1>& ath, T0& r0, T1& r1)
	{
		timed_wait_trig(-1, ath, r0, r1);
	}

	template <typename T0>
	__yield_interrupt void wait_trig(actor_trig_handle<T0>& ath, T0& r0)
	{
		timed_wait_trig(-1, ath, r0);
	}

	template <typename T0>
	__yield_interrupt T0 wait_trig(actor_trig_handle<T0>& ath)
	{
		T0 r0;
		timed_wait_trig(-1, ath, r0);
		return r0;
	}

	__yield_interrupt void wait_trig(actor_trig_handle<>& ath);
private:
	template <typename T0, typename T1, typename T2, typename T3>
	std::shared_ptr<msg_pool_status::pck<T0, T1, T2, T3> > msg_pool_pck(bool make = true)
	{
		typedef msg_pool_status::pck<T0, T1, T2, T3> pck_type;

		auto& msgPumpList = _msgPoolStatus._msgPumpList[func_type<T0, T1, T2, T2>::number];
		if (0 == func_type<T0, T1, T2, T2>::number)
		{
			if (!msgPumpList.empty())
			{
				return std::static_pointer_cast<pck_type>(msgPumpList.front());
			}
		}
		else
		{
			for (auto it = msgPumpList.begin(); it != msgPumpList.end(); it++)
			{
				auto pool = std::dynamic_pointer_cast<pck_type>(*it);
				if (pool)
				{
					return pool;
				}
			}
		}
		if (make)
		{
			std::shared_ptr<pck_type> newPck(new pck_type(_strand));
			msgPumpList.push_back(newPck);
			return newPck;
		}
		return std::shared_ptr<pck_type>();
	}

	template <typename T0, typename T1, typename T2, typename T3>
	void clear_msg_list(const std::shared_ptr<msg_pool_status::pck<T0, T1, T2, T3>>& msgPck)
	{
		check_stack();
		if (msgPck->_next)
		{
			msgPck->_next->lock(this);
			clear_msg_list<T0, T1, T2, T3>(msgPck->_next);
			msgPck->_next->unlock(this);
		}
		else
		{
			if (msgPck->_msgPool)
			{
				auto& msgPool_ = msgPck->_msgPool;
				send(msgPool_->_strand, [&msgPool_]()
				{
					msgPool_->disconnect();
				});
			}
			if (msgPck->_msgPump)
			{
				auto& msgPump_ = msgPck->_msgPump;
				send(msgPump_->_strand, [&msgPump_]()
				{
					msgPump_->clear();
				});
			}
		}
		msgPck->_msgPool.reset();
	}

	template <typename T0, typename T1, typename T2, typename T3>
	void update_msg_list(const std::shared_ptr<msg_pool_status::pck<T0, T1, T2, T3>>& msgPck, const std::shared_ptr<msg_pool<T0, T1, T2, T3>>& newPool)
	{
		typedef typename msg_pool<T0, T1, T2, T3>::pump_handler pump_handler;

		check_stack();
		if (msgPck->_next)
		{
			msgPck->_next->lock(this);
			update_msg_list<T0, T1, T2, T3>(msgPck->_next, newPool);
			msgPck->_next->unlock(this);
		}
		else
		{
			if (msgPck->_msgPool)
			{
				auto& msgPool_ = msgPck->_msgPool;
				send(msgPool_->_strand, [&msgPool_]()
				{
					msgPool_->disconnect();
				});
			}
			if (msgPck->_msgPump)
			{
				auto& msgPump_ = msgPck->_msgPump;
				if (newPool)
				{
					auto ph = send<pump_handler>(newPool->_strand, [&newPool, &msgPump_]()->pump_handler
					{
						return newPool->connect_pump(msgPump_);
					});
					send(msgPump_->_strand, [&msgPump_, &ph]()
					{
						if (msgPump_->_hostActor && !msgPump_->_hostActor->is_quited())
						{
							msgPump_->connect(ph);
						}
					});
				}
				else
				{
					send(msgPump_->_strand, [&msgPump_]()
					{
						msgPump_->clear();
					});
				}
			}
		}
		msgPck->_msgPool = newPool;
	}
private:
	/*!
	@brief 把本Actor内消息由伙伴Actor代理处理
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt void msg_agent_to(const actor_handle& childActor)
	{
		typedef std::shared_ptr<msg_pool_status::pck<T0, T1, T2, T3>> pck_type;

		assert_enter();
		assert(childActor);
		if (childActor->parent_actor() && childActor->parent_actor()->self_id() == self_id())
		{
			auto msgPck = msg_pool_pck<T0, T1, T2, T3>();
			quit_guard qg(this);
			msgPck->lock(this);
			if (msgPck->_next)
			{
				msgPck->_next->lock(this);
				clear_msg_list<T0, T1, T2, T3>(msgPck->_next);
				msgPck->_next->unlock(this);
			}
			auto childPck = send<pck_type>(childActor->self_strand(), [&childActor]()->pck_type
			{
				return childActor->msg_pool_pck<T0, T1, T2, T3>();
			});
			msgPck->_next = childPck;
			childPck->lock(this);
			childPck->_isHead = false;
			auto& msgPool_ = msgPck->_msgPool;
			update_msg_list<T0, T1, T2, T3>(childPck, msgPool_);
			childPck->unlock(this);
			msgPck->unlock(this);
			return;
		}
		assert(false);
	}
public:
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt void msg_agent_to(child_actor_handle& childActor)
	{
		msg_agent_to<T0, T1, T2, T3>(childActor.get_actor());
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt void msg_agent_to(child_actor_handle& childActor)
	{
		msg_agent_to<T0, T1, T2, void>(childActor.get_actor());
	}

	template <typename T0, typename T1>
	__yield_interrupt void msg_agent_to(child_actor_handle& childActor)
	{
		msg_agent_to<T0, T1, void, void>(childActor.get_actor());
	}

	template <typename T0>
	__yield_interrupt void msg_agent_to(child_actor_handle& childActor)
	{
		msg_agent_to<T0, void, void, void>(childActor.get_actor());
	}

	__yield_interrupt void msg_agent_to(child_actor_handle& childActor);

public:
	/*!
	@brief 把消息指定到一个特定Actor函数体去处理
	@return 返回处理该消息的子Actor句柄
	*/
	template <typename T0, typename T1, typename T2, typename T3, typename Handler>
	child_actor_handle::child_actor_param msg_agent_to_actor(bool autoRun, const Handler& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		child_actor_handle::child_actor_param childActor = create_child_actor([agentActor](my_actor* self)
		{
			agentActor(self, self->connect_msg_pump<T0, T1, T2, T3>());
		}, stackSize);
		msg_agent_to<T0, T1, T2, T3>(childActor._actor);
		if (autoRun)
		{
			childActor._actor->notify_run();
		}
		return childActor;
	}

	template <typename T0, typename T1, typename T2, typename Handler>
	child_actor_handle::child_actor_param msg_agent_to_actor(bool autoRun, const Handler& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<T0, T1, T2, void>(autoRun, agentActor, stackSize);
	}

	template <typename T0, typename T1, typename Handler>
	child_actor_handle::child_actor_param msg_agent_to_actor(bool autoRun, const Handler& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<T0, T1, void, void>(autoRun, agentActor, stackSize);
	}

	template <typename T0, typename Handler>
	child_actor_handle::child_actor_param msg_agent_to_actor(bool autoRun, const Handler& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<T0, void, void, void>(autoRun, agentActor, stackSize);
	}

	template <typename Handler>
	child_actor_handle::child_actor_param msg_agent_to_actor(bool autoRun, const Handler& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<void, void, void, void>(autoRun, agentActor, stackSize);
	}
public:
	/*!
	@brief 断开伙伴代理该消息
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt void msg_agent_off()
	{
		assert_enter();
		auto msgPck = msg_pool_pck<T0, T1, T2, T3>();
		if (msgPck)
		{
			quit_guard qg(this);
			msgPck->lock(this);
			if (msgPck->_next)
			{
				msgPck->_next->lock(this);
				clear_msg_list<T0, T1, T2, T3>(msgPck->_next);
				msgPck->_next->_isHead = true;
				msgPck->_next->unlock(this);
				msgPck->_next.reset();
			}
			msgPck->unlock(this);
		}
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt void msg_agent_off()
	{
		msg_agent_off<T0, T1, T2, void>();
	}

	template <typename T0, typename T1>
	__yield_interrupt void msg_agent_off()
	{
		msg_agent_off<T0, T1, void, void>();
	}

	template <typename T0>
	__yield_interrupt void msg_agent_off()
	{
		msg_agent_off<T0, void, void, void>();
	}

	__yield_interrupt void msg_agent_off();
public:
	/*!
	@brief 连接消息通知到一个伙伴Actor，该Actor必须是子Actor或没有父Actor
	@param makeNew false 如果存在返回之前，否则创建新的通知；true 强制创建新的通知，之前的将失效，且断开与buddyActor的关联
	@param fixedSize 消息队列内存池长度
	@warning 如果 makeNew = false 且该节点为父的代理，将创建失败
	@return 消息通知函数
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt post_actor_msg<T0, T1, T2, T3> connect_msg_notifer_to(const actor_handle& buddyActor, bool makeNew = false, size_t fixedSize = 16)
	{
		typedef msg_pool<T0, T1, T2, T3> pool_type;
		typedef typename pool_type::pump_handler pump_handler;
		typedef std::shared_ptr<msg_pool_status::pck<T0, T1, T2, T3>> pck_type;

		assert_enter();
		if (!(buddyActor && (!buddyActor->parent_actor() || buddyActor->parent_actor()->self_id() == self_id())))
		{
			assert(false);
			return post_actor_msg<T0, T1, T2, T3>();
		}
#ifdef _DEBUG
		{
			auto pa = parent_actor();
			while (pa)
			{
				assert(pa->self_id() != buddyActor->self_id());
				pa = pa->parent_actor();
			}
		}
#endif
		auto msgPck = msg_pool_pck<T0, T1, T2, T3>();
		quit_guard qg(this);
		msgPck->lock(this);
		auto childPck = send<pck_type>(buddyActor->self_strand(), [&buddyActor]()->pck_type
		{
			return buddyActor->msg_pool_pck<T0, T1, T2, T3>();
		});
		if (makeNew)
		{
			auto newPool = pool_type::make(buddyActor->self_strand(), fixedSize);
			childPck->lock(this);
			childPck->_isHead = true;
			update_msg_list<T0, T1, T2, T3>(childPck, newPool);
			childPck->unlock(this);
			if (msgPck->_next == childPck)
			{
				msgPck->_next.reset();
				if (msgPck->_msgPump)
				{
					auto& msgPump_ = msgPck->_msgPump;
					if (msgPck->_msgPool)
					{
						auto& msgPool_ = msgPck->_msgPool;
						msgPump_->connect(this->send<pump_handler>(msgPool_->_strand, [&msgPool_, &msgPump_]()->pump_handler
						{
							return msgPool_->connect_pump(msgPump_);
						}));
					}
					else
					{
						msgPump_->clear();
					}
				}
			}
			msgPck->unlock(this);
			return post_actor_msg<T0, T1, T2, T3>(newPool);
		}
		childPck->lock(this);
		if (childPck->_isHead)
		{
			assert(msgPck->_next != childPck);
			if (childPck->_msgPool)
			{
				auto childPool = childPck->_msgPool;
				update_msg_list<T0, T1, T2, T3>(childPck, childPool);
				childPck->unlock(this);
				msgPck->unlock(this);
				return post_actor_msg<T0, T1, T2, T3>(childPool);
			}
			auto newPool = pool_type::make(buddyActor->self_strand(), fixedSize);
			update_msg_list<T0, T1, T2, T3>(childPck, newPool);
			childPck->unlock(this);
			msgPck->unlock(this);
			return post_actor_msg<T0, T1, T2, T3>(newPool);
		}
		childPck->unlock(this);
		msgPck->unlock(this);
		return post_actor_msg<T0, T1, T2, T3>();
	}

	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt post_actor_msg<T0, T1, T2, T3> connect_msg_notifer_to(child_actor_handle& childActor, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<T0, T1, T2, T3>(childActor.get_actor(), makeNew, fixedSize);
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt post_actor_msg<T0, T1, T2> connect_msg_notifer_to(const actor_handle& buddyActor, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<T0, T1, T2, void>(buddyActor, makeNew, fixedSize);
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt post_actor_msg<T0, T1, T2> connect_msg_notifer_to(child_actor_handle& childActor, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<T0, T1, T2, void>(childActor.get_actor(), makeNew, fixedSize);
	}

	template <typename T0, typename T1>
	__yield_interrupt post_actor_msg<T0, T1> connect_msg_notifer_to(const actor_handle& buddyActor, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<T0, T1, void, void>(buddyActor, makeNew, fixedSize);
	}

	template <typename T0, typename T1>
	__yield_interrupt post_actor_msg<T0, T1> connect_msg_notifer_to(child_actor_handle& childActor, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<T0, T1, void, void>(childActor.get_actor(), makeNew, fixedSize);
	}

	template <typename T0>
	__yield_interrupt post_actor_msg<T0> connect_msg_notifer_to(const actor_handle& buddyActor, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<T0, void, void, void>(buddyActor, makeNew, fixedSize);
	}

	template <typename T0>
	__yield_interrupt post_actor_msg<T0> connect_msg_notifer_to(child_actor_handle& childActor, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<T0, void, void, void>(childActor.get_actor(), makeNew, fixedSize);
	}

	__yield_interrupt post_actor_msg<> connect_msg_notifer_to(const actor_handle& buddyActor, bool makeNew = false);
	__yield_interrupt post_actor_msg<> connect_msg_notifer_to(child_actor_handle& childActor, bool makeNew = false);

	/*!
	@brief 连接消息通知到自己的Actor
	@param makeNew false 如果存在返回之前，否则创建新的通知；true 强制创建新的通知，之前的将失效，且断开与buddyActor的关联
	@param fixedSize 消息队列内存池长度
	@warning 如果该节点为父的代理，那么将创建失败
	@return 消息通知函数
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt post_actor_msg<T0, T1, T2, T3> connect_msg_notifer_to_self(bool makeNew = false, size_t fixedSize = 16)
	{
		typedef msg_pool<T0, T1, T2, T3> pool_type;

		assert_enter();
		auto msgPck = msg_pool_pck<T0, T1, T2, T3>();
		quit_guard qg(this);
		msgPck->lock(this);
		if (msgPck->_isHead)
		{
			if (makeNew || !msgPck->_msgPool)
			{
				auto newPool = pool_type::make(self_strand(), fixedSize);
				update_msg_list<T0, T1, T2, T3>(msgPck, newPool);
				msgPck->unlock(this);
				return post_actor_msg<T0, T1, T2, T3>(newPool);
			}
			auto msgPool = msgPck->_msgPool;
			update_msg_list<T0, T1, T2, T3>(msgPck, msgPool);
			msgPck->unlock(this);
			return post_actor_msg<T0, T1, T2, T3>(msgPool);
		}
		msgPck->unlock(this);
		return post_actor_msg<T0, T1, T2, T3>();
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt post_actor_msg<T0, T1, T2> connect_msg_notifer_to_self(bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to_self<T0, T1, T2, void>(makeNew, fixedSize);
	}

	template <typename T0, typename T1>
	__yield_interrupt post_actor_msg<T0, T1> connect_msg_notifer_to_self(bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to_self<T0, T1, void, void>(makeNew, fixedSize);
	}

	template <typename T0>
	__yield_interrupt post_actor_msg<T0> connect_msg_notifer_to_self(bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to_self<T0, void, void, void>(makeNew, fixedSize);
	}

	__yield_interrupt post_actor_msg<> connect_msg_notifer_to_self(bool makeNew = false);

	/*!
	@brief 创建一个消息通知函数，在该Actor所依赖的ios无关线程中使用，且在该Actor调用 notify_run() 之前
	@param fixedSize 消息队列内存池长度
	@return 消息通知函数
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	post_actor_msg<T0, T1, T2, T3> connect_msg_notifer(size_t fixedSize = 16)
	{
		typedef post_actor_msg<T0, T1, T2, T3> post_type;

		return _strand->syncInvoke<post_type>([this, fixedSize]()->post_type
		{
			typedef msg_pool<T0, T1, T2, T3> pool_type;
			if (!this->parent_actor() && !this->is_started())
			{
				auto msgPck = this->msg_pool_pck<T0, T1, T2, T3>();
				msgPck->_msgPool = pool_type::make(this->self_strand(), fixedSize);
				return post_type(msgPck->_msgPool);
			}
			assert(false);
			return post_type();
		});
	}

	template <typename T0, typename T1, typename T2>
	post_actor_msg<T0, T1, T2> connect_msg_notifer(size_t fixedSize = 16)
	{
		return connect_msg_notifer<T0, T1, T2, void>(fixedSize);
	}

	template <typename T0, typename T1>
	post_actor_msg<T0, T1> connect_msg_notifer(size_t fixedSize = 16)
	{
		return connect_msg_notifer<T0, T1, void, void>(fixedSize);
	}

	template <typename T0>
	post_actor_msg<T0> connect_msg_notifer(size_t fixedSize = 16)
	{
		return connect_msg_notifer<T0, void, void, void>(fixedSize);
	}

	post_actor_msg<> connect_msg_notifer();
	//////////////////////////////////////////////////////////////////////////

	/*!
	@brief 连接消息泵到消息池
	@return 返回消息泵句柄
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	typename msg_pump<T0, T1, T2, T3>::handle connect_msg_pump()
	{
		typedef msg_pump<T0, T1, T2, T3> pump_type;
		typedef msg_pool<T0, T1, T2, T3> pool_type;
		typedef typename pool_type::pump_handler pump_handler;

		assert_enter();
		auto msgPck = msg_pool_pck<T0, T1, T2, T3>();
		quit_guard qg(this);
		msgPck->lock(this);
		if (msgPck->_next)
		{
			msgPck->_next->lock(this);
			clear_msg_list<T0, T1, T2, T3>(msgPck->_next);
			msgPck->_next->unlock(this);
		}
		msgPck->_next.reset();
		if (!msgPck->_msgPump)
		{
			msgPck->_msgPump = pump_type::make(shared_from_this());
		}
		auto msgPump = msgPck->_msgPump;
		auto msgPool = msgPck->_msgPool;
		if (msgPool)
		{
			msgPump->connect(send<pump_handler>(msgPool->_strand, [&msgPck]()->pump_handler
			{
				return msgPck->_msgPool->connect_pump(msgPck->_msgPump);
			}));
		}
		else
		{
			msgPump->clear();
		}
		msgPck->unlock(this);
		return msgPump.get();
	}

	template <typename T0, typename T1, typename T2>
	typename msg_pump<T0, T1, T2>::handle connect_msg_pump()
	{
		return connect_msg_pump<T0, T1, T2, void>();
	}

	template <typename T0, typename T1>
	typename msg_pump<T0, T1>::handle connect_msg_pump()
	{
		return connect_msg_pump<T0, T1, void, void>();
	}

	template <typename T0>
	typename msg_pump<T0>::handle connect_msg_pump()
	{
		return connect_msg_pump<T0, void, void, void>();
	}

	msg_pump<>::handle connect_msg_pump();
private:
	template <typename PUMP, typename DST>
	bool _timed_pump_msg(const PUMP& pump, DST& dstRef, int tm, bool checkDis)
	{
		assert(pump->_hostActor && pump->_hostActor->self_id() == self_id());
		if (!pump->read_msg(dstRef))
		{
			if (checkDis && pump->isDisconnected())
			{
				pump->_waiting = false;
				pump->_dstRef = NULL;
				throw pump_disconnected_exception();
			}
			pump->_checkDis = checkDis;
			bool timeOut = false;
			if (tm >= 0)
			{
				actor_handle shared_this = shared_from_this();
				delay_trig(tm, [shared_this, &timeOut]()
				{
					if (!shared_this->_quited)
					{
						timeOut = true;
						shared_this->pull_yield();
					}
				});
			}
			push_yield();
			if (!timeOut)
			{
				if (tm >= 0)
				{
					cancel_delay_trig();
				}
				if (pump->_checkDis)
				{
					assert(checkDis);
					pump->_checkDis = false;
					throw pump_disconnected_exception();
				}
				return true;
			}
			pump->_checkDis = false;
			pump->_waiting = false;
			pump->_dstRef = NULL;
			return false;
		}
		return true;
	}
public:

	/*!
	@brief 从消息泵中提取消息
	@param tm 超时时间
	@param checkDis 检测是否被断开连接，是就抛出 pump_disconnected_exception 异常
	@return 超时完成返回false，成功取到消息返回true
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt bool timed_pump_msg(int tm, const typename msg_pump<T0, T1, T2, T3>::handle& pump, T0& r0, T1& r1, T2& r2, T3& r3, bool checkDis = false)
	{
		assert_enter();
		ref_ex<T0, T1, T2, T3> dstRef(r0, r1, r2, r3);
		return _timed_pump_msg(pump, dstRef, tm, checkDis);
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt bool timed_pump_msg(int tm, const typename msg_pump<T0, T1, T2>::handle& pump, T0& r0, T1& r1, T2& r2, bool checkDis = false)
	{
		assert_enter();
		ref_ex<T0, T1, T2> dstRef(r0, r1, r2);
		return _timed_pump_msg(pump, dstRef, tm, checkDis);
	}

	template <typename T0, typename T1>
	__yield_interrupt bool timed_pump_msg(int tm, const typename msg_pump<T0, T1>::handle& pump, T0& r0, T1& r1, bool checkDis = false)
	{
		assert_enter();
		ref_ex<T0, T1> dstRef(r0, r1);
		return _timed_pump_msg(pump, dstRef, tm, checkDis);
	}

	template <typename T0>
	__yield_interrupt bool timed_pump_msg(int tm, const typename msg_pump<T0>::handle& pump, T0& r0, bool checkDis = false)
	{
		assert_enter();
		ref_ex<T0> dstRef(r0);
		return _timed_pump_msg(pump, dstRef, tm, checkDis);
	}

	__yield_interrupt bool timed_pump_msg(int tm, const msg_pump<>::handle& pump, bool checkDis = false);

	/*!
	@brief 从消息泵中提取消息
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt void pump_msg(const typename msg_pump<T0, T1, T2, T3>::handle& pump, T0& r0, T1& r1, T2& r2, T3& r3, bool checkDis = false)
	{
		timed_pump_msg(-1, pump, r0, r1, r2, r3, checkDis);
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt void pump_msg(const typename msg_pump<T0, T1, T2>::handle& pump, T0& r0, T1& r1, T2& r2, bool checkDis = false)
	{
		timed_pump_msg(-1, pump, r0, r1, r2, checkDis);
	}

	template <typename T0, typename T1>
	__yield_interrupt void pump_msg(const typename msg_pump<T0, T1>::handle& pump, T0& r0, T1& r1, bool checkDis = false)
	{
		timed_pump_msg(-1, pump, r0, r1, checkDis);
	}

	template <typename T0>
	__yield_interrupt void pump_msg(const typename msg_pump<T0>::handle& pump, T0& r0, bool checkDis = false)
	{
		timed_pump_msg(-1, pump, r0, checkDis);
	}

	template <typename T0>
	__yield_interrupt T0 pump_msg(msg_pump<T0>& pump, bool checkDis = false)
	{
		T0 r0;
		timed_pump_msg(-1, &pump, r0, checkDis);
		return r0;
	}

	__yield_interrupt void pump_msg(const msg_pump<>::handle& pump, bool checkDis = false);
public:
	/*!
	@brief 测试当前下的Actor栈是否安全
	*/
	void check_stack();

	/*!
	@brief 获取当前Actor剩余安全栈空间
	*/
	size_t stack_free_space();

	/*!
	@brief 获取当前Actor调度器
	*/
	shared_strand self_strand();

	/*!
	@brief 返回本对象的智能指针
	*/
	actor_handle shared_from_this();

	/*!
	@brief 获取当前ActorID号
	*/
	long long self_id();

	/*!
	@brief 获取Actor切换计数
	*/
	size_t yield_count();

	/*!
	@brief Actor切换计数清零
	*/
	void reset_yield();

	/*!
	@brief 开始运行建立好的Actor
	*/
	void notify_run();

	/*!
	@brief 强制退出该Actor，不可滥用，有可能会造成资源泄漏
	*/
	void notify_quit();

	/*!
	@brief 强制退出该Actor，完成后回调
	*/
	void notify_quit(const std::function<void (bool)>& h);

	/*!
	@brief Actor是否已经开始运行
	*/
	bool is_started();

	/*!
	@brief Actor是否已经退出
	*/
	bool is_quited();

	/*!
	@brief 锁定当前Actor，暂时不让强制退出
	*/
	void lock_quit();

	/*!
	@brief 解除退出锁定
	*/
	void unlock_quit();

	/*!
	@brief 暂停Actor
	*/
	void notify_suspend();
	void notify_suspend(const std::function<void ()>& h);

	/*!
	@brief 恢复已暂停Actor
	*/
	void notify_resume();
	void notify_resume(const std::function<void ()>& h);

	/*!
	@brief 切换挂起/非挂起状态
	*/
	void switch_pause_play();
	void switch_pause_play(const std::function<void (bool isPaused)>& h);

	/*!
	@brief 等待Actor退出，在Actor所依赖的ios无关线程中使用
	*/
	bool outside_wait_quit();

	/*!
	@brief 添加一个Actor结束回调
	*/
	void append_quit_callback(const std::function<void (bool)>& h);

	/*!
	@brief 启动一堆Actor
	*/
	void actors_start_run(const list<actor_handle>& anotherActors);

	/*!
	@brief 强制退出另一个Actor，并且等待完成
	*/
	__yield_interrupt bool actor_force_quit(const actor_handle& anotherActor);
	__yield_interrupt void actors_force_quit(const list<actor_handle>& anotherActors);

	/*!
	@brief 等待另一个Actor结束后返回
	*/
	__yield_interrupt bool actor_wait_quit(const actor_handle& anotherActor);
	__yield_interrupt void actors_wait_quit(const list<actor_handle>& anotherActors);

	/*!
	@brief 挂起另一个Actor，等待其所有子Actor都调用后才返回
	*/
	__yield_interrupt void actor_suspend(const actor_handle& anotherActor);
	__yield_interrupt void actors_suspend(const list<actor_handle>& anotherActors);

	/*!
	@brief 恢复另一个Actor，等待其所有子Actor都调用后才返回
	*/
	__yield_interrupt void actor_resume(const actor_handle& anotherActor);
	__yield_interrupt void actors_resume(const list<actor_handle>& anotherActors);

	/*!
	@brief 对另一个Actor进行挂起/恢复状态切换
	@return 都已挂起返回true，否则false
	*/
	__yield_interrupt bool actor_switch(const actor_handle& anotherActor);
	__yield_interrupt bool actors_switch(const list<actor_handle>& anotherActors);

	void assert_enter();
private:
	void time_out(int ms, const std::function<void ()>& h);
	void expires_timer();
	void cancel_timer();
	void suspend_timer();
	void resume_timer();
	void start_run();
	void force_quit(const std::function<void (bool)>& h);
	void suspend(const std::function<void ()>& h);
	void resume(const std::function<void ()>& h);
	void suspend();
	void resume();
	void run_one();
	void pull_yield();
	void push_yield();
	void force_quit_cb_handler();
	void exit_callback();
	void child_suspend_cb_handler();
	void child_resume_cb_handler();
private:
	void* _actorPull;///<Actor中断点恢复
	void* _actorPush;///<Actor中断点
	void* _stackTop;///<Actor栈顶
	long long _selfID;///<ActorID
	size_t _stackSize;///<Actor栈大小
	shared_strand _strand;///<Actor调度器
	DEBUG_OPERATION(bool _inActor);///<当前正在Actor内部执行标记
	bool _started;///<已经开始运行的标记
	bool _quited;///<已经准备退出标记
	bool _suspended;///<Actor挂起标记
	bool _hasNotify;///<当前Actor挂起，有外部触发准备进入Actor标记
	bool _isForce;///<是否是强制退出的标记，成功调用了force_quit
	bool _notifyQuited;///<当前Actor被锁定后，收到退出消息
	size_t _lockQuit;///<锁定当前Actor，如果当前接收到退出消息，暂时不退，等到解锁后退出
	size_t _yieldCount;//yield计数
	size_t _childOverCount;///<子Actor退出时计数
	size_t _childSuspendResumeCount;///<子Actor挂起/恢复计数
	std::weak_ptr<my_actor> _parentActor;///<父Actor
	main_func _mainFunc;///<Actor入口
	list<suspend_resume_option> _suspendResumeQueue;///<挂起/恢复操作队列
	list<actor_handle> _childActorList;///<子Actor集合
	list<std::function<void (bool)> > _exitCallback;///<Actor结束后的回调函数，强制退出返回false，正常退出返回true
	list<std::function<void ()> > _quitHandlerList;///<Actor退出时强制调用的函数，后注册的先执行
	msg_pool_status _msgPoolStatus;//消息池列表
	timer_pck* _timer;///<提供延时功能
	std::weak_ptr<my_actor> _weakThis;
};

#endif