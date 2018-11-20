/*
 * threadpool.cpp
 *
 *  Created on: 2018年7月23日
 *      Author: pmsl
 */

#include "threadpool.h"
#include <sys/time.h>

namespace thread{
	int ThreadPool::timeout = 300;

	ThreadPool::ThreadPool(void):
			isInitialize_(false),
			taskList_(),finiTaskList_(),
			taskList_mutex_(),threadList_mutex(),finiTaskList_mutex_(),
			workThreadList_(),idelThreadList_(),threadList_(),
			maxThreadCount_(0),extraNewThreadCount_(0),currentThreadCount_(0),currentIdelThreadCount_(0),normalThreadCount_(0),
			finiTaskList_count_(0),isDestroyed_(false)
	{
			pthread_mutex_init(&taskList_mutex_,NULL);
			pthread_mutex_init(&threadList_mutex,NULL);
			pthread_mutex_init(&finiTaskList_mutex_,NULL);
	}

	ThreadPool::~ThreadPool()
	{
		// TODO isDestroyed_ && threadList_.szie() == 0
	}

	void ThreadPool::finalise(void)
	{
		destroy();
	}

	void ThreadPool::destroy(void)
	{
		isDestroyed_ = true;
		pthread_mutex_lock(&threadList_mutex);
		LOG_INFO("ThreadPool::destroy : starting size %s ",threadList_.size());
		pthread_mutex_unlock(&threadList_mutex);

		int trycount = 0;
		while(true)
		{
			Sleep(300);
			trycount++;
			pthread_mutex_lock(&threadList_mutex);
			int count = threadList_.size();
			std::list<Thread*>::iterator iter = threadList_.begin();
			for(; iter != threadList_.end(); ++iter)
			{
				if((*iter))
				{
					if( (*iter)->state() != Thread::THREAD_STATE_END )
					{
						(*iter)->wakeup();
					}
					else
					{
						count--;
					}
				}
			}
			pthread_mutex_unlock(&threadList_mutex);
			if( count <= 0 )
			{
				break;
			}
			else{
				LOG_INFO("ThreadPool::destroy : waiting for %d thread to destroy,try count : %d",count,trycount);
			}
		}
		pthread_mutex_lock(&threadList_mutex);
		Sleep(100);
		std::list<Thread*>::iterator iter = threadList_.begin();
		for(; iter != threadList_.end(); ++iter)
		{
			if((*iter))
			{
				delete (*iter);
				(*iter) = NULL;
			}
		}
		threadList_.clear();
		pthread_mutex_unlock(&threadList_mutex);

		pthread_mutex_lock(&finiTaskList_mutex_);
		if( finiTaskList_.size() > 0 )
		{
			LOG_INFO("ThreadPool::destroy : Discarding %d finished tasks.",finiTaskList_.size());
			std::list<THTask*>::iterator iter = finiTaskList_.begin();
			for(; iter != finiTaskList_.end(); iter++)
			{
				delete (*iter);
				(*iter) = NULL;
			}
			finiTaskList_.clear();
			finiTaskList_count_ = 0;
		}
		pthread_mutex_unlock(&finiTaskList_mutex_);

		pthread_mutex_lock(&taskList_mutex_);
		if( taskList_.size() > 0 )
		{
			LOG_INFO("ThreadPool::destroy : Discarding %d tasks",taskList_.size());
			while(taskList_.size() > 0)
			{
				THTask* ptr = taskList_.front();
				taskList_.pop();
				delete ptr;
			}
		}
		pthread_mutex_unlock(&taskList_mutex_);

		pthread_mutex_destroy(&threadList_mutex);
		pthread_mutex_destroy(&finiTaskList_mutex_);
		pthread_mutex_destroy(&taskList_mutex_);
		LOG_INFO("ThreadPool::destroy : successfully!");
	}

	THTask* ThreadPool::popbufferTask(void)
	{
		THTask* ptr = NULL;
		pthread_mutex_lock(&taskList_mutex_);
		size_t count = taskList_.size();
		if(count > 0)
		{
			ptr = taskList_.front();
			taskList_.pop();
			if( count > BUSY_SIZE )
			{
				LOG_INFO("ThreadPool::popbufferTask: Task list size %d",count);
			}
		}
		pthread_mutex_unlock(&taskList_mutex_);
		return ptr;
	}

	void ThreadPool::addFiniTask(THTask* ptr)
	{
		pthread_mutex_lock(&finiTaskList_mutex_);
		finiTaskList_.push_back(ptr);
		++finiTaskList_count_;
		pthread_mutex_unlock(&finiTaskList_mutex_);
	}

	bool ThreadPool::createThreadPool(uint32_t inewThreadCount, uint32_t inormalMaxThreadCount, uint32_t imaxThreadCount)
	{
		assert(!isInitialize_);
		LOG_INFO("ThreadPool::createThreadPool : creating threadpool ...");
		extraNewThreadCount_ = inewThreadCount;
		normalThreadCount_ = inormalMaxThreadCount;
		maxThreadCount_ = imaxThreadCount;
		for(uint32_t i = 0; i < normalThreadCount_; ++i)
		{
			Thread* ptr = createThread(0);
			if( !ptr )
			{
				LOG_ERROR("ThreadPool::createThread : create thread error!");
				return false;
			}
			currentIdelThreadCount_++;
			currentThreadCount_++;
			idelThreadList_.push_back(ptr);
			threadList_.push_back(ptr);
		}

		LOG_INFO("ThreadPool::createThreadPool: successfully %d, newThreadCount=%d normalMaxThreadCount=%d maxThreadCount=%d",
				currentThreadCount_,inewThreadCount,inormalMaxThreadCount,imaxThreadCount);
		isInitialize_ = true;
		Sleep(100);
		LOG_INFO("ThreadPool::createThreadPool sleep ok");
		return true;
	}

	void ThreadPool::onMainThreadTick()
	{
		std::vector<THTask*> finiTask;
		pthread_mutex_lock(&finiTaskList_mutex_);
		if( finiTaskList_.size() == 0 )
		{
			pthread_mutex_unlock(&finiTaskList_mutex_);
			return;
		}
		std::copy(finiTaskList_.begin(), finiTaskList_.end(), std::back_inserter(finiTask));
		finiTaskList_.clear();
		pthread_mutex_unlock(&finiTaskList_mutex_);
		std::vector<THTask*>::iterator iter = finiTask.begin();
		for(;iter != finiTask.end();)
		{
			thread::THTask::THTASKSTATE state = (*iter)->presentMainThread();
			switch(state)
			{
			case thread::THTask::THREAD_STATE_COMPLETE:
				delete (*iter);
				iter = finiTask.erase(iter);
				--finiTaskList_count_;
				break;
			case thread::THTask::THREAD_STATE_CHILDTHREAD:
				this->addTask((*iter));
				iter = finiTask.erase(iter);
				--finiTaskList_count_;
				break;
			case thread::THTask::THREAD_STATE_MAINTHREAD:
				pthread_mutex_lock(&finiTaskList_mutex_);
				finiTaskList_.push_back((*iter));
				pthread_mutex_unlock(&finiTaskList_mutex_);
				break;
			default:
				LOG_INFO("ThreadPool::onMainThreadTick : unknown task state %d",state);
				break;
			}
		}
	}

	void ThreadPool::bufferTask(THTask* ptask)
	{
		LOG_INFO("Enter ThreadPool::bufferTask");
		pthread_mutex_lock(&taskList_mutex_);
		taskList_.push(ptask);
		size_t size = taskList_.size();
		if( size > BUSY_SIZE)
		{
			LOG_INFO("ThreadPool::bufferTask : task buffered %d",size);
		}
		pthread_mutex_unlock(&taskList_mutex_);
	}

	Thread* ThreadPool::createThread(int waitSecond ,bool isImmediately )
	{
		Thread* ptr = new Thread(this,waitSecond);
		if( isImmediately )
			ptr->createThread();
		return ptr;
	}

	bool ThreadPool::addIdelThread(Thread* ptr)
	{
		pthread_mutex_lock(&threadList_mutex);
		std::list<Thread*>::iterator iter;
		iter = find(workThreadList_.begin(),workThreadList_.end(),ptr);
		if( iter != workThreadList_.end() )
		{
			workThreadList_.erase(iter);
		}
		else{
			pthread_mutex_unlock(&threadList_mutex);
			delete ptr;
			LOG_ERROR("ThreadPool::addIdelThread workThreadList_ not found thread %d",ptr->id());
			return false;
		}
		idelThreadList_.push_back(ptr);
		currentIdelThreadCount_++;
		pthread_mutex_unlock(&threadList_mutex);
		return true;
	}

	bool ThreadPool::addWorkThread(Thread* ptr)
	{
		pthread_mutex_lock(&threadList_mutex);
		std::list<Thread*>::iterator iter;
		iter = find(idelThreadList_.begin(),idelThreadList_.end(),ptr);
		if ( iter != idelThreadList_.end() )
		{
			idelThreadList_.erase(iter);
		}
		else{
			pthread_mutex_unlock(&threadList_mutex);
			LOG_ERROR("ThreadPool::addWorkThread : idelThreadList not found thread %d",ptr->id());
			return false;
		}
		workThreadList_.push_back(ptr);
		--currentIdelThreadCount_;
		pthread_mutex_unlock(&threadList_mutex);
		return true;
	}

	bool ThreadPool::delHangThread(Thread* ptr)
	{
		bool ret = false;
		pthread_mutex_lock(&threadList_mutex);
		std::list<Thread*>::iterator iter,iter1;
		iter = find(threadList_.begin(),threadList_.end(),ptr);
		iter1 = find(idelThreadList_.begin(),idelThreadList_.end(),ptr);
		if( iter != threadList_.end() && iter1 != idelThreadList_.end())
		{
			idelThreadList_.erase(iter1);
			threadList_.erase(iter);
			--currentThreadCount_;
			--currentIdelThreadCount_;
			LOG_INFO("ThreadPool::delHangThread : thread %d is destroy. currentIdelThreadCount:%d ,currentThreadCount:%d",
					static_cast<uint32_t>(ptr->id()),currentThreadCount_,currentIdelThreadCount_);
			SAFE_RELEASE(ptr);
			ret = true;
		}
		else{
			LOG_ERROR("ThreadPool::delHangThread : not found thread. %d",static_cast<uint32_t>(ptr->id()));
		}
		pthread_mutex_unlock(&threadList_mutex);
		return ret;
	}

	bool ThreadPool::addTask(THTask* ptask)
	{
		LOG_INFO("ENter ThreadPool::addTask")
		if ( NULL == ptask )
		{
			LOG_ERROR("ThreadPool::addTask : argument is null");
			return false;
		}
		pthread_mutex_lock(&threadList_mutex);
		if( currentIdelThreadCount_ > 0 )
		{
			bool ret = _addTask(ptask);
			pthread_mutex_unlock(&threadList_mutex);
			return ret;
		}
		bufferTask(ptask);
		if( isThreadCountMax())
		{
			pthread_mutex_unlock(&threadList_mutex);
			LOG_ERROR("ThreadPool::addTask : can't create thread, the threadpool is full");
			return false;
		}
		for( uint32_t i = 0;i< extraNewThreadCount_;i++)
		{
			bool isImmediately = i > 0;
			Thread* ptr = createThread(ThreadPool::timeout,isImmediately);
			if( !ptr )
			{
				LOG_ERROR("the ThreadPool create thread error!")
			}
			threadList_.push_back(ptr);
			if( isImmediately )
			{
				idelThreadList_.push_back(ptr);
				++currentIdelThreadCount_;
			}else{
				THTask* task = ptr->tryGetTask();
				if( task )
				{
					workThreadList_.push_back(ptr);
					ptr->task(task);
				}else{
					idelThreadList_.push_back(ptr);
					++currentIdelThreadCount_;
				}
				ptr->createThread();
			}
			++currentThreadCount_;
		}
		LOG_INFO("ThreadPool::addTask: new task, currentThreadCount: %d",currentThreadCount_);
		pthread_mutex_unlock(&threadList_mutex);
		return true;
	}

	bool ThreadPool::_addTask(THTask* ptask)
	{
		LOG_INFO("Enter ThreadPool::_addTask");
		std::list<Thread*>::iterator iter = idelThreadList_.begin();
		Thread* thptr = static_cast<Thread*>(*iter);
		idelThreadList_.erase(iter);
		workThreadList_.push_back(thptr);
		--currentIdelThreadCount_;
		thptr->task(ptask);
		if( thptr->wakeup() != 0 )
		{
			LOG_ERROR("ThreadPool::_addTask : try wakeup thread error!");
			return false;
		}
		LOG_INFO("ThreadPool::_addTask wakeup ok");
		return true;
	}

	bool ThreadPool::hasThread(Thread* ptr)
	{
		bool ret = true;
		pthread_mutex_lock(&threadList_mutex);
		std::list<Thread*>::iterator iter = find(threadList_.begin(),threadList_.end(),ptr);
		if( iter == threadList_.end() )
		{
			ret = false;
		}
		pthread_mutex_unlock(&threadList_mutex);
		return ret;
	}

	//---------------------- Thread ---------------------------

	pthread_t Thread::createThread()
	{
		if( pthread_create(&tid_,NULL,Thread::run,static_cast<void*>(this)) != 0 )
		{
			LOG_ERROR("Thread::createThread : create thread error!");
		}
		return tid_;
	}

	bool Thread::join()
	{
		void* status;
		if( pthread_join(id(),&status)){
			LOG_ERROR("Thread::join : can't join thread!");
			return false;
		}
		return false;
	}

	void* Thread::run(void* arg)
	{
		Thread* ptr = static_cast<Thread*>(arg);
		ThreadPool* ptrPool = ptr->threadPool();
		bool isRun = true;
		bool isStop = false;
		ptr->reset_done_tasks();
		pthread_detach(pthread_self());
		ptr->onStart();
		while(isRun)
		{
			if( NULL != ptr->task() )
			{
				isRun = true;
			}else{
				ptr->reset_done_tasks();
				isRun = ptr->onWaitCondSignal();
			}
			if( !isRun || ptrPool->isDestroyed())
			{
				if( !ptrPool->hasThread(ptr))
				{
					ptr = NULL;
				}
				isStop = true;
				break;
			}

			THTask* task = ptr->task();
			if( NULL == task )
			{
				continue;
			}
			ptr->state_ = THREAD_STATE_BUSY;
			while( task && !ptr->threadPool()->isDestroyed() )
			{
				ptr->inc_done_tasks();
				ptr->onProcessTaskStart(task);
				ptr->processTask(task);
				ptr->onProcessTaskEnd(task);
				THTask * nextTask = ptr->tryGetTask();
				if( !nextTask )
				{
					ptr->state_ = THREAD_STATE_PENDING;
					ptr->onTaskCompleted();
					break;
				}else{
					ptrPool->addFiniTask(task);
					task = nextTask;
					ptr->task(task);
				}
			}
		}
		if (isStop)
		{
			if( ptr )
			{
				ptr->stop();
			}
		}
		return NULL;
	}

	void Thread::stop()
	{
		THTask* task = this->task();
		if ( task )
		{
			LOG_INFO("Thread::stop : task not finish, thread will exit");
			delete task;
		}
		onEnd();
		state_ = THREAD_STATE_END;
		reset_done_tasks();
		pthread_exit(NULL);
	}

	bool Thread::onWaitCondSignal()
	{
		LOG_INFO("Thread::onWaitCondSignal Enter")
		if( waitSecond_ <= 0 )
		{
			lock();
			state_ = THREAD_STATE_SLEEP;
			pthread_cond_wait(&cond_,&mutex_);
			unlock();
		}else{
			struct timeval now;
			struct timespec timeout;
			gettimeofday(&now,NULL);
			timeout.tv_sec = now.tv_sec + waitSecond_;
			timeout.tv_nsec = now.tv_usec * 1000;
			lock();
			state_ = THREAD_STATE_SLEEP;
			int ret = pthread_cond_timedwait(&cond_,&mutex_,&timeout);
			unlock();
			if( ret == ETIMEDOUT )
			{
				pool_->delHangThread(this);
				return false;
			}else if( ret != 0 )
			{
				LOG_ERROR("Thread::onWaitCondSignal : pthread_cond_wait error")
			}
		}
		LOG_INFO("Thread::onWaitCondSignal Exit")
		return true;
	}

	void Thread::onTaskCompleted()
	{
		pool_->addFiniTask(task_);
		task_ = NULL;
		pool_->addIdelThread(this);
	}

	THTask* Thread::tryGetTask()
	{
		return pool_->popbufferTask();
	}

}	// namsespace thread
