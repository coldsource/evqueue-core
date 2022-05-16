cmake_minimum_required(VERSION 2.8)

set(EVQUEUE_VERSION "3.3")

set(CMAKE_BUILD_TYPE Release)

project(evqueue)

# Detect libgit2 library
find_library(HAS_LIBGIT2 libgit2.so)

# We prefer shared version
if(HAS_LIBGIT2)
	option(USELIBGIT2 "Enable libgit2 support" ON)
else()
	option(USELIBGIT2 "Enable libgit2 support" OFF)
endif(HAS_LIBGIT2)

add_definitions(-DEVQUEUE_VERSION="${EVQUEUE_VERSION}")

aux_source_directory(src/WorkflowInstance srcWorkflowInstance)
aux_source_directory(src/DOM srcDOM)
aux_source_directory(src/XPath srcXPath)
aux_source_directory(src/Configuration srcConfiguration)
aux_source_directory(src/Notification srcNotification)
aux_source_directory(src/Tag srcTag)
aux_source_directory(src/User srcUser)
aux_source_directory(src/Schedule srcSchedule)
aux_source_directory(src/Exception srcException)
aux_source_directory(src/Workflow srcWorkflow)
aux_source_directory(src/XML srcXML)
aux_source_directory(src/Logs srcLogs)
aux_source_directory(src/ELogs srcELogs)
aux_source_directory(src/API srcAPI)
aux_source_directory(src/Logger srcLogger)
aux_source_directory(src/Cluster srcCluster)
aux_source_directory(src/Crypto srcCrypto)
aux_source_directory(src/DB srcDB)
aux_source_directory(src/Process srcProcess)
aux_source_directory(src/IO srcIO)
aux_source_directory(src/Queue srcQueue)
aux_source_directory(src/Git srcGit)
aux_source_directory(src/WS srcWS)
aux_source_directory(src/Zip srcZip)
aux_source_directory(src/Thread srcThread)
aux_source_directory(src/Utils srcUtils)

add_executable(evqueue
	${srcWorkflowInstance}
	${srcDOM}
	${srcXPath}
	${srcConfiguration}
	${srcNotification}
	${srcTag}
	${srcUser}
	${srcSchedule}
	${srcException}
	${srcWorkflow}
	${srcXML}
	${srcLogs}
	${srcELogs}
	${srcAPI}
	${srcLogger}
	${srcCluster}
	${srcCrypto}
	${srcDB}
	${srcProcess}
	${srcIO}
	${srcQueue}
	${srcWS}
	${srcZip}
	${srcThread}
	${srcUtils}
	
	src/evqueue.cpp
	)

target_compile_definitions(evqueue PRIVATE USE_DATA_PIPER)
target_compile_definitions(evqueue PRIVATE BUILD_MODULE_EVQUEUE_CORE)

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

if(USELIBGIT2)
	target_sources(evqueue PRIVATE ${srcGit})
	target_link_libraries(evqueue git2)
	Message("Git support is enabled")
endif(USELIBGIT2)

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

find_path(MYSQL_INCLUDE mysql.h /usr/include/mysql)
if(MYSQL_INCLUDE)
	add_definitions(-DMYSQL_PATH_MYSQL)
	message("Found mysql.h in /usr/include/mysql")
else()
	add_definitions(-DMYSQL_PATH_MARIADB)
	message("Found mysql.h in /usr/include/mariadb")
endif()

find_path(XERCES_INCLUDE DOM.hpp /usr/include/xercesc/dom)
if(XERCES_INCLUDE)
	message("xerces-c development library is installed")
else()
	message(FATAL_ERROR "xerces-c development library is not installed")
endif()

find_path(WEBSOCKETS_INCLUDE libwebsockets.h /usr/include)
if(WEBSOCKETS_INCLUDE)
	message("websockets development library is installed")
else()
	message(FATAL_ERROR "websockets development library is not installed")
endif()

find_path(ZIP_INCLUDE zip.h /usr/include)
if(ZIP_INCLUDE)
	message("zip development library is installed")
else()
	message(FATAL_ERROR "zip development library is not installed")
endif()

target_link_libraries(evqueue z)
target_link_libraries(evqueue pthread)
target_link_libraries(evqueue xerces-c)
target_link_libraries(evqueue websockets)
target_link_libraries(evqueue zip)




project(evqueue_wfmanager)

add_executable(evqueue_wfmanager
	${srcDOM}
	${srcXPath}
	${srcXML}
	${srcCrypto}
	${srcException}
	${srcConfiguration}
	src/API/ClientBase.cpp src/API/XMLResponse.cpp src/API/XMLMessage.cpp src/API/SocketSAX2Handler.cpp
	src/IO/NetworkInputSource.cpp src/IO/BinNetworkInputStream.cpp
	
	src/evqueue_wfmanager.cpp
)

include_directories(src/include /usr/include)

target_link_libraries(evqueue_wfmanager xerces-c)



project(evqueue_api)

add_executable(evqueue_api
	${srcDOM}
	${srcXPath}
	${srcXML}
	${srcCrypto}
	${srcException}
	${srcConfiguration}
	src/API/ClientBase.cpp src/API/XMLResponse.cpp src/API/XMLMessage.cpp src/API/SocketSAX2Handler.cpp
	src/IO/NetworkInputSource.cpp src/IO/BinNetworkInputStream.cpp
	
	src/evqueue_api.cpp
	)

include_directories(src/include /usr/include)

target_link_libraries(evqueue_api xerces-c)



project(evqueue_agent)

add_executable(evqueue_agent
	${srcException}
	src/Configuration/Configuration.cpp
	src/Process/DataSerializer.cpp src/Process/ProcessExec.cpp src/Process/DataPiper.cpp

	src/evqueue_agent.cpp
	)

include_directories(src/include /usr/include)

target_link_libraries(evqueue_agent pthread)



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



install(TARGETS evqueue DESTINATION bin COMPONENT core)
install(FILES debian/etc/evqueue.conf DESTINATION /etc COMPONENT core)
install(FILES debian/etc/rsyslog.d/90-evqueue.conf DESTINATION /etc/rsyslog.d PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ COMPONENT core)
install(DIRECTORY DESTINATION /usr/share/evqueue-core COMPONENT core)
install(DIRECTORY DESTINATION /usr/share/evqueue-core/plugins COMPONENT core)
install(DIRECTORY DESTINATION /usr/share/evqueue-core/plugins/notifications COMPONENT core)
install(DIRECTORY DESTINATION /usr/share/evqueue-core/tasks COMPONENT core)
install(DIRECTORY debian/usr/share/doc/evqueue DESTINATION /usr/share/doc DIRECTORY_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ COMPONENT core)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/evqueue.8.gz DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man8 COMPONENT core)

install(TARGETS evqueue_wfmanager evqueue_api DESTINATION bin COMPONENT utils)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/evqueue_wfmanager.8.gz DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man8 COMPONENT utils)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/evqueue_api.8.gz DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man8 COMPONENT utils)

install(TARGETS evqueue_agent DESTINATION bin COMPONENT agent)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/evqueue_agent.8.gz DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man8 COMPONENT agent)
