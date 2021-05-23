#include"ThreadPool.h"
#include<iostream>
#include<ctime>
#include<fstream>
#include<chrono>

using namespace std;

mutex mtx;
//自定义任务函数继承于：Task抽象类
class myTask :public Task {
	int m_data;
public:
	myTask(int data) {
		m_data = data;
	}
	void run() {
		mtx.lock();
		cout << "线程： " << this_thread::get_id() << "输出： " << m_data << endl;
		mtx.unlock();
	}
};

int main() {
	ThreadPool pool(5);
	for (int i = 0; i < 10000; i++) {
		pool.addTask(new myTask(i));
	}
	pool.exit();
	return 0;
}