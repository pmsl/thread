/*
 * test.cpp
 *
 *  Created on: 2018年7月23日
 *      Author: pmsl
 */
#include "common/common.h"
#include "log/log.h"
#include "threadpool.h"

class MTask:public thread::THTask{
private:
	int ID;
	int count;
public:
	MTask(int id,int c):ID(id),count(c){}
	virtual ~MTask(){}
	virtual bool process()
	{
		LOG_INFO("MTask::process %d %d",count,ID);
		for(int i = 0; i< count ; ++i )
		{
			LOG_INFO("MTask ID[%d] step[%d]",ID,i);
			Sleep(100);
		}
		return true;
	}
};

int main(int argc,char* argv[])
{
	Log::EnableCrashRecord();
	thread::ThreadPool pool ;
	pool.createThreadPool(1, 1, 4);

	for( int i = 0; i < 20;i++)
	{
		thread::THTask * ptr = new MTask(i,50);
		if( NULL != ptr )
		{
			pool.addTask(ptr);
		}else{
			return -1;
		}
	}


	while(true)
	{
		LOG_INFO("MAIN Thread goto sleep");
		Sleep(1000);
	}
	return 0;
}


