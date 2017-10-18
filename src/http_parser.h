#ifndef TARANTOOL_HTTP_PARSER_H
#define TARANTOOL_HTTP_PARSER_H

#define HEADER_LEN 32

enum {
	HTTP_PARSE_OK,
	HTTP_PARSE_DONE,
	HTTP_PARSE_INVALID
};

struct http_parser {
	char *header_value_start;
	char *header_value_end;

	int http_major;
	int http_minor;

	char header_name[HEADER_LEN];
	int header_name_index;
};

/*
 * @brief Parse line containing http header info
 * @param parser object
 * @param bufp pointer to buffer with data
 * @param end_buf
 * @return	HTTP_DONE - line was parsed
 * 			HTTP_OK - header was read
 * 			HTTP_PARSE_INVALID - error during parsing
 */
int
http_parse_header_line(struct http_parser *parser, char **bufp, const char *end_buf);

#endif //TARANTOOL_HTTP_PARSER_H
