cmake_minimum_required(VERSION 2.8)

set(EVQUEUE_VERSION "1.5")

set(CMAKE_BUILD_TYPE Release)

project(evqueue)

add_definitions(-DEVQUEUE_VERSION="${EVQUEUE_VERSION}")

add_executable(evqueue
	evqueue.cpp Configuration.cpp ConfigurationReader.cpp DB.cpp Exception.cpp
	Queue.cpp QueuePool.cpp WorkflowInstance.cpp WorkflowInstances.cpp Scheduler.cpp
	Retrier.cpp SocketQuerySAX2Handler.cpp ProcessManager.cpp WorkflowParameters.cpp
	handle_connection.cpp Statistics.cpp NetworkInputSource.cpp BinNetworkInputStream.cpp
	Schedule.cpp WorkflowSchedule.cpp WorkflowScheduler.cpp Tasks.cpp Task.cpp
	Workflow.cpp Workflows.cpp Logger.cpp GarbageCollector.cpp RetrySchedule.cpp
	RetrySchedules.cpp SequenceGenerator.cpp tools.cpp tools_ipc.cpp Notification.cpp
	Notifications.cpp Sockets.cpp FileManager.cpp base64.cpp sha1.c tables.cpp tools_db.cpp
	workflow.xsd.cpp XMLUtils.cpp QueryHandlers.cpp QueryResponse.cpp Cluster.cpp ping.cpp
	SocketResponseSAX2Handler.cpp retry_schedule.xsd.cpp WorkflowSchedules.cpp
	NotificationType.cpp NotificationTypes.cpp WorkflowInstanceAPI SocketSAX2Handler.cpp
	hmac.cpp AuthHandler.cpp User.cpp Users.cpp Logs.cpp ClientBase.cpp Client.cpp)

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



project(evqueue_wfmanager)

add_executable(evqueue_wfmanager evqueue_wfmanager.cpp)

include_directories(include /usr/include)

target_link_libraries(evqueue_wfmanager xerces-c)
target_link_libraries(evqueue_wfmanager xqilla)



project(evqueue_api)

add_executable(evqueue_api
	evqueue_api.cpp ClientBase.cpp Exception.cpp SocketResponseSAX2Handler.cpp SocketSAX2Handler.cpp NetworkInputSource.cpp BinNetworkInputStream.cpp
	hmac.cpp sha1.c)

include_directories(include /usr/include)

target_link_libraries(evqueue_api xerces-c)
target_link_libraries(evqueue_api xqilla)



ADD_CUSTOM_TARGET(man ALL)
ADD_CUSTOM_COMMAND(
  TARGET man
  COMMAND gzip -9n ${CMAKE_CURRENT_SOURCE_DIR}/evqueue.8 --stdout >${CMAKE_CURRENT_BINARY_DIR}/evqueue.8.gz
)

ADD_CUSTOM_COMMAND(
  TARGET man
  COMMAND gzip -9n ${CMAKE_CURRENT_SOURCE_DIR}/evqueue_wfmanager.8 --stdout >${CMAKE_CURRENT_BINARY_DIR}/evqueue_wfmanager.8.gz
)



install(TARGETS evqueue evqueue_monitor evqueue_notification_monitor DESTINATION bin COMPONENT core)
install(FILES ../debian/etc/evqueue.conf DESTINATION /etc COMPONENT core)
install(FILES ../debian/etc/rsyslog.d/90-evqueue.conf DESTINATION /etc/rsyslog.d PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ COMPONENT core)
install(DIRECTORY plugins DESTINATION /usr/share/evqueue-core DIRECTORY_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ COMPONENT core)
install(DIRECTORY plugins/notifications DESTINATION /usr/share/evqueue-core/plugins DIRECTORY_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ COMPONENT core)
install(DIRECTORY DESTINATION /usr/share/evqueue-core/tasks COMPONENT core)
install(DIRECTORY ../debian/usr/share/doc/evqueue DESTINATION /usr/share/doc DIRECTORY_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ COMPONENT core)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/evqueue.8.gz DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man8 COMPONENT core)

install(TARGETS evqueue_wfmanager DESTINATION bin COMPONENT utils)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/evqueue_wfmanager.8.gz DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man8 COMPONENT utils)
