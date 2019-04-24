//
//  redis_parser.h
//  supex
//
//  Created by zhoukai on 16/1/16.
//  Copyright @ 2016年 zhoukai. All rights reserved.
//

#ifndef redis_parser_h
#define redis_parser_h

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if defined(__cplusplus)
#if !defined(__BEGIN_DECLS)
#define __BEGIN_DECLS \
	extern "C" {
#define __END_DECLS	\
	}
#endif
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

__BEGIN_DECLS

enum redis_parser_type
{
	REDIS_REQUEST = 0x01, REDIS_REPLY,
};

/*-响应类型-*/
enum
{
	redis_reply_none,
	redis_reply_num,
	redis_reply_info,
	redis_reply_err,
	redis_reply_bulk,
	redis_reply_multi,
};

/*--命令类型*/
/*--用A-Z的26进制编码纯字母字符串的命令。-*/

#undef CHARS_TO_INT64
#define CHARS_TO_INT64(c1, c2, c3, c4, c5, c6, c7, c8) \
	((uint64_t)(c1) << 56 | (uint64_t)(c2) << 48 | \
	(uint64_t)(c3) << 40 | (uint64_t)(c4) << 32| \
	(uint64_t)(c5) << 24 | (uint64_t)(c6) << 16 | \
	(uint64_t)(c7) << 8 | (uint64_t)(c8))

#define redis_command_set		CHARS_TO_INT64(0, 0, 0, 0, 0, 'S', 'E', 'T')
#define redis_command_del		CHARS_TO_INT64(0, 0, 0, 0, 0, 'D', 'E', 'L')
#define redis_command_mset		CHARS_TO_INT64(0, 0, 0, 0, 'M', 'S', 'E', 'T')
#define redis_command_hset		CHARS_TO_INT64(0, 0, 0, 0, 'H', 'S', 'E', 'T')
#define redis_command_lpush		CHARS_TO_INT64(0, 0, 0, 'L', 'P', 'U', 'S', 'H')
#define redis_command_rpush		CHARS_TO_INT64(0, 0, 0, 'R', 'P', 'U', 'S', 'H')
#define redis_command_lpushx	CHARS_TO_INT64(0, 0, 'L', 'P', 'U', 'S', 'H', 'X')
#define redis_command_rpushx	CHARS_TO_INT64(0, 0, 'R', 'P', 'U', 'S', 'H', 'X')


#define redis_command_get 		CHARS_TO_INT64(0, 0, 0, 0, 0, 'G', 'E', 'T')
#define redis_command_hget		CHARS_TO_INT64(0, 0, 0, 0, 'H', 'G', 'E', 'T')
#define redis_command_hmget		CHARS_TO_INT64(0, 0, 0, 'H', 'M', 'G', 'E', 'T')
#define redis_command_hgetall	CHARS_TO_INT64(0, 'H', 'G', 'E', 'T', 'A', 'L', 'L')
#define redis_command_lrange	CHARS_TO_INT64(0, 0, 'L', 'R', 'A', 'N', 'G', 'E')
#define redis_command_keys		CHARS_TO_INT64(0, 0, 0, 0, 'K', 'E', 'Y', 'S')
#define redis_command_info		CHARS_TO_INT64(0, 0, 0, 0, 'I', 'N', 'F', 'O')
#define redis_command_exists	CHARS_TO_INT64(0, 0, 'E', 'X', 'I', 'S', 'T', 'S')
#define redis_command_ping		CHARS_TO_INT64(0, 0, 0, 0, 'P', 'I', 'N', 'G')

typedef struct redis_command redis_command;
typedef struct redis_parser redis_parser;
typedef struct redis_parser_settings redis_parser_settings;
/*-返回值不为0，则表示整个协议解析失败-*/
typedef int(*redis_cb)(redis_parser *parser, int64_t value);
typedef int(*redis_data_cb)(redis_parser *parser, const char *data, size_t len);

struct redis_command
{
	// 命令的每个字符转大写后按8位或之和 比如 'get' ==> 'G' << 16 | 'E' << 8 | 'T'
	uint64_t command;
	// 以下每个字段都是命令后携带的字段个数要求，-1表示不检查
	int16_t eq_fields; // 字段数必须等于此数
	int16_t ne_fields; // 字段数必须不等此数
	int16_t lt_fields; // 字段数必须小于此数
	int16_t gt_fields; // 字段数必须大于此数
	int16_t is_odevity;// 必须为奇数或偶数 0为偶数 1为奇数
};

struct redis_parser
{
	/*private : read only*/
	unsigned        type : 6;	/**< 解析种类:REDIS_REQUEST/REDIS_REPLY*/
	unsigned        state : 16;	/**< 当前解析状态，0为初始化状态， 0xffff为已解析完成*/
	unsigned        redis_errno : 10;	/**< 发生的错误，0为没有错误*/
	int             fields;			/**< 还需要处理的字段数，在完成时为0或-1（表示空对象）*/
	struct
	{
		int64_t integer;
		uint32_t notfirst : 1;
		uint32_t negative : 1;
		uint32_t prec : 6;
		uint32_t fractional : 24;
	}               parse_number;	/**< 当前如果在分析数字，则记录在此*/
	union
	{
		unsigned        reply_type;	/**< 回复类型*/
		uint64_t        command_type;	/**< 命令类型*/
	};
	int64_t         content_length;		/**< 当前字段内容的长度*/
										/*public : read and write*/
	void            *data;			/**<用户数据*/
};

/* ---------------------------------            *\
* 当回调函数返回非0值时，表示中止解析后续数据，	*
* 必须调用redis_parser_init()重新来过			*
\* ---------------------------------            */
struct redis_parser_settings
{
	/*在确定了响应类型或命令类型后回调，和将要解析的总字段数量*/
	redis_cb 		on_message_begin;

	/*
	*如果字段是以长度标示的，在获取了完整的长度后回调，
	*如果长度为-1，则表示该字段为nil字段
	* redis_reply_num,redis_reply_err,redis_reply_info类型的解析会在
	* on_content()之后调用此函数
	*在调用此函数时，在fields中给出剩余的字段数量
	*/
	redis_cb 		on_content_len;

	/*
	*解析普通响应消息或字段内容时回调，可能回调多次
	*在调用此函数时，在fields中给出剩余的字段数量
	*/
	redis_data_cb	on_content;
	/*在解析完成后回调，第二个参数为0或-1，否则视代码有bug*/
	redis_cb 		on_message_complete;
	/*命令检查数组，最后一个元素必须为 { 0, -1, -1, -1, -1, -1 }*/
	const redis_command* redis_commands;
};

/**
* 初始化解析器
* 在每次解析一条新的协议数据前调用
* @param type 必须是REDIS_REQUEST或REDIS_REPLY
*/
void redis_parser_init(redis_parser *parser, enum redis_parser_type type);

/**
* 分析数据
* 根据给出的数据可以多次调用，直到解析完成
* @param settings 对应状态的回调
* @param data 数据，可以为二进制
* @param len 数据的长度
* @return 本次解析的长度为0，且传入数据长度不为0，则可能已经解析到完整的协议数据，
* 或发生了错误（会设置parser.redis_errno字段为相应的错误值）
*/
size_t redis_parser_execute(redis_parser *parser, const redis_parser_settings *settings, const char *data, size_t len);

/**
* 根据错误值获取错误名称
*/
const char *redis_errno_name(int redis_errno);

/**
* 根据错误值获取错误描述，符合redis错误响应字串
*/
const char *redis_errno_description(int redis_errno);

/**
* 得到命令名
*/
const char *redis_command_name(uint64_t command);


extern redis_command default_redis_commands[];

__END_DECLS
#endif	/* redis_parser_h */
