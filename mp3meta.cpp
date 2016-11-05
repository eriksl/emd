#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <id3tag.h>

#include <algorithm>

#include "mp3meta.h"
#include "syslog.h"

mp3meta::mp3meta(const string & filename_in) throw(string)
{
	id3_valid	= false;
	filename	= filename_in;
}	

mp3meta::~mp3meta()
{
}

string mp3meta::_get_tag(const struct id3_tag * tag, const string & framename) throw(string)
{
	string retval = "";
	const struct id3_frame * frame;
	unsigned int ix;

	if(!(frame = id3_tag_findframe(tag, framename.c_str(), 0)))
	{
		// vlog("%s\n", (string("mp3meta::_get_tag::id3_tag_findframe: no such tag: ") + framename).c_str());
		throw(string("mp3meta::_get_tag::id3_tag_findframe: no such tag: ") + framename);
	}

	if(frame->nfields < 2)
	{
		// vlog("%s\n", (string("mp3meta::_get_tag: nfields < 2")).c_str());
		throw(string("mp3meta::_get_tag: nfields < 2"));
	}

	const union id3_field * field = &frame->fields[1];

	unsigned int nstrings = id3_field_getnstrings(field);

	for(ix = 0; ix < nstrings; ix++)
	{
		const id3_ucs4_t * ucs4;

		if(!(ucs4 = id3_field_getstrings(field, ix)))
			continue;

		id3_latin1_t * latin1;

		if(!(latin1 = id3_ucs4_latin1duplicate(ucs4)))
			continue;

		if(retval == "")
		{
			retval = (const char *)latin1;
		}
		else
		{
			retval += "\n";
			retval += (const char *)latin1;
		}

		::free(latin1);
	}

	return(retval);
}

void mp3meta::_get_id3() throw(string)
{
	struct id3_file *	id3file;
	struct id3_tag *	id3tag;

	if(!(id3file = id3_file_open(filename.c_str(), ID3_FILE_MODE_READONLY)))
	{
		// vlog("%s\n", (string("mp3meta::_get_id3::open cannot open file: ") + filename).c_str());
		throw(string("mp3meta::_get_id3::open cannot open file: ") + filename);
	}

	if(!(id3tag = id3_file_tag(id3file)))
	{
		id3_file_close(id3file);
		// vlog("%s\n", (string("mp3meta::_get_id3::id3_file_tag: cannot get tag for file: ") + filename).c_str());
		throw(string("mp3meta::_get_id3::id3_file_tag: cannot get tag for file: ") + filename);
	}

	try
	{
		album = _get_tag(id3tag, ID3_FRAME_ALBUM);
	}
	catch(string e)
	{
		album = "(unknown album)";
	}

	try
	{
		song = _get_tag(id3tag, ID3_FRAME_TITLE);
	}
	catch(string e)
	{
		song = "(unknown song)";
	}

	try
	{
		artist = _get_tag(id3tag, ID3_FRAME_ARTIST);
	}
	catch(string e)
	{
		artist = "(unknown artist)";
	}

	try
	{
		track = _get_tag(id3tag, ID3_FRAME_TRACK);
	}
	catch(string e)
	{
		track = "-";
	}

	id3_tag_delete(id3tag);
	id3_file_close(id3file);

	//vlog("\n\nmp3meta::getid3() (filename = %s)\n", filename.c_str());
	//vlog("mp3meta::getid3() (album = %s)\n", album.c_str());
	//vlog("mp3meta::getid3() (artist = %s)\n", artist.c_str());
	//vlog("mp3meta::getid3() (song = %s)\n", song.c_str());
	//vlog("mp3meta::getid3() (track = %d)\n", track);

	id3_valid = true;
}	

string mp3meta::get_filename() throw(string)
{
	return(filename);
}

string mp3meta::get_path() throw(string)
{
	string	rv;
	size_t	slash;

	rv = filename;
	slash = rv.rfind('/');

	if(slash == string::npos)
		return("");

	rv = rv.substr(0, slash);

	return(rv);
}

string mp3meta::get_base() throw(string)
{
	string	rv;
	size_t	slash;

	rv = filename;
	slash = rv.rfind('/');

	if(slash == string::npos)
		return(rv);

	rv = rv.substr(slash + 1);

	return(rv);
}

string mp3meta::get_artist() throw(string)
{
	if(!id3_valid)
		_get_id3();

	return(artist);
}

string mp3meta::get_album() throw(string)
{
	if(!id3_valid)
		_get_id3();

	return(album);
}

string mp3meta::get_song() throw(string)
{
	if(!id3_valid)
		_get_id3();

	return(song);
}

string mp3meta::get_track() throw(string)
{
	if(!id3_valid)
		_get_id3();

	return(track);
}
