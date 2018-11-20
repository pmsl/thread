/*
 *	线程互斥体
 *  Created on: 2018年7月19日
 *      Author: pmsl
 */

#ifndef LIBS_THREAD_THREADMUTEX_HPP_
#define LIBS_THREAD_THREADMUTEX_HPP_
#include <pthread.h>
namespace thread{

	/*
	 * 空互斥体
	 */
	class TMutexNull{
	public:
		TMutexNull(void){}
		virtual ~TMutexNull(){}
		virtual void lock(){}
		virtual void unlock(){}
	};		// class TMutexNull

	/*
	 * 线程互斥体
	 */
	class TMutex:TMutexNull{
	private:
		pthread_mutex_t  mutex_;
	public:
		TMutex(void){
			pthread_mutex_init(&mutex_,NULL);
		}

		/*
		 *  这里不容进行拷贝构造，会造成多次释放互斥体
		 */
		TMutex(const TMutex&)
		{
			pthread_mutex_init(&mutex_,NULL);
		}

		virtual ~TMutex(void){
			pthread_mutex_destroy(&mutex_);
		}

		virtual void locak(void){
			pthread_mutex_lock(&mutex_);
		}

		virtual void unlock(void){
			pthread_mutex_unlock(&mutex_);
		}
	};		// class TMutex

	class ThreadGuard{
	private:
		TMutexNull* mutexPtr_;
	public:
		explicit ThreadGuard(TMutexNull * mutexPtr):mutexPtr_(mutexPtr)
		{
			mutexPtr_->lock();
		}
		virtual ~ThreadGuard()
		{
			mutexPtr_->unlock();
		}
	};		// class ThreadGuard
}	// namespace thread



#endif /* LIBS_THREAD_THREADMUTEX_HPP_ */
