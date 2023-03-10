#include <vector>
#include <iostream>
using namespace std;

#include "../misc.h"
#include "test_task_service.h"

int main(int argc_, char* argv_[])
{
    NEW_SERVICE();

    application_tool_t::block_all_signal();
    log_tool_t::start_log_service("test_task_service.log", 6, 1, 1);
    TS().start(1);

    test_task_service_performance();

    application_tool_t::wait_signal();
    TS().stop();
    log_tool_t::stop_log_service();

    DEL_SERVICE();

    return 0;
}

