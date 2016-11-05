#ifndef _program_h_
#define _program_h_

#include "string"

using std::string;

#include "deque"

using std::deque;

class program
{
	public:

		program(const string & parameters);
		~program(void);

		void exec(void);
		void addparameter(const string & pattern, const string & parameter);
		void addparameter(const string & pattern, int parameter);
		void clearparameters(void);

	private:

		typedef		deque<string> args_t;
		
		string		path;
		args_t		args;

		class param_t
		{
			public:

				string 	pattern;
				string	subststring;

				param_t(const string & in1, const string & in2)
				{
					pattern = in1;
					subststring = in2;
				};
		};

		typedef deque<param_t>	params_t;
		params_t				params;
};

#endif
