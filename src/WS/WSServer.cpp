#include <WS/WSServer.h>
#include <User/User.h>
#include <API/APISession.h>
#include <API/XMLQuery.h>
#include <Exception/Exception.h>
#include <API/QueryResponse.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <API/QueryHandlers.h>
#include <WS/Events.h>
#include <Crypto/base64.h>
#include <DB/DB.h>
#include <API/ActiveConnections.h>
#include <Logger/Logger.h>
#include <API/Statistics.h>
#include <API/QueryHandlers.h>
#include <IO/NetworkConnections.h>

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	ConfigurationEvQueue *config = ConfigurationEvQueue::GetInstance();
	
	// Create Websocket TCP socket
	NetworkConnections::GetInstance()->RegisterTCP("WebSocket (tcp)", config->Get("ws.bind.ip"), config->GetInt("ws.bind.port"), config->GetInt("ws.listen.backlog"), [](int s) {
		if(ActiveConnections::GetInstance()->GetWSNumber()>=ConfigurationEvQueue::GetInstance()->GetInt("ws.connections.max"))
		{
			close(s);
			
			Logger::Log(LOG_WARNING,"Max WebSocket connections reached, dropping connection");
		}
		
		ActiveConnections::GetInstance()->StartWSConnection(s);
	});
	
	return (APIAutoInit *)0;
});

using namespace std;

WSServer *WSServer::instance = 0;

const struct lws_protocols protocols[] =
{
	{"http-only", WSServer::callback_http, 0, 0},
	{
		"api",
		WSServer::callback_evq,
		sizeof(WSServer::per_session_data),
		32768,
		WSServer::en_protocols::API
	},
	{
		"events",
		WSServer::callback_evq,
		sizeof(WSServer::per_session_data),
		32768,
		WSServer::en_protocols::EVENTS
	},
	{0,0,0,0,0}
};

WSServer::WSServer()
{
	ConfigurationEvQueue *config = ConfigurationEvQueue::GetInstance();
	memset( &info, 0, sizeof(info) );

	lws_set_log_level(LLL_ERR | LLL_WARN, 0);

	info.keepalive_timeout = config->GetInt("ws.keepalive");
	info.timeout_secs = config->GetInt("ws.rcv.timeout");
	info.port = CONTEXT_PORT_NO_LISTEN; // CONTEXT_PORT_NO_LISTEN_SERVER Should be used but is not available in this version of LWS
	info.protocols = protocols;
	info.gid = -1;
	info.uid = -1;
	info.count_threads = config->GetInt("ws.workers");
	info.server_string = "evQueue websockets server";
	info.vhost_name = "default";
	/*{
			info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
			info.ssl_cert_filepath = ssl_cert;
			info.ssl_private_key_filepath = ssl_key;
	}*/

	context = lws_create_context(&info);
	if(!context)
		throw Exception("Websocket","Unable to bind port "+to_string(info.port));
	
	Events::GetInstance()->SetContext(context);
	
	instance = this;
	
	for(int i=0;i<info.count_threads;i++)
		threads.emplace_back(thread(event_loop, i));
	
	Logger::Log(LOG_NOTICE, "WS server: started "+to_string(info.count_threads)+" threads");
}

WSServer::~WSServer()
{
	Shutdown();
	WaitForShutdown();
}
 
int WSServer::callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len )
{
	switch( reason )
	{
		case LWS_CALLBACK_HTTP:
			lws_write(wsi, (unsigned char *)"HTTP/1.0 403 Forbidden\nContent-type: text/plain\n\nForbidden", 58, LWS_WRITE_HTTP);
			return -1;
		default:
			break;
	}

	return 0;
}
 
int WSServer::callback_evq(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	per_session_data *context = (per_session_data *)user;
	const lws_protocols *protocol = lws_get_protocol(wsi);
	
	Logger::Log(LOG_DEBUG, "WS callback called for reason : "+to_string(reason));
	
	try
	{
		switch(reason)
		{
			case LWS_CALLBACK_PROTOCOL_INIT:
				break;
			
			case LWS_CALLBACK_PROTOCOL_DESTROY:
				break;
			
			case LWS_CALLBACK_ESTABLISHED:
			{
				Statistics::GetInstance()->IncWSAcceptedConnections();
				
				// Init context data
				context->session = new APISession("Websocket",wsi);
				
				// Tell we want to send challenge when connection is established
				lws_callback_on_writable(wsi);
				break;
			}
			
			case LWS_CALLBACK_CLOSED:
			{
				// Clean context data
				delete context->session;
				Events::GetInstance()->UnsubscribeAll(wsi);
				
				// Notify that connection is over
				int s = lws_get_socket_fd(wsi);
				ActiveConnections::GetInstance()->EndWSConnection(s);
				break;
			}
			
			case LWS_CALLBACK_RECEIVE:
			{
				// Message has been received
				string input_xml((char *)in,len);
				XMLQuery query("Websocket",input_xml);
				
				if(context->session->GetStatus()==APISession::en_status::WAITING_CHALLENGE_RESPONSE)
				{
					// We are authenticating, this is challenge response
					context->session->ChallengeReceived(&query);
					
					lws_callback_on_writable(wsi); // We have to send greeting
				}
				else if(context->session->GetStatus()==APISession::en_status::READY)
				{
					if(protocol->id==API)
					{
						// We only receive queries in API protocol
						context->session->QueryReceived(&query);
						
						lws_callback_on_writable(wsi); // We have to send response
					}
					else if(protocol->id==EVENTS)
					{
						if(query.GetQueryGroup()!="event")
							throw Exception("Websocket","Only event query group is supported by this protocol","UNKNOWN_COMMAND");
						
						if(query.GetRootAttribute("action")=="subscribe")
						{
							string type = query.GetRootAttribute("type");
							unsigned int object_id = query.GetRootAttributeInt("object_id",0);
							int external_id = query.GetRootAttributeInt("external_id",0);
							string api_cmd_base64 = query.GetRootAttribute("api_cmd");
							string api_cmd;
							if(!base64_decode_string(api_cmd,api_cmd_base64))
								throw Exception("Websocket","Invalid base64 sequence","INVALID_PARAMETER");
							
							// Subscribe event
							Events::GetInstance()->Subscribe(type,wsi,object_id,external_id,api_cmd);
							
							// Sent API command immediatly for initialization
							if(query.GetRootAttributeBool("send_now",false))
								Events::GetInstance()->Create(type, object_id);
						}
						else if(query.GetRootAttribute("action")=="unsubscribe")
						{
							string type = query.GetRootAttribute("type");
							unsigned int object_id = query.GetRootAttributeInt("object_id",0);
							int external_id = query.GetRootAttributeInt("external_id",0);
							Events::GetInstance()->Unsubscribe(type,wsi,object_id,external_id);
						}
						else if(query.GetRootAttribute("action")=="unsubscribeall")
						{
							Events::GetInstance()->UnsubscribeAll(wsi);
						}
						else if(query.GetRootAttribute("action")=="ack")
						{
							unsigned long long event_id = query.GetRootAttributeLL("event_id");
							Events::GetInstance()->Ack(wsi, event_id);
						}
					}
				}
				break;
			}
			
			case LWS_CALLBACK_SERVER_WRITEABLE:
			{
				if(context->session->GetStatus()==APISession::en_status::INITIALIZED)
				{
					context->session->SendChallenge();
					if(context->session->GetStatus()==APISession::en_status::AUTHENTICATED)
						lws_callback_on_writable(wsi); // Need to send greeting
				}
				else if(context->session->GetStatus()==APISession::en_status::AUTHENTICATED)
					context->session->SendGreeting();
				else if(context->session->GetStatus()==APISession::en_status::QUERY_RECEIVED)
					context->session->SendResponse(); // For API protocol
				else if(context->session->GetStatus()==APISession::en_status::READY && protocol->id==EVENTS)
				{
					string api_cmd;
					int external_id;
					string object_id;
					unsigned long long event_id;
					if(Events::GetInstance()->Get(wsi, &external_id, object_id, &event_id, api_cmd))
					{
						context->session->Query(api_cmd, external_id, object_id, event_id);
						lws_callback_on_writable(wsi); // Set writable again in case we have more than one event
					}
				}
				break;
			}
		}
	}
	catch(Exception &e)
	{
		Statistics::GetInstance()->IncAPIExceptions();
		lws_close_reason(wsi,LWS_CLOSE_STATUS_UNEXPECTED_CONDITION,(unsigned char *)e.error.c_str(),e.error.length());
		return -1;
	}
	
	return 0;
}

void WSServer::Shutdown(void)
{
	is_cancelling = true;
	lws_cancel_service(context);
}

void WSServer::WaitForShutdown(void)
{
	int nthreads = ConfigurationEvQueue::GetInstance()->GetInt("ws.workers");
	for(int i=0;i<nthreads;i++)
		threads[i].join();
	
	lws_context_destroy(context);
}

void WSServer::Adopt(int fd)
{
	// Configure socket
	Configuration *config = ConfigurationEvQueue::GetInstance();
	struct timeval tv;
	
	tv.tv_sec = config->GetInt("ws.rcv.timeout");
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
	
	tv.tv_sec = config->GetInt("ws.snd.timeout");
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
	
#if defined(LWS_ADOPT_SOCKET) || LWS_LIBRARY_VERSION_MAJOR >= 4
	lws_sock_file_fd_type sock;
	sock.sockfd = fd;
	struct lws_vhost *vhost = lws_get_vhost_by_name(context, "default");
	lws_adopt_descriptor_vhost(vhost, (lws_adoption_type)(LWS_ADOPT_SOCKET | LWS_ADOPT_HTTP), sock, 0, 0);
#else
	lws_adopt_socket(context, fd);
#endif
	
	lws_cancel_service(context);
}

void WSServer::event_loop(int tsi)
{
	WSServer *ws = WSServer::GetInstance();
	
	DB::StartThread();
	
	Logger::Log(LOG_INFO, "WS thread #"+to_string(tsi)+" starting service");
	
	while( 1 )
	{
		lws_service_tsi(ws->context,10000, tsi);
		
		if(ws->is_cancelling)
			break;
	}
	
	DB::StopThread();
}
