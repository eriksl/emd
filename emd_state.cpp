#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <unistd.h>

#include <algorithm>

#include "emd_state.h"
#include "syslog.h"

EmdState::EmdState() throw(string) :
	_current(0),
	_play_state(stopped),
	_skip(false),
	_quit(false)
{
	pthread_rwlock_init(&_rwlock, 0);
}

EmdState::~EmdState() throw(string)
{
	pthread_rwlock_destroy(&_rwlock);
}

bool EmdState::empty()
{
	pthread_rwlock_rdlock(&_rwlock);
	bool rv = _mp3metas.empty();
	pthread_rwlock_unlock(&_rwlock);
	return(rv);
}

int EmdState::size()
{
	pthread_rwlock_rdlock(&_rwlock);
	int rv = _mp3metas.size();
	pthread_rwlock_unlock(&_rwlock);
	return(rv);
}

void EmdState::clear()
{
	pthread_rwlock_wrlock(&_rwlock);
	_play_state = stopped;
	_skip		= true;
	_mp3metas.clear();
	pthread_rwlock_unlock(&_rwlock);
}

void EmdState::erase(int ix)
{
	pthread_rwlock_wrlock(&_rwlock);

	if((ix < 0) || (ix > (int)_mp3metas.size()))
	{
		pthread_rwlock_unlock(&_rwlock);
		throw(string("EmdState::erase: index out of range"));
	}

	_mp3metas.erase(_mp3metas.begin() + ix);

	pthread_rwlock_unlock(&_rwlock);
}

void EmdState::add(const mp3meta & file) throw(string)
{
	pthread_rwlock_wrlock(&_rwlock);
	_mp3metas.push_back(file);
	pthread_rwlock_unlock(&_rwlock);
}

void EmdState::add(string filename) throw(string)
{
	pthread_rwlock_wrlock(&_rwlock);
	_mp3metas.push_back(mp3meta(filename));
	pthread_rwlock_unlock(&_rwlock);
}

void EmdState::next()
{
	pthread_rwlock_wrlock(&_rwlock);

	if(_mp3metas.empty())
	{
		pthread_rwlock_unlock(&_rwlock);
		vlog("EmdState::next: no files");
	}

	if(++_current >= (int)_mp3metas.size())
		_current = 0;

	_skip = true;

	pthread_rwlock_unlock(&_rwlock);
}

void EmdState::previous()
{
	pthread_rwlock_wrlock(&_rwlock);

	if(_mp3metas.empty())
	{
		pthread_rwlock_unlock(&_rwlock);
		vlog("EmdState::previous: no files");
	}

	if(--_current < 0)
		_current = _mp3metas.size() - 1;

	_skip		= true;

	pthread_rwlock_unlock(&_rwlock);
}

mp3meta EmdState::get_last() throw(string)
{
	pthread_rwlock_rdlock(&_rwlock);
	mp3meta rv(_mp3metas.back());
	pthread_rwlock_unlock(&_rwlock);
	return(rv);
}

mp3meta EmdState::operator[](int ix) throw(string)
{
	pthread_rwlock_rdlock(&_rwlock);

	if((ix < 0) || (ix >= (int)_mp3metas.size()))
	{
		pthread_rwlock_unlock(&_rwlock);
		throw(string("EmdState::operator[]: index out of range"));
	}

	mp3meta rv(_mp3metas[ix]);
	pthread_rwlock_unlock(&_rwlock);
	return(rv);
}

mp3meta EmdState::get_previous() throw(string)
{
	pthread_rwlock_rdlock(&_rwlock);

	if((_current < 0) || (_current > (int)_mp3metas.size()))
	{
		pthread_rwlock_unlock(&_rwlock);
		throw(string("EmdState::get_next: current index out of range"));
	}

	int ix = _current - 1;

	if(ix < 0)
		ix = _mp3metas.size() - 1;

	mp3meta rv(_mp3metas[ix]);
	pthread_rwlock_unlock(&_rwlock);
	return(rv);
}

mp3meta EmdState::get_current() throw(string)
{
	pthread_rwlock_rdlock(&_rwlock);

	if((_current < 0) || (_current > (int)_mp3metas.size()))
	{
		pthread_rwlock_unlock(&_rwlock);
		throw(string("EmdState::get_current: current index out of range"));
	}

	mp3meta rv(_mp3metas[_current]);
	pthread_rwlock_unlock(&_rwlock);
	return(rv);
}

mp3meta EmdState::get_next() throw(string)
{
	pthread_rwlock_rdlock(&_rwlock);

	if((_current < 0) || (_current > (int)_mp3metas.size()))
	{
		pthread_rwlock_unlock(&_rwlock);
		throw(string("EmdState::get_next: current index out of range"));
	}

	int ix = _current + 1;

	if(ix >= (int)_mp3metas.size())
		ix = 0;

	mp3meta rv(_mp3metas[ix]);
	pthread_rwlock_unlock(&_rwlock);
	return(rv);
}

ptrdiff_t EmdState::_shuffle_random_value(ptrdiff_t mod)
{
	uint64_t 		rv;
	struct timeval	tv;

	gettimeofday(&tv, 0);
	rv = tv.tv_sec * 1000000 + tv.tv_usec;
	return(rv % mod);
}

ptrdiff_t (*EmdState::_shuffle_random_value_ptr)(ptrdiff_t) = EmdState::_shuffle_random_value;

void EmdState::shuffle(void)
{
	pthread_rwlock_wrlock(&_rwlock);
	random_shuffle(_mp3metas.begin(), _mp3metas.end(), _shuffle_random_value_ptr);
	pthread_rwlock_unlock(&_rwlock);
}

#if 0
bool mp3meta::operator <(mp3meta & other)
{
	if(id3_valid)
	{
		if(artist < other.get_artist())
			return(true);
		else if(artist > other.get_artist())
			return(false);
		else if(album < other.get_album())
			return(true);
		else if(album > other.get_album())
			return(false);
		else if(track < other.get_track())
			return(true);
		else if(track > other.get_track())
			return(false);
		else if(song < other.get_song())
			return(true);
		else if(song > other.get_song())
			return(false);
		else if(filename < other.get_filename())
			return(true);
		else if(filename > other.get_filename())
			return(false);
		else
			return(false);
	}
	else
		return(filename < other.get_filename());
}
#endif

int EmdState::get_previous_index()
{
	pthread_rwlock_rdlock(&_rwlock);

	int rv = _current - 1;

	if(rv < 0)
		rv = _mp3metas.size() - 1;

	pthread_rwlock_unlock(&_rwlock);

	return(rv);
}

int EmdState::get_current_index()
{
	pthread_rwlock_rdlock(&_rwlock);
	int rv = _current;
	pthread_rwlock_unlock(&_rwlock);
	return(rv);
}

int EmdState::get_next_index()
{
	pthread_rwlock_rdlock(&_rwlock);

	int rv = _current + 1;

	if(rv >= (int)_mp3metas.size())
		rv = 0;

	pthread_rwlock_unlock(&_rwlock);

	return(rv);
}

void EmdState::set_skip(bool in)
{
	pthread_rwlock_wrlock(&_rwlock);
	_skip = in;
	pthread_rwlock_unlock(&_rwlock);
}

bool EmdState::get_skip()
{
	pthread_rwlock_rdlock(&_rwlock);
	bool rv = _skip;
	pthread_rwlock_unlock(&_rwlock);
	return(rv);
}

void EmdState::go_to(int newcurrent) throw(string)
{
	pthread_rwlock_wrlock(&_rwlock);

	if((_current < 0) || (_current > (int)_mp3metas.size()))
	{
		pthread_rwlock_unlock(&_rwlock);
		throw(string("EmdState::goto: out of range"));
	}

	_current	= newcurrent;
	_skip		= true;

	pthread_rwlock_unlock(&_rwlock);
}

void EmdState::set_quit(bool in)
{
	pthread_rwlock_wrlock(&_rwlock);
	_quit = in;
	pthread_rwlock_unlock(&_rwlock);
}

bool EmdState::get_quit()
{
	pthread_rwlock_rdlock(&_rwlock);
	bool rv = _quit;
	pthread_rwlock_unlock(&_rwlock);
	return(rv);
}

void EmdState::set_stopped()
{
	pthread_rwlock_wrlock(&_rwlock);

	if(_play_state == playing)
	{
		_play_state = stopped;
		_skip		= true;
		_current	= 0;
	}
	pthread_rwlock_unlock(&_rwlock);
}

void EmdState::set_paused()
{
	pthread_rwlock_wrlock(&_rwlock);

	if(_play_state == playing)
	{
		_play_state = paused;
		_skip		= true;
	}
	pthread_rwlock_unlock(&_rwlock);
}

void EmdState::set_playing()
{
	pthread_rwlock_wrlock(&_rwlock);

	if(_play_state != playing)
	{
		_play_state = playing;
		_skip		= false;
	}
	pthread_rwlock_unlock(&_rwlock);
}

bool EmdState::is_stopped()
{
	pthread_rwlock_rdlock(&_rwlock);
	bool rv = (_play_state == stopped);
	pthread_rwlock_unlock(&_rwlock);
	return(rv);
}

bool EmdState::is_paused()
{
	pthread_rwlock_rdlock(&_rwlock);
	bool rv = (_play_state == paused);
	pthread_rwlock_unlock(&_rwlock);
	return(rv);
}

bool EmdState::is_playing()
{
	pthread_rwlock_rdlock(&_rwlock);
	bool rv = (_play_state == playing);
	pthread_rwlock_unlock(&_rwlock);
	return(rv);
}

string EmdState::get_playing_state()
{
	pthread_rwlock_rdlock(&_rwlock);
	string rv;

	switch(_play_state)
	{
		case(stopped): rv = "stopped"; break;
		case(paused): rv = "paused"; break;
		case(playing): rv = "playing"; break;
		default: rv = "(unknown)"; break;
	}

	pthread_rwlock_unlock(&_rwlock);
	return(rv);
}

void EmdState::add_tree(string tree) throw(string)
{
	DIR * 			dir;
	deque<string>	dirs;
	struct dirent * dirdata;	
	string			dirname;
	struct stat		st;
	string			full_filename;

	dirs.push_front(tree);

	while(!dirs.empty())
	{
		dirname = dirs.back();
		dirs.pop_back();

		if(!(dir = opendir(dirname.c_str())))
			continue;

		while((dirdata = readdir(dir)) != 0)
		{
			if(!strcmp(dirdata->d_name, "..") || !strcmp(dirdata->d_name, "."))
				continue;

			full_filename = dirname + "/" + dirdata->d_name;

			if(stat(full_filename.c_str(), &st))
				continue;

			if(S_ISDIR(st.st_mode))
				dirs.push_front(full_filename);
			else
				if(strstr(dirdata->d_name, ".mp3"))
					add(full_filename);
		}

		closedir(dir);
	}

	_current = 0;
}

void EmdState::stream_send_buffer(int fd, int _size) const throw(string)
{
	int			requested = _size;
	int			assigned = -1;
	socklen_t	assigned_size = sizeof(assigned);

	if(getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &assigned, &assigned_size))
		throw(string("EmdState::stream_send_buffer::getsockopt(SO_SNDBUF)"));

	// vlog("+ [%d] send buffer default: %d\n", fd, assigned / 2);
	// vlog("+ [%d] send buffer request: %d\n", fd, requested / 1);

	if(setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &requested, sizeof(requested)))
		throw(string("EmdState::stream_send_buffer::setsockopt(SO_SNDBUF)"));

	if(getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &assigned, &assigned_size))
		throw(string("EmdState::stream_send_buffer::getsockopt(SO_SNDBUF)"));

	// vlog("+ [%d] send buffer assigned: %d\n", fd, assigned / 2);

}

void EmdState::stream_receive_buffer(int fd, int _size) const throw(string)
{
	int			requested = _size;
	int			assigned = -1;
	socklen_t	assigned_size = sizeof(assigned);

	if(getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &assigned, &assigned_size))
		throw(string("EmdState::stream_receive_buffer::getsockopt(SO_RCVBUF)"));

	// vlog("+ [%d] receive buffer default: %d\n", fd, assigned / 2);
	// vlog("+ [%d] receive buffer request: %d\n", fd, requested / 1);

	if(setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &requested, sizeof(requested)))
		throw(string("EmdState::stream_receive_buffer::setsockopt(SO_RCVBUF)"));

	if(getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &assigned, &assigned_size))
		throw(string("EmdState::stream_receive_buffer::getsockopt(SO_RCVBUF)"));

	// vlog("+ [%d] receive buffer assigned: %d\n", fd, assigned / 2);

}

int EmdState::stream_add_pipe(int socket_buffer_size) throw(string)
{
	int new_pipes[2];

	if(socketpair(AF_LOCAL, SOCK_STREAM, 0, new_pipes))
		throw(string("EmdState::stream_add_pipe::socketpair()"));

	if(socket_buffer_size != -1)
		stream_send_buffer(new_pipes[0], socket_buffer_size);
	stream_receive_buffer(new_pipes[0], 0);

	stream_send_buffer(new_pipes[1], 0);
	if(socket_buffer_size != -1)
		stream_receive_buffer(new_pipes[1], socket_buffer_size);

	pthread_rwlock_wrlock(&_rwlock);
	_pipe_fds.push_back(new_pipes[0]);
	pthread_rwlock_unlock(&_rwlock);

	return(new_pipes[1]);
}

int EmdState::stream_count()
{
	pthread_rwlock_rdlock(&_rwlock);
	int rv = _pipe_fds.size();
	pthread_rwlock_unlock(&_rwlock);

	return(rv);
}

int EmdState::stream_put(int _size, const unsigned char * data)
{
	pthread_rwlock_wrlock(&_rwlock);

	int						count = 0;
	_pipe_fds_t::iterator	it;
	ssize_t					written;

	for(it = _pipe_fds.begin(); it != _pipe_fds.end(); it++)
	{
		if((written = send(*it, data, _size, MSG_DONTWAIT | MSG_NOSIGNAL)) <= 0)
		{
			if(errno == EPIPE)
			{
				vlog("EmdState::stream_put: remote socket closed down, closing fd %d\n", *it);
				close(*it);
				*it = -1;
				continue;
			}

			if((errno == EAGAIN) || (errno == EWOULDBLOCK))
			{
				vlog("EmdState::stream_put: fd %d not ready (3), %d bytes offered, %d bytes sent, error: %m\n", *it,
						_size, written);

				int queued00, queued01, queued10, queued11;

				ioctl(*it, SIOCINQ, &queued00);
				ioctl(*it, SIOCOUTQ, &queued01);

				ioctl(*it + 1, SIOCINQ, &queued10);
				ioctl(*it + 1, SIOCOUTQ, &queued11);

				vlog(">>> fd %2d not ready (3), "
						"%8d bytes queued(in,0), %8d bytes queued(out,0)"
						"%8d bytes queued(in,1), %8d bytes queued(out,1)\n",
						*it, queued00, queued01, queued10, queued11);

				continue;
			}

			// vlog("stream_put: %m\n");
			close(*it);
			*it = -1;
			continue;
		}

		if(written != _size)
		{
			vlog("EmdState::stream_put: fd %d not ready (4), %d bytes offered, %d bytes sent\n", *it, _size, written);

			int queued;

			ioctl(*it, SIOCOUTQ, &queued);
			vlog(">>> fd %2d not ready (4), %8d bytes queued\n", *it, queued);
		}

		count++;
	}

	pthread_rwlock_unlock(&_rwlock);
	pthread_rwlock_wrlock(&_rwlock);

restart:

	for(it = _pipe_fds.begin(); it != _pipe_fds.end(); it++)
	{
		if(*it == -1)
		{
			_pipe_fds.erase(it);
			pthread_rwlock_unlock(&_rwlock);
			pthread_rwlock_wrlock(&_rwlock);
			goto restart;

		}
	}

	pthread_rwlock_unlock(&_rwlock);

	return(count);
}

int EmdState::stream_get(int fd, uint64_t timeout, int _size, unsigned char * data) const throw(string)
{
	int				pv;
	ssize_t			received;
	struct pollfd	pfd = { fd, POLLIN | POLLHUP | POLLRDHUP, 0 };

	pv = poll(&pfd, 1, timeout / 1000);

	if(pv < 0)
		throw(string("EmdState::stream_get: poll error"));

	if(pv == 0)
		return(-1);

	if(pfd.revents & (POLLRDHUP | POLLHUP))
	{
		close(fd);
		return(0);
	}

	if(pfd.revents & (POLLERR | POLLNVAL))
		throw(string("EmdState::stream_get: fd poll error"));

	if(!(pfd.revents & POLLIN))
		return(-1);

	received = recv(fd, data, _size, 0);

	if((received < 0) && (errno == EAGAIN) && (errno == EWOULDBLOCK))
		return(-1);

	if(received < 0)
	{
		close(fd);
		throw(string("EmdState::stream_get::get: read error"));
	}

	if(received == 0)
		close(fd);

	return((int)received);
}

void EmdState::stream_flush(int64_t timeout)
{
	_pipe_fds_t::iterator	it;
	int						queued_total;
	int						queued;
	int64_t					start_time;
	int64_t					now_time;
	struct timeval			tv;

	gettimeofday(&tv, 0);
	start_time = (tv.tv_sec * 1000000) + tv.tv_usec;

	for(;;)
	{
		queued_total = 0;

		pthread_rwlock_rdlock(&_rwlock);

		for(it = _pipe_fds.begin(); it != _pipe_fds.end(); it++)
		{
			ioctl(*it, SIOCOUTQ, &queued);
			queued_total += queued;
		}

		pthread_rwlock_unlock(&_rwlock);

		if(queued_total == 0)
			break;

		gettimeofday(&tv, 0);
		now_time = (tv.tv_sec * 1000000) + tv.tv_usec;

		if((now_time - start_time) > timeout)
		{
			vlog("EmdState::stream_flush: timeout with %d bytes queued\n", queued_total);
			break;
		}

		// vlog("EmdState::stream_flush: still waiting for %d bytes to be flushed\n", queued_total);
		usleep(10000);
	}

	//vlog("EmdState::stream_wait: done\n");
}
