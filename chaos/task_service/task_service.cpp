/*
 * Copyright 2012, Yunjie Lu.  All rights reserved.
 * https://github.com/lyjdamzwf/chaos
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the License file.
 */
#include <unistd.h>
#include <fcntl.h>

#include <stdlib.h>
#include <assert.h>

#include <chaos/task_service/task_service.h>

/*!
 *  @file           task_service.cpp
 *  @author         yunjie.lu
 *  @email          lyjdamzwf@gmail.com
 *  @weibo          http://weibo.com/crazyprogramerlyj
 *  @date           2012.3.24
 *  @brief          task service
 *  @last changed
 *
 */

namespace chaos
{

namespace task_service
{

task_service_t::task_service_t(const string& service_name_)
    :   m_started(false),
        m_service_name(service_name_),
        m_stop_signal(false),
        m_thread_num(0),
        m_fetch_num_per_loop(0xffffffff),
        m_async_event_trigger(false),
        m_cur_lock_thread(0)
{
#if COMMUNICATION_MODE == PIPE || COMMUNICATION_MODE == SOCKET_PAIR || COMMUNICATION_MODE == EVENTFD
    memset(&m_comm_fds, 0 , sizeof(m_comm_fds));
#endif
}

task_service_t::~task_service_t()
{
    stop();
}

void read_byte_from_fd(fd_t fd_, int event_, void* arg_)
{
    char byte;
    int ret;
    if ((ret = ::read(fd_, &byte, sizeof(char))) != sizeof(char))
    {
        LOGWARN((TASK_SERVICE_MODULE, "read_byte_from_fd read failed ret:[%d]",
                    ret
               ));
    }
}

int task_service_t::start(int32_t thread_num_)
{
    LOGTRACE((TASK_SERVICE_MODULE, "task_service_t::start service(%s) begin", m_service_name.c_str()));

    if (m_started)
    {
        LOGWARN((TASK_SERVICE_MODULE, "task_service_t::start service(%s) has started, return.", m_service_name.c_str()));

        return -1;
    }

    m_thread_num = thread_num_;

    for (int i = 0; i < m_thread_num; ++i)
    {
        thread_t* tmp_thread = new thread_t;
        if (NULL == tmp_thread)
        {
            LOGERROR((TASK_SERVICE_MODULE, "task_service_t::start service(%s) new thread_t failed, return.", m_service_name.c_str()));
            return -1;
        }

        m_thread_group.add(tmp_thread);
    }

    //! yunjie: ??????????????????????????????
    m_task_queue.initialize(false);
    //! yunjie: ????????????????????????task_service??????, timer??????????????????????????????
    bool lock_flag = (m_thread_num <= 1) ? false : true;
    m_timer_manager.initialize(lock_flag);
    m_io_handler.initialize(lock_flag);

    //! yunjie: ?????????IPC
#if COMMUNICATION_MODE == PIPE
    if (::pipe(m_comm_fds) < 0)
    {
        LOGERROR((TASK_SERVICE_MODULE, "task_service_t::start service(%s) create pipe failed, exit", m_service_name.c_str()));
        exit(EXIT_FAILURE);
    }

    //! yunjie: ??????pipe????????????touch_atime??????
    fcntl(m_comm_fds[0], F_SETFL, O_NOATIME);
    register_io_event(m_comm_fds[0], READ_EVENT_FLAG, read_byte_from_fd, NULL, true);
#elif COMMUNICATION_MODE == SOCKET_PAIR
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, m_comm_fds) < 0)
    {
        LOGERROR((TASK_SERVICE_MODULE, "task_service_t::start service(%s) create socketpair failed, exit", m_service_name.c_str()));
        exit(EXIT_FAILURE);
    }
    register_io_event(m_comm_fds[0], READ_EVENT_FLAG, read_byte_from_fd, NULL, true);
#elif COMMUNICATION_MODE == EVENTFD
    if ((m_comm_fds[1] = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) < 0)
    {
        LOGERROR((TASK_SERVICE_MODULE, "task_service_t::start service(%s) create eventfd failed, exit", m_service_name.c_str()));
        exit(EXIT_FAILURE);
    }
    register_io_event(m_comm_fds[1], READ_EVENT_FLAG, read_byte_from_fd, NULL, true);
#endif

    start_thread_t start_thread(*this);
    m_thread_group.exec_all(start_thread);

    m_started = true;

    LOGTRACE((TASK_SERVICE_MODULE, "task_service_t::start service(%s) end", m_service_name.c_str()));
    return 0;
}

int task_service_t::stop()
{
    LOGTRACE((TASK_SERVICE_MODULE, "task_service_t::stop service(%s) begin", m_service_name.c_str()));

    if (!m_started)
    {
        LOGWARN((TASK_SERVICE_MODULE, "task_service_t::stop service(%s) has stopped, return.", m_service_name.c_str()));

        return -1;
    }

    //! yunjie: ??????????????????, ??????????????????????????????
    post_async_stop_signal();

    //! yunjie: thread* ?????????join_all()??????delete
    m_thread_group.join_all();

#if COMMUNICATION_MODE == PIPE || COMMUNICATION_MODE == SOCKET_PAIR
    ::close(m_comm_fds[0]);
    ::close(m_comm_fds[1]);
#elif COMMUNICATION_MODE == EVENTFD
    ::close(m_comm_fds[1]);
#endif

    m_thread_num = 0;
    m_fetch_num_per_loop = 0xffffffff;

    m_started = false;

    LOGTRACE((TASK_SERVICE_MODULE, "task_service_t::stop service(%s) end", m_service_name.c_str()));
    return 0;
}

int task_service_t::post_async_stop_signal()
{
    LOGTRACE((TASK_SERVICE_MODULE, "task_service_t::post_async_stop_signal service(%s) begin", m_service_name.c_str()));

    this->post(bindfunc(this, &task_service_t::set_stop_signal, true));

    LOGTRACE((TASK_SERVICE_MODULE, "task_service_t::post_async_stop_signal service(%s) end", m_service_name.c_str()));

    return 0;
}

bool task_service_t::is_run_on_service()
{
    return m_thread_group.check_is_self(thread_t::get_current_thread_id());
}

void task_service_t::exec_task(thread_t* thd_)
{
    LOGTRACE((TASK_SERVICE_MODULE, "task_service_t::exec_task service(%s) begin", m_service_name.c_str()));

    task_queue_t::container_t tasks;
    uint32_t all_task_num = 0;
    uint32_t thread_num = m_thread_group.size();

    if (NULL == thd_)
    {
        LOGWARN((TASK_SERVICE_MODULE, "task_service_t::exec_task service(%s) thread is NULL, return", m_service_name.c_str()));
        return;
    }

    if (!thread_num)
    {
        LOGWARN((TASK_SERVICE_MODULE, "task_service_t::exec_task service(%s) thread_num equals zero, return", m_service_name.c_str()));
        return;
    }

    while (1)
    {
        try
        {
            struct timeval cached_now;

            m_timer_manager.flush_time();
            cached_now = m_timer_manager.get_cached_time();

            //! yunjie: ??????????????????
            m_timer_manager.exec();

            //! yunjie: ????????????IO??????, ?????????????????? ???/??? ??????
            int wake_num = m_io_handler.wait_io_notification();


            //! yunjie: lock begin
            {
                scope_mutex_lock_t lock(m_mutex);
                m_cur_lock_thread = thd_->get_thread_id();

                if (m_stop_signal && m_task_queue.empty())
                {
                    goto OUT;
                }

                // yunjie: ?????????????????? ??? I/O??????????????????cond_wait
                if (!m_async_event_trigger && !wake_num)
                {
#if COMMUNICATION_MODE == PTHREAD_COND_VAR
                    m_cond.wait(m_mutex, cached_now, 0, TIMEDOUT_US);
#elif COMMUNICATION_MODE == SLEEP
                    thread_t::usleep(TIMEDOUT_US);
#endif
                }

                if (m_async_event_trigger)
                {
                    //! yunjie: ???????????????????????????m_fetch_num_per_loop???????????????????????????????????????
                    //          ???????????????????????????, ???????????????????????????????????????
                    m_task_queue.fetch_task(tasks, all_task_num, m_fetch_num_per_loop);

                    m_async_event_trigger = false;
                }
            }
            //! yunjie: lock end


            //! yunjie: ??????????????????
            if (!tasks.empty())
            {
                for (deque<async_method_t>::iterator it = tasks.begin(); it != tasks.end(); ++it)
                {
                    EXCEPTION_BEGIN
                            (*it)();
                    EXCEPTION_END(TASK_SERVICE_MODULE, "exec task")
                }

                tasks.clear();
            }
        }
        catch (const std::exception& ex)
        {
            LOGERROR((TASK_SERVICE_MODULE, "task_service_t::exec_task service(%s) exception caught info:[%s]",
                        m_service_name.c_str(),
                        ex.what()
                     ));
            //! abort();
        }
        catch (...)
        {
            LOGERROR((TASK_SERVICE_MODULE, "task_service_t::exec_task service(%s) unknown exception caught info",
                        m_service_name.c_str()
                     ));
            //! abort();
        }
    }

OUT:

    m_timer_manager.clear();

    LOGTRACE((TASK_SERVICE_MODULE, "task_service_t::exec_task service(%s) end", m_service_name.c_str()));
}

void task_service_t::set_stop_signal(bool signal_)
{
    LOGTRACE((TASK_SERVICE_MODULE, "task_service_t::set_stop_signal service(%s) m_stop_signal:[%d] begin", m_service_name.c_str(), m_stop_signal));

    m_stop_signal = signal_;

    LOGTRACE((TASK_SERVICE_MODULE, "task_service_t::set_stop_signal service(%s) end", m_service_name.c_str()));
}

int task_service_t::post(
                            const async_method_t&   async_method_,
                            void*                   ext_data_,
                            task_prior_e            prior_,
                            bool                    exec_local_
                        )
{
    /* yunjie: post?????????????????????LOGXXXX,?????????????????????(??????LOGXXXX?????????post???????????????) */


    //! yunjie: ????????????????????????????????????????????????, ?????????????????????, ?????????????????????????????????.
    if (exec_local_ && is_run_on_service())
    {
        const_cast<async_method_t&>(async_method_)();

        return 0;
    }

    scope_mutex_lock_t lock(m_mutex);

    m_task_queue.push(async_method_);
    m_async_event_trigger = true;

#if COMMUNICATION_MODE == PTHREAD_COND_VAR
    m_cond.broadcast();
#elif COMMUNICATION_MODE == PIPE || COMMUNICATION_MODE == SOCKET_PAIR || COMMUNICATION_MODE == EVENTFD
        char byte;
        int ret = 0;
        if ((ret = ::write(m_comm_fds[1], &byte, sizeof(char))) != sizeof(char))
        {
            //! yunjie: post?????????????????????LOGXXXX,?????????????????????(??????LOGXXXX?????????post???????????????)
        }
#endif

    return 0;
}

int task_service_t::clear_all_task()
{
    scope_mutex_lock_t lock(m_mutex);
    m_task_queue.clear_all_queue();

    return 0;
}

void task_service_t::register_timer(
                                    uint32_t                        interval_,
                                    const time_event_callback_t&    callback_,
                                    bool                            persist_,
                                    time_t                          start_time_
                                   )
{
    //! yunjie: ??????one loop mutilthread?????????????????????, ?????????callback???????????????????????????????????????
    bool is_allow_exec_local = (m_thread_num > 1 ? false : true);

    this->post(bindfunc(
                        &m_timer_manager,
                        &timer_manager_t::register_timer,
                        interval_,
                        callback_,
                        persist_,
                        start_time_
                       ),
               NULL,
               TASK_PRIOR_NORMAL,
               is_allow_exec_local
              );
}

void task_service_t::register_io_event(
                                        fd_t                    fd_,
                                        int                     event_type_flag_,
                                        callback_on_event_t     callback_,
                                        void*                   cb_arg_,
                                        bool                    is_persist_
                                     )
{
    //! yunjie: ??????one loop mutilthread?????????????????????, ?????????callback???????????????????????????????????????
    bool is_allow_exec_local = (m_thread_num > 1 ? false : true);

    this->post(bindfunc(
                        &m_io_handler,
                        &io_multiplex_handler_t::register_io_event,
                        fd_,
                        event_type_flag_,
                        callback_,
                        cb_arg_,
                        is_persist_
                       ),
               NULL,
               TASK_PRIOR_NORMAL,
               is_allow_exec_local
              );
}

void task_service_t::remove_fd_from_epoll(fd_t fd_)
{
    //! yunjie: ??????one loop mutilthread?????????????????????, ?????????callback???????????????????????????????????????
    bool is_allow_exec_local = (m_thread_num > 1 ? false : true);

    this->post(bindfunc(
                        &m_io_handler,
                        &io_multiplex_handler_t::remove_fd_from_epoll,
                        fd_
                       ),
               NULL,
               TASK_PRIOR_NORMAL,
               is_allow_exec_local
              );
}

}

}

