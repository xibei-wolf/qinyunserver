add_executable(main main.cpp  ${SRC_LOG} ${SRC_SERVER})
add_executable(client client.cpp  ${SRC_LOG} ${SRC_SERVER})
add_executable(testcli testcli.c  ${SRC_LOG} ${SRC_SERVER})