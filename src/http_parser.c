
#include <string.h>
#include "httpc.h"
#include "http_parser.h"

#define LF     (unsigned char) '\n'
#define CR     (unsigned char) '\r'
#define CRLF   "\r\n"

int
http_parse_status_line(struct http_parser *parser, char **bufp, const char *end_buf)
{
	char   ch;
	char  *p = *bufp;
	enum {
		sw_start = 0,
		sw_H,
		sw_HT,
		sw_HTT,
		sw_HTTP,
		sw_first_major_digit,
		sw_major_digit,
		sw_first_minor_digit,
		sw_minor_digit,
		sw_status,
		sw_space_after_status,
		sw_status_text,
		sw_almost_done
	} state;

	state = sw_start;
	int status_count = 0;
	for (;p < end_buf; p++) {
		ch = *p;

		switch (state) {

			/* "HTTP/" */
			case sw_start:
				switch (ch) {
					case 'H':
						state = sw_H;
						break;
					default:
						return HTTP_PARSE_INVALID;
				}
				break;

			case sw_H:
				switch (ch) {
					case 'T':
						state = sw_HT;
						break;
					default:
						return HTTP_PARSE_INVALID;
				}
				break;

			case sw_HT:
				switch (ch) {
					case 'T':
						state = sw_HTT;
						break;
					default:
						return HTTP_PARSE_INVALID;
				}
				break;

			case sw_HTT:
				switch (ch) {
					case 'P':
						state = sw_HTTP;
						break;
					default:
						return HTTP_PARSE_INVALID;
				}
				break;

			case sw_HTTP:
				switch (ch) {
					case '/':
						state = sw_first_major_digit;
						break;
					default:
						return HTTP_PARSE_INVALID;
				}
				break;

				/* the first digit of major HTTP version */
			case sw_first_major_digit:
				if (ch < '1' || ch > '9') {
					return HTTP_PARSE_INVALID;
				}

				parser->http_major = ch - '0';
				state = sw_major_digit;
				break;

				/* the major HTTP version or dot */
			case sw_major_digit:
				if (ch == '.') {
					state = sw_first_minor_digit;
					break;
				}

				if (ch < '0' || ch > '9') {
					return HTTP_PARSE_INVALID;
				}

				if (parser->http_major > 99) {
					return HTTP_PARSE_INVALID;
				}

				parser->http_major = parser->http_major * 10 + (ch - '0');
				break;

				/* the first digit of minor HTTP version */
			case sw_first_minor_digit:
				if (ch < '0' || ch > '9') {
					return HTTP_PARSE_INVALID;
				}

				parser->http_minor = ch - '0';
				state = sw_minor_digit;
				break;

				/* the minor HTTP version or the end of the request line */
			case sw_minor_digit:
				if (ch == ' ') {
					state = sw_status;
					break;
				}

				if (ch < '0' || ch > '9') {
					return HTTP_PARSE_INVALID;
				}

				if (parser->http_minor > 99) {
					return HTTP_PARSE_INVALID;
				}

				parser->http_minor = parser->http_minor * 10 + (ch - '0');
				break;

				/* HTTP status code */
			case sw_status:
				if (ch == ' ') {
					break;
				}

				if (ch < '0' || ch > '9') {
					return HTTP_PARSE_INVALID;
				}
				if (++status_count == 3) {
					state = sw_space_after_status;
				}
				break;

				/* space or end of line */
			case sw_space_after_status:
				switch (ch) {
					case ' ':
						state = sw_status_text;
						break;
					case '.':                    /* IIS may send 403.1, 403.2, etc */
						state = sw_status_text;
						break;
					case CR:
						state = sw_almost_done;
						break;
					case LF:
						goto done;
					default:
						return HTTP_PARSE_INVALID;
				}
				break;

				/* any text until end of line */
			case sw_status_text:
				switch (ch) {
					case CR:
						state = sw_almost_done;

						break;
					case LF:
						goto done;
				}
				break;

				/* end of status line */
			case sw_almost_done:
				switch (ch) {
					case LF:
						goto done;
					default:
						return HTTP_PARSE_INVALID;
				}
		}
	}

	done:

	*bufp = p + 1;

	return HTTP_PARSE_OK;
}

int
http_parse_header_line(struct http_parser *parser, char **bufp, const char *end_buf)
{
	char c, ch;
	char *p = *bufp;
	char *header_name_start = p;
	parser->header_name_index = 0;

	enum {
		sw_start = 0,
		sw_name,
		sw_space_before_value,
		sw_value,
		sw_space_after_value,
		sw_ignore_line,
		sw_almost_done,
		sw_header_almost_done
	} state = sw_start;

	/* the last '\0' is not needed because string is zero terminated */
	static char  lowcase[] =
			"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
					"\0\0\0\0\0\0\0\0\0\0\0\0\0-\0\0" "0123456789\0\0\0\0\0\0"
					"\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
					"\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
					"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
					"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
					"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
					"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

	for (; p < end_buf; p++) {
		ch = *p;

		switch (state) {

			/* first char */
			case sw_start:

				switch (ch) {
					case CR:
						parser->header_value_end = p;
						state = sw_header_almost_done;
						break;
					case LF:
						parser->header_value_end = p;
						goto header_done;
					default:
						state = sw_name;

						c = lowcase[ch];

						if (c) {
							parser->header_name[0] = c;
							parser->header_name_index= 1;
							break;
						}

						if (ch == '_') {
							parser->header_name[0] = ch;
							parser->header_name_index = 1;
							break;
						}

						if (ch == '\0') {
							return HTTP_PARSE_INVALID;
						}
						break;
				}
				break;

				/* http_header name */
			case sw_name:
				c = lowcase[ch];

				if (c) {
					parser->header_name[parser->header_name_index++] = c;
					parser->header_name_index &= (HEADER_LEN - 1);
					break;
				}

				if (ch == '_') {
					parser->header_name[parser->header_name_index++] = ch;
					parser->header_name_index &= (HEADER_LEN - 1);
					break;
				}

				if (ch == ':') {
					state = sw_space_before_value;
					break;
				}

				if (ch == CR) {
					parser->header_value_start = p;
					parser->header_value_end = p;
					state = sw_almost_done;
					break;
				}

				if (ch == LF) {
					parser->header_value_start = p;
					parser->header_value_end = p;
					goto done;
				}

				/* handle "HTTP/1.1 ..." lines */
				if (ch == '/'
					&& p - header_name_start == 4
					&& strncmp(header_name_start, "HTTP", 4) == 0) {
					state = sw_ignore_line;

					int rc = http_parse_status_line(parser, &header_name_start, end_buf);
					if (rc == HTTP_PARSE_INVALID) {
						parser->http_minor = -1;
						parser->http_major = -1;
					}
					break;
				}

				if (ch == '\0') {
					return HTTP_PARSE_INVALID;
				}

				break;

			/* space* before http_header value */
			case sw_space_before_value:
				switch (ch) {
					case ' ':
						break;
					case CR:
						parser->header_value_start = p;
						parser->header_value_end = p;
						state = sw_almost_done;
						break;
					case LF:
						parser->header_value_start = p;
						parser->header_value_end = p;
						goto done;
					case '\0':
						return HTTP_PARSE_INVALID;
					default:
						parser->header_value_start = p;
						state = sw_value;
						break;
				}
				break;

				/* http_header value */
			case sw_value:
				switch (ch) {
					case ' ':
						parser->header_value_end = p;
						state = sw_space_after_value;
						break;
					case CR:
						parser->header_value_end = p;
						state = sw_almost_done;
						break;
					case LF:
						parser->header_value_end = p;
						goto done;
					case '\0':
						return HTTP_PARSE_INVALID;
				}
				break;

				/* space* before end of http_header line */
			case sw_space_after_value:
				switch (ch) {
					case ' ':
						break;
					case CR:
						state = sw_almost_done;
						break;
					case LF:
						goto done;
					case '\0':
						return HTTP_PARSE_INVALID;
					default:
						state = sw_value;
						break;
				}
				break;

				/* ignore http_header line */
			case sw_ignore_line:
				switch (ch) {
					case LF:
						state = sw_start;
						break;
					default:
						break;
				}
				break;

				/* end of http_header line */
			case sw_almost_done:
				switch (ch) {
					case LF:
						goto done;
					case CR:
						break;
					default:
						return HTTP_PARSE_INVALID;
				}
				break;

				/* end of http_header */
			case sw_header_almost_done:
				switch (ch) {
					case LF:
						goto header_done;
					default:
						return HTTP_PARSE_INVALID;
				}
		}
	}

	done:

	*bufp = p + 1;
	return HTTP_PARSE_OK;

	header_done:

	*bufp = p + 1;
	return HTTP_PARSE_DONE;
}
