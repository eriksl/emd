#ifndef _mp3meta_
#define _mp3meta_

#include <sys/types.h>
#include <pthread.h>
#include <id3tag.h>

#include "string"
using std::string;

#include "deque"
using std::deque;

class mp3meta
{
	private:

		bool				id3_valid;
		string				filename;
		string				artist;
		string				album;
		string				song;
		string				track;

		static string		_get_tag(const id3_tag * tag, const string & name)	throw(string);
		void				_get_id3()											throw(string);

	public:

				mp3meta(const string & file) throw(string);
				~mp3meta();

		string	get_filename()	throw(string);
		string	get_path()		throw(string);
		string	get_base()		throw(string);
		string	get_artist()	throw(string);
		string	get_album()		throw(string);
		string	get_song()		throw(string);
		string	get_track()		throw(string);

		// bool	operator <	(mp3meta &);
};

#endif
