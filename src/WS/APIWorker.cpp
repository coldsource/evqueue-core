/*
 * This file is part of evQueue
 * 
 * evQueue is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * evQueue is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with evQueue. If not, see <http://www.gnu.org/licenses/>.
 * 
 * Author: Thibault Kummer <bob@coldsource.net>
 */

#include <WS/APIWorker.h>
#include <WS/WSServer.h>
#include <WS/APICmdBuffer.h>
#include <API/XMLQuery.h>
#include <API/APISession.h>

using namespace std;

void APIWorker::get()
{
	APICmdBuffer *buffer = (APICmdBuffer *)producer;
	st_cmd item = buffer->front();
	buffer->pop();
	
	wsi = item.wsi;
	cmd = item.cmd;
	
	WSServer::per_session_data *context = (WSServer::per_session_data *)lws_wsi_user(wsi);
	session = context->session;
	session->Acquire();
}

void APIWorker::process()
{
	XMLQuery query("Websocket API worker", cmd);
	session->QueryReceived(&query);
	
	
	if(session->Release())
	{
		delete session;
		return;
	}
	
	lws_callback_on_writable(wsi); // Response is ready to send
	lws_cancel_service(ws_context); // Cancel LWS event loop to handle this event
}
