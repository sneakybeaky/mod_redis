/*
 * Copyright (c) 2012,  G Sharp
 * Copyright (c) 2012,  J Barber
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list of conditions
 * and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 * and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Simple Apache module for REDIS
 * Compile: apxs -c -I $HIREDIS_HOME $HIREDIS_HOME/libhiredis.a mod_redis.c
 * Install: sudo apxs -i -a mod_redis.la
 * Restart: sudo apachectl restart
 *
 */

#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "http_log.h"
#include "ap_config.h"
#include "ap_mpm.h"
#include "apr_strings.h"
#include "apr_tables.h"
#include "string.h"

#include <time.h>

#include "hiredis.h"

#define MAX_ADDR					256		/* max server addr length */
#define CMD_PAGE_SIZE				25
#define MAX_CONNECT					256
#define JSONP_CALLBACK_ARGUMENT		"callback"

/*
 * Replacement arguments
 */
#define ARG_REQUEST_BODY			-1
#define ARG_FORM_FIELD				-2
#define ARG_QUERY_PARAM				-3

/*
 * macros
 */
#define RDEBUG(r,format,...) { if(sconf->loglevel==APLOG_DEBUG) ap_log_rerror(APLOG_MARK,APLOG_DEBUG,0,r,format,## __VA_ARGS__); }
#define SDEBUG(format,...) { if(sconf->server) ap_log_error(APLOG_MARK,APLOG_DEBUG,0,sconf->server,format,## __VA_ARGS__); }

module AP_MODULE_DECLARE_DATA redis_module;

/*
 * RedisAlias definitions
 */
typedef struct cmd_alias {

	ap_regex_t * expression;
	char *command;
	int arguments;
	int method;
	const char ** argv;
	size_t * argc;
	int * tokenargs;
	const char ** tokenfields;

} cmd_alias;

/*
 * per-server configuration and data
 */
typedef struct mod_redis_conf {

	apr_thread_mutex_t *lock;
	server_rec * server;

	redisContext * context;
	char *ip;
	int port;
	int timeout;

	int pageSize;

	cmd_alias * aliases;
	int count;

	int loglevel;
	clock_t timer1;
	clock_t timer2;

} mod_redis_conf;

static mod_redis_conf * sconf = 0;
static int threaded_mpm = 0;

/*
 * REDIS command handlers
 */
typedef struct command_handler {

	char * command;
	int length;
	int (*func)(request_rec *r, int args, const char ** argv, const size_t * argc, void ** reply);

} command_handler;

static command_handler commands[] = {};

/*
 * response data definitions
 */
typedef struct format {

	int type;
	char *format;

} format;

static format xml[] = { { REDIS_REPLY_STATUS, "<status>%s</status>" },
						{ REDIS_REPLY_STRING, "<string>%s</string>" },
						{ REDIS_REPLY_ERROR, "<error>%s</error>" },
						{ REDIS_REPLY_INTEGER, "<integer>%d</integer>" },
						{ REDIS_REPLY_ARRAY, "<array>%a</array>" },
						{ REDIS_REPLY_NIL, "<nil></nil>" },
						{ 0, 0 } };

static format json[] = {{ REDIS_REPLY_STATUS, "{ \"status\" : \"%s\" }" },
						{ REDIS_REPLY_STRING, "{ \"string\" : \"%s\" }" },
						{ REDIS_REPLY_ERROR, "{ \"error\" : \"%s\" }" },
						{ REDIS_REPLY_INTEGER, "{ \"integer\" : \"%d\" }" },
						{ REDIS_REPLY_ARRAY, "{ \"array\" : [ %A ] }" },
						{ REDIS_REPLY_NIL, "{ \"nil\" : \"nil\" }" },
						{ 0, 0 } };

int min(int a,int b) {
	return a < b ? a : b;
}

/**
 * Format a redisReply as a string in the specified format
 *
 * Optionally this function can be called with an output of null to calculate the
 * length of the string required to contain the formatted output
 */
static int formatReplyAsString(request_rec *r, redisReply *reply,format * const reply_format, char * output, int len)
{
	char * buffer = output;

	if (!reply) {
		return 0;
	}

	int 	index 			= 0,
			orginalLength 	= len;

	while (reply_format[index].type != reply->type) {
		index++;
	}

	if (!reply_format[index].type) {
		return 0;
	}

	char * stringFormat = reply_format[index].format;

	switch (reply->type) {
		case REDIS_REPLY_STATUS:
		case REDIS_REPLY_STRING:
		case REDIS_REPLY_ERROR:
			if(buffer) {
				len -= min(snprintf(buffer, len, stringFormat, reply->str),len);
			} else {
				len -= strlen(stringFormat) + strlen(reply->str);
			}
			break;

		case REDIS_REPLY_INTEGER:
			if(buffer) {
				len -= min(snprintf(buffer, len, stringFormat, (int) reply->integer),len);
			} else {
				len -= strlen(stringFormat) + 32;
			}
			break;

		case REDIS_REPLY_ARRAY: {
			int index = 0,
				chars = 0;

			char * tokenpos = 0;

			if (!((tokenpos = strstr(stringFormat, "%a")) || (tokenpos = strstr(stringFormat, "%A"))))
				break;

			if(buffer) {
				chars = min(snprintf(buffer,len,"%*.*s",(int)(tokenpos-stringFormat),(int)(tokenpos-stringFormat),stringFormat),len);
				buffer += chars;
				len -= chars;
			} else {
				len -= tokenpos - stringFormat;
			}

			for (index = 0; (index < reply->elements) && (len > 0); index++) {
				chars = formatReplyAsString(r, reply->element[index], reply_format, buffer, len);
				len -= chars;

				if(buffer) {
					buffer += chars;
				}

				if ((tokenpos[1] == 'A') && ((index + 1) < reply->elements)) {
					if (len > 2) {
						if(buffer) {
							*buffer++ = ',';
						}
						len--;
					}
				}
			}

			if(buffer) {
				chars = min(snprintf(buffer,len,"%s",tokenpos + 2),len);
				len -= chars;
				buffer += chars;
			} else {
				len -= strlen(tokenpos + 2);
			}


		}
			break;
		case REDIS_REPLY_NIL:
			if(buffer) {
				len -= min(snprintf(buffer, len, "%s", stringFormat),len);
			} else {
				len -= strlen(stringFormat);
			}
			break;
	}

	return orginalLength - len;
}

static int countChars(const char * cmd, int len,char match)
{
	int count = 0;
	for(;len;cmd++,len--) {
		if(*cmd == match) {
			count++;
		}
	}
	return count;
}

static const char ** parseCommandIntoArgArray(const char * cmd, int len, char delimiter, size_t ** fieldlens, int * count)
{
	if (!delimiter)
		delimiter = ' ';

	int fieldCount = 0,
		allocCount = countChars(cmd,len,delimiter) + 1;

	const char ** fields = calloc(allocCount, sizeof(char*));
	size_t * items = calloc(allocCount, sizeof(size_t));

	while (*cmd) {

		if(*cmd == '\"') {
			cmd++;
			const char * sos = cmd;
			while (*cmd != '\"') {
				cmd++;
			}
			const char * eos = cmd;

			fields[fieldCount] = sos;
			items[fieldCount++] = eos - sos;
			cmd++;

		} else if (*cmd != delimiter) {
			const char * sos = cmd;
			while (*cmd && (*cmd != delimiter)) {
				cmd++;
			}
			const char * eos = cmd;

			fields[fieldCount] = sos;
			items[fieldCount++] = eos - sos;

		} else {
			cmd++;
		}
	}

	*count = fieldCount;
	*fieldlens = items;
	return fields;
}

static redisReply * execRedisCommandsArgv(request_rec *r, int args, const char ** argv, const size_t * argc)
{
	redisReply * reply = 0;
	struct timeval timeout;

	if (sconf->lock) {
		apr_thread_mutex_lock(sconf->lock);
	}

	/*
	 * Connect to the server if required
	 */
	if (!sconf->context) {
		timeout.tv_sec = sconf->timeout / 1000;
		timeout.tv_usec = (sconf->timeout - (timeout.tv_sec * 1000)) * 1000;

		RDEBUG(r,"Connecting to REDIS on %s:%d (%d)", sconf->ip, sconf->port, sconf->timeout);
		sconf->context = redisConnectWithTimeout(sconf->ip,sconf->port,timeout);

		if((!sconf->context) || (sconf->context->err != REDIS_OK)) {
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Connection to REDIS failed to %s:%d", sconf->ip, sconf->port);
		}
    }

	if (sconf->context && (sconf->context->err == 0)) {

		if (!(reply = calloc(1, sizeof(redisReply)))) {
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,"Failed to allocate memory for reply");
		} else {

			RDEBUG(r,"Command: %*.*s", (int) argc[0],(int) argc[0],argv[0]);

			int response = 0,
				index	 = 0,
				match	 = 0;

			/*
			 * Execute a command handler
			 */
			for(index=0;index<(sizeof(commands)/sizeof(command_handler));index++) {

				if (!memcmp(argv[0], commands[index].command, commands[index].length) && (argc[0] == commands[index].length)) {
					response = (commands[index].func)(r, args, argv, argc,(void **) &reply);
					match = 1;
				}
			}

			/*
			 * Pass it straight through to REDIS
			 */
			if(!match) {

				clock_t t = clock();
				if (redisAppendCommandArgv(sconf->context, args, argv, argc) != REDIS_ERR) {
					response = redisGetReply(sconf->context, (void **) &reply);
				}
				sconf->timer2 += clock() - t;
			}

			/*
			 * Debug dump the response
			 */
			if(sconf->loglevel == APLOG_DEBUG) {
				char s[256] = "";
				formatReplyAsString(r, reply, json, s, (sizeof(s) / sizeof(char)) - 1);
				RDEBUG(r,"%s", s);
			}
		}
	}

	/*
	 * In the event of an error, close the connection to the server and
	 * wait for a reconnection later
	 */
	if (sconf->context && (sconf->context->err != REDIS_OK)) {
		ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "REDIS error: %d %s", sconf->context->err, sconf->context->errstr);
		redisFree(sconf->context);
		sconf->context = 0;
	}

	if (sconf->lock) {
		apr_thread_mutex_unlock(sconf->lock);
	}

	return reply;
}

apr_table_t * parseFormData(request_rec *r,const char *data) {
	const char * sof;
	const char * sov;
	const char * eof;
	const char * eov;
	char * key;
	char * value;

	apr_table_t * form;

	if((form = apr_table_make(r->pool,1)) == 0) {
		ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Failed to allocate new table from pool");
		return 0;
	}

	while(*data) {
		sof = eof = data;
		while(*eof && (*eof != '=')) {
			eof++;
			data++;
		}

		if(*data++ != '=') {
			ap_log_rerror(	APLOG_MARK,
							APLOG_ERR,
							0,
							r,
							"Invalid data format, missing '=' delimiter between key and value for key '%*.*s'",
							(int)(eof-sof),
							(int)(eof-sof),
							sof);
			return 0;
		}
		sov = eov = data;
		while(*eov && (*eov != '&')) {
			eov++;
			data++;
		}

		value = apr_pstrmemdup(r->pool,sov,(int)(eov-sov));
		ap_unescape_url(value);

		key = apr_pstrmemdup(r->pool,sof,(int)(eof-sof));
		apr_table_set(form,key,value);

		if(*data == '&') {
			data++;
		}
	}

	return form;
}

/*
 * mod_redis request handler
 */
static int redis_handler(request_rec *r) {
	const char * path = 0;
	format * responseFormat = xml;
	redisReply * reply = 0;
	char * data = 0;
	apr_size_t datalen = 0;
	apr_table_t * queryparams = 0;
	const char * callback = 0;
	const char * fileExtension = 0;
	int isJSONP = 0;

	if(strcmp(r->handler, "redis")) {
		return DECLINED;
	}

	RDEBUG(r,"%s", r->path_info);

	if(r->header_only) {
		return OK;
	}

	path = ((const char *) r->path_info) + ((*r->path_info == '/') ? 1 : 0);

	if((fileExtension = strrchr(path,'.')) == 0) {
		fileExtension = path + strlen(path);
	}

	/*
	 * Get the body of the request
	 */
	if((r->method_number == M_POST) || (r->method_number == M_PUT)) {

		apr_bucket_brigade * bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
		apr_status_t rv;
		int eos = 0;

		for(;!eos;) {

			rv = ap_get_brigade(r->input_filters, bb, AP_MODE_READBYTES,APR_BLOCK_READ, HUGE_STRING_LEN);

			apr_bucket *bucket;

			if (rv != APR_SUCCESS) {
				ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, "Error reading request entity data");
				break;
			}

			for(bucket = APR_BRIGADE_FIRST(bb);
				bucket != APR_BRIGADE_SENTINEL(bb);
				bucket = APR_BUCKET_NEXT(bucket))
			{
				if (APR_BUCKET_IS_EOS(bucket)) {
					eos = 1;
					break;
				}

				/* We can't do much with this. */
				if (APR_BUCKET_IS_FLUSH(bucket)) {
					continue;
				}

				if(!APR_BUCKET_IS_METADATA(bucket)) {
					if(bucket->length != (apr_size_t) -1) {
						datalen += bucket->length;
					}
				}
			}
		}

		if(datalen && (data = apr_palloc(r->pool,datalen + 1)) != 0) {
			rv = apr_brigade_flatten(bb,data,&datalen);

			if(rv == APR_SUCCESS) {
				data[datalen] = 0;
				RDEBUG(r,"request data: %s",data);
			}
		}

		apr_brigade_cleanup(bb);

		if((rv != APR_SUCCESS) || (!data)) {
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Error (flatten) reading form data");
			return HTTP_INTERNAL_SERVER_ERROR;
		}
	}

	/*
	 * Parse the query parameters
	 */
	if(r->parsed_uri.query) {
		if((queryparams = parseFormData(r,r->parsed_uri.query)) == 0) {
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Unable to parse query parameters");
			return HTTP_INTERNAL_SERVER_ERROR;
		}

		callback = apr_table_get(queryparams,JSONP_CALLBACK_ARGUMENT);
	}


	/*
	 * Measure the time taken in this hander
	 */
	clock_t t = clock();

	// Remove any file extension
	path = apr_pstrmemdup(r->pool,path,fileExtension-path);

	/*
	 * Match the command to a defined alias
	 */
	int alias_index = 0;
	cmd_alias * alias = 0;
	ap_regmatch_t matches[16];

	for (alias_index = 0; alias_index < sconf->count; alias_index++) {
		memset(matches, 0, sizeof(matches));

		if(ap_regexec(sconf->aliases[alias_index].expression, path, sizeof(matches) / sizeof(ap_regmatch_t), &matches[0], 0))
			continue;

		if(sconf->aliases[alias_index].method != r->method_number)
			continue;

		alias = &sconf->aliases[alias_index];
		break;
	}

	/*
	 * substitue the command arguments for the variables specified
	 */
	if(alias) {
		const char * fields[16];
		size_t items[16];
		int index = 0;
		apr_table_t * form = 0;

		/*
		 * for each item in aliased command, i.e. GET $1 is { "GET", "$1" }
		 */
		for (index = 0; (index < alias->arguments) && (index < (sizeof(items)/sizeof(size_t))); index++) {
			items[index] = alias->argc[index];
			fields[index] = alias->argv[index];

			if (alias->tokenargs && alias->tokenargs[index]) {
				int substitute = (int) alias->tokenargs[index];

				/*
				 * Determine the matching regular expression and replace in the
				 * temporary command array created
				 */
				switch(substitute) {
					case ARG_REQUEST_BODY:
						fields[index] = data;
						items[index] = (size_t) (data ? strlen(data) : 0);
						break;

					case ARG_FORM_FIELD:
						fields[index] = 0;
						items[index] = 0;
						/*
						 * Lazy parse the form data as we need a field from it now
						 */
						if(!form && data) {
							const char * content_type = apr_table_get(r->headers_in,"Content-Type");

							if(!content_type || strcasecmp(content_type,"application/x-www-form-urlencoded")) {
								ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Unexpected content type: %s",content_type ? content_type : "null");
								return HTTP_INTERNAL_SERVER_ERROR;
							}

							if((form = parseFormData(r,data)) == 0) {
								ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Unable to parse form");
								return HTTP_INTERNAL_SERVER_ERROR;
							}
						}
						if(!form)
							break;
						/*
						 * Lookup the form field in the table
						 */
						fields[index] = apr_table_get(form,alias->tokenfields[index]);
						items[index] = (size_t) (fields[index] ? strlen(fields[index]) : 0);
						break;

					case ARG_QUERY_PARAM:
						RDEBUG(r,"QSA lookup: %s",alias->tokenfields[index]);
						fields[index] = apr_table_get(queryparams,alias->tokenfields[index]);
						items[index] = (size_t) (fields[index] ? strlen(fields[index]) : 0);
						break;

					default:
						fields[index] = path + (int) matches[substitute].rm_so;
						items[index] = (size_t) (matches[substitute].rm_eo - matches[substitute].rm_so);
						break;
				}

				RDEBUG(r,"argument %d, \"%*.*s\" replaced with \"%*.*s\"", index,(int) alias->argc[index],(int) alias->argc[index],alias->argv[index],(int) items[index],(int) items[index],fields[index]);
			}
		}

		reply = execRedisCommandsArgv(r, alias->arguments, fields, items);

	} else {
		ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Unknown command %s", path);
		return HTTP_INTERNAL_SERVER_ERROR;
	}

	/*
	 * Write the response
	 */
	if(*fileExtension) {
		if((isJSONP = !strcmp(fileExtension, ".jsonp")) != 0) {
			responseFormat = json;
		} else if (!strcmp(fileExtension, ".json")) {
			responseFormat = json;
		}
	}

	if (responseFormat == json) {
		r->content_type = "application/json";
	} else {
		r->content_type = "text/xml";
		ap_rputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n", r);
		ap_rputs("<response>\r\n", r);
	}

	if (reply) {
		int alloc = formatReplyAsString(r, reply, responseFormat, NULL, 0xFFFF);

		if(isJSONP) {
			alloc += strlen(callback) + 8;
		}

		char * response = apr_pcalloc(r->pool,alloc + 1);

		if(response) {

			int offset = 0,
				bytes = 0;

			if(isJSONP) {
				offset = sprintf(response,"%s(",callback);
			}

			bytes = formatReplyAsString(r, reply, responseFormat, response + offset, (alloc + 1) - offset);

			if(isJSONP) {
				sprintf(response + offset + bytes,");");
			}

			ap_rputs(response, r);
		}

		freeReplyObject(reply);
	}

	if (responseFormat == xml) {
		ap_rputs("</response>\r\n", r);
	}

	sconf->timer1 += clock() - t;

	return OK;
}

static const char * set_ip_address(cmd_parms *parms, void *in_struct_ptr, const char *arg) {
	if (strlen(arg) == 0) {
		return "RedisIPAddress argument must be a string representing a server address";
	}
	
	mod_redis_conf *conf = ap_get_module_config(parms->server->module_config, &redis_module);
	conf->ip = apr_pstrdup(parms->pool, arg);

	return NULL;
}

static const char * set_port(cmd_parms *parms, void *in_struct_ptr, const char *arg) {
	int port;

	if (sscanf(arg, "%d", &port) != 1) {
		return "RedisPort argument must be an integer representing the port number";
	}
	
	mod_redis_conf *conf = ap_get_module_config(parms->server->module_config, &redis_module);	
	conf->port = port;
	
	return NULL;
}

static const char * set_timeout(cmd_parms *parms, void *in_struct_ptr, const char *arg) {
	int timeout;

	if (sscanf(arg, "%d", &timeout) != 1) {
		return "RedisTimeout argument must be an integer representing the timeout setting for a connection";
	}

	mod_redis_conf *conf = ap_get_module_config(parms->server->module_config, &redis_module);	
	conf->timeout = timeout;
	
	return NULL;
}

static const char * set_alias(cmd_parms *parms, void *in_struct_ptr,const char *w, const char *w2,const char *w3) {
	cmd_alias * aliases;
	mod_redis_conf *conf = ap_get_module_config(parms->server->module_config, &redis_module);	

	if ((aliases = apr_pcalloc(parms->pool,sizeof(cmd_alias) * (conf->count + 1))) == 0) {
		return "Failed to allocate memory for the alias configuration";
	}

	if (conf->aliases) {
		memcpy(aliases, conf->aliases, sizeof(cmd_alias) * conf->count);
	}

	conf->aliases = aliases;

	cmd_alias * alias = &conf->aliases[conf->count];
	alias->tokenargs = 0;

	if ((alias->command = apr_pcalloc(parms->pool,strlen(w2) + 1)) != 0) {
		strcpy(alias->command, w2);
	}

	if (!(alias->expression = ap_pregcomp(parms->pool, w, AP_REG_EXTENDED))) {
		return "RedisAlias argument must be a valid regular expression";
	}

	alias->argv = parseCommandIntoArgArray(alias->command, strlen(alias->command), 0, &alias->argc, &alias->arguments);
	SDEBUG("Parsed RedisAlias command: \'%s\' into %d arguments", alias->command, alias->arguments);

	if (alias->arguments) {

		alias->tokenargs = apr_pcalloc(parms->pool,sizeof(int) * alias->arguments);
		alias->tokenfields = apr_pcalloc(parms->pool,sizeof(char*) * alias->arguments);

		if(!alias->tokenargs || !alias->tokenfields) {
			return "Failed to allocate memory for RedisAlias expression";
		}

		int tokens = 0,
				id = 0;
		size_t * cmdlen = 0;
		const char ** cmds = 0;
		char field[256] = "";

		/*
		 * loop through the tokens {"ZREVRANGE", "GLOBAL", "$1	", "24"}
		 */
		for (tokens=0, cmds=alias->argv, cmdlen=alias->argc; tokens<alias->arguments; tokens++, cmds++, cmdlen++) {

			/*
			 * record that { 0, 0, 1, 0 }
			 */
			if (sscanf(*cmds, "$%d", &id) == 1) {
				alias->tokenargs[tokens] = id;
			} else if((*cmdlen<sizeof(field)) && (sscanf(*cmds, "${FORM:%[^}]}", field) == 1)) {
				alias->tokenargs[tokens] = ARG_FORM_FIELD;
				alias->tokenfields[tokens] = apr_pstrdup(parms->pool,field);
			} else if((*cmdlen<sizeof(field)) && (sscanf(*cmds, "${QSA:%[^}]}", field) == 1)) {
				alias->tokenargs[tokens] = ARG_QUERY_PARAM;
				alias->tokenfields[tokens] = apr_pstrdup(parms->pool,field);
			} else if((*cmdlen == 7) && !memcmp(*cmds,"%{DATA}",7)) {
				alias->tokenargs[tokens] = ARG_REQUEST_BODY;
			}

			if(alias->tokenargs[tokens]) {
				SDEBUG(" - RedisAlias argument %d, to be replaced with request expression group %d%s%s",
						tokens,
						alias->tokenargs[tokens],
						alias->tokenfields[tokens] ? " field: " : "",
					    alias->tokenfields[tokens] ? alias->tokenfields[tokens] : "");
			}
		}
	}

	/* Determine the method supported for this argument */
	alias->method = M_GET;

	if(w3) {
		if(ap_strcasestr(w3,"POST")) {
			alias->method = M_POST;
		} else if(ap_strcasestr(w3,"PUT")) {
			alias->method = M_PUT;
		} else if(ap_strcasestr(w3,"DELETE")) {
			alias->method = M_DELETE;
		}
	}

	conf->count++;

	return NULL;
}

static apr_status_t redis_pool_cleanup(void * parm)
{
	mod_redis_conf * conf = (mod_redis_conf *) parm;

	if (!conf)
		return APR_SUCCESS;

	/*
	 * Free the REDIS connection
	 */
	if (conf->lock) {
		apr_thread_mutex_lock(conf->lock);
	}

	if (conf->context) {
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, conf->server, "Closing REDIS connection");
		redisFree(conf->context);
		conf->context = 0;
	}

	if (conf->lock) {
		apr_thread_mutex_unlock(conf->lock);
	}

	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, conf->server, "time spent in handler %d",(int) (conf->timer1 / (CLOCKS_PER_SEC / 1000))) ;
	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, conf->server, "time spent in REDIS %d",(int) (conf->timer2 / (CLOCKS_PER_SEC / 1000)));

	/*
	 * Free the alias dictionary
	 */
	if (conf->aliases) {
		int alias_index = 0;

		for (alias_index = 0; alias_index < conf->count; alias_index++) {

			if (conf->aliases[alias_index].argv) {
				free(conf->aliases[alias_index].argv);
			}

			if (conf->aliases[alias_index].argc) {
				free(conf->aliases[alias_index].argc);
			}
		}

		conf->aliases = 0;
	}

	return APR_SUCCESS;
}

static void *redis_create_config(apr_pool_t *p, server_rec *s)
{
	if (!(sconf = apr_pcalloc(p, sizeof(mod_redis_conf)))) {
		return 0;
	}

    ap_mpm_query(AP_MPMQ_IS_THREADED, &threaded_mpm);
    if (threaded_mpm) {
        apr_thread_mutex_create(&sconf->lock, APR_THREAD_MUTEX_DEFAULT, p);
    }

	strcpy(sconf->ip, "127.0.0.1");
	sconf->port = 6379;
	sconf->timeout = 1500;
	sconf->server = s;
	sconf->count = 0;
	sconf->aliases = 0;
	sconf->context = 0;
	sconf->pageSize = CMD_PAGE_SIZE;

	return sconf;
}

static void redis_child_init(apr_pool_t *p, server_rec *s)
{
	apr_pool_cleanup_register(p, sconf, redis_pool_cleanup, apr_pool_cleanup_null);
}

static int redis_post_config(apr_pool_t *p,apr_pool_t *plog,apr_pool_t *ptemp,server_rec *s)
{
	if(s && sconf)
#if (AP_SERVER_MAJORVERSION_NUMBER == 2) && (AP_SERVER_MINORVERSION_NUMBER >= 4)
		sconf->loglevel = s->log.level;
#else
		sconf->loglevel = s->loglevel;
#endif

	return OK;
}

static void redis_register_hooks(apr_pool_t *p)
{
	ap_hook_handler(redis_handler, NULL, NULL, APR_HOOK_MIDDLE);
	ap_hook_child_init(redis_child_init, NULL, NULL, APR_HOOK_MIDDLE);
	ap_hook_post_config(redis_post_config, NULL, NULL, APR_HOOK_MIDDLE);
}

static const command_rec redis_cmds[] = {
	AP_INIT_TAKE1("RedisIPAddress", set_ip_address, NULL, RSRC_CONF,
			"The address of the REDIS server"),
	AP_INIT_TAKE1("RedisPort", set_port, NULL, RSRC_CONF,
			"The port number of the REDIS server"),
	AP_INIT_TAKE1("RedisTimeout", set_timeout, NULL, RSRC_CONF,
			"The timeout for connections to the REDIS server"),
	AP_INIT_TAKE23("RedisAlias", set_alias, NULL, RSRC_CONF,
			"An alias pattern to map to REDIS commands"), { NULL }
};

/* Dispatch list for API hooks */
module AP_MODULE_DECLARE_DATA redis_module = {
	STANDARD20_MODULE_STUFF,
	NULL,					/* create per-dir    config structures */
	NULL, 					/* merge  per-dir    config structures */
	redis_create_config, 	/* create per-server config structures */
	NULL, 					/* merge  per-server config structures */
	redis_cmds, 			/* table of config file commands       */
	redis_register_hooks 	/* register hooks                      */
};

