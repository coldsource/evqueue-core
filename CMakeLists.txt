cmake_minimum_required(VERSION 2.8)

set(EVQUEUE_VERSION "1.5")

project(evqueue)

add_definitions(-DEVQUEUE_VERSION="${EVQUEUE_VERSION}")

add_executable(evqueue evqueue.cpp Configuration.cpp ConfigurationReader.cpp DB.cpp Exception.cpp Queue.cpp QueuePool.cpp WorkflowInstance.cpp WorkflowInstances.cpp Scheduler.cpp Retrier.cpp SocketQuerySAX2Handler.cpp ProcessManager.cpp WorkflowParameters.cpp handle_connection.cpp Statistics.cpp NetworkInputSource.cpp BinNetworkInputStream.cpp Schedule.cpp WorkflowSchedule.cpp WorkflowScheduler.cpp Tasks.cpp Task.cpp Workflow.cpp Workflows.cpp Logger.cpp GarbageCollector.cpp RetrySchedule.cpp RetrySchedules.cpp SequenceGenerator.cpp tools.cpp tools_ipc.cpp Notification.cpp Notifications.cpp Sockets.cpp FileManager.cpp base64.cpp sha1.c tables.cpp tools_db.cpp workflow.xsd.cpp XMLUtils.cpp QueryHandlers.cpp QueryResponse.cpp Cluster.cpp)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
    message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER} has no C++11 support.")
endif()

include_directories(include /usr/include)

target_link_libraries(evqueue mysqlclient)
target_link_libraries(evqueue pthread)
target_link_libraries(evqueue xerces-c)
target_link_libraries(evqueue xqilla)
target_link_libraries(evqueue rt)
target_link_libraries(evqueue pcrecpp)



project(evqueue_monitor)

add_executable(evqueue_monitor evqueue_monitor.cpp tools_ipc.cpp)

include_directories(include /usr/include)



project(evqueue_notification_monitor)

add_executable(evqueue_notification_monitor evqueue_notification_monitor.cpp tools_ipc.cpp)

include_directories(include /usr/include)




ADD_CUSTOM_TARGET(man ALL)

ADD_CUSTOM_COMMAND(
  TARGET man
  COMMAND gzip ${CMAKE_CURRENT_SOURCE_DIR}/evqueue.8 --stdout >${CMAKE_CURRENT_BINARY_DIR}/evqueue.8.gz
)



install(TARGETS evqueue evqueue_monitor evqueue_notification_monitor DESTINATION bin)
install(FILES debian/etc/evqueue.conf DESTINATION /etc)
install(FILES debian/etc/init.d/evqueue DESTINATION /etc/init.d PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ)
install(FILES debian/etc/default/evqueue DESTINATION /etc/default PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)
install(FILES debian/etc/rsyslog.d/90-evqueue.conf DESTINATION /etc/rsyslog.d PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)
install(FILES debian/lib/systemd/system/evqueue.service DESTINATION /lib/systemd/system)
install(DIRECTORY plugins DESTINATION /usr/share/evqueue-core DIRECTORY_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ)
install(DIRECTORY plugins/notifications DESTINATION /usr/share/evqueue-core/plugins DIRECTORY_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ)
install(DIRECTORY DESTINATION /usr/share/evqueue-core/tasks)
install(DIRECTORY debian/usr/share/doc/evqueue DESTINATION /usr/share/doc DIRECTORY_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/evqueue.8.gz DESTINATION ${CMAKE_INSTALL_PREFIX}/man/man8)

set(CPACK_PACKAGE_FILE_NAME "evqueue-core_${EVQUEUE_VERSION}_amd64")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "evQueue core module")
set(CPACK_PACKAGE_DESCRIPTION "evQueue is a fast event driven scheduler and queueing engine")
set(CPACK_PACKAGE_VERSION "1.5")

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/debian/DEBIAN/conffiles;${CMAKE_CURRENT_SOURCE_DIR}/debian/DEBIAN/postinst;${CMAKE_CURRENT_SOURCE_DIR}/debian/DEBIAN/prerm;${CMAKE_CURRENT_SOURCE_DIR}/debian/DEBIAN/postrm;")

set(CPACK_GENERATOR "DEB" "RPM")
set(CPACK_DEBIAN_PACKAGE_NAME "evqueue-core")
set(CPACK_DEBIAN_PACKAGE_VERSION "${EVQUEUE_VERSION}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Thibault Kummer <bob@coldsource.net>")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)
