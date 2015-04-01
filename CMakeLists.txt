cmake_minimum_required(VERSION 2.8)

project(evqueue)

add_executable(evqueue queueing.cpp Configuration.cpp ConfigurationReader.cpp DB.cpp Exception.cpp Queue.cpp QueuePool.cpp WorkflowInstance.cpp WorkflowInstances.cpp Scheduler.cpp Retrier.cpp SocketQuerySAX2Handler.cpp ProcessManager.cpp WorkflowParameters.cpp handle_connection.cpp Statistics.cpp NetworkInputSource.cpp BinNetworkInputStream.cpp Schedule.cpp WorkflowSchedule.cpp WorkflowScheduler.cpp Tasks.cpp Task.cpp Workflow.cpp Workflows.cpp Logger.cpp GarbageCollector.cpp RetrySchedule.cpp RetrySchedules.cpp)

include_directories(include /usr/include)

target_link_libraries(evqueue mysqlclient)
target_link_libraries(evqueue pthread)
target_link_libraries(evqueue xerces-c)
target_link_libraries(evqueue xqilla)
target_link_libraries(evqueue rt)

project(evqueue_monitor)

add_executable(evqueue_monitor evqueue_monitor.cpp)

include_directories(include /usr/include)

install(TARGETS evqueue evqueue_monitor DESTINATION bin)
install(FILES etc/evqueue.conf DESTINATION /etc)
install(FILES etc/init.d/evqueue DESTINATION /etc/init.d PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ)
install(FILES etc/default/evqueue DESTINATION /etc/default PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)
install(DIRECTORY var/run/evqueue DESTINATION /var/run DIRECTORY_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ)
install(FILES var/run/evqueue/evqueue.pid DESTINATION /var/run/evqueue)

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_NAME "evqueue-core")
set(CPACK_DEBIAN_PACKAGE_VERSION "1.3")
set(CPACK_PACKAGE_FILE_NAME "evqueue-core_1.3_amd64")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Thibault Kummer <bob@coldsource.net>")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "evQueue core module")
set(CPACK_PACKAGE_DESCRIPTION "evQueue is a fast event driven scheduler and queueing engine")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)
