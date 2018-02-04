#ifndef __http_server_h__
#define __http_server_h__

#include <sys/types.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <time.h>
#include <microhttpd.h>
#include <pthread.h>

#include <string>
using std::string;

#include <map>
using std::map;

#include "syslog.h"
#include "emd_state.h"

class HttpServer
{
	private:

		enum
		{
			streaming_buffer_size = 128
		};

		typedef map<string, string> string_string_map;

		struct KeyValues
		{
			string_string_map	data;
			string				dump(bool html) const;
		};

		typedef struct 
		{
			int							callback_count;
			struct MHD_PostProcessor *	postprocessor;
			KeyValues					values;
		} ConnectionData;

		typedef struct 
		{
			EmdState *	emd_state;
			int			fd;
			int			shoutcast_interleave_offset;
			int			shoutcast_current_offset;
		} StreamingData;

		typedef struct
		{
			typedef int (HttpServer::*dispatcher_function_t)(MHD_Connection *, const string & method,
							ConnectionData * con_cls) const;
			typedef map<string, dispatcher_function_t> map_t;

			map_t data;
		} PageHandler;

		PageHandler::map_t	page_dispatcher_map;

		struct MHD_Daemon * daemon;
		EmdState &			emd_state;
		bool				multithread;
		static const char *	id_cookie_name;

		static string		html_header(const string & title = "", int reload = -1, string reload_url = "");
		static string		html_footer();

		int					send_html(MHD_Connection * connection, const string & title, int http_code,
									const string & data, int reload = -1, const string & reload_url = "",
									const string & cookie_id = "", const string & cookie_value = "") const throw(string);

		int					http_error(MHD_Connection * connection, int code,
									const string & message) const throw(string);

		static int 			callback_keyvalue_iterator(void * cls, enum MHD_ValueKind kind, const char * key, const char * value);
		KeyValues			get_http_values(struct MHD_Connection * connection, enum MHD_ValueKind kind) const;

		static int			callback_answer_connection(void * object,
								struct MHD_Connection * connection,
								const char * url, const char * method, const char * version,
								const char * upload_data, size_t * upload_data_size,
								void ** con_cls);

		int					answer_connection(struct MHD_Connection * connection,
								const string & url, const string & method, const string & version,
								ConnectionData * con_cls, size_t * upload_data_size, const char * upload_data) const;

		static int			callback_postdata_iterator(void * cls, enum MHD_ValueKind kind,
								const char * key, const char * filename, const char * content_type,
								const char * transfer_encoding, const char * data, uint64_t off, size_t size);

		static void *		callback_request_completed(void * cls, struct MHD_Connection * connection,
								void ** con_cls, enum MHD_RequestTerminationCode toe);

		int					send_stream(struct MHD_Connection *, bool shoutcast) const throw(string);

		static ssize_t		send_stream_callback(void * cls, uint64_t pos, char * buf, size_t max);
		static void			send_stream_free_callback(void * cls);

		int page_dispatcher_root			(MHD_Connection *, const string & method, ConnectionData * con_cls) const;
		int page_dispatcher_debug			(MHD_Connection *, const string & method, ConnectionData * con_cls) const;
		int page_dispatcher_previous		(MHD_Connection *, const string & method, ConnectionData * con_cls) const;
		int page_dispatcher_next			(MHD_Connection *, const string & method, ConnectionData * con_cls) const;
		int page_dispatcher_stop			(MHD_Connection *, const string & method, ConnectionData * con_cls) const;
		int page_dispatcher_pause			(MHD_Connection *, const string & method, ConnectionData * con_cls) const;
		int page_dispatcher_play			(MHD_Connection *, const string & method, ConnectionData * con_cls) const;
		int page_dispatcher_list			(MHD_Connection *, const string & method, ConnectionData * con_cls) const;
		int page_dispatcher_goto			(MHD_Connection *, const string & method, ConnectionData * con_cls) const;
		int page_dispatcher_stream_http		(MHD_Connection *, const string & method, ConnectionData * con_cls) const;
		int page_dispatcher_stream_shoutcast(MHD_Connection *, const string & method, ConnectionData * con_cls) const;

	public:

				HttpServer(EmdState & emd_state , int tcp_port = 8889, bool multithread = true) throw(string);
				~HttpServer() throw(string);

		void	poll(int timeout) throw(string);
};

#endif
