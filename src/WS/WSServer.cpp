#include <WS/WSServer.h>
#include <User/User.h>
#include <API/APISession.h>
#include <API/SocketQuerySAX2Handler.h>
#include <Exception/Exception.h>
#include <API/QueryResponse.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <API/QueryHandlers.h>
#include <WS/Events.h>
#include <Crypto/base64.h>

#include <mysql/mysql.h>

using namespace std;

WSServer *WSServer::instance = 0;

const struct lws_protocols protocols[] =
{
	{"http-only", WSServer::callback_http, 0, 0},
	{
		"api",
		WSServer::callback_minimal,
		sizeof(WSServer::per_session_data),
		32768,
		WSServer::en_protocols::API
	},
	{
		"events",
		WSServer::callback_minimal,
		sizeof(WSServer::per_session_data),
		32768,
		WSServer::en_protocols::EVENTS
	},
	{0,0,0,0,0}
};

WSServer::WSServer()
{
	
	memset( &info, 0, sizeof(info) );

	lws_set_log_level(LLL_ERR | LLL_WARN, 0);

	info.keepalive_timeout = 120;
	info.timeout_secs = 30;
	info.port = ConfigurationEvQueue::GetInstance()->GetInt("ws.bind.port");
	info.protocols = protocols;
	info.gid = -1;
	info.uid = -1;
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
	
	events = new Events(context);
	
	instance = this;
}

WSServer::~WSServer()
{
	delete events;
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
 
int WSServer::callback_minimal(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	per_session_data *context = (per_session_data *)user;
	const lws_protocols *protocol = lws_get_protocol(wsi);
	
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
				break;
			}
			
			case LWS_CALLBACK_RECEIVE:
			{
				// Message has been received
				string input_xml((char *)in,len);
				printf("%s\n",input_xml.c_str());
				SocketQuerySAX2Handler saxh("Websocket",input_xml);
				
				if(context->session->GetStatus()==APISession::en_status::WAITING_CHALLENGE_RESPONSE)
				{
					// We are authenticating, this is challenge response
					context->session->ChallengeReceived(&saxh);
					
					lws_callback_on_writable(wsi); // We have to send greeting
				}
				else if(context->session->GetStatus()==APISession::en_status::READY)
				{
					if(protocol->id==API)
					{
						// We only receive queries in API protocol
						context->session->QueryReceived(&saxh);
						
						lws_callback_on_writable(wsi); // We have to send response
					}
					else if(protocol->id==EVENTS)
					{
						if(saxh.GetQueryGroup()!="event")
							throw Exception("Websocket","Only event query group is supported by this protocol","UNKNOWN_COMMAND");
						
						if(saxh.GetRootAttribute("action")=="subscribe")
						{
							string type = saxh.GetRootAttribute("type");
							unsigned int instance_id = saxh.GetRootAttributeInt("instance_id",0);
							string api_cmd_base64 = saxh.GetRootAttribute("api_cmd");
							string api_cmd;
							if(!base64_decode_string(api_cmd,api_cmd_base64))
								throw Exception("Websocket","Invalid base64 sequence","INVALID_PARAMETER");
							
							// Subscribre event
							Events::GetInstance()->Subscribe(type,wsi,instance_id,api_cmd);
							
							// Sent API command immediatly for initialization
							if(saxh.GetRootAttributeBool("send_now",false))
								context->session->Query(api_cmd);
						}
						else if(saxh.GetRootAttribute("action")=="unsubscribe")
						{
							string type = saxh.GetRootAttribute("type");
							unsigned int instance_id = saxh.GetRootAttributeInt("instance_id",0);
							Events::GetInstance()->Unsubscribe(type,wsi,instance_id);
						}
						else if(saxh.GetRootAttribute("action")=="unsubscribeall")
						{
							Events::GetInstance()->UnsubscribeAll(wsi);
						}
					}
				}
				break;
			}
			
			case LWS_CALLBACK_SERVER_WRITEABLE:
			{
				if(context->session->GetStatus()==APISession::en_status::INITIALIZED)
					context->session->SendChallenge();
				else if(context->session->GetStatus()==APISession::en_status::AUTHENTICATED)
					context->session->SendGreeting();
				else if(context->session->GetStatus()==APISession::en_status::QUERY_RECEIVED)
					context->session->SendResponse(); // For API protocol
				else if(context->session->GetStatus()==APISession::en_status::READY && protocol->id==EVENTS)
				{
					string api_cmd;
					if(Events::GetInstance()->Get(wsi,api_cmd))
						context->session->Query(api_cmd);
				}
				break;
			}
		}
	}
	catch(Exception &e)
	{
		lws_close_reason(wsi,LWS_CLOSE_STATUS_UNEXPECTED_CONDITION,(unsigned char *)e.error.c_str(),e.error.length());
		return -1;
	}
	
	return 0;
}

void WSServer::Start(void)
{
	event_loop_thread = thread(WSServer::event_loop,this);
}

void WSServer::Shutdown(void)
{
	is_cancelling = true;
	lws_cancel_service(context);
}

void WSServer::WaitForShutdown(void)
{
	event_loop_thread.join();
	
	lws_context_destroy(context);
}

void WSServer::event_loop(WSServer *ws)
{
	mysql_thread_init();
	
	while( 1 )
	{
		lws_service(ws->context, 10000);
		if(ws->is_cancelling)
			break;
	}
	
	mysql_thread_end();
}
