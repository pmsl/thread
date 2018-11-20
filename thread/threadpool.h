/*
 * threadpool.hpp
 *
 *  Created on: 2018年7月19日
 *      Author: pmsl
 */

#ifndef LIBS_THREAD_THREADPOOL_H_
#define LIBS_THREAD_THREADPOOL_H_
#include <stdint.h>
#include <time.h>
#include <queue>
#include <list>
#include <algorithm>
#include  "log/log.h"
#include "common/common.h"
#include "threadmutex.hpp"
#include "task.h"

namespace thread{
// 线程池中线程大于这个数则处于繁忙状态
#define	 BUSY_SIZE 32
class ThreadPool;
class Thread{
	friend class ThreadPool;
	// 线程状态 -1未启动 0 睡眠 1 繁忙
	enum THREAD_STATE{
		THREAD_STATE_STOP 		=  -1,
		THREAD_STATE_SLEEP 		=   0,
		THREAD_STATE_BUSY		=	1,
		THREAD_STATE_END		=	2,
		THREAD_STATE_PENDING	=	3,
	};
private:
	pthread_cond_t					cond_;			// 线程信号量
	pthread_mutex_t				mutex_;			// 线程互斥体
	int								waitSecond_;	// 空闲线程超过指定秒数则退出线程，小于0 为常驻线程
	THTask*							task_;			// 该线程执行的任务
	pthread_t						tid_;			// 本线程ID
	ThreadPool*					pool_;			// 线程池指针
	THREAD_STATE					state_;			// 线程当前状态 -1 未启动 0 挂起 1 繁忙
	uint32_t						done_task_;		// 线程在启动一次后未改变到闲置状态下连续执行任务的次数
public:
	Thread(ThreadPool* pool,int waitSecond = 0):
		waitSecond_(waitSecond),
		task_(nullptr),
		pool_(pool),
		done_task_(0)
		{
			state_ = THREAD_STATE_STOP;
			initCond();
			initMutex();
		}
	virtual ~Thread()
	{
		deleteCond();
		deleteMutex();
		LOG_INFO("Thread::~Thread()");
	}

	virtual void onStart(){}
	virtual void onEnd(){}

	virtual void onProcessTaskStart(THTask* task){}
	virtual void processTask(THTask* task){
		task->process();
	}

	virtual void onProcessTaskEnd(THTask* task){}
	inline pthread_t id(void) const;
	inline void id(pthread_t pid);
	/*
	 * 创建一个线程并将自己与这个线程绑定
	 * @para	void
	 * @2return 线程ID
	 */
	pthread_t createThread(void);

	/*
	 *  初始化信号量
	 */
	virtual void initCond(void)
	{
		pthread_cond_init(&cond_,NULL);
		LOG_INFO("Thread::initCond");
	}

	/*
	 * 销毁信号量
	 */
	virtual void deleteCond(void)
	{
		pthread_cond_destroy(&cond_);
	}
	/*
	 * 	初始化互斥体
	 */
	virtual void initMutex(void)
	{
		pthread_mutex_init(&mutex_,nullptr);
	}

	/*
	 * 销毁互斥体
	 */
	virtual void deleteMutex(void)
	{
		pthread_mutex_destroy(&mutex_);
	}

	/*
	 * 对互斥体枷锁
	 */
	virtual void lock(void)
	{
		pthread_mutex_lock(&mutex_);
	}

	/*
	 * 	对互斥体解锁
	 */
	virtual void unlock(void)
	{
		pthread_mutex_unlock(&mutex_);
	}

	/*
	 * 获取当前任务
	 */
	virtual THTask* tryGetTask(void);

	/*
	 *
	 */
	bool  onWaitCondSignal(void);

	bool	join(void);
	inline THTask* task(void) const;
	inline void task(THTask* task);
	inline int  state(void) const;
	void	onTaskCompleted(void);
	static void* run(void* arg);
	void		  stop();
	inline ThreadPool* threadPool();

	void reset_done_tasks(){
		done_task_ = 0;
	}

	void inc_done_tasks(){
		++done_task_;
	}

	/// @brief 唤醒线程
	int wakeup()
	{
		while( true )
		{
			lock();
			if( state_ == THREAD_STATE_PENDING )
			{
				unlock();
			}else{
				break;
			}
		}
		int ret = pthread_cond_signal(&cond_);
		unlock();
		return ret;
	}
};

class ThreadPool{
public:
	ThreadPool();
	virtual ~ThreadPool();
	void finalise();

	virtual void onMainThreadTick();
	bool hasThread(Thread* pThread);

	std::string toString();

	inline uint32_t currentThreadCount(void) const;

	inline uint32_t  currentIdelThreadCount(void) const;

	/// @brief	创建线程池
	/// @param	inewThreadCount	系统繁忙时线程池会临时增加改数量线程
	/// @param  inormalMaxThreadCount 线程池一直会保持指定数量的线程
	/// @param  imaxThreadCount	线程池最多容许有的线程数量
	bool createThreadPool(uint32_t inewThreadCount, uint32_t inormalMaxThreadCount, uint32_t imaxThreadCount);

	/// @brief	向线程池中增加一个任务
	bool addTask(THTask* ptask);
	bool _addTask(THTask* ptask);

	/// @brief	线程数量是否达到最大个数
	inline bool isThreadCountMax(void) const;

	/// @brief 线程池是否处于繁忙状态
	/// @note 未处理任务非常多时，说明处于繁忙状态
	inline bool isBusy(void) const;

	/// @brief 线程池是否已经初始化
	inline bool isInitialize(void) const;

	/// @brief 判断线程池是否已经销毁
	inline bool isDestroyed(void) const;

	/// @brief 销毁线程池
	inline void destroy(void);

	/// @brief 获取缓存任务数量
	inline uint32_t bufferTaskSize(void) const;

	/// @brief	获取缓存的任务队列
	inline std::queue<thread::THTask*>& bufferTaskList();

	/// @brief 锁定任务队列
	inline void lockTaskList(void);

	/// @brief 解锁任务队列
	inline void unlockTaskList(void);

	/// @brief 获取已经完成的任务数量
	inline uint32_t finiTaskSize() const;

	virtual std::string name() const { return "ThreadPool"; }
public:
	static int timeout;

	/// @brief	创建一个线程
	/// @para  waitSecond 等待时间 默认为 ThreadPool::timeout
	/// @para  isImmediately 是否立即执行 默认立即执行
	virtual Thread* createThread(int waitSecond = ThreadPool::timeout,bool isImmediately = true);

	/// @brief 将任务添加到未处理列表
	void bufferTask(THTask* ptask);

	/// @brief 从未处理列表中取出一个任务，并从任务列表中删除
	THTask* popbufferTask(void);

	/// @brief 将线程移动到空闲队列
	bool addIdelThread(Thread* ptd);

	/// @将线程移动到工作列表
	bool addWorkThread(Thread* ptd);

	/// @brief 添加一个已经完成的任务
	void addFiniTask(THTask* ptask);

	/// @brief 删除一个挂起或者超时的任务
	bool delHangThread(Thread* ptd);

	/// @brief 初始化监控
	bool initializeWatcher();

protected:
	bool 					isInitialize_;							// 是否初始化过
	std::queue<THTask*> 	taskList_;								// 待处理任务队列
	std::list<THTask*> 	finiTaskList_;							// 已完成的任务队列
	size_t 					finiTaskList_count_;					// 已完成的任务数量
	pthread_mutex_t  		taskList_mutex_;						// 待处理任务列表互斥锁
	pthread_mutex_t	   	threadList_mutex;						// 线程列表互斥锁
	pthread_mutex_t	   	finiTaskList_mutex_;					// 已完成任务列表互斥锁

	std::list<Thread*>		workThreadList_;
	std::list<Thread*>  	idelThreadList_;
	std::list<Thread*>  	threadList_;

	uint32_t				maxThreadCount_;						// 最大线程数量
	uint32_t				extraNewThreadCount_;					// 每次新增的线程数量
	uint32_t				currentThreadCount_;					// 当前线程数量
	uint32_t				currentIdelThreadCount_;				// 空闲线程数量
	uint32_t				normalThreadCount_;						// 标准状态下线程数量
	bool					isDestroyed_;							// 是否已经销毁
};

}			// thread
#include "threadpool.inl"
#endif /* LIBS_THREAD_THREADPOOL_H_ */
