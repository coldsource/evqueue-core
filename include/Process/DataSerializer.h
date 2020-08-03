#ifndef _DATASERIALIZER_H_
#define _DATASERIALIZER_H_

#include <map>
#include <vector>
#include <string>

class DataSerializer
{
	static int readlen(int fd, char *value, int value_len);
	
	public:
		static std::string Serialize(const std::map<std::string,std::string> &map);
		static std::string Serialize(const std::vector<std::string> &vector);
		static std::string Serialize(const std::string &str);
		static std::string Serialize(int n);
		
		static bool Unserialize(int fd, std::map<std::string,std::string> &map);
		static bool Unserialize(int fd, std::vector<std::string> &vector);
		static bool Unserialize(int fd, std::string &str);
		static bool Unserialize(int fd, int *n);
};

#endif
