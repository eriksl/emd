#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <string>
using std::string;

#include <sstream>
using std::ostringstream;

#include <deque>
using std::deque;

#include "program.h"
#include "syslog.h"

program::program(const string & strargs)
{
	size_t first = 0;
	size_t last;
	bool command = true;

	while((last = strargs.find_first_of(" 	", first)) != string::npos)
	{
		if(command)
		{
			path = strargs.substr(first, last - first);
			command = false;
		}

		args.push_back(strargs.substr(first, last - first));

		first = last + 1;
	}

	if(first < strargs.length())
	{
		if(command)
			path = strargs.substr(first, strargs.length() - first);
		else
			args.push_back(strargs.substr(first, last - strargs.length()));
	}
}

program::~program(void)
{
	args.clear();
	params.clear();
}

void program::exec(void)
{
	int pid;

	if((pid = vfork()))
		(void)0;
	else
	{
		int		cur;
		int		argc = args.size() + 1;
		char ** argv = (char **)malloc(sizeof(char *) * (argc + 2));
		program::args_t::iterator it;
		program::params_t::iterator pit;
		int fd;

		for(cur = 0, it = args.begin(); it != args.end(); it++, cur++)
		{
			for(pit = params.begin(); pit != params.end(); pit++)
				if(!pit->pattern.compare(*it))
					break;

			if(pit == params.end())
				argv[cur] = strdup(it->c_str());
			else
				argv[cur] = strdup(pit->subststring.c_str());
		}

		argv[cur] = (char *)0;

		fd = open("/dev/null", O_RDWR);
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		close(fd);

		execvp(path.c_str(), argv);
		perror("execvp");
		exit(-1);
	}
}

void program::addparameter(const string & pattern, const string & subststring)
{
	params.push_back(param_t(pattern, subststring));
}

void program::addparameter(const string & pattern, int parameter)
{
	ostringstream converter;

	converter << parameter;

	params.push_back(param_t(pattern, converter.str()));
}

void program::clearparameters(void)
{
	params.clear();
}
