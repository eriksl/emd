#ifndef emd_state_h__
#define emd_state_h__

#include <stdint.h>
#include <cstddef>
#include <sys/types.h>
#include <pthread.h>

#include <string>
using std::string;

#include <deque>
using std::deque;

#include "mp3meta.h"

class EmdState
{
	private:

		typedef enum
		{
			stopped,
			paused,
			playing
		} play_state_t;

		typedef deque<mp3meta>	_mp3metas_t;
		typedef deque<int>		_pipe_fds_t;

		static ptrdiff_t	_shuffle_random_value(ptrdiff_t mod);
		static ptrdiff_t	(*_shuffle_random_value_ptr)(ptrdiff_t);

		pthread_rwlock_t	_rwlock;
		_mp3metas_t			_mp3metas;
		_pipe_fds_t			_pipe_fds;
		int					_current;
		play_state_t		_play_state;
		bool				_skip;
		bool				_quit;
		bool				_debug;

		void		stream_send_buffer(int fd, int size) const throw(string);
		void		stream_receive_buffer(int fd, int size) const throw(string);

	public:
					EmdState() throw(string);
					~EmdState() throw(string);

		bool		empty();
		int			size();
		void		clear();
		void		erase(int ix);
		void		add(const mp3meta & file) throw(string);
		void		add(string filename) throw(string);
		void		add_tree(string directory) throw(string);
		mp3meta		get_last() throw(string);
		mp3meta		operator[] (int ix) throw(string);
		mp3meta		get_previous() throw(string);
		mp3meta		get_current() throw(string);
		mp3meta		get_next() throw(string);
		void		shuffle(void);

		int			stream_add_pipe(int buffer_size = -1) throw(string);
		int			stream_count();
		int			stream_put(int size, const unsigned char * data);
		int			stream_get(int fd, uint64_t timeout, int size, unsigned char * data) const throw(string);
		void		stream_flush(int64_t timeout = -1);

		void		next();
		void		previous();

		int			get_previous_index();
		int			get_current_index();
		int			get_next_index();
		void		go_to(int) throw(string);

		void		set_skip(bool);
		bool		get_skip();

		void		set_quit(bool);
		bool		get_quit();

		void		set_debug(bool);
		bool		get_debug();

		void		set_stopped();
		void		set_paused();
		void		set_playing();

		bool		is_stopped();
		bool		is_paused();
		bool		is_playing();

		string		get_playing_state();
};

#endif
