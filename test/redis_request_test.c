//
//  test_request.c
//
//  Created by zhoukai on 16/1/19.
//  Copyright @ 2016年 zhoukai. All rights reserved.
//
#include "src/RedisProto.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) ((int)(sizeof(a) / sizeof((a)[0])))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define UPPER(c)        (unsigned char)((c) & (~0x20))

#define ONCE_PARSE_SIZE (3)

static int message_begin(redis_parser *parser, int64_t value);

static int content_len(redis_parser *parser, int64_t len);

static int content(redis_parser *parser, const char *data, size_t len);

static int message_complete(redis_parser *parser, int64_t fields);



static void parse_right_request();
static void parse_error_request();

static redis_parser_settings callbacks = {
	.on_message_begin       = message_begin,
	.on_content_len         = content_len,
	.on_content             = content,
	.on_message_complete    = message_complete,
	.redis_commands 		= default_redis_commands
};

const char		set_request[] = "*3\r\n$3\r\nSeT\r\n$4\r\nname\r\n$8\r\nzhou\tkai\r\n";
const char		del_request[] = "*4\r\n$3\r\nDeL\r\n$4\r\nname\r\n$3\r\nage\r\n$6\r\ngender\r\n";
const char		mset_request[] = "*5\r\n$4\r\nmset\r\n$4\r\nname\r\n$8\r\nzhou\tkai\r\n$3\r\nage\r\n$2\r\n30\r\n";
const char		hset_request[] = "*4\r\n$4\r\nhset\r\n$4\r\nname\r\n$4\r\nzhou\r\n$3\r\nkai\r\n";
const char		lpush_request[] = "*6\r\n$5\r\nlpush\r\n$4\r\nlist\r\n$1\r\n1\r\n$1\r\n2\r\n$1\r\n3\r\n$1\r\n4\r\n";
const char		rpush_request[] = "*6\r\n$5\r\nrpush\r\n$4\r\nlist\r\n$1\r\n1\r\n$1\r\n2\r\n$1\r\n3\r\n$1\r\n4\r\n";
const char		lpushx_request[] = "*3\r\n$6\r\nlpushx\r\n$4\r\nlist\r\n$1\r\n1\r\n";
const char		rpushx_request[] = "*3\r\n$6\r\nrpushx\r\n$4\r\nlist\r\n$1\r\n1\r\n";

const char		get_request[] = "*2\r\n$3\r\nGeT\r\n$4\r\nname\r\n";
const char		hget_request[] = "*3\r\n$4\r\nHGeT\r\n$4\r\nname\r\n$4\r\nzhou\r\n";
const char		hmget_request[] = "*5\r\n$5\r\nHMGeT\r\n$4\r\nhash\r\n$2\r\nf1\r\n$2\r\nf2\r\n$2\r\nf3\r\n";
const char		hgetall_request[] = "*2\r\n$7\r\nhgetall\r\n$4\r\nhash\r\n";
const char		lrange_request[] = "*4\r\n$6\r\nlrange\r\n$4\r\nlist\r\n$1\r\n0\r\n$2\r\n-1\r\n";
const char		keys_request[] = "*2\r\n$4\r\nKEYS\r\n$1\r\n*\r\n";
const char		info_request[] = "*1\r\n$4\r\nINFO\r\n";
const char		ping_request[] = "*2\r\n$4\r\nPING\r\n$7\r\nzhoukai\r\n";
const char		exists_request[] = "*4\r\n$6\r\nexists\r\n$4\r\nname\r\n$3\r\nage\r\n$6\r\ngender\r\n";

const char		e_set_request[] = "*3\r\n$3\r\nsex\r\n$4\r\nname\r\n$7\r\nzhou\x01kai\r\n";

#define max_fields (100)

#define right_test(x)									 \
	do {										 \
		redis_parser_init(&parser, REDIS_REQUEST);				 \
		n = redis_parser_execute(&parser, &callbacks, (x), ARRAY_SIZE((x)) - 1); \
		assert(n == ARRAY_SIZE((x)) - 1);					 \
		redis_parser_init(&parser, REDIS_REQUEST);				 \
		for (i = 0; i < ARRAY_SIZE((x)) - 1; ) {				 \
			size_t b = ONCE_PARSE_SIZE;					 \
			b = MIN(b, ARRAY_SIZE((x)) - 1 - i);				 \
			n = redis_parser_execute(&parser, &callbacks, &(x)[i], b);	 \
			assert(n == b);							 \
			i += n;								 \
		}									 \
	} while (0)

#define error_test(x)										   \
	do {											   \
		redis_parser_init(&parser, REDIS_REQUEST);					   \
		n = redis_parser_execute(&parser, &callbacks, (x), ARRAY_SIZE((x)) - 1);	   \
		if (n < ARRAY_SIZE((x)) - 1) {							   \
			printf("\nerror : %s", redis_errno_description(parser.redis_errno));	   \
		}										   \
		redis_parser_init(&parser, REDIS_REQUEST);					   \
		for (i = 0; i < ARRAY_SIZE((x)) - 1; ) {					   \
			size_t b = ONCE_PARSE_SIZE;						   \
			b = MIN(b, ARRAY_SIZE((x)) - 1 - i);					   \
			n = redis_parser_execute(&parser, &callbacks, &(x)[i], b);		   \
			if (n < b) {								   \
				printf("\nerror : %s", redis_errno_description(parser.redis_errno)); \
				break;								   \
			}									   \
			i += n;									   \
		}										   \
	} while (0)


struct result
{
	union
	{
		unsigned        reply_type;
		uint64_t        command_type;
	};
	int        		fields;
	struct
	{
		const char      *at;
		unsigned        len;
	}               value[max_fields];
};

static void print_result(struct result *result);

int main(int argc, char **argv)
{
	parse_right_request();
	parse_error_request();
#ifdef _WIN32
	getchar();
#endif // _WIN32

	return EXIT_SUCCESS;
}

static void print_result(struct result *result)
{
	int i = 0;
	printf("{\n");
	for (i = 0; i < result->fields; i++) {
		if (result->value[i].at) {
			printf("\t%u : %.*s\n", result->value[i].len, result->value[i].len, result->value[i].at);
		} else {
			printf("\t-1 : nil\n");
		}
		
	}
	printf("}\n");
}

static int message_begin(redis_parser *parser, int64_t value)
{
	struct result *result = parser->data;
	memset(result, 0, sizeof(*result));
	result->command_type = parser->command_type;
	result->fields = parser->fields;
	
	union {
		char		chbit[8];
		uint64_t 	value;
	} command = { .value = parser->command_type };
	
	printf("\n-----------------%c%c%c%c%c%c%c%c------------------\n",
			command.chbit[7] ? command.chbit[7] : ' ',
			command.chbit[6] ? command.chbit[6] : ' ',
			command.chbit[5] ? command.chbit[5] : ' ',
			command.chbit[4] ? command.chbit[4] : ' ',
			command.chbit[3] ? command.chbit[3] : ' ',
			command.chbit[2] ? command.chbit[2] : ' ',
			command.chbit[1] ? command.chbit[1] : ' ',
			command.chbit[0] ? command.chbit[0] : ' ');
	printf("fields:%d", result->fields);
	return 0;
}

static int content_len(redis_parser *parser, int64_t len)
{
	struct result *result = parser->data;
	result->value[result->fields - parser->fields].len = (unsigned)len;
//	printf("\n%lld:", len);
	return 0;
}

static int content(redis_parser *parser, const char *data, size_t len)
{
	struct result *result = parser->data;
	int i = 0;
	i = result->fields - parser->fields;
	if (result->value[i].at == NULL) {
		result->value[i].at = data;
	}
//	printf("%.*s", (int)len, data);
	//	fflush(stdout);
	return 0;
}

static int message_complete(redis_parser *parser, int64_t fields)
{
	printf("\n------------------------------------\n");
	print_result(parser->data);
	printf("------------------------------------\n");
	return 0;
}


static void parse_right_request()
{
	size_t i = 0;
	size_t n = 0;
	struct result result;
	memset(&result, 0, sizeof(result));
	redis_parser parser = { .data = &result };
	
	/* ----------------			*/
	right_test(set_request);
	/* ----------------			*/
	right_test(del_request);
	/* ----------------			*/
	right_test(mset_request);
	/* ----------------			*/
	right_test(hset_request);
	/* ----------------			*/
	right_test(lpush_request);
	/* ----------------			*/
	right_test(rpush_request);
	/* ----------------			*/
	right_test(lpushx_request);
	/* ----------------			*/
	right_test(rpushx_request);
	/* ----------------			*/
	right_test(get_request);
	/* ----------------			*/
	right_test(hget_request);
	/* ----------------			*/
	right_test(hmget_request);
	/* ----------------			*/
	right_test(hgetall_request);
	/* ----------------			*/
	right_test(lrange_request);
	/* ----------------			*/
	right_test(keys_request);
	/* ----------------			*/
	right_test(info_request);
	/* ----------------			*/
	right_test(ping_request);
	/* ----------------			*/
	right_test(exists_request);
}

static void parse_error_request()
{
	size_t i = 0;
	size_t n = 0;
	struct result result;
	memset(&result, 0, sizeof(result));
	redis_parser parser = { .data = &result };
	/* ----------------			*/
	error_test(e_set_request);
}
