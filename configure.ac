#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(0.001)
AC_INIT(chaos, 0.6.30, lyjdamzwf@gmail.com)
AM_INIT_AUTOMAKE([foreign])
chaos_ver=0.6.30
AC_SUBST(chaos_ver)

# Checks for programs.
AC_PROG_RANLIB
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

AC_CONFIG_HEADERS([conf.h])

# Add a line like "export ENV_MAKE=-D_DEBUG_NYS" in ~/.bash_rc to cutomize make process
CXXFLAGS="-D__STDC_FORMAT_MACROS -Wall -static -g -O0 -fPIC"
AC_SUBST(CXXFLAGS)

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.


AC_ARG_ENABLE([log],
			AS_HELP_STRING([--enable-log], [enbale log trace,info,debug]),
			[enable_log="yes"],
			[enable_log="yes"])
if test "x$enable_log" = "xyes"; then
		AC_DEFINE([ENABLE_LOG_FATAL],[],[enable fatal output])
		AC_DEFINE([ENABLE_LOG_ERROR],[],[enable error output])
		AC_DEFINE([ENABLE_LOG_WARN],[],[enable warn output])
		AC_DEFINE([ENABLE_LOG_INFO],[],[enable info output])
		AC_DEFINE([ENABLE_LOG_TRACE],[],[enable trace output])
		AC_DEFINE([ENABLE_LOG_DEBUG],[],[enable debug output])
fi

AC_CONFIG_FILES([Makefile
        chaos/Makefile
		chaos/utility/Makefile
		chaos/thread/Makefile
		chaos/task_service/Makefile
		chaos/log/Makefile
		chaos/network/Makefile
		chaos/statistic/Makefile
		chaos/script/Makefile
        test/Makefile
		test/arg_helper/Makefile
		test/async_method/Makefile
		test/echo_server/Makefile
		test/log/Makefile
		test/task_service/Makefile
		test/thread/Makefile
		test/throughput_client/Makefile
		test/press_client/Makefile
		test/timer/Makefile
		test/msg_buffer/Makefile
		test/utility/Makefile
		test/perf/Makefile
        test/script/Makefile
                ])

# yunjie: 获取内核信息
full_kernel_version=$(uname -r)
echo $full_kernel_version
fir_v=$(echo $full_kernel_version|awk -F"." '{print $1}')
sec_v=$(echo $full_kernel_version|awk -F"." '{print $2}')
suffix=$(echo $full_kernel_version|awk -F"." '{print $3}')
thi_v=$(echo $suffix|awk -F"-" '{print $1}')

AC_DEFINE_UNQUOTED([KER_V1],[${fir_v}],[the first kernel version])
AC_DEFINE_UNQUOTED([KER_V2],[${sec_v}],[the second kernel version])
AC_DEFINE_UNQUOTED([KER_V3],[${thi_v}],[the third kernel version])
AC_OUTPUT

cp conf.h chaos

