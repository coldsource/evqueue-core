cmake_minimum_required(VERSION 2.8)

set(EVQUEUE_VERSION "2.0")

set(CMAKE_BUILD_TYPE Release)

project(evqueue)

add_definitions(-DEVQUEUE_VERSION="${EVQUEUE_VERSION}")

add_executable(evqueue
	src/evqueue.cpp src/Configuration.cpp src/ConfigurationReader.cpp src/DB.cpp src/Exception.cpp
	src/Queue.cpp src/QueuePool.cpp src/WorkflowInstance.cpp src/WorkflowInstances.cpp src/Scheduler.cpp
	src/Retrier.cpp src/SocketQuerySAX2Handler.cpp src/ProcessManager.cpp src/WorkflowParameters.cpp
	src/handle_connection.cpp src/Statistics.cpp src/NetworkInputSource.cpp src/BinNetworkInputStream.cpp
	src/Schedule.cpp src/WorkflowSchedule.cpp src/WorkflowScheduler.cpp src/Tasks.cpp src/Task.cpp
	src/Workflow.cpp src/Workflows.cpp src/Logger.cpp src/GarbageCollector.cpp src/RetrySchedule.cpp
	src/RetrySchedules.cpp src/SequenceGenerator.cpp src/tools.cpp src/tools_ipc.cpp src/Notification.cpp
	src/Notifications.cpp src/Sockets.cpp src/FileManager.cpp src/base64.cpp src/tables.cpp src/tools_db.cpp
	src/workflow.xsd.cpp src/XMLUtils.cpp src/QueryHandlers.cpp src/QueryResponse.cpp src/Cluster.cpp src/ping.cpp
	src/SocketResponseSAX2Handler.cpp src/retry_schedule.xsd.cpp src/WorkflowSchedules.cpp
	src/NotificationType.cpp src/NotificationTypes.cpp src/sha1.c src/WorkflowInstanceAPI.cpp src/SocketSAX2Handler.cpp
	src/hmac.cpp src/AuthHandler.cpp src/User.cpp src/Users.cpp src/Logs.cpp src/ClientBase.cpp src/Client.cpp
	src/ActiveConnections.cpp src/LibGit2.cpp src/Git.cpp src/Sha1String.cpp src/pbkdf2.cpp src/Random.cpp
	src/Filesystem.cpp src/DOMNode.cpp src/DOMText.cpp src/DOMElement.cpp src/DOMXPathResult.cpp
	src/DOMDocument.cpp src/DOMNamedNodeMap.cpp src/XPathTokens.cpp src/XPathParser.cpp src/XPathEval.cpp
	src/XPathOperators.cpp src/XPathFunctions.cpp src/DOMXPath.cpp src/WorkflowXPathFunctions.cpp
	src/ExceptionManager.cpp src/ExceptionWorkflowContext.cpp src/XMLString.cpp)

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

target_link_libraries(evqueue git2)
target_link_libraries(evqueue z)
target_link_libraries(evqueue mysqlclient)
target_link_libraries(evqueue pthread)
target_link_libraries(evqueue xerces-c)
target_link_libraries(evqueue rt)
target_link_libraries(evqueue pcrecpp)
target_link_libraries(evqueue curl)
target_link_libraries(evqueue ssh2)
target_link_libraries(evqueue http_parser)



project(evqueue_monitor)

add_executable(evqueue_monitor src/evqueue_monitor.cpp src/tools_ipc.cpp src/tools_env.cpp)

include_directories(src/include /usr/include)



project(evqueue_notification_monitor)

add_executable(evqueue_notification_monitor src/evqueue_notification_monitor.cpp src/tools_ipc.cpp)

include_directories(src/include /usr/include)



project(evqueue_wfmanager)

add_executable(evqueue_wfmanager
	src/evqueue_wfmanager.cpp src/ClientBase.cpp src/Exception.cpp src/ExceptionManager.cpp src/SocketResponseSAX2Handler.cpp src/SocketSAX2Handler.cpp
	src/NetworkInputSource.cpp src/BinNetworkInputStream.cpp src/hmac.cpp src/sha1.c src/Sha1String.cpp src/XMLString.cpp src/DOMDocument.cpp src/DOMElement.cpp src/DOMNode.cpp src/DOMText.cpp
	src/DOMXPath.cpp src/DOMXPathResult.cpp src/XPathTokens.cpp src/XPathEval.cpp src/XPathOperators.cpp
	src/XPathFunctions.cpp src/DOMNamedNodeMap.cpp src/XPathEval.cpp src/XPathParser.cpp)

include_directories(src/include /usr/include)

target_link_libraries(evqueue_wfmanager xerces-c)
target_link_libraries(evqueue_wfmanager pcrecpp)



project(evqueue_api)

add_executable(evqueue_api
	src/evqueue_api.cpp src/ClientBase.cpp src/Exception.cpp src/ExceptionManager.cpp src/SocketResponseSAX2Handler.cpp src/SocketSAX2Handler.cpp
	src/NetworkInputSource.cpp src/BinNetworkInputStream.cpp src/hmac.cpp src/sha1.c src/XMLFormatter.cpp
	src/Sha1String.cpp src/XMLString.cpp src/DOMDocument.cpp src/DOMElement.cpp src/DOMNode.cpp src/DOMText.cpp
	src/DOMXPath.cpp src/DOMXPathResult.cpp src/XPathTokens.cpp src/XPathEval.cpp src/XPathOperators.cpp
	src/XPathFunctions.cpp src/DOMNamedNodeMap.cpp src/XPathEval.cpp src/XPathParser.cpp)

include_directories(src/include /usr/include)

target_link_libraries(evqueue_api xerces-c)
target_link_libraries(evqueue_api pcrecpp)



project(evqueue_agent)

add_executable(evqueue_agent src/evqueue_agent.cpp src/tools_env.cpp)

include_directories(src/include /usr/include)



ADD_CUSTOM_TARGET(man ALL)
ADD_CUSTOM_COMMAND(
  TARGET man
  COMMAND gzip -9n ${CMAKE_CURRENT_SOURCE_DIR}/src/evqueue.8 --stdout >${CMAKE_CURRENT_BINARY_DIR}/evqueue.8.gz
)

ADD_CUSTOM_COMMAND(
  TARGET man
  COMMAND gzip -9n ${CMAKE_CURRENT_SOURCE_DIR}/src/evqueue_wfmanager.8 --stdout >${CMAKE_CURRENT_BINARY_DIR}/evqueue_wfmanager.8.gz
)

ADD_CUSTOM_COMMAND(
  TARGET man
  COMMAND gzip -9n ${CMAKE_CURRENT_SOURCE_DIR}/src/evqueue_api.8 --stdout >${CMAKE_CURRENT_BINARY_DIR}/evqueue_api.8.gz
)

ADD_CUSTOM_COMMAND(
  TARGET man
  COMMAND gzip -9n ${CMAKE_CURRENT_SOURCE_DIR}/src/evqueue_agent.8 --stdout >${CMAKE_CURRENT_BINARY_DIR}/evqueue_agent.8.gz
)



install(TARGETS evqueue evqueue_monitor evqueue_notification_monitor DESTINATION bin COMPONENT core)
install(FILES debian/etc/evqueue.conf DESTINATION /etc COMPONENT core)
install(FILES debian/etc/rsyslog.d/90-evqueue.conf DESTINATION /etc/rsyslog.d PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ COMPONENT core)
install(DIRECTORY src/plugins DESTINATION /usr/share/evqueue-core DIRECTORY_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ COMPONENT core)
install(DIRECTORY src/plugins/notifications DESTINATION /usr/share/evqueue-core/plugins DIRECTORY_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ COMPONENT core)
install(DIRECTORY DESTINATION /usr/share/evqueue-core/tasks COMPONENT core)
install(DIRECTORY debian/usr/share/doc/evqueue DESTINATION /usr/share/doc DIRECTORY_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ COMPONENT core)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/evqueue.8.gz DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man8 COMPONENT core)

install(TARGETS evqueue_wfmanager evqueue_api DESTINATION bin COMPONENT utils)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/evqueue_wfmanager.8.gz DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man8 COMPONENT utils)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/evqueue_api.8.gz DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man8 COMPONENT utils)

install(TARGETS evqueue_agent DESTINATION agent)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/evqueue_agent.8.gz DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man8 COMPONENT agent)
