#ifndef _PROCESSEXEC_H_
#define _PROCESSEXEC_H_

#include <string>
#include <vector>
#include <map>

class ProcessExec
{
	struct st_pipe
	{
		int read_end;
		int write_end;
	};
	
	std::string path;
	std::vector<std::string> arguments;
	std::map<std::string,std::string> env;
	
	int stdin_pipe[2] = {-1,-1};
	std::string stdin_data;
	
	std::map<int,int> file_rdr;
	std::map<int,int> fd_rdr;
	std::map<int,st_pipe> parent_rdr;
	
	public:
		ProcessExec();
		ProcessExec(const std::string &path);
		~ProcessExec();

		void SetPath(const std::string &path);
		std::string GetPath() { return path; }
		void SetScript(const std::string &path, const std::string &script);
		
		void AddArgument(const std::string &value, bool escape=false);
		
		void AddEnv(const std::string &name, const std::string &value);
		
		void PipeMap(const std::map<std::string,std::string> &data);
		void PipeString(const std::string &data);
		void Pipe(const std::string &data);
		
		void FileRedirect(int fd, const std::string &filename);
		void FDRedirect(int src_fd, int dst_fd);
		int ParentRedirect(int fd);
		
		pid_t Exec();
	
	private:
		void init_stdin_pipe();
		int open_log_file(const std::string &filename_base, int log_fileno);
};

#endif
