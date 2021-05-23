#include"ThreadPool.h"
#include<iostream>

//工作线程
WorkThread::WorkThread() :m_task{ nullptr }, m_bStop{ false } {
	m_bRunning.store(true);			//说明该对象创建后就开始运行起来了
	//初始化执行线程
	m_thread = thread(&WorkThread::run, this);
}

WorkThread::~WorkThread() {
	if (!m_bStop) {				//停止线程运行，调用stop()函数
		stop();
	}
	if (m_thread.joinable()) {	//等待执行任务的线程m_thread = thread(&WorkThread::run, this);结束再销毁该类实例化对象的资源
		m_thread.join();
	}
}

//给此线程分配任务
bool WorkThread::assign(Task * task) {
	m_mutexTask.lock();
	if (m_task != nullptr) {	//任务不为空，分配失败
		m_mutexTask.unlock();
		return false;
	}
	m_task = task;
	m_mutexTask.unlock();
	m_condition.notify_one();	//通知线程，唤醒创建的工作线程m_thread的入口函数run()（其中使用条件变量m_condition阻塞等待）
	return true;
}

//工作线程的入口函数
void WorkThread::run() {
	while (true) {
		if (!m_bRunning) {			//未运行,退出
			m_mutexTask.lock();
			if (m_task == nullptr) {	//并且未分配任务,跳出while循环，结束线程
				m_mutexTask.unlock();
				break;	//break语句的调用，起到跳出循环(while或for)或者分支语句(switch)作用。
			}
			m_mutexTask.unlock();
		}
		Task * task = nullptr;
		//等待任务，如果线程未退出并且没有任务，则线程阻塞
		{
			unique_lock<mutex> lock(m_mutexTask);	//需要判断m_task,加锁
			//等待信号,没有任务并且此线程正在运行，则阻塞此线程
			/*
			wait()是条件变量的成员函数，用来等一个东西，如果第二个参数lambda表达式返回值是false，那么wait将解锁第一个参数（互斥量），并堵塞到本行。
			堵塞到什么时候呢？堵塞到其他某个线程调用notify_one（）成员函数为止。如果返回true，那么wait()直接返回。
			如果没有第二个参数，就跟默认第二个参数返回false效果一样。
			*/
			m_condition.wait(lock,
				[this]() {
				return !((m_task == nullptr) && this->m_bRunning.load()); });
			task = m_task;
			m_task = nullptr;
			if (task == nullptr) {	//处理m_bRunning为false时，说明线程暂停或退出，此时若m_task为空，则无任务退出，否则需要执行完已分配的任务再退出
				continue;		//任务为空，跳过下面的运行代码
			}
		}
		task->run();	//执行任务
		delete task;	//执行完释放资源
		task = nullptr;
	}
}

thread::id WorkThread::getThreadID() {
	return this_thread::get_id();
}

void WorkThread::stop() {
	m_bRunning.store(false);	//设置对象的停止标志位，线程停止运行
	m_mutexThread.lock();
	if (m_thread.joinable()) {
		/*
		一定要先设置标志位，然后通知唤醒该对象的线程（run()函数中m_condition可能处于等待状态）
		*/
		m_condition.notify_all();	//唤醒run()函数后，由于m_bRunning设为fasle,则run函数判断能否退出
		m_thread.join();
	}
	m_mutexThread.unlock();
	m_bStop = true;
}

//此处无用，该类中只管理一个线程，可不加锁
void WorkThread::notify() {
	m_mutexCondition.lock();
	m_condition.notify_one();
	m_mutexCondition.unlock();
}

//此处无用，该类中只管理一个线程
void WorkThread::notify_all() {
	m_mutexCondition.lock();
	m_condition.notify_all();
	m_mutexCondition.unlock();
}
bool WorkThread::isExecuting() {
	//此时将任务分配给工作线程后，实例化对象中的m_task设为nullptr,则可以继续向该对象分配任务设置m_task进行排队，如果m_task已经有排队的则添加失败
	bool ret;
	m_mutexTask.lock();		//访问m_task时需要加锁
	ret = (m_task == nullptr);	//不为空则正在运行
	m_mutexTask.unlock();
	return !ret;
}

//空闲线程列表
LeisureThreadList::LeisureThreadList(const size_t counts) {
	assign(counts);		//创建多个工作线程
}

LeisureThreadList::~LeisureThreadList() {
//删除线程列表中的所有线程。析构函数不用加锁，因为ThreadPool对象析构前需先调用exit()函数，终止管理线程的运行，会调用此类实例化对象的stop()函数
	while (!m_threadList.empty()) {		//删除m_threadList中的工作线程类实例化对象
		WorkThread * temp = m_threadList.front();
		m_threadList.pop_front();
		delete temp;
	}
}

//创建初始化个数的线程
void LeisureThreadList::assign(const size_t counts) {
	for (size_t i = 0u; i < counts; i++) {
		m_threadList.push_back(new WorkThread);		//创建线程
	}
}

//添加线程,向线程列表中添加线程，须加锁
void LeisureThreadList::push(WorkThread *thread) {
	if (thread == nullptr) {
		return;
	}
	m_mutexThread.lock();
	m_threadList.push_back(thread);
	m_mutexThread.unlock();
}

//返回第一个线程指针,涉及对线程列表操作，须加锁
WorkThread * LeisureThreadList::top() {
	WorkThread * thread;	//需要定义一个指针获取，在解锁之后return
	m_mutexThread.lock();
	if (m_threadList.empty()) {
		thread = nullptr;
	}
	else {
		thread = m_threadList.front();
	}
	m_mutexThread.unlock();
	return thread;
}

void LeisureThreadList::pop() {
	m_mutexThread.lock();
	if (!m_threadList.empty()) { m_threadList.pop_front(); }
	m_mutexThread.unlock();
}

size_t LeisureThreadList::size() {
	size_t counts = 0u;
	m_mutexThread.lock();
	counts = m_threadList.size();
	m_mutexThread.unlock();
	return counts;
}

//停止运行
void LeisureThreadList::stop() {
	m_mutexThread.lock();
	for (auto thread : m_threadList) {
		thread->stop();				//调用工作线程类对象的内部终止函数，线程退出，但对象还在，也就是LeisureList对象析构时需要调用WorkThread的析构函数即delete命令
	}
	m_mutexThread.unlock();
}


//线程池类的具体定义,在此类初始化的时候需要同时对成员变量m_leisureThreadList调用其构造函数进行初始化，
/*如果我们有一个类成员，它本身是一个类或者是一个结构，而且这个成员它只有一个带参数的构造函数，
而没有默认构造函数，这时要对这个类成员进行初始化，就必须调用这个类成员的带参数的构造函数，
如果没有初始化列表，那么他将无法完成第一步，就会报错。*/
ThreadPool::ThreadPool(const size_t counts):m_leisureThreadList(counts),m_bExit(false) {
	m_threadCounts = counts;
	m_bRunning.store(true);		//用于暂停管理线程运行
	m_bEnd.store(true);			//用于终止管理线程运行
	m_thread = thread(&ThreadPool::run, this);		//线程池对象的任务分配线程，即管理线程
}

ThreadPool::~ThreadPool() {
	if (!m_bExit) {
		exit();		//析构时调用此函数，会终止管理线程（入口函数run()函数中），调用leisureList的stop()函数（其中调用WorkThread的stop（）函数）
	}
}

size_t ThreadPool::threadCounts() {
	return m_threadCounts.load();	//	原子类型
}

bool ThreadPool::isRunning() {
	return m_bRunning.load();		//原子类型
}

//管理线程入口函数
void ThreadPool::run() {
	while (true) {
		if (!m_bEnd.load()) {		//m_bEnd用于终止管理者线程的运行，若没有并且任务队列为空则管理线程退出
			m_taskMutex.lock();
			if (m_taskList.empty()) {
				m_taskMutex.unlock();
				break;				//运行m_bEnd为false，任务队列为空，终止循环
			}
			m_taskMutex.unlock();
		}

		//若管理线程暂停执行，进入阻塞状态
		{
			unique_lock<mutex> lockRunning(m_runningMutex);
			m_condition_running.wait(lockRunning, [this]() {	//线程池类对象与管理者线程公用，m_condition_running用于两者间的通信，管理线程暂停
				return this->m_bRunning.load(); });				//线程池类对象暂停运行stop()函数将m_bRunning设为false则管理者线程阻塞在此处
		}
		Task * task = nullptr;
		WorkThread * thread = nullptr;
		//如果没有任务并且正在运行，则阻塞
		{
			unique_lock<mutex> lock(m_taskMutex);	//需要操作taskList，加锁
			m_condition_task.wait(lock, [this]() {
				return !(this->m_taskList.empty() && this->m_bEnd.load()); });
			//若被唤醒，检查是有新任务进列表还是程序终止
			if (!m_taskList.empty()) {
				task = m_taskList.front();		//若是有新任务则将任务取出
				m_taskList.pop();
			}
		}
		//选择空闲的线程执行任务,m_leisureThreadList内部的操作已经是加锁保证安全的，此处不需要加锁
		do {
			thread = m_leisureThreadList.top();
			m_leisureThreadList.pop();
			m_leisureThreadList.push(thread);
		} while (thread->isExecuting());	//直到找到一个未运行的线程,m_task不为空则正在运行

		//找到后通知线程执行
		thread->assign(task);			//上一个isExecuting()已经判断m_task为nullptr，若是因为终止m_bEnd=false进入，怎么分配task=nullptr，不影响工作线程
		thread->notify();
	}
}

//操作任务列表，加锁。添加好任务后需要唤醒消费者管理线程消费
void ThreadPool::addTask(Task * task) {
	if (task == nullptr) { return; }
	m_taskMutex.lock();
	m_taskList.push(task);
	m_taskMutex.unlock();
	//通知正在等待的线程,也就是唤醒管理者线程run函数中的等待，
	m_condition_task.notify_one();
}

void ThreadPool::start() {
	m_bRunning.store(true);
	m_condition_running.notify_one();	//唤醒因暂停阻塞在run函数中的m_condition_running
}

void ThreadPool::stop() {				//此处为暂停执行与start()函数对应，只是让管理者线程处于阻塞状态，并不影响已工作线程
	m_bRunning.store(false);			//设为false后，由于run函数的循环，会进入m_condition_running的阻塞
}

void ThreadPool::exit() {	//！！！此处的exit()函数对应leisureList的stop和WorkThread的stop，真正让线程处理完已分配的任务后终止
	m_bEnd.store(false);	//置为false后需要唤醒可能处于m_condition_task阻塞的管理者线程
	m_condition_task.notify_all();
	m_mutexThread.lock();
	if (m_thread.joinable()) {
		m_thread.join();
	}
	m_mutexThread.unlock();
	m_leisureThreadList.stop();		//工作线程停止，内部实现会把线程中已经存在的任务执行完再退出
	m_bExit = true;
}