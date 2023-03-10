#ifndef _CHAOS_SERVER_CONFIG_H_
#define _CHAOS_SERVER_CONFIG_H_

#include "../misc.h"

#define CONFIG_ECHO_SERVER_TABLE            "echo_server_config"
#define CONFIG_OPTION_DAEMON                "daemon"
#define CONFIG_OPTION_LOG_LEVEL             "log_level"
#define CONFIG_OPTION_LOG_PRINT_TO_FILE     "log_print_to_file"
#define CONFIG_OPTION_LOG_PRINT_TO_SCREEN   "log_print_to_screen"
#define CONFIG_OPTION_TCP_SNDBUF_SIZE       "tcp_sndbuf_size"
#define CONFIG_OPTION_TCP_RCVBUF_SIZE       "tcp_rcvbuf_size"
#define CONFIG_OPTION_MAX_SEND_BUFFER_SIZE  "max_send_buffer_size"
#define CONFIG_OPTION_MAX_RECV_BUFFER_SIZE  "max_recv_buffer_size"
#define CONFIG_OPTION_TCP_NODELAY           "tcp_nodelay"
#define CONFIG_OPTION_ADDRESS               "address"
#define CONFIG_OPTION_PORT                  "port"
#define CONFIG_OPTION_CONN_TIMEOUT          "conn_timeout"
#define CONFIG_OPTION_WORK_THREAD_NUM       "work_thread_num"

#define CONFIG_LOG_MODULES_TABLE            "log_modules"

struct server_config_t
{
    server_config_t()
        :
            daemon(0),
            log_level(0),
            log_print_to_file(0),
            log_print_to_screen(0),
            tcp_sndbuf_size(0),
            tcp_rcvbuf_size(0),
            max_send_buffer_size(0),
            max_recv_buffer_size(0),
            tcp_nodelay(0),
            port(0),
            conn_timeout(0),
            work_thread_num(0)
    {
    }

    //! yunjie: 进程相关选项配置
    int32_t                     daemon;
    int32_t                     log_level;
    int32_t                     log_print_to_file;
    int32_t                     log_print_to_screen;

    //! yunjie: 网络选项配置
    int32_t                     tcp_sndbuf_size;
    int32_t                     tcp_rcvbuf_size;
    int32_t                     max_send_buffer_size;
    int32_t                     max_recv_buffer_size;
    int32_t                     tcp_nodelay;

    //! yunjie: 服务相关选项配置
    string                      address;
    int32_t                     port;
    int32_t                     conn_timeout;
    int32_t                     work_thread_num;

    string dump()
    {
        char buf[1024] = {0};
        snprintf(buf, sizeof(buf), "------- server config dump ------\n"
                " daemon:[%d]\n"
                " log_level:[%d]\n"
                " log_print_to_file:[%d]\n"
                " log_print_to_screen:[%d]\n"
                " tcp_sndbuf_size:[%d]\n"
                " tcp_rcvbuf_size:[%d]\n"
                " max_send_buffer_size:[%d]\n"
                " max_recv_buffer_size:[%d]\n"
                " tcp_nodelay:[%d]\n"
                " address:[%s]\n"
                " port:[%d]\n"
                " conn_timeout:[%d]\n"
                " work_thread_num:[%d]\n"
                "------- server config dump ------\n",
                daemon,
                log_level,
                log_print_to_file,
                log_print_to_screen,
                tcp_sndbuf_size,
                tcp_rcvbuf_size,
                max_send_buffer_size,
                max_recv_buffer_size,
                tcp_nodelay,
                address.c_str(),
                port,
                conn_timeout,
                work_thread_num
                );

        return buf;
    }
};

#define CFG_ASSIGN_INT(ser_mem, lua_key) \
    server_config_.ser_mem = atoi(lua_config_.get(CONFIG_ECHO_SERVER_TABLE, lua_key).c_str());

#define CFG_ASSIGN_STR(ser_mem, lua_key) \
    server_config_.ser_mem = lua_config_.get(CONFIG_ECHO_SERVER_TABLE, lua_key).c_str();


static void process_lua_config(const lua_config_t& lua_config_, server_config_t& server_config_)
{
    CFG_ASSIGN_INT(daemon, CONFIG_OPTION_DAEMON);
    CFG_ASSIGN_INT(log_level, CONFIG_OPTION_LOG_LEVEL);
    CFG_ASSIGN_INT(log_print_to_file, CONFIG_OPTION_LOG_PRINT_TO_FILE);
    CFG_ASSIGN_INT(log_print_to_screen, CONFIG_OPTION_LOG_PRINT_TO_SCREEN);
    CFG_ASSIGN_INT(tcp_sndbuf_size, CONFIG_OPTION_TCP_SNDBUF_SIZE);
    CFG_ASSIGN_INT(tcp_rcvbuf_size, CONFIG_OPTION_TCP_RCVBUF_SIZE);
    CFG_ASSIGN_INT(max_send_buffer_size, CONFIG_OPTION_MAX_SEND_BUFFER_SIZE);
    CFG_ASSIGN_INT(max_recv_buffer_size, CONFIG_OPTION_MAX_RECV_BUFFER_SIZE);
    CFG_ASSIGN_INT(tcp_nodelay, CONFIG_OPTION_TCP_NODELAY);
    CFG_ASSIGN_STR(address, CONFIG_OPTION_ADDRESS);
    CFG_ASSIGN_INT(port, CONFIG_OPTION_PORT);
    CFG_ASSIGN_INT(conn_timeout, CONFIG_OPTION_CONN_TIMEOUT);
    CFG_ASSIGN_INT(work_thread_num, CONFIG_OPTION_WORK_THREAD_NUM);

    lua_config_.enumerate(CONFIG_LOG_MODULES_TABLE, (void (*) (const string&, const string&))&log_tool_t::enable_log_module);
}


#endif //! _CHAOS_SERVER_CONFIG_H_
