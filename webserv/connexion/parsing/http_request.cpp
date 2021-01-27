#include "parsing.hpp"

void parse_first_line(t_http_req &req, std::string const &line, t_config &conf)
{
	std::list<std::string> parts;

	parts = split(line, " ");
	req.method = parts.front();
	parts.pop_front();
	req.URL = parts.front().substr(1);
	if (req.URL.size() != 0 && req.URL[req.URL.size() - 1] == '/')
		req.URL = req.URL.substr(0, req.URL.size() - 1);
	if (req.URL.size() != 0 && req.URL[req.URL.size() - 1] == ':')
		req.URL = req.URL.substr(0, req.URL.size() - 1);
	// P("Before: " << req.URL);
	URL_to_local_path(req, conf);
	// P("After: " << req.URL);
	parts.pop_front();
	req.protocol_version = parts.front();
}

void parse_header_fields(t_http_req &req, std::string const &line)
{
	if (check_line(line, "Accept-Charsets"))
		req.header_fields.Accept_Charsets = following_contents(line, "Accept-Charsets:");
	else if (check_line(line, "Accept-Language"))
			req.header_fields.Accept_Language = following_contents(line, "Accept-Language:");
	else if (check_line(line, "Allow"))
			req.header_fields.Allow = following_contents(line, "Allow:");
	else if (check_line(line, "Authorization"))
			req.header_fields.Authorization = following_contents(line, "Authorization:");
	else if (check_line(line, "Content-Language"))
			req.header_fields.Content_Language = following_contents(line, "Content-Language:");
	else if (check_line(line, "Content-Length"))
			req.header_fields.Content_Length = following_contents(line, "Content-Length:");
	else if (check_line(line, "Content-Location"))
			req.header_fields.Content_Location = following_contents(line, "Content-Location:");
	else if (check_line(line, "Content-Type"))
			req.header_fields.Content_Type = following_contents(line, "Content-Type:");
	else if (check_line(line, "Date"))
			req.header_fields.Date = following_contents(line, "Date:");
	else if (check_line(line, "Host"))
			req.header_fields.Host = following_contents(line, "Host:");
	else if (check_line(line, "Last-Modified"))
			req.header_fields.Last_Modified = following_contents(line, "Last-Modified:");
	else if (check_line(line, "Location"))
			req.header_fields.Location = following_contents(line, "Location:");
	else if (check_line(line, "Referer"))
			req.header_fields.Referer = following_contents(line, "Referer:");
	else if (check_line(line, "Retry-After"))
			req.header_fields.Retry_After = following_contents(line, "Retry-After:");
	else if (check_line(line, "Server"))
			req.header_fields.Server = following_contents(line, "Server:");
	else if (check_line(line, "Transfer-Encoding"))
			req.header_fields.Transfer_Encoding = following_contents(line, "Transfer-Encoding:");
	else if (check_line(line, "User-Agent"))
			req.header_fields.User_Agent = following_contents(line, "User-Agent:");
	else if (check_line(line, "WWW_Authenticate"))
			req.header_fields.WWW_Authenticate = following_contents(line, "WWW_Authenticate:");
}

void parse_body(t_http_req &req, std::list<std::string> &body_lines, t_config &conf)
{
	unsigned int i;

	i = 0;
	req.message_body = "";
	for (std::list<std::string>::iterator line = body_lines.begin(); line != body_lines.end(); line++)
	{
		req.message_body += *line;
		req.message_body += "\n";
	}
	if (static_cast<int>(req.message_body.size()) > conf.body_size_limit)
		req.message_body = req.message_body.substr(0, conf.body_size_limit);
}

void parse_non_body(t_http_req &req, std::list<std::string> &non_body_lines, t_config &conf)
{
	unsigned int i;

	i = 0;
	for (std::list<std::string>::iterator line = non_body_lines.begin(); line != non_body_lines.end(); line++)
	{
		if (i == 0)
			parse_first_line(req, *line, conf);
		else
			parse_header_fields(req, *line);
		i++;
	}
}

std::string find_start(std::string &message)
{
	int ret;

	if ((ret = message.find("GET")) != std::string::npos)
		return message.substr(ret);
	if ((ret = message.find("HEAD")) != std::string::npos)
		return message.substr(ret);
	if ((ret = message.find("PUT")) != std::string::npos)
		return message.substr(ret);
	if ((ret = message.find("POST")) != std::string::npos)
		return message.substr(ret);
	if ((ret = message.find("DELETE")) != std::string::npos)
		return message.substr(ret);
	return message;
}

int find_first_two_line_returns(std::string const &req)
{
	unsigned int counter;
	bool follow;

	counter = 0;
	follow = false;
	if (req.size() == 0)
		return -1; //Returning error in this case does not seem correct, so return not ready, comes from recv fail
	// P("body find:|" << req << "|" << "size: " << req.size());
	for (unsigned int i = 0; i < req.size() ; i++)
	{
		if (req[i] == '\n' && follow == true)
					return i;
		if (req[i] == '\n')
		{
			follow = true;
			counter++;
		}
		else if (!equal_to(req[i], " \t\v\f\r"))
			follow = false;
	}
	return -1; //Returns -1 if not found //If not found it means not complete request
}

bool completed_request_chunked(std::string const &req)
{
	int i;

	i = req.size() - 1;
	while (is_white_space(req[i]))
	{
		P(req[i]);
		i--;
	}
	if (req[i] != '0')
		return false;
	if (find_first_two_line_returns(req.substr(i)) == -1)
		return false;
	return true;
}

bool completed_request_lenght(std::string const &req, std::string const &lenght)
{
	size_t len;

	len = 0;
	try
	{
		len = std::stoi(lenght);
	}
	catch(std::exception &e)
	{
		P(e.what());
	}
	if (req.size() >= len)
		return true;
	return false;
}

bool completed_request_norm(std::string const req)
{
	int pos;

	if ((pos = find_first_two_line_returns(req)) == -1)
		return false;
	if (req.substr(pos).find_first_not_of(" \t\n\v\f\r") == std::string::npos) //Check if body follows, if it does not return end
		return true;
	return completed_request_norm(req.substr(pos + 1)); //Continue searching as not until end
}

bool completed_request(std::string const &message, t_http_req const &req)
{
	if (req.header_fields.Transfer_Encoding.front() == std::string("chunked"))
		return completed_request_chunked(message);
	else if (req.header_fields.Content_Length.front() != std::string("None"))
		return completed_request_lenght(message, req.header_fields.Content_Length.front());
	return completed_request_norm(message);
}

int find_body(std::string const &req)
{
	return find_first_two_line_returns(req);
}

void default_init(t_http_req &req)
{
	req.header_fields.Accept_Charsets.push_back("None");
	req.header_fields.Accept_Language.push_back("None");
	req.header_fields.Allow.push_back("None");
	req.header_fields.Authorization.push_back("None");
	req.header_fields.Content_Language.push_back("None");
	req.header_fields.Content_Length.push_back("None");
	req.header_fields.Content_Location.push_back("None");
	req.header_fields.Content_Type.push_back("None");
	req.header_fields.Date.push_back("None");
	req.header_fields.Host.push_back("None");
	req.header_fields.Last_Modified.push_back("None");
	req.header_fields.Location.push_back("None");
	req.header_fields.Referer.push_back("None");
	req.header_fields.Retry_After.push_back("None");
	req.header_fields.Server.push_back("None");
	req.header_fields.Transfer_Encoding.push_back("None");
	req.header_fields.User_Agent.push_back("None");
  req.header_fields.WWW_Authenticate.push_back("None");
	req.method = std::string("None");
	req.URL = std::string("None");
	req.loc = 0;
	req.protocol_version = std::string("None");
	req.message_body = std::string("None");
	req.error = false;
	req.ready = false;
}

bool is_valid(std::string message)
{
	if (message.find_first_not_of(" \t\n\v\f\r") == std::string::npos)
		return false;
	message = message.substr(message.find_first_not_of(" \t\n\v\f\r"), message.size());
	if (is_non_ascii(message) == true)
		return false;
	// std::list<std::string> lines = split(message, "\n");
	return (message.find("HTTP/1.1") != std::string::npos
					&& (message.find("GET") != std::string::npos
					|| message.find("HEAD") != std::string::npos
					|| message.find("PUT") != std::string::npos
					|| message.find("POST") != std::string::npos
					|| message.find("DELETE") != std::string::npos));
}

void parse_http_request(t_http_req &ret, std::string &req, t_config &conf)
{
	std::list<std::string> non_body_lines;
	std::list<std::string> body_lines;
	int body_index;

	// P("--------------------------------------------------------------------------");
	// P("REAL REQUEST:");
	// P(req); //test
	// P("--------------------------------------------------------------------------");
	default_init(ret);
	req = find_start(req);
	if ((body_index = find_body(req)) == -1) //If no body line found, no end of non-body part, thus do not start parsing non-body part
		return ;
	non_body_lines = split(req.substr(0, body_index), "\n");
	parse_non_body(ret, non_body_lines, conf);
	if (completed_request(req, ret)) //If body line found, request is complete
		ret.ready = true;
	else
		return ; //If not complete do not start the parsing of whole body
	if (is_valid(req) == false) //If complete command is invalid directly stop
	{
		ret.error = true;
		return ;
	}
	body_lines = split(req.substr(body_index), "\n");
	parse_body(ret, body_lines, conf);
	// P("--------------------------------------------------------------------------");
	// P("PARSED REQUEST:");
	// show_http_request(*ret); //test
	// P("--------------------------------------------------------------------------");
}
