#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/socket.h>

#include <sstream>
using std::stringstream;

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;

#include "http_server.h"
#include "syslog.h"

MHD_Result HttpServer::page_dispatcher_root(MHD_Connection * connection, const string & method, ConnectionData * con_cls) const
{
	string	data;
	string	playing_state		= emd_state.get_playing_state();
	int		previous_index		= emd_state.get_previous_index();
	int		current_index		= emd_state.get_current_index();
	int		next_index			= emd_state.get_next_index();
	mp3meta	previous			= emd_state[previous_index];
	mp3meta	current				= emd_state[current_index];
	mp3meta	next				= emd_state[next_index];

	if(method != "GET")
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	if(con_cls->callback_count != 0)
		return(MHD_NO);

	data = "<table border=\"1\" cellpadding=\"3\" cellspacing=\"0\">\n";
	data += "<tr style=\"background-color: #ccc; text-align: center;\"><td>" + playing_state + "</td><td>#</td><td>artist</td><td>album</td><td>song</td></tr>";
	data += "<tr style=\"background-color: #fff;\"><td style=\"background-color: #ccc; text-align: center;\">previous</td><td>" + lexical_cast<string>(previous_index) + "</td><td>" + previous.get_artist() + "</td><td>" + previous.get_album() + "</td><td>" + previous.get_song() + "</td></tr>";
	data += "<tr style=\"background-color: yellow;\"><td style=\"background-color: #ccc; text-align: center;\">now</td><td>" + lexical_cast<string>(current_index) + "</td><td>" + current.get_artist() + "</td><td>" + current.get_album() + "</td><td>" + current.get_song() + "</td></tr>";
	data += "<tr style=\"background-color: #fff;\"><td style=\"background-color: #ccc; text-align: center;\">next</td><td>" + lexical_cast<string>(next_index) + "</td><td>" + next.get_artist() + "</td><td>" + next.get_album() + "</td><td>" + next.get_song() + "</td></tr>";
	data += "</table>\n";

	data += "<table style=\"margin: 4px 0px 0px 0px; background-color: #ccc\" border=\"1\" cellpadding=\"2\" cellspacing=\"0\">\n";
	data += "<tr><td colspan=\"2\"><form method=\"post\" action=\"/previous\"><div><input type=\"submit\" value=\"previous\"/></div></form></td>\n";
	data += "<td><form method=\"post\" action=\"/next\"><div><input type=\"submit\" value=\"next\"/></div></form></td></tr>\n";
	data += "<tr><td><form method=\"post\" action=\"/stop\"><div><input type=\"submit\" value=\"stop\"/></div></form></td>\n";
	data += "<td><form method=\"post\" action=\"/pause\"><div><input type=\"submit\" value=\"pause\"/></div></form></td>\n";
	data += "<td><form method=\"post\" action=\"/play\"><div><input type=\"submit\" value=\"play\"/></div></form></td></tr>\n";
	data += "<tr><td colspan=\"3\"><form method=\"post\" action=\"/list\"><div><input type=\"text\" name=\"start\" value=\"0\"/><input type=\"hidden\" name=\"amount\" value=\"10\"/><input type=\"submit\" value=\"list/goto\"/></div></form></td></tr>\n";
	data += "</table>\n";

	data += "<table style=\"margin: 4px 0px 0px 0px; background-color: #ddd;\" border=\"1\" cellpadding=\"3\" cellspacing=\"0\">\n";
	data += "<tr><td>stream</td><td><a href=\"/stream_http\">http</a></td><td><a href=\"stream_shoutcast\">shoutcast</a></td></tr>\n";
	data += "</table>\n";

	return(send_html(connection, "/", MHD_HTTP_OK, data, 30));
}

MHD_Result HttpServer::page_dispatcher_debug(MHD_Connection * connection, const string & method, ConnectionData * con_cls) const
{
	string data, text;
	string id = "";

	HttpServer::KeyValues	headers;
	HttpServer::KeyValues	cookies;
	HttpServer::KeyValues	postdata;
	HttpServer::KeyValues	arguments;
	HttpServer::KeyValues	footer;

	if(method != "GET" && method != "POST")
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	headers		= get_http_values(connection, MHD_HEADER_KIND);
	cookies		= get_http_values(connection, MHD_COOKIE_KIND);
	postdata	= get_http_values(connection, MHD_POSTDATA_KIND);
	arguments	= get_http_values(connection, MHD_GET_ARGUMENT_KIND);
	footer		= get_http_values(connection, MHD_FOOTER_KIND);
	
	data += string("<p>method: ") + method + "</p>";
	data +=	"</p>\n<p>headers";
	data += headers.dump(true);
	data +=	"</p>\n<p>cookies";
	data += cookies.dump(true);
	data +=	"</p>\n<p>postdata arguments";
	data += postdata.dump(true);
	data +=	"</p>\n<p>http footer";
	data += footer.dump(true);
	data +=	"</p>\n<p>GET arguments";
	data += arguments.dump(true);
	data += "</p>\n<p>post args";
	data +=	con_cls->values.dump(true);
	data += "</p>\n";

	return(send_html(connection, "debug", MHD_HTTP_OK, data, 5));
}

MHD_Result HttpServer::page_dispatcher_previous(MHD_Connection * connection, const string & method, ConnectionData * con_cls) const
{
	string	data;

	if(method != "POST")
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	if(con_cls->callback_count != 1)
		return(MHD_YES);

	try
	{
		emd_state.previous();
		data = "<p>previous ok</p>\n";
	}
	catch(string e)
	{
		data = "<p>previous error: " + e + "</p>\n";
	}

	return(send_html(connection, "previous", MHD_HTTP_OK, data, 0, "/"));
}

MHD_Result HttpServer::page_dispatcher_next(MHD_Connection * connection, const string & method, ConnectionData * con_cls) const
{
	string	data;

	if(method != "POST")
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	if(con_cls->callback_count != 1)
		return(MHD_YES);

	try
	{
		emd_state.next();
		data = "<p>next ok</p>\n";
	}
	catch(string e)
	{
		data = "<p>next error: " + e + "</p>\n";
	}

	return(send_html(connection, "next", MHD_HTTP_OK, data, 0, "/"));
}

MHD_Result HttpServer::page_dispatcher_stop(MHD_Connection * connection, const string & method, ConnectionData * con_cls) const
{
	string	data;

	if(method != "POST")
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	if(con_cls->callback_count != 1)
		return(MHD_YES);

	try
	{
		emd_state.set_stopped();
		data = "<p>stop ok</p>\n";
	}
	catch(string e)
	{
		data = "<p>stop error: " + e + "</p>\n";
	}

	return(send_html(connection, "stop", MHD_HTTP_OK, data, 0, "/"));
}

MHD_Result HttpServer::page_dispatcher_pause(MHD_Connection * connection, const string & method, ConnectionData * con_cls) const
{
	string	data;

	if(method != "POST")
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	if(con_cls->callback_count != 1)
		return(MHD_YES);

	try
	{
		emd_state.set_paused();
		data = "<p>pause ok</p>\n";
	}
	catch(string e)
	{
		data = "<p>pause error: " + e + "</p>\n";
	}

	return(send_html(connection, "pause", MHD_HTTP_OK, data, 0, "/"));
}

MHD_Result HttpServer::page_dispatcher_play(MHD_Connection * connection, const string & method, ConnectionData * con_cls) const
{
	string	data;

	if(method != "POST")
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	if(con_cls->callback_count != 1)
		return(MHD_YES);

	try
	{
		emd_state.set_playing();
		data = "<p>play ok</p>\n";
	}
	catch(string e)
	{
		data = "<p>play error: " + e + "</p>\n";
	}

	return(send_html(connection, "play", MHD_HTTP_OK, data, 0, "/"));
}

MHD_Result HttpServer::page_dispatcher_list(MHD_Connection * connection, const string & method, ConnectionData * con_cls) const
{
	string	data;
	int		start;
	int		amount;
	int		ix;

	if(method != "POST")
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	if(con_cls->callback_count == 0)
		return(MHD_YES);

	if(con_cls->values.data.find("start") == con_cls->values.data.end())
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, "Missing POST-value: start"));

	try
	{
		start = lexical_cast<int>(con_cls->values.data["start"]);
	}
	catch(...)
	{
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, "Invalid POST-value: start"));
	}

	if(con_cls->values.data.find("amount") == con_cls->values.data.end())
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, "Missing POST-value: amount"));

	try
	{
		amount = lexical_cast<int>(con_cls->values.data["amount"]);
	}
	catch(...)
	{
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, "Invalid POST-value: amount"));
	}

	data = "<table border=\"1\" cellpadding=\"3\" cellspacing=\"0\" width=\"100%\">\n";
	data += "<tr style=\"background-color: #ccc; text-align: center;\"><td style=\"width: 5%;\">#</td><td style=\"width: 30%;\">artist</td><td style=\"width: 30%;\">album</td><td style=\"width: 30%;\">song</td><td/></tr>";

	for(ix = 0; ix < amount; ix++)
	{
		string style = "";

		if((start + ix) == emd_state.get_current_index())
			style = " style=\"background-color: yellow;\"";

		try
		{
			mp3meta current(emd_state[start + ix]);
			data += "<tr" + style + ">";
			data += "<td>" + lexical_cast<string>(start + ix) + "</td><td>" + current.get_artist() + "</td><td>" + current.get_album() + "</td><td>" + current.get_song() + "</td>";
			data += "<td><form method=\"post\" action=\"/goto\"><div><input type=\"hidden\" name=\"value\" value=\"" + lexical_cast<string>(start + ix) + "\"/><input type=\"submit\" value=\"goto\"/></div></form></td>\n";
			data += "</tr>";
		}
		catch(...)
		{
		}
	}

	data += "</table>\n";

	data += "<table style=\"margin: 4px 0px 0px 0px;\" border=\"1\" cellpadding=\"3\" cellspacing=\"0\">\n";
	data += "<tr>";

	if((start - amount) >= 0)
		data += "<td><form method=\"post\" action=\"/list\"><div><input type=\"hidden\" name=\"start\" value=\"" + lexical_cast<string>(start - amount) + "\"/><input type=\"hidden\" name=\"amount\" value=\"" + lexical_cast<string>(amount) + "\"/><input type=\"submit\" value=\"previous " + lexical_cast<string>(amount) + "\"/></div></form></td>\n";
	else
		data += "<td></td>";

	data += "<td><form method=\"get\" action=\"/\"><div><input type=\"submit\" value=\"back\"/></div></form></td>\n";

	if(start < emd_state.size())
		data += "<td><form method=\"post\" action=\"/list\"><div><input type=\"hidden\" name=\"start\" value=\"" + lexical_cast<string>(start + amount) + "\"/><input type=\"hidden\" name=\"amount\" value=\"" + lexical_cast<string>(amount) + "\"/><input type=\"submit\" value=\"next " + lexical_cast<string>(amount) + "\"/></div></form></td>\n";
	else
		data += "<td></td>";

	data += "</tr></table>\n";

	return(send_html(connection, "/", MHD_HTTP_OK, data));
}

MHD_Result HttpServer::page_dispatcher_goto(MHD_Connection * connection, const string & method, ConnectionData * con_cls) const
{
	string	data;
	int		value;

	if(method != "POST")
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, string("Method not allowed: ") + method));

	if(con_cls->callback_count == 0)
		return(MHD_YES);

	if(con_cls->values.data.find("value") == con_cls->values.data.end())
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, "Missing POST-value: value"));

	try
	{
		value = lexical_cast<int>(con_cls->values.data["value"]);
	}
	catch(...)
	{
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, "Invalid POST-value: value"));
	}

	try
	{
		emd_state.go_to(value);
		data = "<p>goto OK</p>\n";
	}
	catch(string e)
	{
		data = "<p>goto error: " + e + "</p>\n";
	}

	return(send_html(connection, "/goto", MHD_HTTP_OK, data, 0, "/"));
}

MHD_Result HttpServer::page_dispatcher_stream_http(MHD_Connection * connection, const string & method, ConnectionData * con_cls) const
{
	MHD_Result rv;

	if(method != "GET")
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, string("Method not allowed: ") + method));

	if(con_cls->callback_count != 0)
		return(MHD_NO);

	try
	{
		rv = send_stream(connection, false);
	}
	catch(string e)
	{
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, e));
	}

	return(rv);
}

MHD_Result HttpServer::page_dispatcher_stream_shoutcast(MHD_Connection * connection, const string & method, ConnectionData * con_cls) const
{
	MHD_Result rv;

	if(method != "GET")
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, string("Method not allowed: ") + method));

	if(con_cls->callback_count != 0)
		return(MHD_NO);

	try
	{
		rv = send_stream(connection, true);
	}
	catch(string e)
	{
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, e));
	}

	return(rv);
}
