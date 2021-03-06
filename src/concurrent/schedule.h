#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include"thread.h"
#include"lock.h"
#include"coroutine.h"
#include<vector>
#include<list>
#include<atomic>

	//调度器设计
	//通过线程池调度协程
  //作为io调度器的基类

	class schedule{
	public:
		typedef std::shared_ptr<schedule>schedule_ptr;
		schedule(size_t thread_num, const std::string& name,bool use_mainfiber=true);
		virtual ~schedule();
	private:
		std::string m_name;//调度器名称
		Mutex* m_mutex;
		std::vector<Thread::thread_ptr>m_threads;//内部线程池
		size_t m_threadsnum;//线程数
		std::atomic<uint32_t>m_freethreadnum{ 0 };//空闲等待的线程数量
		std::atomic<uint32_t>m_activethreadnum{ 0 };//
		//使用原子操作，避免线程安全问题
		bool m_running;//运行状态
		bool m_autostop;//能否自动停止运行
		std::vector<int>m_threads_id;
		uint32_t m_root_threadid;
		coroutine::coroutine_ptr m_root_coroutine;//调度协程		
	public:
		static coroutine* get_currentcoroutine();//获取调度器当前协程
		static schedule* get_this_schedule();//获取当前调度器
		static void set_this_schedule(schedule*);
		//static std::string get_this_name();
		void start();//启动
		void run();//运行
		void stop();//停止
	protected:
	  bool can_stop();//判断是否具有停止的条件
	  virtual	void notify();//消息通知
		virtual void idle();//空闲处理

	private:
		struct coroutine_s {
			std::shared_ptr<coroutine> s_coroutine;
			std::function<void()>s_function;
			int thread_id;//执行任务的线程id
			coroutine_s():
			 s_coroutine(nullptr),s_function(nullptr),thread_id(0){};
			coroutine_s(int _thread_id, std::function<void()>_function) :
			 thread_id(_thread_id),s_function(_function) {};
			coroutine_s(int _thread_id, std::function<void()>*_function) :
			  thread_id(_thread_id) {
				s_function.swap(*_function);
			}
			coroutine_s(int _thread_id, coroutine::coroutine_ptr fiber) :
			 thread_id(_thread_id),s_coroutine(fiber){};
			coroutine_s(int _thread_id, coroutine::coroutine_ptr *fiber) :
			  thread_id(_thread_id) {
				s_coroutine.swap(*fiber);
			}
			void reset() {
				thread_id = -1;
				s_function = NULL;
				s_coroutine.reset();
			}
		};
		//任务接口的结构，绑定协程（协程里面绑定任务回调函数）
	private:
		std::list<schedule::coroutine_s>m_coroutines;
		//存储执行的任务

		//分配任务
		template<typename callback_fiber>
		bool m_dispatch(int thread_id, callback_fiber function) {
			bool need_call = m_coroutines.empty();
			schedule::coroutine_s task(thread_id, function);
			if (task.s_coroutine || task.s_function)
				m_coroutines.push_back(task);
			return need_call;
		}//向任务队列插入任务 当任务队列为空时，需要进行唤醒

	public:
		template<typename callback_fiber>
		void dispatch(int thread_id, callback_fiber function) {
			bool need_call = false;
			{
				locallock<Mutex>lock(*m_mutex);
				need_call = m_dispatch(thread_id, function);
			}
			if (need_call) {
				notify();
			}
		}

		template<typename callback_fiber>
		void dispatch(callback_fiber function) {
			bool need_call = false;
			{
				locallock<Mutex>lock(*m_mutex);
				need_call = m_dispatch(-1, function);
			}
			if (need_call)
				notify();
		}//一次添加一个任务

		template<typename callback_fiber_iterator>
		void dispatch(int thread_id,callback_fiber_iterator& begin, callback_fiber_iterator& end) {
			bool need_call = false;
			{
				locallock<Mutex>lock(*m_mutex);
				while (begin != end)
				{
					dispatch(thread_id,*begin);
					begin++;
					if (need_call) {
						notify();
					}
				}
			}
		}

		template<typename callback_fiber_iterator>
		void dispatch(callback_fiber_iterator& begin,callback_fiber_iterator& end){
       bool need_call=false;
			{
				locallock<Mutex>lock(*m_mutex);
				while (begin != end){
					dispatch(-1,*begin);
					begin++;
					if (need_call) {
						notify();
					}
				}
			}
		}
		//采用迭代器一次性添加批量任务
	};

#endif
