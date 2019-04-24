//
//  redis_parser.c
//
//  Created by zhoukai on 16/1/16.
//  Copyright @ 2016年 zhoukai. All rights reserved.
//

#include "RedisProto.h"

/*-普通消息中可以包含的字符-*/
static const char PRINT_CH[256] = {
	/*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
	0,   0,    0,   0,   0,    0,   0,   0,
	/*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
	0,   '\t', 0,   0,   0,    0,   0,   0,
	/*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
	0,   0,    0,   0,   0,    0,   0,   0,
	/*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
	0,   0,    0,   0,   0,    0,   0,   0,
	/*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
	' ', '!',  '"', '#', '$',  '%', '&', '\'',
	/*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
	'(', ')',  '*', '+', ',',  '-', '.', '/',
	/*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
	'0', '1',  '2', '3', '4',  '5', '6', '7',
	/*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
	'8', '9',  ':', ';', '<',  '=', '>', '?',
	/*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
	0,   'a',  'b', 'c', 'd',  'e', 'f', 'g',
	/*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
	'h', 'i',  'j', 'k', 'l',  'm', 'n', 'o',
	/*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
	'p', 'q',  'r', 's', 't',  'u', 'v', 'w',
	/*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
	'x', 'y',  'z', '[', '\\', ']', '^', '_',
	/*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
	'`', 'a',  'b', 'c', 'd',  'e', 'f', 'g',
	/* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
	'h', 'i',  'j', 'k', 'l',  'm', 'n', 'o',
	/* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
	'p', 'q',  'r', 's', 't',  'u', 'v', 'w',
	/* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
	'x', 'y',  'z', '{', '|',  '}', '~', 0
};
/* gcc version. for example : v4.1.2 is 40102, v3.4.6 is 30406 */
#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

/*
*-逻辑跳转优化
*/
#if GCC_VERSION
/*-条件大多数为真，与if配合使用，直接执行if中语句-*/
#define likely(x)     __builtin_expect(!!(x), 1)
/*-条件大多数为假，与if配合使用，直接执行else中语句-*/
#define unlikely(x)   __builtin_expect(!!(x), 0)
#else
#define likely(x)     (!!(x))
#define unlikely(x)   (!!(x))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) ((int)(sizeof(a) / sizeof((a)[0])))
#endif

enum
{
	s_none = 0, /*初始化状态*/ 
	s_reply_start, /*确认是响应命令后的状态*/
	s_request_start,/*确认是请求后的状态*/

	s_reply_num,
	s_reply_info,
	s_reply_err,

	s_reply_num_almost_done,
	s_reply_info_almost_done,
	s_reply_err_almost_done,

	s_reply_multi_fields,
	s_reply_multi_fields_almost_done,
	s_reply_multi_size_start,
	s_reply_multi_size,
	s_reply_multi_size_almost_done,
	s_reply_multi_content,
	s_reply_multi_content_almost_done1,
	s_reply_multi_content_almost_done2,

	s_reply_bulk_size,
	s_reply_bulk_size_almost_done,
	s_reply_bulk_content,
	s_reply_bulk_content_almost_done1,
	s_reply_bulk_content_almost_done2,

	s_request_fields,	/*-正在解析字段数量-*/
	s_request_fields_almost_done,
	s_request_command_size_start,
	s_request_command_size,
	s_request_command_size_almost_done,
	s_request_command,
	s_request_command_almost_done1,
	s_request_command_almost_done2,
	s_request_field_size_start,
	s_request_field_size,
	s_request_field_size_almost_done,
	s_request_field_content,
	s_request_field_content_almost_done1,
	s_request_field_content_almost_done2,

	s_reply_num_done,
	s_reply_info_done,
	s_reply_err_done,
	s_reply_bulk_done,
	s_reply_multi_done,
	s_complete_almost_done, /*-解析快要完成-*/

	s_complete = 0xffff,
};

/* ------ 枚举和数组映射 ------- */

#define REDIS_ERRNO_MAP(XX)									    \
	/*00*/ XX(OK, "+OK\r\n"),								    \
	/*01*/ XX(PROTO, "-PROTOCAL ERROR\r\n"),						    \
	/*02*/ XX(UNKNOW, "-UNKNOW ERROR\r\n"),							    \
	/*03*/ XX(COMPLETED, "-HAS COMPLETED\r\n"),						    \
	/*04*/ XX(INVALID_LENGTH, "-INVALID CHARACTER IN LENGTH-FIELD\r\n"),			    \
	/*05*/ XX(INVALID_CONTENT, "-INVALID CHARACTER IN DIGITAL OR TEXT REPLY\r\n"),		    \
	/*06*/ XX(INVALID_COMMAND_TOKEN, "-INVALID CHARACTER IN COMMAND\r\n"),			    \
	/*07*/ XX(UNMATCH_COMMAND, "-UNKNOW THIS COMMAND\r\n"),					    \
	/*08*/ XX(UNMATCH_COMMAND_KVS, "-THE NUMBER OF KEY-VALUE DOES NOT MATCH THIS COMMAND\r\n"), \
	/*09*/ XX(CR_EXPECTED, "-CR CHARACTER EXPECTED\r\n"),					    \
	/*10*/ XX(LF_EXPECTED, "-LF CHARACTER EXPECTED\r\n"),					    \
	/*11*/ XX(CB_message_begin, "-THE ON_MESSAGE_BEGIN CALLBACK FAILED\r\n"),		    \
	/*12*/ XX(CB_content_len, "-THE ON_CONTENT_LEN CALLBACK FAILED\r\n"),			    \
	/*13*/ XX(CB_content, "-THE ON_CONTENT CALLBACK FAILED\r\n"),				    \
	/*14*/ XX(CB_message_complete, "-THE ON_MESSAGE_COMPLETE CALLBACK FAILED\r\n"),		    \

enum redis_errno
{
#define REDIS_ERRNO_GEN(n, s) RDE_##n
	REDIS_ERRNO_MAP(REDIS_ERRNO_GEN)
#undef REDIS_ERRNO_GEN
};

struct
{
	const char      *name;
	const char      *desc;
} redis_errno_tab[] = {
#define REDIS_ERRNO_GEN(n, s) { #n, s }
	REDIS_ERRNO_MAP(REDIS_ERRNO_GEN)
#undef REDIS_ERRNO_GEN
};

void redis_parser_init(redis_parser *parser, enum redis_parser_type type)
{
	assert(parser);
	void *data = parser->data;
	memset(parser, 0, sizeof(*parser));
	parser->state = type == REDIS_REPLY ?
		s_reply_start : (type == REDIS_REQUEST ? s_request_start : s_none);
	parser->redis_errno = RDE_OK;
	parser->data = data;
	parser->fields = -1;
}

#define CR      '\r'
#define LF      '\n'
#define LOWER(c)        (unsigned char)((c) | 0x20)
#define UPPER(c)        (unsigned char)((c) & (~0x20))
#define IS_ALPHA(c)     (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z'))
#define IS_NUM(c)       ((c) >= '0' && (c) <= '9')
#define IS_ALPHANUM(c)  (IS_ALPHA(c) || IS_NUM(c))
#define IS_TOKEN(x)     (PRINT_CH[(int)(x)])
#define SET_ERRNO(e)    (parser->redis_errno = (e))
#define CLEAN_PARSE_NUMBER(p) (memset(&(p)->parse_number, 0, sizeof((p)->parse_number)))

#define CURRENT_STATE() (p_state)
#define UPDATE_STATE(V) (p_state = (V))
#define RETURN(V)				 \
	do {					 \
		parser->state = CURRENT_STATE(); \
		return (V);			 \
	} while (0);
#define REEXECUTE()     goto reexecute

/*-如果当前的状态需要回调传递数据指针，则永远标记起始指针-*/
#define MARK(FOR)			\
	do {				\
		if (!FOR##_mark) {	\
			FOR##_mark = p;	\
		}			\
	} while (0)

#define CALLBACK_NOTIFY_(FOR, VAL, ER)					      \
	do {								      \
		assert(likely(parser->redis_errno == RDE_OK));		      \
		if (likely(settings->on_##FOR)) {			      \
			parser->state = CURRENT_STATE();		      \
			if (unlikely(0 != settings->on_##FOR(parser, VAL))) { \
				SET_ERRNO(RDE_CB_##FOR);		      \
			}						      \
			UPDATE_STATE(parser->state);			      \
			/*-如果回调函数返回错误，则退出-*/				      \
			if (unlikely(parser->redis_errno != RDE_OK)) {	      \
				return (ER);				      \
			}						      \
		}							      \
	} while (0)

#define CALLBACK_DATA_(FOR, LEN, ER)							  \
	do {										  \
		assert(likely(parser->redis_errno == RDE_OK));				  \
		if (FOR##_mark && (LEN) > 0) {						  \
			if (likely(settings->on_##FOR)) {				  \
				parser->state = CURRENT_STATE();			  \
				if (unlikely(0 !=					  \
					settings->on_##FOR(parser, FOR##_mark, (LEN)))) { \
					SET_ERRNO(RDE_CB_##FOR);			  \
				}							  \
				UPDATE_STATE(parser->state);				  \
				/*-如果回调函数返回错误，则退出-*/					  \
				if (unlikely(parser->redis_errno != RDE_OK)) {		  \
					return (ER);					  \
				}							  \
			}								  \
			FOR##_mark = NULL;						  \
		}									  \
	} while (0)

#define CALLBACK_NOTIFY(FOR, VAL)               CALLBACK_NOTIFY_(FOR, VAL, p - data + 1)
#define CALLBACK_NOTIFY_NOADVANCE(FOR, VAL)     CALLBACK_NOTIFY_(FOR, VAL, p - data)
#define CALLBACK_DATA(FOR)                      CALLBACK_DATA_(FOR, p - FOR##_mark, p - data + 1)
#define CALLBACK_DATA_NOADVANCE(FOR)            CALLBACK_DATA_(FOR, p - FOR##_mark, p - data)
/**@return -1 错误的数 0 以前是正确的数 1 仍然是正确的数*/
static int parse_number(redis_parser *parser, char ch);

/**@return -1 不识别的命令， -2 命令参数不匹配，0 成功*/
static int check_command(uint64_t type, int fields, const redis_command * redis_commands);

size_t redis_parser_execute(redis_parser *parser,
	const redis_parser_settings *settings, const char *data, size_t len)
{
	assert(parser && data);
	char            ch = '\0';
	int             p_state = parser->state;
	const char      *p = data;
	const char      *content_mark = NULL; //整段数据解析的起始标记

	/*-错误的状态，不做分析，直接返回；或没有数据，则返回-*/
	if (unlikely(
		(parser->redis_errno != RDE_OK) || (len == 0)
		)) {
		return 0;
	}

	for (p = data; likely(p < data + len); p++) {
		ch = *p;
	reexecute:	/*-不需要解析数据，仅需要解析状态-*/
		switch (CURRENT_STATE())
		{
		case s_reply_start:
		{
			switch (ch)
			{
			case '+':
				parser->fields = 1;
				parser->reply_type = redis_reply_info;
				UPDATE_STATE(s_reply_info);
				break;

			case '-':
				parser->fields = 1;
				parser->reply_type = redis_reply_err;
				UPDATE_STATE(s_reply_err);
				break;

			case ':':
				parser->fields = 1;
				parser->reply_type = redis_reply_num;
				UPDATE_STATE(s_reply_num);
				break;

			case '$':
				parser->fields = 1;
				parser->reply_type = redis_reply_bulk;
				UPDATE_STATE(s_reply_bulk_size);
				break;
			/*
			 * 以上的响应类型字段数是固定的
			 * 以下的响应类型字段数是可变的
			 */
			case '*':
				parser->fields = -1;
				parser->reply_type = redis_reply_multi;
				UPDATE_STATE(s_reply_multi_fields);
				break;

			default:
				SET_ERRNO(RDE_PROTO);
				goto error;
				break;
			}
			/*如果不是多字段响应，则通知调用层，携带消息解析已开始*/
			if (ch != '*') {
				/*-响应消息开始，通知用户当前是何种类型的响应-*/
				CALLBACK_NOTIFY(message_begin, parser->reply_type);
			}
		}
		break;

		/* --------------				*/
		case s_request_start:
		{
			/*请求字段已'*'开始*/
			if (likely(ch == '*')) {
				UPDATE_STATE(s_request_fields);
			}
			else {
				SET_ERRNO(RDE_PROTO);
				goto error;
			}
		}
		break;

		/* --------------				*/
		case s_reply_info:
		{
			/*标记整段解析的首位置*/
			MARK(content);

			if (likely(IS_TOKEN(ch))) {
				/*必须是可以显示的字符*/
			}
			else if (likely(ch == CR)) {
				UPDATE_STATE(s_reply_info_almost_done);
				/*-fixed:回调-*/
				parser->content_length += (unsigned)(p - data + 1);
				CALLBACK_DATA(content);
			}
			else {
				SET_ERRNO(RDE_CR_EXPECTED);
				goto error;
			}
		}
		break;

		case s_reply_err:
		{
			/*标记整段解析的首位置*/
			MARK(content);

			if (likely(IS_TOKEN(ch))) {
				/*必须是可以显示的字符*/
			}
			else if (likely(ch == CR)) {
				UPDATE_STATE(s_reply_err_almost_done);
				/*-fixed:回调-*/
				parser->content_length += (unsigned)(p - data + 1);
				CALLBACK_DATA(content);
			}
			else {
				SET_ERRNO(RDE_CR_EXPECTED);
				goto error;
			}
		}
		break;

		case s_reply_num:
		{
			MARK(content);
			int f = parse_number(parser, ch);

			if (likely(f == 1)) {
				/*-不希望是小数或负数-*/
				if (unlikely(parser->parse_number.negative ||
					parser->parse_number.prec)) {
					SET_ERRNO(RDE_INVALID_CONTENT);
					goto error;
				}
			} else if (likely((f == 0) && (ch == CR))) {
				UPDATE_STATE(s_reply_num_almost_done);
				/*-fixed:回调-*/
				parser->content_length += (unsigned)(p - data + 1);
				CALLBACK_DATA(content);
			} else {
				SET_ERRNO(RDE_INVALID_CONTENT);
				goto error;
			}
		}
		break;

		/* --------------				*/
		case s_reply_info_almost_done:
		{
			if (likely(ch == LF)) {
				UPDATE_STATE(s_reply_info_done);
				/*-fixed:回调结束-*/
				REEXECUTE();
			}
			else {
				SET_ERRNO(RDE_LF_EXPECTED);
				goto error;
			}
		}
		break;

		case s_reply_err_almost_done:
		{
			if (likely(ch == LF)) {
				UPDATE_STATE(s_reply_err_done);
				/*-fixed:回调结束-*/
				REEXECUTE();
			}
			else {
				SET_ERRNO(RDE_LF_EXPECTED);
				goto error;
			}
		}
		break;

		case s_reply_num_almost_done:
		{
			if (likely(ch == LF)) {
				UPDATE_STATE(s_reply_num_done);
				/*-fixed:回调结束-*/
				REEXECUTE();
			}
			else {
				SET_ERRNO(RDE_LF_EXPECTED);
				goto error;
			}
		}
		break;

		/* --------------				*/

		case s_reply_bulk_size:
		{
			int f = parse_number(parser, ch);

			if (likely(f == 1)) {
				/*-不希望是小数-*/
				if (unlikely(parser->parse_number.prec)) {
					SET_ERRNO(RDE_INVALID_LENGTH);
					goto error;
				}
			}
			else if (likely((f == 0) && (ch == CR))) {
				UPDATE_STATE(s_reply_bulk_size_almost_done);
				parser->content_length = parser->parse_number.integer;
				CLEAN_PARSE_NUMBER(parser);
				/*-可以是零长度或-1，-1表示nil，0表示零长-*/
				if (likely(parser->content_length >= -1)) {
					/*-fixed:回调-*/
					CALLBACK_NOTIFY(content_len, parser->content_length);
				} else {
					SET_ERRNO(RDE_INVALID_LENGTH);
					goto error;
				}
			}
			else {
				SET_ERRNO(RDE_CR_EXPECTED);
				goto error;
			}
		}
		break;

		case s_reply_bulk_size_almost_done:
		{
			if (likely(ch == LF)) {
				if (parser->content_length > 0) {
					UPDATE_STATE(s_reply_bulk_content);
				} else {
					if (parser->content_length == -1) {
						parser->fields = -1; /*nil表示没有字段*/
					}

					UPDATE_STATE(s_reply_bulk_done);
					REEXECUTE();
				}
			}
			else {
				SET_ERRNO(RDE_LF_EXPECTED);
				goto error;
			}
		}
		break;

		case s_reply_bulk_content:
		{
			int64_t to_read = 0;
			/*计算当前可以一次解析的内容长度*/
			to_read = MIN(parser->content_length, ((int64_t)((data + len) - p)));
			assert(likely(to_read > 0));
			/*如果需要则标记改一整块长度的起始*/
			MARK(content);

			parser->content_length -= to_read;
			/*
			 *最多移动到内容的最后一个字节处，
			 *因为该条语句执行后可能会自增加1再退出循环，再调用回调
			 */
			p += to_read - 1;

			if (parser->content_length == 0) {
				/*当前给出的数据够要求的长度，循环内部再次处理状态*/
				UPDATE_STATE(s_reply_bulk_content_almost_done1);
				/*-fixed:回调-*/
				CALLBACK_DATA_(content, p - content_mark + 1, p - data + 1);
			}
		}
		break;

		case s_reply_bulk_content_almost_done1:
		{
			if (likely(ch == CR)) {
				UPDATE_STATE(s_reply_bulk_content_almost_done2);
			} else {
				SET_ERRNO(RDE_CR_EXPECTED);
				goto error;
			}
		}
		break;

		case s_reply_bulk_content_almost_done2:
		{
			if (likely(ch == LF)) {
				--parser->fields;
				UPDATE_STATE(s_reply_bulk_done);
				REEXECUTE();
			} else {
				SET_ERRNO(RDE_LF_EXPECTED);
				goto error;
			}
		}
		break;

		/* --------------				*/
		case s_reply_multi_fields:
		{
			int f = parse_number(parser, ch);

			if (likely(f == 1)) {
				/*-不希望是小数-*/
				if (unlikely(parser->parse_number.prec)) {
					SET_ERRNO(RDE_INVALID_LENGTH);
					goto error;
				}
			}
			else if (likely((f == 0) && (ch == CR))) {
				UPDATE_STATE(s_reply_multi_fields_almost_done);
				int value = (int)parser->parse_number.integer;
				CLEAN_PARSE_NUMBER(parser);
				/*-数量块可以是空块(0)或空对象列表(-1)-*/
				if (likely(value >= -1)) {
					parser->fields = value;
					/*-fixed:回调-*/
					/*-响应消息开始，通知用户当前是何种类型的响应-*/
					CALLBACK_NOTIFY(message_begin, parser->reply_type);
				}
				else {
					SET_ERRNO(RDE_INVALID_LENGTH);
					goto error;
				}
			}
			else {
				SET_ERRNO(RDE_CR_EXPECTED);
				goto error;
			}
		}
		break;

		case s_reply_multi_fields_almost_done:
		{
			if (likely(ch == LF)) {
				if (parser->fields > 0) {
					UPDATE_STATE(s_reply_multi_size_start);
				}
				else {
					UPDATE_STATE(s_reply_multi_done);
					REEXECUTE();
				}
			} else {
				SET_ERRNO(RDE_LF_EXPECTED);
				goto error;
			}
		}
		break;

		case s_reply_multi_size_start:
		{
			if (likely(ch == '$')) {
				UPDATE_STATE(s_reply_multi_size);
			}
			else {
				SET_ERRNO(RDE_PROTO);
			}
		}
		break;

		case s_reply_multi_size:
		{
			int f = parse_number(parser, ch);

			if (likely(f == 1)) {
				/*-不希望是小数-*/
				if (unlikely(parser->parse_number.prec)) {
					SET_ERRNO(RDE_INVALID_LENGTH);
					goto error;
				}
			} else if (likely((f == 0) && (ch == CR))) {
				UPDATE_STATE(s_reply_multi_size_almost_done);
				int64_t value = parser->parse_number.integer;
				CLEAN_PARSE_NUMBER(parser);
				/*-长度字段大于等于0或者为-1的空字段-*/
				if (likely(value >= -1)) {
					parser->content_length = value;
					/*-fixed:回调-*/
					CALLBACK_NOTIFY(content_len, parser->content_length);
				} else {
					SET_ERRNO(RDE_INVALID_LENGTH);
					goto error;
				}
			} else {
				SET_ERRNO(RDE_CR_EXPECTED);
				goto error;
			}
		}
		break;

		case s_reply_multi_size_almost_done:
		{
			if (likely(ch == LF)) {
				if (unlikely(parser->content_length < 1)) {
					/*没有需要更多的字段需要解析时*/
					if (likely(--parser->fields > 0)) {
						UPDATE_STATE(s_reply_multi_size_start);
					} else {
						UPDATE_STATE(s_reply_multi_done);
						REEXECUTE();
					}
				} else {
					UPDATE_STATE(s_reply_multi_content);
				}
			}
			else {
				SET_ERRNO(RDE_LF_EXPECTED);
				goto error;
			}
		}
		break;

		case s_reply_multi_content:
		{
			int64_t to_read = 0;
			to_read = MIN(parser->content_length, ((int64_t)((data + len) - p)));

			assert(likely(to_read > 0));

			MARK(content);

			parser->content_length -= to_read;
			/*
			 *最多移动到内容的最后一个字节处，
			 *因为该条语句执行后可能会自增加1再退出循环，再调用回调
			 */
			p += to_read - 1;

			if (parser->content_length == 0) {
				UPDATE_STATE(s_reply_multi_content_almost_done1);
				/*-fixed:回调-*/
				CALLBACK_DATA_(content, p - content_mark + 1, p - data + 1);
			}
		}
		break;

		case s_reply_multi_content_almost_done1:
		{
			if (likely(ch == CR)) {
				UPDATE_STATE(s_reply_multi_content_almost_done2);
			}
			else {
				SET_ERRNO(RDE_CR_EXPECTED);
				goto error;
			}
		}
		break;

		case s_reply_multi_content_almost_done2:
		{
			if (likely(ch == LF)) {
				/*-字段超出范围，结束分析-*/
				if (--parser->fields > 0) {
					UPDATE_STATE(s_reply_multi_size_start);
				}
				else {
					UPDATE_STATE(s_reply_multi_done);
					REEXECUTE();
				}
			}
			else {
				SET_ERRNO(RDE_LF_EXPECTED);
				goto error;
			}
		}
		break;

		/* ----------------					*/
		case s_request_fields:
		{
			int f = parse_number(parser, ch);

			if (likely(f == 1)) {
				if (unlikely(parser->parse_number.prec)) {
					SET_ERRNO(RDE_INVALID_LENGTH);
					goto error;
				}
			} else if (likely((f == 0) && (ch == CR))) {
				UPDATE_STATE(s_request_fields_almost_done);
				int value = (int)parser->parse_number.integer;
				CLEAN_PARSE_NUMBER(parser);
				/*-数量块必须大于0-*/
				if (likely(value > 0)) {
					parser->fields = value;
					/*-fixed:回调-*/
					//解析命令后再回调
				} else {
					SET_ERRNO(RDE_INVALID_LENGTH);
					goto error;
				}
			} else {
				SET_ERRNO(RDE_CR_EXPECTED);
				goto error;
			}
		}
		break;

		case s_request_fields_almost_done:
		{
			if (likely(ch == LF)) {
				UPDATE_STATE(s_request_command_size_start);
			}
			else {
				SET_ERRNO(RDE_LF_EXPECTED);
				goto error;
			}
		}
		break;

		case s_request_command_size_start:
		{
			if (likely(ch == '$')) {
				UPDATE_STATE(s_request_command_size);
			}
			else {
				SET_ERRNO(RDE_PROTO);
			}
		}
		break;

		case s_request_command_size:
		{
			int f = parse_number(parser, ch);

			if (likely(f == 1)) {
				if (unlikely(parser->parse_number.prec)) {
					SET_ERRNO(RDE_INVALID_LENGTH);
					goto error;
				}
			} else if (likely((f == 0) && (ch == CR))) {
				UPDATE_STATE(s_request_command_size_almost_done);
				int64_t value = parser->parse_number.integer;
				CLEAN_PARSE_NUMBER(parser);
				/*-命令长度必须大于0-*/
				if (likely(value > 0 && value < 9)) {
					parser->content_length = value;
				} else {
					SET_ERRNO(RDE_INVALID_LENGTH);
					goto error;
				}
			} else {
				SET_ERRNO(RDE_CR_EXPECTED);
				goto error;
			}
		}
		break;

		case s_request_command_size_almost_done:
		{
			if (likely(ch == LF)) {
				UPDATE_STATE(s_request_command);
			}
			else {
				SET_ERRNO(RDE_LF_EXPECTED);
				goto error;
			}
		}
		break;

		case s_request_command:
		{
			int64_t to_read = 0;
			to_read = MIN(parser->content_length, (int64_t)(data + len - p));
			assert(likely(to_read > 0));
			/*-转换为大写字母按8位或-*/
			int64_t pos = 0;

			for (pos = 0; pos < to_read; pos++) {
				uint8_t ch = UPPER(p[pos]);

				if (unlikely(!IS_ALPHA(ch))) {
					SET_ERRNO(RDE_INVALID_COMMAND_TOKEN);
					goto error;
				}
				parser->command_type <<= 8;
				parser->command_type |= ch;
			}

			parser->content_length -= to_read;
			/*-最多移动到内容的最后一个字节处，因为该条语句执行后可能会退出循环，再调用回调-*/
			p += to_read - 1;

			if (parser->content_length == 0) {
				parser->fields--;
				/*-判断命令的有效性和字段数量是否匹配-*/
				UPDATE_STATE(s_request_command_almost_done1);
				int f = check_command(
							parser->command_type, parser->fields,
							settings->redis_commands
						);

				if (likely(f == 0)) {
					CALLBACK_NOTIFY(message_begin, parser->command_type);
				}
				else {
					if (f == -1) {
						/*-不认识的命令-*/
						SET_ERRNO(RDE_UNMATCH_COMMAND);
					}
					else {
						/*-字段数量和命令不匹配-*/
						SET_ERRNO(RDE_UNMATCH_COMMAND_KVS);
					}

					goto error;
				}
			}
		}
		break;

		case s_request_command_almost_done1:
		{
			if (likely(ch == CR)) {
				UPDATE_STATE(s_request_command_almost_done2);
			}
			else {
				SET_ERRNO(RDE_CR_EXPECTED);
				goto error;
			}
		}
		break;

		case s_request_command_almost_done2:
		{
			if (likely(ch == LF)) {
				if (likely(parser->fields > 0)) {
					UPDATE_STATE(s_request_field_size_start);
				}
				else {
					UPDATE_STATE(s_complete_almost_done);
					REEXECUTE();
				}
			}
			else {
				SET_ERRNO(RDE_LF_EXPECTED);
				goto error;
			}
		}
		break;

		case s_request_field_size_start:
		{
			if (likely(ch == '$')) {
				UPDATE_STATE(s_request_field_size);
			}
			else {
				SET_ERRNO(RDE_PROTO);
				goto error;
			}
		}
		break;

		case s_request_field_size:
		{
			int f = parse_number(parser, ch);

			if (likely(f == 1)) {
				/*-不希望是小数-*/
				if (unlikely(parser->parse_number.prec)) {
					SET_ERRNO(RDE_INVALID_LENGTH);
					goto error;
				}
			} else if (likely((f == 0) && (ch == CR))) {
				UPDATE_STATE(s_request_field_size_almost_done);
				int64_t value = parser->parse_number.integer;
				CLEAN_PARSE_NUMBER(parser);
				/*-长度字段大于等于0-*/
				if (likely(value >= 0)) {
					parser->content_length = value;
					/*-fixed:回调-*/
					CALLBACK_NOTIFY(content_len, parser->content_length);
				} else {
					SET_ERRNO(RDE_INVALID_LENGTH);
					goto error;
				}
			} else {
				SET_ERRNO(RDE_CR_EXPECTED);
				goto error;
			}
		}
		break;

		case s_request_field_size_almost_done:
		{
			if (likely(ch == LF)) {
				if (unlikely(parser->content_length < 1)) {
					/*字段长度为0或空*/
					if (--parser->fields > 0) {
						/*继续下一个字段长度解析*/
						UPDATE_STATE(s_request_field_size_start);
					} else {
						/*没有需要更多的字段需要解析*/
						UPDATE_STATE(s_complete_almost_done);
						REEXECUTE();
					}
				} else {
					UPDATE_STATE(s_request_field_content);
				}
			}
			else {
				SET_ERRNO(RDE_LF_EXPECTED);
				goto error;
			}
		}
		break;

		case s_request_field_content:
		{
			int64_t to_read = 0;
			/*-计算本次解析数据还剩余多少字节和需要读取的长度做一个比较，找出最小值-*/
			to_read = MIN(parser->content_length, ((int64_t)((data + len) - p)));

			assert(likely(to_read > 0));
			/*-标记读取的首位置-*/
			MARK(content);
			/*-从需要的读取的长度中减少已读取长度-*/
			parser->content_length -= to_read;
			/*-最多移动到内容的最后一个字节处，因为该条语句执行后可能会退出循环，再调用回调-*/
			p += to_read - 1;
			/*-如果有读取全部的数据，则在循环内回调；否则退出循环，在循环外回调，退出本次解析，返回已解析的长度-*/
			if (likely(parser->content_length == 0)) {
				UPDATE_STATE(s_request_field_content_almost_done1);
				/*fixed:回调*/
				CALLBACK_DATA_(content, p - content_mark + 1, p - data + 1);
				/*-继续循环，p会自增一次，如果还有字段或者已超出解析需要的数据，则循环内做出操作-*/
			}
		}
		break;

		case s_request_field_content_almost_done1:
		{
			if (likely(ch == CR)) {
				UPDATE_STATE(s_request_field_content_almost_done2);
			}
			else {
				SET_ERRNO(RDE_CR_EXPECTED);
				goto error;
			}
		}
		break;

		case s_request_field_content_almost_done2:
		{
			if (likely(ch == LF)) {
				if (--parser->fields > 0) {
					UPDATE_STATE(s_request_field_size_start);
				}
				else {
					UPDATE_STATE(s_complete_almost_done);
					REEXECUTE();
				}
			}
			else {
				SET_ERRNO(RDE_LF_EXPECTED);
				goto error;
			}
		}
		break;

		/* ----------------					*/
		case s_reply_info_done:
		case s_reply_err_done:
		case s_reply_num_done:
			/*-这三类会在最后确定内容长度，并回调-*/
			CALLBACK_NOTIFY(content_len, parser->content_length);
			parser->fields = -1;

		case s_reply_bulk_done:
		case s_reply_multi_done:
		case s_complete_almost_done:
			UPDATE_STATE(s_complete);
			assert(likely(parser->fields == 0 || parser->fields == -1));
			CALLBACK_NOTIFY(message_complete, parser->fields);
			/*-fixed:回调结束-*/
			RETURN((p - data) + 1);
			break;

		case s_complete:
			SET_ERRNO(RDE_COMPLETED);
			goto error;
			break;

		default:
			goto error;
			break;
		}
	}

	/*-是否有回调-*/
	CALLBACK_DATA_NOADVANCE(content);

	if ((CURRENT_STATE() == redis_reply_info) ||
		(CURRENT_STATE() == redis_reply_err) ||
		(CURRENT_STATE() == redis_reply_num)) {
		parser->content_length += p - data;
	}

	RETURN(len);
error:

	if (parser->redis_errno == RDE_OK) {
		SET_ERRNO(RDE_PROTO);
	}

	RETURN(p - data);
}

const char *redis_errno_name(int redis_errno)
{
	assert(redis_errno > -1 && redis_errno < ARRAY_SIZE(redis_errno_tab));
	return redis_errno_tab[redis_errno].name;
}

const char *redis_errno_description(int redis_errno)
{
	assert(redis_errno > -1 && redis_errno < ARRAY_SIZE(redis_errno_tab));
	return redis_errno_tab[redis_errno].desc;
}

const char *redis_command_name(uint64_t command)
{
	switch (command)
	{
	case redis_command_set:
		return "SET";

		break;

	case redis_command_del:
		return "DEL";

		break;

	case redis_command_mset:
		return "MSET";

		break;

	case redis_command_hset:
		return "HSET";

		break;

	case redis_command_lpush:
		return "LPUSH";

		break;

	case redis_command_rpush:
		return "RPUSH";

		break;

	case redis_command_lpushx:
		return "LPUSHX";

		break;

	case redis_command_rpushx:
		return "RPUSHX";

		break;

	case redis_command_get:
		return "GET";

		break;

	case redis_command_hget:
		return "HGET";

		break;

	case redis_command_hmget:
		return "HMGET";

		break;

	case redis_command_hgetall:
		return "HGETALL";

		break;

	case redis_command_lrange:
		return "LRANGE";

		break;

	case redis_command_keys:
		return "KEYS";

		break;

	case redis_command_info:
		return "INFO";

		break;

	case redis_command_ping:
		return "PING";

		break;

	case redis_command_exists:
		return "EXISTS";

		break;

	default:
		break;
	}

	return NULL;
}

struct redis_command default_redis_commands[] = {
	{ redis_command_info, 0, -1, -1, -1, -1 },
	
	{ redis_command_get, 1, -1, -1, -1, -1 },
	{ redis_command_keys, 1, -1, -1, -1, -1 },
	{ redis_command_hgetall, 1, -1, -1, -1, -1 },
	
	{ redis_command_set, 2, -1, -1, -1, -1 },
	{ redis_command_hget, 2, -1, -1, -1, -1 },
	{ redis_command_lpushx, 2, -1, -1, -1, -1 },
	{ redis_command_rpushx, 2, -1, -1, -1, -1 },
	
	{ redis_command_lrange, 3, -1, -1, -1, -1 },
	{ redis_command_hset, 3, -1, -1, -1, -1 },
	
	{ redis_command_del, -1, -1, -1, 0, -1 },
	{ redis_command_exists, -1, -1, -1, 0, -1 },
	
	{ redis_command_lpush, -1, -1, -1, 1, -1 },
	{ redis_command_rpush, -1, -1, -1, 1, -1 },
	{ redis_command_hmget, -1, -1, -1, 1, -1 },
	
	{ redis_command_mset, -1, 0, -1, -1, 0 },

	{ redis_command_ping, -1, -1, 2, -1, -1 },

	{ 0, -1, -1, -1, -1, -1 }
};

static int check_command(uint64_t type, int fields, const redis_command * redis_commands)
{
	int rc = -1;
	int i = 0;
	
	if (unlikely(!redis_commands))
		return 0;
	
	for (i = 0; likely(redis_commands[i].command != 0); i++)
	{
		if (redis_commands[i].command == type)
		{
			rc = 0;

			if (redis_commands[i].eq_fields != -1)
			{
				if (unlikely(!(fields == redis_commands[i].eq_fields)))
				{
					rc = -2;
					break;
				}
			}
			
			if (redis_commands[i].ne_fields != -1)
			{
				if (unlikely(!(fields != redis_commands[i].ne_fields)))
				{
					rc = -2;
					break;
				}
			}
			
			if (redis_commands[i].lt_fields != -1)
			{
				if (unlikely(!(fields < redis_commands[i].lt_fields)))
				{
					rc = -2;
					break;
				}
			}
			
			if (redis_commands[i].gt_fields != -1)
			{
				if (unlikely(!(fields > redis_commands[i].gt_fields)))
				{
					rc = -2;
					break;
				}
			}
			
			if (redis_commands[i].is_odevity == 0) {
				if (unlikely((fields & 0x01) != 0))
				{
					rc = -2;
					break;
				}
			} else if (redis_commands[i].is_odevity == 1) {
				if (unlikely((fields & 0x01) == 0))
				{
					rc = -2;
					break;
				}
			}
			
			break;
		}
	}

	if (unlikely(rc)) {
		union {
			char		chbit[8];
			uint64_t 	value;
		} command = { .value = type };
		
		fprintf(stderr, "error command : %c%c%c%c%c%c%c%c\n",
				command.chbit[7] ? command.chbit[7] : ' ',
				command.chbit[6] ? command.chbit[6] : ' ',
				command.chbit[5] ? command.chbit[5] : ' ',
				command.chbit[4] ? command.chbit[4] : ' ',
				command.chbit[3] ? command.chbit[3] : ' ',
				command.chbit[2] ? command.chbit[2] : ' ',
				command.chbit[1] ? command.chbit[1] : ' ',
				command.chbit[0] ? command.chbit[0] : ' ');
	}

	return rc;
}

static int parse_number(redis_parser *parser, char ch)
{
	bool notfirst = parser->parse_number.notfirst;
	int rc = 1;

	assert(parser);
	if (unlikely(!parser->parse_number.notfirst)) {
		parser->parse_number.notfirst = 1; //第一次解析
	}
	
	if (likely(IS_NUM(ch))) {
		if (unlikely(!notfirst)) {
			parser->parse_number.negative = 0; //未确定正负数，则为正数
		}
		
		/*-分析数字-*/
		if (likely(!parser->parse_number.prec)) {
			/*-整数部分-*/
			parser->parse_number.integer *= 10;
			parser->parse_number.integer += ch - '0';
		} else {
			/*-小数部分-*/
			parser->parse_number.fractional *= 10;
			parser->parse_number.fractional += ch - '0';
			/*-增加小数据精度-*/
			parser->parse_number.prec++;
		}
		
		/*00不允许00.xxx 01201等数字合法*/
		if (unlikely(notfirst && !parser->parse_number.integer)) {
			rc = -1;
		}
	} else if (unlikely(ch == '-')) {
		if (unlikely(!notfirst)) {
			parser->parse_number.negative = 1; //未确定正负数，则为正数
		} else {
			rc = -1;
		}
	} else if (unlikely(ch == '.')) {
		/*允许".156"或"-.156"这样的小数*/
		if (likely(!parser->parse_number.prec)) {
			parser->parse_number.prec++;
		} else {
			rc = -1;
		}
	} else {
		/*当前为非数字组成字符，则可能解析完毕*/
		if ( unlikely(parser->parse_number.negative) )
			parser->parse_number.integer = -parser->parse_number.integer;
		rc = 0;
	}
	if (unlikely(rc == -1))
		CLEAN_PARSE_NUMBER(parser);
	/*rc == 1后续的字符可能也是正确数字组成*/
	return rc;
}
