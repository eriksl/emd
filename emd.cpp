#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <pwd.h>
#include <getopt.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/errno.h>

#include <string>
using std::string;

#include <deque>
using std::deque;

#include "mp3meta.h"
#include "emd_state.h"
#include "program.h"
#include "http_server.h"
#include "mp3reader.h"
#include "syslog.h"

typedef program mp3notifier;
typedef deque<mp3notifier> mp3notifiers;

static bool interrupt = false;

static void sigint(int)
{
	vlog("interrupt\n");
	interrupt = true;
}

static void sigchild(int)
{
	while(waitpid(0, 0, WNOHANG) > 0)
		(void)0;
}

#if 0
static string stamp_to_string(uint64_t stamp)
{
	int min;
	int sec;
	int csec;

	sec		= stamp / 1000000;
	min		= sec / 60;
	sec		= sec % 60;
	csec	= stamp % 1000000;
	csec	= csec / 10000;

	char outstring[32];
	snprintf(outstring, sizeof(outstring), "%02d:%02d.%02d", min, sec, csec);
	return(string(outstring));
}
#endif

static void call_notifiers(mp3notifiers & notifiers, mp3meta & current_meta)
{
	mp3notifiers::iterator notify;

	for(notify = notifiers.begin(); notify != notifiers.end(); notify++)
	{
		string path, base, filename;
		string artist, album, song, track;

		notify->clearparameters();

		try
		{
			path		= current_meta.get_path();
			base		= current_meta.get_base();
			filename	= current_meta.get_filename();
			artist		= current_meta.get_artist();
			album		= current_meta.get_album();
			song		= current_meta.get_song();
			track		= current_meta.get_track();
		}
		catch(string e)
		{
		}

		notify->addparameter("%p", path);
		notify->addparameter("%f", base);
		notify->addparameter("%F", filename);

		notify->addparameter("%A", artist);
		notify->addparameter("%a", album);
		notify->addparameter("%s", song);
		notify->addparameter("%t", track);

		//vlog("path = %s\n", path.c_str());
		//vlog("base = %s\n", base.c_str());
		//vlog("filename = %s\n", filename.c_str());

		//vlog("artist = %s\n", artist.c_str());
		//vlog("album = %s\n", album.c_str());
		//vlog("song = %s\n", song.c_str());
		//vlog("track = %s\n", track.c_str());

		notify->exec();
	}
}

static int real_main(int argc, char ** argv)
{
	typedef enum
	{
		notify_opt,
		start_opt
	} option_types;

	static const struct option options[] =
	{
		{ "notify", 1, 0, notify_opt },
		{ "start", 1, 0, start_opt },
		{ "?", 0, 0, 0 },
		{ 0, 0, 0, 0 }
	};

	int				opt;
	string			start;
	mp3notifiers	notifiers;
	EmdState		emd_state;

	signal(SIGINT,  sigint);
	signal(SIGCHLD, sigchild);

	while((opt = getopt_long_only(argc, argv, "", options, 0)) != -1)
	{
		switch(opt)
		{
			case(notify_opt):
			{
				mp3notifier notifier(optarg);
				notifiers.push_back(notifier);
				break;
			}

			case(start_opt):
			{
				start = optarg;
				break;
			}

			default:
			{
				fprintf(stderr, "usage: emd {--notify <notify_program>} [--start <path>]\n");
				exit(-1);
				break;
			}
		}
	}

	if(start != "")
	{
		emd_state.set_stopped();
		emd_state.add_tree(start);
		emd_state.shuffle();
		emd_state.set_playing();
	}

#ifdef MHD_mode_multithread
	HttpServer http_server(emd_state, 8889, true);
#else
#ifdef MHD_mode_singlethread
	HttpServer http_server(emd_state, 8889, false);
#else
#error "Either MHD_mode_singlethread or MHD_mode_multithread should be set"
#endif
#endif

	int				position = 0;
	uint64_t		start_stamp;
	uint64_t		elapsed_stamp;
	int				wait_time;
	struct timeval	tv;
	// int			logcount = 0;

	while(!emd_state.get_quit() && !interrupt)
	{
		while(!emd_state.is_playing() && !emd_state.get_quit() && !interrupt)
			http_server.poll(1000000);

		if(emd_state.get_quit() || interrupt)
			continue;

		mp3meta current_meta = emd_state.get_current();

		call_notifiers(notifiers, current_meta);

		gettimeofday(&tv, 0);
		start_stamp = (tv.tv_sec * 1000000) + tv.tv_usec;

		// vlog("emd: open file %s\n", current_meta.get_filename().c_str());

		try
		{
			MP3Reader mp3_reader(current_meta.get_filename());
			MP3Reader::mp3reader_frame_t frame;

			emd_state.set_skip(false);

			while(!emd_state.get_quit() && !emd_state.get_skip() && !interrupt && (mp3_reader.get_frame(frame) > 0))
			{
				position += frame.length;

				gettimeofday(&tv, 0);
				elapsed_stamp = (tv.tv_sec * 1000000) + tv.tv_usec;
				elapsed_stamp -= start_stamp;

				emd_state.stream_put(frame.length, frame.buffer);
				wait_time	= frame.stamp - elapsed_stamp;

				if(wait_time < 0)
					wait_time = 1000;

				//if((logcount++ % 250) == 0)
					//vlog("conns: %02d, sent: %4d, elapsed: %s, pos: %s (%d), wait: %d\n", connections, frame.length,
							//stamp_to_string(elapsed_stamp).c_str(), stamp_to_string(frame.stamp).c_str(), position, wait_time);

				http_server.poll(wait_time);
			}

			emd_state.stream_flush(10000000);
		}
		catch(string e)
		{
			vlog("caught exception \"%s\" for file \"%s\"\n", e.c_str(), current_meta.get_filename().c_str());
			emd_state.next();
		}

		if(!emd_state.get_skip())
			emd_state.next();
	}

	return(0);
}

int main(int argc, char ** argv)
{
	try
	{
		return(real_main(argc, argv));
	}
	catch(string error)
	{
		vlog("Caught fatal error: %s, additional info: %s\n", error.c_str(), strerror(errno));
		exit(-1);
	}
}
