namespace thread{

//-------------------------- Thread --------------------------
inline pthread_t Thread::id(void) const
{
	return tid_;
}

inline void Thread::id(pthread_t tid)
{
	tid_ = tid;
}

inline THTask* Thread::task(void) const
{
	return task_;
}

inline void Thread::task(THTask* task)
{
	task_ = task;
}

inline int Thread::state(void) const
{
	return state_;
}

inline ThreadPool* Thread::threadPool()
{
	return pool_;
}

//------------------------- ThreadPool -------------------

inline bool ThreadPool::isInitialize(void) const
{
	return isInitialize_;
}

inline bool ThreadPool::isBusy(void) const
{
	return taskList_.size() > BUSY_SIZE;
}

inline bool ThreadPool::isThreadCountMax(void) const
{
	return currentThreadCount_ >= maxThreadCount_;
}

inline uint32_t ThreadPool::currentThreadCount(void) const
{
	return currentThreadCount_;
}

inline uint32_t ThreadPool::currentIdelThreadCount(void) const
{
	return currentIdelThreadCount_;
}

inline bool ThreadPool::isDestroyed() const
{
	return isDestroyed_;
}

}		// namespace thread