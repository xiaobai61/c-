## 安装根目录
chaosdir=$(prefix)/chaos-@chaos_ver@

## 安装库目录
libdir=$(chaosdir)/lib

lib_LIBRARIES = libchaos_network.a
libchaos_network_a_LIBFLAGS =
libchaos_network_a_LIBADD =

libchaos_network_a_SOURCES = \
						  acceptor_service.cpp \
						  work_service_group.cpp \
						  connection.cpp \
						  tcp_service.cpp \
						  work_service.cpp \
						  msg_buffer.cpp \
						  buffer_list.cpp \
						  default_conn_strategy.cpp
