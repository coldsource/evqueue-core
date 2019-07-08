#ifndef _DATASERIALIZER_H_
#define _DATASERIALIZER_H_

#include <map>
#include <string>

class DataSerializer
{
	public:
		static std::string Serialize(const std::map<std::string,std::string> &map);
		static std::string Serialize(const std::string &str);
		
		static bool Unserialize(int fd, std::map<std::string,std::string> &map);
		static bool Unserialize(int fd, std::string &str);
};

#endif
