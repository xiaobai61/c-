/*
 * Copyright 2012, Yunjie Lu.  All rights reserved.
 * https://github.com/lyjdamzwf/chaos
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the License file.
 */

#include <netinet/tcp.h>

#include <chaos/network/connection.h>
#include <chaos/network/work_service.h>

/*!
 *  @file           connection.cpp
 *  @author         yunjie.lu
 *  @email          lyjdamzwf@gmail.com
 *  @weibo          http://weibo.com/crazyprogramerlyj
 *  @date           2012.4.16
 *  @brief          connection
 *  @last changed
 *
 */

namespace chaos
{

namespace network
{

//! ------------------------------------ static function begin ------------------------------------

void connection_t::on_peer_event(fd_t fd_, int event_type_, void* arg_)
{
    LOGTRACE((CONNECTION_MODULE, "connection_t::on_peer_event args-[fd:%d, event_type:%d] begin", fd_, event_type_));

    conn_ptr_t conn_ptr = (conn_ptr_t)arg_;
    if (NULL == conn_ptr)
    {
        LOGWARN((CONNECTION_MODULE, "connection_t::on_peer_event arg_ is NULL, return. args-[fd:%d, event_type:%d]", fd_, event_type_));
        return;
    }

    task_service_t* service_ptr = conn_ptr->get_service_ptr();
    if (NULL == service_ptr)
    {
        LOGWARN((CONNECTION_MODULE, "connection_t::on_peer_event service_ptr is NULL, return. args-[fd:%d, event_type:%d]", fd_, event_type_));
        return;
    }

    switch (event_type_)
    {
        case IO_READ_EVENT:
        {
            conn_ptr->on_recv_data();
        }
        break;
        case IO_WRITE_EVENT:
        {
            conn_ptr->on_send_data();
        }
        break;
        case IO_ERROR_EVENT:
        {
            conn_ptr->on_error_occur();
        }
        break;
        default:
        {
            LOGWARN((CONNECTION_MODULE, "connection_t::on_peer_event event_type error args-[fd:%d, event_type:%d]", fd_, event_type_));
        }
    }

    LOGTRACE((CONNECTION_MODULE, "connection_t::on_peer_event args-[fd:%d, event_type:%d] end", fd_, event_type_));
}

int connection_t::async_close(const struct conn_id_t& conn_id_, bool is_del_from_hb_, conn_event_e close_type_)
{
    LOGTRACE((CONNECTION_MODULE, "connection_t::async_close args-[fd:%d] begin", conn_id_.socket));

    work_service_t* service_ptr = conn_id_.service_ptr;
    if (NULL == service_ptr)
    {
        LOGWARN((CONNECTION_MODULE, "connection_t::async_close error args-[fd:%d]", conn_id_.socket));
        return -1;
    }

    //! yunjie: sync_close_i?????????????????????????????????, ???????????????delete connection_t??????, ???????????????????????????false
    service_ptr->post(bindfunc(&connection_t::sync_close_i, conn_id_, is_del_from_hb_, close_type_), NULL, TASK_PRIOR_NORMAL, false);

    LOGTRACE((CONNECTION_MODULE, "connection_t::async_close args-[fd:%d] end", conn_id_.socket));
    return 0;
}

int connection_t::async_send(const struct conn_id_t& conn_id_, const packet_wrapper_t& msg_)
{
    LOGTRACE((CONNECTION_MODULE, "connection_t::async_send begin"));

    work_service_t* service_ptr = conn_id_.service_ptr;
    if (NULL == service_ptr)
    {
        LOGWARN((CONNECTION_MODULE, "connection_t::async_send service_ptr is NULL, return."));
        return -1;
    }

    service_ptr->post(bindfunc(&connection_t::sync_send_wrapper_i, conn_id_, msg_));

    LOGTRACE((CONNECTION_MODULE, "connection_t::async_send end"));
    return 0;
}

int connection_t::async_send(const struct conn_id_t& conn_id_, const char* msg_, uint32_t size_)
{
    LOGTRACE((CONNECTION_MODULE, "connection_t::async_send begin"));

    work_service_t* service_ptr = conn_id_.service_ptr;
    if (NULL == service_ptr)
    {
        LOGWARN((CONNECTION_MODULE, "connection_t::async_send service_ptr is NULL, return."));
        return -1;
    }

    if (service_ptr->is_run_on_service())
    {
        //! yunjie: ????????????????????????, ????????????
        sync_send_i(conn_id_, msg_, size_);
    }
    else
    {
        //! yunjie: ?????????????????????????????????, ?????????packet_wrapper_t, post????????????
        packet_wrapper_t msg_wrapper(msg_, size_);
        service_ptr->post(bindfunc(&connection_t::sync_send_wrapper_i, conn_id_, msg_wrapper));
    }

    LOGTRACE((CONNECTION_MODULE, "connection_t::async_send end"));
    return 0;
}

int connection_t::sync_close_i(const struct conn_id_t& conn_id_, bool is_del_from_hb_, conn_event_e close_type_)
{
    LOGTRACE((CONNECTION_MODULE, "connection_t::sync_close_i args-[fd:%d] begin", conn_id_.socket));

    work_service_t* service_ptr = conn_id_.service_ptr;
    if (NULL == service_ptr)
    {
        LOGWARN((CONNECTION_MODULE, "connection_t::sync_close_i error, return. args-[fd:%d]", conn_id_.socket));
        return -1;
    }

    conn_sptr_t conn_sptr = service_ptr->get_conn(conn_id_);
    if (NULL == conn_sptr)
    {
        LOGWARN((CONNECTION_MODULE, "connection_t::sync_close_i connection not found, it maybe closed, return. args-[fd:%d]", conn_id_.socket));
        return -1;
    }

    if (ST_CLOSED == conn_sptr->get_status())
    {
        LOGTRACE((CONNECTION_MODULE, "connection_t::sync_close_i connection has closed, return. args-[fd:%d]", conn_id_.socket));
        return 0;
    }

    //! yunjie: ??????connection???socket fd, ???epoll?????????
    conn_sptr->close_i();

    if (is_del_from_hb_ && conn_sptr->is_enable_hb())
    {
        //! yunjie: ???heart_beart?????????connection
        service_ptr->async_del_hb_element(conn_sptr->m_conn_id);
    }

    //! yunjie: connection?????????????????????????????????,
    //!         ???????????????async_del_connection???
    //!         delete connection??????
    conn_status_e conn_status = conn_sptr->get_status();
    on_conn_event_t event_callback = conn_sptr->m_conn_event_callback;
    void* user_data = conn_sptr->m_user_data;

    //! yunjie: ????????????????????????????????????????????????, ?????????????????????
    if (EV_DECONSTRUCT != close_type_)
    {
        //! yunjie: ??????????????????connection?????????(delete)
        //!         ???: ???????????????async_del_connection??????????????????,
        //!         ??????sync_close_i????????????work??????????????????, ??????
        //!         ?????????????????????????????????????????????
        service_ptr->async_del_connection(conn_id_);
    }

    //! yunjie: ???delete??????callback?????????, ???????????????????????????????????????
    if (NULL != event_callback)
    {
        //! yunjie: ???????????????????????????
        event_callback(close_type_, conn_status, conn_id_, user_data);
    }

    LOGTRACE((CONNECTION_MODULE, "connection_t::sync_close_i args-[fd:%d] end", conn_id_.socket));
    return 0;
}

int connection_t::sync_send_wrapper_i(
                                        const struct conn_id_t&         conn_id_,
                                        const packet_wrapper_t&         msg_
                                     )
{
    LOGTRACE((CONNECTION_MODULE, "connection_t::sync_send_wrapper_i args-[fd:%d] begin", conn_id_.socket));

    work_service_t* service_ptr = conn_id_.service_ptr;
    if (NULL == service_ptr)
    {
        LOGWARN((CONNECTION_MODULE, "connection_t::sync_send_wrapper_i error, return. args-[fd:%d]", conn_id_.socket));
        return -1;
    }

    conn_sptr_t conn_sptr = service_ptr->get_conn(conn_id_);
    if (NULL == conn_sptr)
    {
        LOGWARN((CONNECTION_MODULE, "connection_t::sync_send_wrapper_i close failed, because connection not found, return. args-[fd:%d]", conn_id_.socket));
        return -1;
    }

    if (ST_CLOSED == conn_sptr->get_status())
    {
        LOGWARN((CONNECTION_MODULE, "connection_t::sync_send_wrapper_i connection has closed, return. args-[fd:%d]", conn_id_.socket));
        return 0;
    }

    conn_sptr->m_write_buffer.append(msg_.data(), msg_.size());
    conn_sptr->start_drive_send_i();

    LOGTRACE((CONNECTION_MODULE, "connection_t::sync_send_wrapper_i args-[fd:%d] end", conn_id_.socket));
    return 0;
}

int connection_t::sync_send_i(
                                    const struct conn_id_t& conn_id_,
                                    const char* msg_,
                                    uint32_t size_
                                   )
{
    work_service_t* service_ptr = conn_id_.service_ptr;
    if (NULL == service_ptr)
    {
        LOGWARN((CONNECTION_MODULE, "connection_t::sync_send_i error, return. args-[fd:%d]", conn_id_.socket));
        return -1;
    }

    conn_sptr_t conn_sptr = service_ptr->get_conn(conn_id_);
    if (NULL == conn_sptr)
    {
        LOGWARN((CONNECTION_MODULE, "connection_t::sync_send_i close failed, because connection not found, return. args-[fd:%d]", conn_id_.socket));
        return -1;
    }

    if (ST_CLOSED == conn_sptr->get_status())
    {
        LOGWARN((CONNECTION_MODULE, "connection_t::sync_send_i connection has closed, return. args-[fd:%d]", conn_id_.socket));
        return 0;
    }

    conn_sptr->m_write_buffer.append(msg_, size_);
    conn_sptr->start_drive_send_i();

    return 0;
}

//! ------------------------------------ static function end ------------------------------------





//! ------------------------------------ member function begin ------------------------------------

connection_t::connection_t()
    :
        m_socket(0),
        m_service_ptr(NULL),
        m_conn_type(T_UNKNOWN),
        m_conn_status(ST_UNKNOWN),
        m_conn_event_callback(NULL),
        m_sending_flag(false),
        m_enable_hb(false),
        m_user_data(NULL)
{
    memset(&m_timestamp, 0, sizeof(struct timeval));
}

connection_t::~connection_t()
{
    if (NULL != m_conn_event_callback)
    {
        m_conn_event_callback(EV_DECONSTRUCT, m_conn_status, m_conn_id, m_user_data);
    }

    //! yunjie: ??????I/O???????????????
    m_read_buffer.clear();
    m_write_buffer.clear();
}


int connection_t::initialize(
                            fd_t                        socket_,
                            const timeval&              timestamp_,
                            work_service_t*             work_service_,
                            conn_type_e                 conn_type_,
                            on_conn_event_t             event_func_,
                            const inner_conn_sptr_t&    self_sptr_,
                            network_config_t*           config_,
                            bool                        enable_hb_
                            )
{
    if (-1 == network_tool_t::make_socket_nonblocking(socket_))
    {
        LOGWARN((CONNECTION_MODULE, "connection_t::initialize make_socket_nonblocking failed"));
        return -1;
    }

    //! yunjie: ???????????????
    m_socket = socket_;
    m_timestamp.tv_sec = timestamp_.tv_sec;
    m_timestamp.tv_usec = timestamp_.tv_usec;
    m_service_ptr = work_service_;

    //! yunjie: conn id
    m_conn_id.socket = socket_;
    m_conn_id.timestamp.tv_sec = timestamp_.tv_sec;
    m_conn_id.timestamp.tv_usec = timestamp_.tv_usec;
    m_conn_id.service_ptr = work_service_;

    m_conn_type = conn_type_;
    m_conn_status = ST_ESTABLISHED;

    m_conn_event_callback = event_func_;

    m_self_sptr = self_sptr_;

    m_config_holder.set_config(config_);

    m_enable_hb = enable_hb_;

    //! yunjie: ??????I/O???????????????????????????
    m_write_buffer.set_buffer_max_limit((*m_config_holder).max_send_buffer_size);
    m_read_buffer.set_buffer_max_limit((*m_config_holder).max_recv_buffer_size);

    //! yunjie: ????????????????????????
    if (T_ACTIVE == conn_type_)
    {
        //! yunjie: ??????????????????
        int sndbuf_size = (*m_config_holder).tcp_sndbuf_size;
        if(-1 == ::setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, &sndbuf_size, sizeof(sndbuf_size)))
        {
            LOGWARN((CONNECTION_MODULE, "connection_t::initialize set socket option size of sndbuf failed."));
            TEMP_FAILURE_RETRY(::close(m_socket));
            return -1;
        }

        int rcvbuf_size = (*m_config_holder).tcp_rcvbuf_size;
        if(-1 == ::setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(rcvbuf_size)))
        {
            LOGWARN((CONNECTION_MODULE, "connection_t::initialize set socket option size of rcvbuf failed."));
            TEMP_FAILURE_RETRY(::close(m_socket));
            return -1;
        }
    }

    //! yunjie: ??????nagling??????,??????????????????????????????????????????
    if ((*m_config_holder).tcp_nodelay)
    {
        int nodelay = 1;
        if(::setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (void*)&nodelay, sizeof(nodelay)))
        {
            LOGWARN((CONNECTION_MODULE, "connection_t::initialize set nodelay failed socket:[%u]", m_socket));
            return -1;
        }
    }

    //! yunjie: connection???????????????, ??????????????????
    if (NULL != m_conn_event_callback)
    {
        m_conn_event_callback(EV_INIT_COMPLETE, m_conn_status, m_conn_id, m_user_data);
    }

    //! yunjie: ?????????event loop???
    work_service_->async_add_connection(m_self_sptr);

    if (m_enable_hb)
    {
        m_service_ptr->async_add_hb_element(m_conn_id);
    }

    //! yunjie: connection????????????event loop???, ??????????????????
    if (NULL != m_conn_event_callback)
    {
        conn_event_e conn_ev;
        if (T_PASSIVE == m_conn_type)
        {
            conn_ev = EV_ACCEPTED_COMPLETE;
        }
        else if (T_ACTIVE == m_conn_type)
        {
            conn_ev = EV_CONNECT_SUCCESS;
        }
        else
        {
            conn_ev = EV_UNKNOWN;
        }

        m_conn_event_callback(conn_ev, m_conn_status, m_conn_id, m_user_data);
    }

    return 0;
}

int connection_t::on_recv_data()
{
    LOGTRACE((CONNECTION_MODULE, "connection_t::on_recv_data begin"));

    if (m_enable_hb)
    {
        m_service_ptr->async_update_hb_element(m_conn_id);
    }

    int recv_ret = 0;
    int ret = m_read_buffer.recv_to_buffer(m_socket, recv_ret);
    if (-1 == ret)
    {
        //! yunjie: ??????????????????, ????????????callback, ??????buffer????????????????????????
        LOGWARN((CONNECTION_MODULE, "connection_t::on_recv_data recv_to_buffer failed socket fd:[%u] read buf data size:[%u]",
                m_socket,
                m_read_buffer.size()
                ));

        on_read_complete(m_read_buffer);

        return 0;
    }

    if (0 == recv_ret)
    {
        LOGWARN((CONNECTION_MODULE, "connection_t::on_recv_data recv_ret == 0 socket fd:[%u]", m_socket));

        async_close(m_conn_id, true, EV_PASSIVE_CLOSED);

        return 0;
    }
    else if (recv_ret < 0)
    {
        //! yunjie: ????????????EAGAIN???EINTR, ???????????????callback??????????????????
        if (errno != EAGAIN && errno != EINTR)
        {
            if (NULL != m_conn_event_callback)
            {
                m_conn_event_callback(EV_ERROR_OCCURRED, m_conn_status, m_conn_id, m_user_data);
            }
        }

        LOGWARN((CONNECTION_MODULE, "connection_t::on_recv_data recv_ret:[%d] socket fd:[%u] errno:[%s]", recv_ret, m_socket, STRERR));

        return 0;
    }
    else
    {
        //! yunjie: ?????????????????????????????????
        on_read_complete(m_read_buffer);
    }

    LOGTRACE((CONNECTION_MODULE, "connection_t::on_recv_data end"));
    return 0;
}

int connection_t::on_send_data()
{
    LOGTRACE((CONNECTION_MODULE, "connection_t::on_send_data begin"));

    if (m_enable_hb)
    {
        m_service_ptr->async_update_hb_element(m_conn_id);
    }

    uint32_t transferred_size = 0;

    while (m_write_buffer.size())
    {
        uint32_t msg_size = m_write_buffer.size();
        int32_t ret = ::send(m_socket, m_write_buffer.data(), msg_size, 0);

        if (0 == ret)
        {
            LOGWARN((CONNECTION_MODULE, "connection_t::on_send_data send ret == 0 socket fd:[%u]", m_socket));

            async_close(m_conn_id, true, EV_PASSIVE_CLOSED);
            return 0;
        }
        else if (ret < 0)
        {
            if (EINTR == errno)
            {
                //! yunjie: ????????????send
                LOGWARN((CONNECTION_MODULE, "connection_t::on_send_data recv EINTR send ret:[%d] socket fd:[%u] errno:[%s]",
                            ret, m_socket, STRERR
                        ));
                continue;
            }
            else if (EAGAIN == errno)
            {
                //! yunjie: ????????????I/O??????
                m_service_ptr->register_io_event(
                        m_socket,
                        WRITE_EVENT_FLAG,
                        &connection_t::on_peer_event,
                        (void*)this,
                        false
                        );
            }
            else
            {
                //! yunjie: ???EINTR,???EAGAIN, ??????????????????????????????
                if (NULL != m_conn_event_callback)
                {
                    m_conn_event_callback(EV_ERROR_OCCURRED, m_conn_status, m_conn_id, m_user_data);
                }
            }

            LOGWARN((CONNECTION_MODULE, "connection_t::on_send_data send ret:[%d] socket fd:[%u] errno:[%s]",
                        ret, m_socket, STRERR
                    ));
            break;
        }
        else
        {
            transferred_size += ret;

            m_write_buffer.drain_size(ret);

            if (ret != (int32_t)msg_size)
            {
                m_service_ptr->register_io_event(
                        m_socket,
                        WRITE_EVENT_FLAG,
                        &connection_t::on_peer_event,
                        (void*)this,
                        false
                        );
                break;
            }
        }
    }

    if (transferred_size)
    {
        //! yunjie: ?????????????????????????????????
        on_write_complete(transferred_size);
    }

    if (!m_write_buffer.size())
    {
        m_sending_flag = false;
    }

    LOGTRACE((CONNECTION_MODULE, "connection_t::on_send_data end"));
    return 0;
}

int connection_t::on_error_occur()
{
    LOGTRACE((CONNECTION_MODULE, "connection_t::on_error_occur begin"));

    if (NULL != m_conn_event_callback)
    {
        //! yunjie: ???????????????????????????
        m_conn_event_callback(EV_ERROR_OCCURRED, m_conn_status, m_conn_id, m_user_data);
    }

    LOGTRACE((CONNECTION_MODULE, "connection_t::on_error_occur end"));
    return 0;
}

int connection_t::start_drive_send_i()
{
    if (!m_sending_flag)
    {
        //! yunjie: ???????????????, ???persist
        m_service_ptr->register_io_event(
                                        m_socket,
                                        WRITE_EVENT_FLAG,
                                        &connection_t::on_peer_event,
                                        (void*)this,
                                        false
                                        );

        m_sending_flag = true;

        return 0;
    }

    return 1;
}

int connection_t::close_i()
{
    LOGTRACE((CONNECTION_MODULE, "connection_t::close_i begin"));

    //! yunjie: ???epoll????????????fd?????????
    m_service_ptr->remove_fd_from_epoll(m_socket);

    ::close(m_socket);

    m_conn_status = ST_CLOSED;
    m_read_buffer.clear();
    m_write_buffer.clear();

    m_self_sptr.reset();

    LOGTRACE((CONNECTION_MODULE, "connection_t::close_i end"));
    return 0;
}

//! ------------------------------------ member function end ------------------------------------

}

}
