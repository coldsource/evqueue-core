cmake_minimum_required(VERSION 2.8)

set(EVQUEUE_VERSION "2.2")

set(CMAKE_BUILD_TYPE Release)

project(evqueue)

# Detect libgit2 library
find_library(HAS_LIBGIT2 libgit2.so)
find_library(HAS_STATICLIBGIT2 libgit2.a)

# We prefer shared version
if(HAS_LIBGIT2)
	option(USELIBGIT2 "Enable libgit2 support" ON)
else()
	option(USELIBGIT2 "Enable libgit2 support" OFF)
	
	# Shared version is not available, try static one
	if(HAS_STATICLIBGIT2)
		option(USESTATICLIBGIT2 "Enable static libgit2 support" ON)
	else()
		option(USESTATICLIBGIT2 "Enable static libgit2 support" OFF)
	endif(HAS_STATICLIBGIT2)
endif(HAS_LIBGIT2)

add_definitions(-DEVQUEUE_VERSION="${EVQUEUE_VERSION}")

add_executable(evqueue
	src/WorkflowInstance/WorkflowInstance.cpp src/WorkflowInstance/Task.cpp src/WorkflowInstance/WorkflowInstanceAPI.cpp src/WorkflowInstance/WorkflowInstances.cpp src/WorkflowInstance/Datastore.cpp
	src/DOM/DOMDocument.cpp src/DOM/DOMNode.cpp src/DOM/DOMElement.cpp src/DOM/DOMText.cpp src/DOM/DOMNamedNodeMap.cpp src/DOM/DOMXPath.cpp src/DOM/DOMXPathResult.cpp
	src/XPath/XPathTokens.cpp src/XPath/XPathParser.cpp src/XPath/XPathEval.cpp src/XPath/XPathOperators.cpp src/XPath/XPathFunctions.cpp src/XPath/WorkflowXPathFunctions.cpp
	src/Configuration/ConfigurationEvQueue.cpp src/Configuration/ConfigurationReader.cpp src/Configuration/Configuration.cpp
	src/Notification/Notification.cpp src/Notification/Notifications.cpp src/Notification/NotificationType.cpp src/Notification/NotificationTypes.cpp
	src/Tag/Tag.cpp src/Tag/Tags.cpp
	src/User/User.cpp src/User/Users.cpp
	src/Schedule/Scheduler.cpp src/Schedule/RetrySchedule.cpp src/Schedule/RetrySchedules.cpp src/Schedule/Schedule.cpp src/Schedule/WorkflowSchedules.cpp src/Schedule/WorkflowSchedule.cpp src/Schedule/WorkflowScheduler.cpp src/Schedule/retry_schedule.xsd.cpp src/Schedule/Retrier.cpp
	src/Exception/Exception.cpp src/Exception/ExceptionManager.cpp src/Exception/ExceptionWorkflowContext.cpp
	src/Workflow/Workflow.cpp src/Workflow/Workflows.cpp src/Workflow/workflow.xsd.cpp src/Workflow/WorkflowParameters.cpp
	src/XML/XMLString.cpp src/XML/XMLUtils.cpp src/XML/XMLFormatter.cpp
	src/Logs/Logs.cpp src/Logs/LogsAPI.cpp src/Logs/LogsNotifications.cpp
	src/API/ClientBase.cpp src/API/Client.cpp src/API/SocketQuerySAX2Handler.cpp src/API/SocketResponseSAX2Handler.cpp src/API/SocketSAX2Handler.cpp src/API/handle_connection.cpp src/API/QueryHandlers.cpp src/API/QueryResponse.cpp src/API/AuthHandler.cpp src/API/ping.cpp src/API/ActiveConnections.cpp src/API/Statistics.cpp src/API/tools.cpp
	src/Logger/Logger.cpp src/Logger/LoggerAPI.cpp src/Logger/LoggerNotifications.cpp
	src/Cluster/Cluster.cpp src/Cluster/UniqueAction.cpp
	src/Crypto/base64.cpp src/Crypto/sha1.c src/Crypto/hmac.cpp src/Crypto/Sha1String.cpp src/Crypto/pbkdf2.cpp src/Crypto/Random.cpp
	src/DB/DB.cpp src/DB/SequenceGenerator.cpp src/DB/tables.cpp src/DB/tools_db.cpp src/DB/GarbageCollector.cpp
	src/Process/ProcessManager.cpp src/Process/ProcessExec.cpp src/Process/DataSerializer.cpp src/Process/DataPiper.cpp src/Process/tools_ipc.cpp
	src/IO/NetworkInputSource.cpp src/IO/BinNetworkInputStream.cpp src/IO/Sockets.cpp src/IO/FileManager.cpp src/IO/Filesystem.cpp
	src/Queue/Queue.cpp src/Queue/QueuePool.cpp
	src/Git/LibGit2.cpp src/Git/Git.cpp
	
	src/evqueue.cpp
	)

target_compile_definitions(evqueue PRIVATE USE_DATA_PIPER)

if(USELIBGIT2)
	add_definitions(-DUSELIBGIT2)
	target_link_libraries(evqueue git2)
	Message("Git support is enabled")
endif(USELIBGIT2)

if(USESTATICLIBGIT2)
	add_definitions(-DUSELIBGIT2)
	target_link_libraries(evqueue libgit2.a)
	target_link_libraries(evqueue ssh2)
	target_link_libraries(evqueue http_parser)
	target_link_libraries(evqueue curl)
	target_link_libraries(evqueue ssl)
	target_link_libraries(evqueue crypto)
	target_link_libraries(evqueue rt)
	Message("Git static support is enabled")
endif(USESTATICLIBGIT2)

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

find_library(MYSQL_LIB mysqlclient)
find_library(MARIADB_LIB mariadbclient)
if(MYSQL_LIB)
	target_link_libraries(evqueue mysqlclient)
elseif(MARIADB_LIB)
	target_link_libraries(evqueue mariadbclient)
else()
	 message(FATAL_ERROR "Needing lib mysqlclient or mariadbclient")
endif()

find_path(XERCES_INCLUDE DOM.hpp /usr/include/xercesc/dom)
if(XERCES_INCLUDE)
	message("xerces-c development library is installed")
else()
	message(FATAL_ERROR "xerces-c development library is not installed")
endif()

find_path(PCREPP_INCLUDE pcrecpp.h /usr/include)
if(PCREPP_INCLUDE)
	message("pecre++ development library is installed")
else()
	message(FATAL_ERROR "pecre++ development library is not installed")
endif()

target_link_libraries(evqueue z)
target_link_libraries(evqueue pthread)
target_link_libraries(evqueue xerces-c)
target_link_libraries(evqueue pcrecpp)



project(evqueue_monitor)

add_executable(evqueue_monitor
	src/Configuration/Configuration.cpp
	src/Exception/Exception.cpp src/Exception/ExceptionManager.cpp
	src/Process/ProcessExec.cpp src/Process/tools_ipc.cpp src/Process/DataSerializer.cpp
	
	src/evqueue_monitor.cpp
	)

include_directories(src/include /usr/include)



project(evqueue_notification_monitor)

add_executable(evqueue_notification_monitor src/evqueue_notification_monitor.cpp src/Process/tools_ipc.cpp)

include_directories(src/include /usr/include)



project(evqueue_wfmanager)

add_executable(evqueue_wfmanager
	src/DOM/DOMDocument.cpp src/DOM/DOMNode.cpp src/DOM/DOMElement.cpp src/DOM/DOMText.cpp src/DOM/DOMNamedNodeMap.cpp src/DOM/DOMXPath.cpp src/DOM/DOMXPathResult.cpp
	src/XPath/XPathTokens.cpp src/XPath/XPathParser.cpp src/XPath/XPathEval.cpp src/XPath/XPathOperators.cpp src/XPath/XPathFunctions.cpp
	src/Exception/Exception.cpp src/Exception/ExceptionManager.cpp
	src/XML/XMLString.cpp
	src/API/ClientBase.cpp
	src/API/SocketResponseSAX2Handler.cpp src/API/SocketSAX2Handler.cpp
	src/Crypto/hmac.cpp src/Crypto/sha1.c src/Crypto/Sha1String.cpp
	src/IO/NetworkInputSource.cpp src/IO/BinNetworkInputStream.cpp
	
	src/evqueue_wfmanager.cpp
)

include_directories(src/include /usr/include)

target_link_libraries(evqueue_wfmanager xerces-c)
target_link_libraries(evqueue_wfmanager pcrecpp)



project(evqueue_api)

add_executable(evqueue_api
	src/DOM/DOMDocument.cpp src/DOM/DOMNode.cpp src/DOM/DOMElement.cpp src/DOM/DOMText.cpp src/DOM/DOMNamedNodeMap.cpp src/DOM/DOMXPath.cpp src/DOM/DOMXPathResult.cpp
	src/XPath/XPathTokens.cpp src/XPath/XPathParser.cpp src/XPath/XPathEval.cpp src/XPath/XPathOperators.cpp src/XPath/XPathFunctions.cpp
	src/Exception/Exception.cpp src/Exception/ExceptionManager.cpp
	src/XML/XMLFormatter.cpp src/XML/XMLString.cpp
	src/API/ClientBase.cpp
	src/API/SocketResponseSAX2Handler.cpp src/API/SocketSAX2Handler.cpp
	src/Crypto/hmac.cpp src/Crypto/sha1.c src/Crypto/Sha1String.cpp
	src/IO/NetworkInputSource.cpp src/IO/BinNetworkInputStream.cpp
	
	src/evqueue_api.cpp
	)

include_directories(src/include /usr/include)

target_link_libraries(evqueue_api xerces-c)
target_link_libraries(evqueue_api pcrecpp)



project(evqueue_agent)

add_executable(evqueue_agent
	src/Configuration/Configuration.cpp
	src/Exception/Exception.cpp src/Exception/ExceptionManager.cpp
	src/Process/DataSerializer.cpp src/Process/ProcessExec.cpp

	src/evqueue_agent.cpp
	)

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

install(TARGETS evqueue_agent DESTINATION bin COMPONENT agent)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/evqueue_agent.8.gz DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man8 COMPONENT agent)
