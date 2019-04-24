//
//  test.c
//
//  Created by zhoukai on 16/1/16.
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



static void parse_right_reply();
static void parse_error_reply();

static redis_parser_settings callbacks = {
	.on_message_begin       = message_begin,
	.on_content_len         = content_len,
	.on_content             = content,
	.on_message_complete    = message_complete,
};

const char      normal_message[] = "+OK HELLO\t\tWorld!!!\r\n";
const char      error_message[] = "-FAIL\tFUCK YOU~~~\r\n";
const char      number_message[] = ":20160101\r\n";
const char		single_message[] = "$12\r\nname$zhoukai\r\n";
const char 		multi_message[] = "*6\r\n$4\r\nname\r\n$7\r\nzhoukai\r\n$-1\r\n$0\r\n$3\r\nage\r\n$2\r\n30\r\n";


const char      e_normal_message[] = "+OK\x01HELLO\tWorld!!!\r\n";
const char      e_error_message[] = "-FAIL\tFUCK\x02YOU~~~\r\n";
const char      e_number_message[] = ":2016.0101\r\n";
const char		e_single_message[] = "$10\r\nname$zhoukai\r\n";
const char 		e_multi_message[] = "*6\r\n$4\r\nname\r\n$7\r\nzhoukai\r\n$1.2\r\n$3\r\nag\r\n$2\r\n30\r\n";

#define max_fields (100)
#define right_test(x)									 \
	do {										 \
		redis_parser_init(&parser, REDIS_REPLY);				 \
		n = redis_parser_execute(&parser, &callbacks, (x), ARRAY_SIZE((x)) - 1); \
		assert(n == ARRAY_SIZE((x)) - 1);					 \
		redis_parser_init(&parser, REDIS_REPLY);				 \
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
		redis_parser_init(&parser, REDIS_REPLY);					   \
		n = redis_parser_execute(&parser, &callbacks, (x), ARRAY_SIZE((x)) - 1);	   \
		if (n < ARRAY_SIZE((x)) - 1) {							   \
			printf("\nerror : %s", redis_errno_description(parser.redis_errno));	   \
		}										   \
		redis_parser_init(&parser, REDIS_REPLY);					   \
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
		const char	*at;
		int        	len;
	}               value[max_fields];
};

static void print_result(struct result *result);

int main(int argc, char **argv)
{
	
	char s[] = "abcdefghijklmnopqrstuvwxyz";
	int i = 0;
	for (i = 0; i < ARRAY_SIZE(s) - 1; i++) {
		printf("%c", UPPER(s[i]));
	}
	printf("\n");
	
	parse_right_reply();
	parse_error_reply();
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
			if (result->value[i].len == 0) {
				printf("\t0 : NULL\n");
			} else {
				printf("\t-1 : nil\n");
			}
		}
		
	}
	printf("}\n");
}

static int message_begin(redis_parser *parser, int64_t value)
{
	struct result *result = parser->data;
	memset(result, 0, sizeof(*result));
	/*-存储协议类型-*/
	result->fields = parser->fields;
	result->reply_type = parser->reply_type;
	printf("\n--------------%u--------------\n", parser->reply_type);
	printf("fields:%d", result->fields);
	return 0;
}

static int content_len(redis_parser *parser, int64_t len)
{
	struct result *result = parser->data;
	result->value[result->fields - parser->fields].len = (int)len;
	if (len < 1) {
		result->value[result->fields - parser->fields].at = NULL;
	}
//	printf("\n%lld:", len);
	return 0;
}

static int content(redis_parser *parser, const char *data, size_t len)
{
	struct result *result = parser->data;
	int i = 0;
	/*get position*/
	i = result->fields - parser->fields;
	if (result->value[i].at == NULL) {
		/*first call this function*/
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


static void parse_right_reply()
{
	size_t i = 0;
	size_t n = 0;
	struct result result;
	memset(&result, 0, sizeof(result));
	redis_parser parser = { .data = &result };
	/* ----------------			*/
	right_test(normal_message);
	/* ----------------			*/
	right_test(error_message);
	/* ----------------			*/
	right_test(number_message);
	/* ----------------			*/
	right_test(single_message);
	/* ----------------			*/
	right_test(multi_message);
}

static void parse_error_reply()
{
	size_t i = 0;
	size_t n = 0;
	struct result result;
	memset(&result, 0, sizeof(result));
	redis_parser parser = { .data = &result };
	/* ----------------			*/
	error_test(e_normal_message);
	/* ----------------			*/
	error_test(e_error_message);
	/* ----------------			*/
	error_test(e_number_message);
	/* ----------------			*/
	error_test(e_single_message);
	/* ----------------			*/
	error_test(e_multi_message);
}
