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

#include <WorkflowInstance/Datastore.h>
#include <DB/DB.h>
#include <Exception/Exception.h>
#include <API/QueryResponse.h>
#include <API/XMLQuery.h>
#include <Crypto/base64.h>
#include <Configuration/ConfigurationEvQueue.h>

#include <zlib.h>

using namespace std;

string Datastore::gzip(const string &str)
{
	int gzip_level = ConfigurationEvQueue::GetInstance()->GetInt("datastore.gzip.level");
	
	string output;
	char output_chunk[16384];
	
	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	deflateInit2(&strm, gzip_level, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
	
	strm.avail_in = str.length();
	strm.next_in = (Bytef *)str.c_str();
	
	do
	{
		strm.avail_out = 16384;
		strm.next_out = (Bytef *)output_chunk;
		deflate(&strm, Z_FINISH);
		
		output.append(output_chunk,16384-strm.avail_out);
	}while(strm.avail_out==0);
	
	deflateEnd(&strm);
	
	return output;
}

bool Datastore::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	const string action = query->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned int datastore_id = query->GetRootAttributeInt("id");
		
		DB db;
		db.QueryPrintf("SELECT datastore_value FROM t_datastore WHERE datastore_id=%i",&datastore_id);
		if(!db.FetchRow())
			throw Exception("Datastore","Unknown datastore entry");
		
		DOMDocument *dom = response->GetDOM();
		if(ConfigurationEvQueue::GetInstance()->GetBool("datastore.gzip.enable"))
		{
			string value_base64;
			base64_encode_string(gzip(db.GetField(0)),value_base64);
			
			dom->getDocumentElement().setAttribute("gzip","yes");
			dom->getDocumentElement().appendChild(dom->createTextNode(value_base64));
		}
		else
		{
			dom->getDocumentElement().setAttribute("gzip","no");
			dom->getDocumentElement().appendChild(dom->createTextNode(db.GetField(0)));
		}
		
		return true;
	}
	
	return false;
}
