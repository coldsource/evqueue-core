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

#include <Zip/Zip.h>
#include <Exception/Exception.h>

using namespace std;

Zip::Zip(const string &data):data(data)
{
	zip_error ze;
	
	zip_src =  zip_source_buffer_create(data.c_str(), data.size(), 0, &ze);
	if(!zip_src)
		throw Exception("Zip","Unable to create Zip buffer");
	
	zip = zip_open_from_source(zip_src, ZIP_RDONLY, &ze);
	if(!zip)
	{
		zip_source_free(zip_src);
		throw Exception("Zip","Unable to open Zip archive");
	}
}

Zip::~Zip()
{
	zip_close(zip);
}

std::string Zip::GetFile(const std::string &filename)
{
	string content;
	
	zip_file_t *zf = zip_fopen(zip, filename.c_str(), 0);
	if(!zf)
		throw Exception("Zip", "File not found : "+filename);
	
	char buf[16384];
	zip_int64_t  size = 0, ret;
	while((ret = zip_fread(zf, buf, 16384)) > 0)
		content.append(buf, ret);
	
	zip_fclose(zf);
	
	return content;
}
