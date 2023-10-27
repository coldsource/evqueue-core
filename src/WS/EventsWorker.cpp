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

#include <WS/EventsWorker.h>
#include <WS/Events.h>
#include <WS/WSServer.h>
#include <API/XMLQuery.h>
#include <API/APISession.h>

using namespace std;

void EventsWorker::get()
{
	Events *events = (Events *)producer;
	
	events->Get(&wsi, &external_id, object_id, &event_id, api_cmd);
	
	WSServer::per_session_data *context = (WSServer::per_session_data *)lws_wsi_user(wsi);
	session = context->session;
	session->Acquire();
}

void EventsWorker::process()
{
	Events *events = (Events *)producer;
	
	XMLQuery query("Websocket events worker", api_cmd);
	session->QueryReceived(&query, external_id, object_id, event_id);
	
	if(session->Release())
	{
		delete session;
		return;
	}
	
	events->Processed(wsi, api_cmd);
	
	lws_callback_on_writable(wsi); // Response is ready to send
	lws_cancel_service(ws_context); // Cancel LWS event loop to handle this event
}
