#ifndef TARANTOOL_JSON_PATH_H_INCLUDED
#define TARANTOOL_JSON_PATH_H_INCLUDED
/*
 * Copyright 2010-2016 Tarantool AUTHORS: please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parser for JSON paths:
 * <field>, <.field>, <[123]>, <['field']> and their combinations.
 */
struct json_path_parser {
	/** Source string. */
	const char *src;
	/** Length of src. */
	int src_len;
	/** Current parser's position. */
	const char *pos;
	/**
	 * On each step a parser can return either string key or
	 * numeric index. Numeric index can be found only inside
	 * [] brackets - all other identifiers are string keys.
	 */
	union {
		const char *key;
		int index;
	};
	/** Length of key. */
	int key_len;
	/** True, if latest step found a numeric index. */
	bool is_index;
};

static inline bool
json_path_eof(const struct json_path_parser *parser)
{
	return parser->pos == parser->src + parser->src_len;
}

/**
 * Create @a parser.
 * @param[out] parser Parser to create.
 * @param src Source string.
 * @param src_len Length of @a src.
 */
static inline void
json_path_parser_create(struct json_path_parser *parser, const char *src,
			int src_len)
{
	parser->src = src;
	parser->src_len = src_len;
	parser->pos = src;
}

/**
 * Get a next path part.
 * @param parser Parser.
 * @retval 0   Success. For result see @a parser.key, .key_len,
 *             .index, .is_index.
 * @retval > 0 Position of a syntax error. A position is 1-based
 *             and starts from a beginning of a source string.
 */
int
json_next(struct json_path_parser *parser);

#ifdef __cplusplus
}
#endif

#endif /* TARANTOOL_JSON_PATH_H_INCLUDED */