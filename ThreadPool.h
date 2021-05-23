#pragma once
#include<list>
#include<queue>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<atomic>
#include<future>

using namespace std;

//任务类基类，使用时需要继承，然后重写run函数
class Task {
private:
	int m_priority;		//任务的优先级
public:
	enum PRIORITY {
		MIN = 1,
		NORMAL = 15,
		MAX = 25
	};
	Task() {};
	Task(PRIORITY priority) :m_priority(priority) {}
	~Task() {};
	void setPriority(PRIORITY priority) { m_priority = priority; }

	virtual void run() = 0;			//抽象类
};

//工作线程。1.可单独使用，继承该类，重写run()函数。2.在另一个类中包含引用或指针，分配任务。
class WorkThread {
private:
	thread m_thread;		//工作线程
	Task* m_task;			//任务指针
	mutex m_mutexThread;	//关于任务是否执行的互斥量
	mutex m_mutexCondition;	//条件互斥量
	mutex m_mutexTask;		//关于分配任务的互斥量
	condition_variable m_condition;		//任务条件变量
	atomic<bool> m_bRunning;			//判断是否运行的原子变量
	bool m_bStop;

protected:
	virtual void run();		//线程入口函数
public:
	WorkThread();
	~WorkThread();
	//表示删除默认拷贝构造函数，即不能进行默认拷贝
	WorkThread(const WorkThread &thread) = delete; 
	WorkThread &operator=(const WorkThread &thread) = delete;
	//删除右值引用函数，即禁用move（）,禁止将此线程所有权转移。
	WorkThread(const WorkThread &&thread) = delete;
	WorkThread &operator=(const WorkThread &&thread) = delete;

	bool assign(Task *task);	//分配任务
	thread::id getThreadID();	//获取线程id
	void stop();		//停止线程运行，结束线程运行
	void notify();		//唤醒阻塞线程
	void notify_all();	//唤醒所有阻塞线程
	bool isExecuting();	//判断线程是否在运行

};

//空闲线程列表，内部保证线程安全，外部使用不用加锁
class LeisureThreadList {
	list<WorkThread *> m_threadList;	//线程列表
	mutex m_mutexThread;		//线程列表访问互斥量
	void assign(const size_t counts);	//创建线程

public:
	LeisureThreadList(const size_t counts);
	~LeisureThreadList();
	void push(WorkThread *thread);		//添加线程
	WorkThread *top();		//返回第一个线程指针
	void pop();		//删除第一个线程
	size_t size();	//返回线程个数
	void stop();	//停止运行
};

//线程池类，内部操作为线程安全的
/*
该类运行时只要调用addTask（）添加任务即可，任务会自动运行。
如果线程池退出，必须调用exit()函数。
线程池可以暂停执行，调用stop()即可，重新开始运行需要调用start()。
*/
class ThreadPool {
	thread m_thread;		//线程池的任务分配线程，即管理线程
	LeisureThreadList m_leisureThreadList;	//线程列表
	queue<Task *> m_taskList;	//任务队列
	atomic<bool> m_bRunning;	//用来处理线程池暂停操作的标志位
	atomic<bool> m_bEnd;		//是用来终止线程池运行的标志位
	atomic<size_t> m_threadCounts;	//线程总数
	condition_variable m_condition_task;	//任务条件变量
	condition_variable m_condition_thread;	//线程列表条件变量
	condition_variable m_condition_running;	//运行条件变量
	mutex m_runningMutex;		//运行控制变量锁
	mutex m_mutexThread;		//空闲互斥锁
	mutex m_taskMutex;			//访问任务列表的互斥锁
	bool m_bExit;		//是否退出标志位

	void run();			//线程池主线程函数

public:
	ThreadPool(const size_t counts);
	~ThreadPool();
	size_t threadCounts();
	bool isRunning();	//线程是否正在运行任务
	void addTask(Task *task);	//添加任务

	//线程池开始调度任务，线程池创建后不用调用该函数。该函数与stop()函数配合使用
	void start();	//线程池开始执行

	//线程池暂停任务调度，但不影响任务添加。若要开始任务调度，需要调用start()函数。
	void stop();	//线程池暂停运行

	//该函数会在线程池中的所有任务都分配出去以后，结束现成吃的运行。
	//同样的线程列表中的线程会在自己的任务执行完毕后再推出。
	void exit();	//线程池退出

};
