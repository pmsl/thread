/*
 * task.h
 *
 *  Created on: 2018年7月19日
 *      Author: pmsl
 */

#ifndef LIBS_THREAD_TASK_H_
#define LIBS_THREAD_TASK_H_

#include "../common/task.h"

namespace thread{
	class THTask:public Task{
	public:
		enum THTASKSTATE{
			/// 任务已经执行完毕
			THREAD_STATE_COMPLETE = 0,
			/// 继续在主线程执行
			THREAD_STATE_MAINTHREAD = 1,
			/// 继续在子线程执行
			THREAD_STATE_CHILDTHREAD = 2,
		};

		virtual thread::THTask::THTASKSTATE  presentMainThread(){
			return thread::THTask::THREAD_STATE_COMPLETE;
		}
	};
}



#endif /* LIBS_THREAD_TASK_H_ */
