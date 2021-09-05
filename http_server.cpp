#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <string.h>

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;

#include "http_server.h"
#include "syslog.h"

HttpServer::HttpServer(EmdState & emd_state_in, int tcp_port, bool multithread_in) throw(string)
	: emd_state(emd_state_in), multithread(multithread_in)
{
	page_dispatcher_map["/"]					=	&HttpServer::page_dispatcher_root;
	page_dispatcher_map["/debug"]				=	&HttpServer::page_dispatcher_debug;
	page_dispatcher_map["/previous"]			=	&HttpServer::page_dispatcher_previous;
	page_dispatcher_map["/next"]				=	&HttpServer::page_dispatcher_next;
	page_dispatcher_map["/stop"]				=	&HttpServer::page_dispatcher_stop;
	page_dispatcher_map["/pause"]				=	&HttpServer::page_dispatcher_pause;
	page_dispatcher_map["/play"]				=	&HttpServer::page_dispatcher_play;
	page_dispatcher_map["/list"]				=	&HttpServer::page_dispatcher_list;
	page_dispatcher_map["/goto"]				=	&HttpServer::page_dispatcher_goto;
	page_dispatcher_map["/stream_http"]			=	&HttpServer::page_dispatcher_stream_http;
	page_dispatcher_map["/stream_shoutcast"]	=	&HttpServer::page_dispatcher_stream_shoutcast;

	int						fd;
	struct sockaddr_in6		saddr;

	if((fd = socket(PF_INET6, SOCK_STREAM, 0)) < 0)
		throw(string("socket"));

	int value = 1;

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)))
		throw(string("setsockopt(SO_LINGER)"));

	memset(&saddr, 0, sizeof(saddr));
	saddr.sin6_family	= PF_INET6;
	saddr.sin6_port		= htons(tcp_port);

	if(bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)))
		throw(string("bind"));

	if(listen(fd, 1))
		throw(string("listen"));

	if(multithread)
		daemon = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_IPv6 | MHD_USE_DEBUG,
				tcp_port, 0, 0, &HttpServer::callback_answer_connection, this,
				MHD_OPTION_NOTIFY_COMPLETED, &HttpServer::callback_request_completed, this,
				MHD_OPTION_LISTEN_SOCKET, fd,
				MHD_OPTION_END);
	else
		daemon = MHD_start_daemon(MHD_USE_IPv6 | MHD_USE_DEBUG,
				tcp_port, 0, 0, &HttpServer::callback_answer_connection, this,
				MHD_OPTION_NOTIFY_COMPLETED, &HttpServer::callback_request_completed, this,
				MHD_OPTION_LISTEN_SOCKET, fd,
				MHD_OPTION_END);

	if(daemon == 0)
		throw(string("Cannot start http daemon"));
}

HttpServer::~HttpServer() throw(string)
{
	MHD_stop_daemon(daemon);
	daemon = 0;
}

string HttpServer::KeyValues::dump(bool html) const
{
	string rv;
	map<string, string>::const_iterator it;

	if(html)
		rv = "<table border=\"1\" cellspacing=\"0\" cellpadding=\"0\">\n";

	for(it = data.begin(); it != data.end(); it++)
	{
		if(html)
			rv += "<tr><td>\n";

		rv += it->first;

		if(html)
			rv += "</td><td>\n";
		else
			rv += " = ";

		rv += it->second;

		if(html)
			rv += "</td></tr>\n";
		else
			rv += "\n";
	}

	if(html)
		rv += "</table>\n";

	return(rv);
}

void HttpServer::poll(int timeout) throw(string)
{
	if(multithread)
		usleep(timeout);
	else
	{
		fd_set			read_fd_set, write_fd_set, except_fd_set;
		int				max_fd = 0;
		struct timeval	tv;

		FD_ZERO(&read_fd_set);
		FD_ZERO(&write_fd_set);
		FD_ZERO(&except_fd_set);

		if(MHD_get_fdset(daemon, &read_fd_set, &write_fd_set, &except_fd_set, &max_fd) == MHD_NO)
			throw(string("error in MHD_get_fdset"));

		tv.tv_sec	= timeout / 1000000;
		tv.tv_usec	= (timeout % 1000000);

		if(select(max_fd + 1, &read_fd_set, &write_fd_set, &except_fd_set, &tv) != 0)
			MHD_run(daemon);
	}
}

string HttpServer::html_header(const string & title, int reload, string reload_url)
{
	string refresh_header;

	if(reload >= 0)
	{
		refresh_header = "        <meta http-equiv=\"Refresh\" content=\"" + lexical_cast<string>(reload);

		if(reload_url.size() != 0)
			refresh_header += ";url=" + reload_url;

		refresh_header += "\"/>\n";
	}

	return(string("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n") +
				"<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n" +
    			"    <head>\n" +
        		"        <meta http-equiv=\"Content-type\" content=\"text/html; charset=UTF-8\"/>\n" +
				refresh_header +
        		"        <title>" + title + "</title>\n" + 
    			"    </head>\n" +
    			"    <body>\n");
}

string HttpServer::html_footer()
{
	return(string("    </body>\n") +
				"</html>\n");
}

MHD_Result HttpServer::send_html(MHD_Connection * connection, const string & title, int http_code,
			const string & message, int reload, const string & reload_url,
			const string & cookie_id, const string & cookie_value) const throw(string)
{
	MHD_Result				rv;
	string					data;
	struct MHD_Response	*	response;
	char *					dataptr;

	data = html_header(title, reload, reload_url);
	data += message;
	data += html_footer();

	//response = MHD_create_response_from_data(data.size(), (void *)data.c_str(), MHD_NO, MHD_YES);
	dataptr = strdup(data.c_str());
	response = MHD_create_response_from_buffer(data.size(), dataptr, MHD_RESPMEM_MUST_FREE);
	MHD_add_response_header(response, "Content-Type", "text/html");

	if(cookie_id.size())
	{
		string cookie = cookie_id + "=" + cookie_value + "; path=/;";
		MHD_add_response_header(response, "Set-Cookie", cookie.c_str());
	}

	rv = MHD_queue_response(connection, http_code, response);
	MHD_destroy_response(response);

	return(rv);
}

MHD_Result HttpServer::http_error(MHD_Connection * connection, int http_code, const string & message) const throw(string)
{
	return(send_html(connection, "ERROR", http_code, string("<p>") + message + "</p>\n"));
}

MHD_Result HttpServer::callback_answer_connection(void *void_http_server,
		struct MHD_Connection *connection,
		const char *url, const char *method, const char *version,
		const char *upload_data, unsigned long int *upload_data_size,
		void **con_cls)
{
	HttpServer* http_server = (HttpServer *)void_http_server;

	if(*con_cls == 0)
	{
		ConnectionData * ncd		= new(ConnectionData);
		ncd->callback_count			= 0;
		ncd->postprocessor			= MHD_create_post_processor(connection, 1024, callback_postdata_iterator, ncd);
		*con_cls = (void *)ncd;
	}
	else
		(**(ConnectionData **)con_cls).callback_count++;

	if(string(method) == "POST")
	{
		if((**(ConnectionData **)con_cls).callback_count == 0)
			return(MHD_YES);
	}

	if(*upload_data_size)
	{
		MHD_post_process((**(ConnectionData **)con_cls).postprocessor, upload_data, *upload_data_size);
		*upload_data_size = 0;
		return(MHD_YES);
	}

	return(http_server->answer_connection(connection,
		url, method, version, *(ConnectionData **)con_cls, upload_data_size, upload_data));
};

MHD_Result HttpServer::answer_connection(struct MHD_Connection * connection,
		const string & url, const string & method, const string &,
		ConnectionData * con_cls, size_t *, const char *) const
{
	PageHandler::map_t::const_iterator	it;
	PageHandler::dispatcher_function_t	fn;

	for(it = page_dispatcher_map.begin(); it != page_dispatcher_map.end(); it++)
		if(it->first == string(url)) 
			break;

	if(it != page_dispatcher_map.end())
	{
		fn = it->second;
		return((this->*fn)(connection, method, con_cls));
	}

	return(http_error(connection, MHD_HTTP_NOT_FOUND, string("URI ") + url + " not found"));
}

MHD_Result HttpServer::callback_keyvalue_iterator(void * cls, enum MHD_ValueKind, const char * key, const char * value)
{
	KeyValues * rv = (KeyValues *)cls;

	rv->data[string(key)] = string(value);

	return(MHD_YES);
}

HttpServer::KeyValues HttpServer::get_http_values(struct MHD_Connection * connection, enum MHD_ValueKind kind) const
{
	KeyValues rv;

	MHD_get_connection_values(connection, kind, callback_keyvalue_iterator, &rv);

	return(rv);
}

void * HttpServer::callback_request_completed(void *, struct MHD_Connection *, void **con_cls, enum MHD_RequestTerminationCode)
{
	if(con_cls && *con_cls)
	{
		ConnectionData * cdp = (ConnectionData *)*con_cls;

		if(cdp->postprocessor)
		{
			MHD_destroy_post_processor(cdp->postprocessor);
			cdp->postprocessor = 0;
		}

		delete(cdp);
		*con_cls = 0;
	}

	return(0);
}

MHD_Result HttpServer::callback_postdata_iterator(void * con_cls, enum MHD_ValueKind,
		const char * key, const char *, const char *,
		const char *, const char * data, uint64_t, size_t size)
{
	string mangle;
	ConnectionData * condata = (ConnectionData *)con_cls;

	mangle.append(data, size);
	condata->values.data[key] = mangle;
	return(MHD_YES);
}

MHD_Result HttpServer::send_stream(struct MHD_Connection * connection, bool shoutcast) const throw(string)
{
	struct MHD_Response	*	response;
	MHD_Result				rv;

	StreamingData * streaming_data			= new(StreamingData);
	streaming_data->emd_state				= &emd_state;
	streaming_data->fd						= emd_state.stream_add_pipe(streaming_buffer_size);

	streaming_data->shoutcast_interleave_offset	= shoutcast ? 8192 : 0;
	streaming_data->shoutcast_current_offset	= 0;

	response = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, streaming_buffer_size,
			send_stream_callback, streaming_data, send_stream_free_callback);

	if(!response)
		throw(string("HttpServer::send_stream: MHD_create_response_from_callback returns NULL"));
		
	MHD_add_response_header(response, "Content-Type", "audio/mp3");

	if(shoutcast)
	{
		char hostname[256];
		gethostname(hostname, sizeof(hostname));

		MHD_add_response_header(response, "Icy-Name", hostname);
		MHD_add_response_header(response, "Icy-Genre", "mixed");
		MHD_add_response_header(response, "Icy-Url", (string("http://") + hostname + string(":8889")).c_str());
		MHD_add_response_header(response, "Icy-Pub", "0");
		MHD_add_response_header(response, "Icy-MetaInt", (lexical_cast<string>(streaming_data->shoutcast_interleave_offset)).c_str());
	}

	rv = MHD_queue_response(connection, MHD_HTTP_OK | (shoutcast ? MHD_ICY_FLAG : 0), response);
	MHD_destroy_response(response);

	return(rv);
}

ssize_t HttpServer::send_stream_callback(void * cls, uint64_t, char * buf, size_t length)
{
	int received;
	StreamingData * streaming_data = (StreamingData *)cls;

	//if(streaming_data->shoutcast_interleave_offset)
		//vlog("*** interleave_offset = %d, current_offset = %d\n", streaming_data->shoutcast_interleave_offset, streaming_data->shoutcast_current_offset);

	if(streaming_data->shoutcast_interleave_offset != 0)
	{
		if(streaming_data->shoutcast_current_offset == -1)
		{
			mp3meta meta = streaming_data->emd_state->get_current();
			memset(buf, 0, length);
			snprintf(buf, length, "@StreamTitle='%s - %s';", meta.get_artist().c_str(), meta.get_song().c_str());
			length = ((strlen(buf) - 1 + 15) >> 4);
			((uint8_t *)buf)[0] = length & 0xff;
			received = (length << 4) + 1;
			streaming_data->shoutcast_current_offset = 0;

			//vlog("*** sending meta data [%d/%d/%d]: \"%s\"\n", strlen(buf + 1), (uint32_t)buf[0], received, buf + 1);

			return(received);
		}

		int bytes_until_shoutcast_meta = streaming_data->shoutcast_interleave_offset - streaming_data->shoutcast_current_offset;

		if((uint64_t)length > (uint64_t)bytes_until_shoutcast_meta)
		{
			//vlog("*** read length adjusted from %d to %d, bytes counter: %d\n",
					//length, bytes_until_shoutcast_meta, streaming_data->shoutcast_current_offset);
			length = bytes_until_shoutcast_meta;
		}
	}

	try
	{
		int queued1, queued2;

		if(ioctl(streaming_data->fd, SIOCINQ, &queued1) < 0)
			vlog("HttpServer::send_stream::ioctl: %m\n");

		received = streaming_data->emd_state->stream_get(streaming_data->fd, -1, length, (unsigned char *)buf);

		if(ioctl(streaming_data->fd, SIOCINQ, &queued2) < 0)
			vlog("HttpServer::send_stream::ioctl: %m\n");

		if(queued1 > (128*1024))
			vlog("<<< fd %2d filling up, queued: %8d, received: %8d, queued: %8d\n",
					streaming_data->fd, queued1, received, queued2);
	}
	catch(string e)
	{
		vlog("HttpServer::send_stream_callback: stream_get said: %s\n", e.c_str());
		return(-1);
	}

	if(received < 0)
	{
		vlog("HttpServer::send_stream::received < 0\n");
		return(0);
	}

	if(received == 0)
	{
		vlog("HttpServer::send_stream::received == 0\n");
		return(-1);
	}

	streaming_data->shoutcast_current_offset += received;

	if(streaming_data->shoutcast_current_offset == streaming_data->shoutcast_interleave_offset)
		streaming_data->shoutcast_current_offset = -1;

	return(received);
}

void HttpServer::send_stream_free_callback(void * cls)
{
	StreamingData * streaming_data = (StreamingData *)cls;

	// vlog("HttpServer::send_stream_free_callback: closing fd %d\n", streaming_data->fd);

	close(streaming_data->fd);
	delete(streaming_data);
}
