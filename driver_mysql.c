/*
 +----------------------------------------------------------------------+
 |  APM stands for Alternative PHP Monitor                              |
 +----------------------------------------------------------------------+
 | Copyright (c) 2008-2010  Davide Mendolia, Patrick Allaert            |
 +----------------------------------------------------------------------+
 | This source file is subject to version 3.01 of the PHP license,      |
 | that is bundled with this package in the file LICENSE, and is        |
 | available through the world-wide-web at the following url:           |
 | http://www.php.net/license/3_01.txt                                  |
 | If you did not receive a copy of the PHP license and are unable to   |
 | obtain it through the world-wide-web, please send a note to          |
 | license@php.net so we can mail you a copy immediately.               |
 +----------------------------------------------------------------------+
 | Authors: Patrick Allaert <patrickallaert@php.net>                    |
 +----------------------------------------------------------------------+
*/

#include <mysql/mysql.h>
#include "php_apm.h"
#include "php_ini.h"
#include "driver_mysql.h"
#include "ext/standard/php_smart_str.h"
#include "ext/json/php_json.h"
#ifdef NETWARE
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

static long get_table_count(char * table);

ZEND_EXTERN_MODULE_GLOBALS(apm)

ZEND_DECLARE_MODULE_GLOBALS(apm_mysql)

APM_DRIVER_CREATE(mysql)

PHP_INI_BEGIN()
 	/* Boolean controlling whether the driver is active or not */
 	STD_PHP_INI_BOOLEAN("apm.mysql_enabled",       "1",               PHP_INI_PERDIR, OnUpdateBool,   enabled,             zend_apm_mysql_globals, apm_mysql_globals)
	/* error_reporting of the driver */
	STD_PHP_INI_ENTRY("apm.mysql_error_reporting", NULL,              PHP_INI_ALL,    OnUpdateAPMmysqlErrorReporting,   error_reporting,     zend_apm_mysql_globals, apm_mysql_globals)
	/* mysql host */
	STD_PHP_INI_ENTRY("apm.mysql_host",            "localhost",       PHP_INI_PERDIR, OnUpdateString, db_host,             zend_apm_mysql_globals, apm_mysql_globals)
	/* mysql port */
	STD_PHP_INI_ENTRY("apm.mysql_port",            "0",               PHP_INI_PERDIR, OnUpdateLong,   db_port,             zend_apm_mysql_globals, apm_mysql_globals)
	/* mysql user */
	STD_PHP_INI_ENTRY("apm.mysql_user",            "root",            PHP_INI_PERDIR, OnUpdateString, db_user,             zend_apm_mysql_globals, apm_mysql_globals)
	/* mysql password */
	STD_PHP_INI_ENTRY("apm.mysql_pass",            "",                PHP_INI_PERDIR, OnUpdateString, db_pass,             zend_apm_mysql_globals, apm_mysql_globals)
	/* mysql database */
	STD_PHP_INI_ENTRY("apm.mysql_db",              "apm",             PHP_INI_PERDIR, OnUpdateString, db_name,             zend_apm_mysql_globals, apm_mysql_globals)
PHP_INI_END()

/* Returns the MYSQL instance (singleton) */
MYSQL * mysql_get_instance() {
	if (APM_MY_G(event_db) == NULL) {
		APM_MY_G(event_db) = malloc(sizeof(MYSQL));
		mysql_init(APM_MY_G(event_db));
		APM_DEBUG("[MySQL driver] Connecting to server...");
		if (mysql_real_connect(APM_MY_G(event_db), APM_MY_G(db_host), APM_MY_G(db_user), APM_MY_G(db_pass), APM_MY_G(db_name), APM_MY_G(db_port), NULL, 0) == NULL) {
			APM_DEBUG("FAILED! Message: %s\n", mysql_error(APM_MY_G(event_db)));

			free(APM_MY_G(event_db));
			APM_MY_G(event_db) = NULL;
			return NULL;
		}
		APM_DEBUG("OK\n");
		mysql_set_character_set(APM_MY_G(event_db), "utf8");
	}

	return APM_MY_G(event_db);
}

/* Insert an event in the backend */
void apm_driver_mysql_insert_event(int type, char * error_filename, uint error_lineno, char * msg, char * trace, char * uri, char * host, char * ip, char * cookies, char * post_vars, char * referer TSRMLS_DC)
{
	char *filename_esc = NULL, *msg_esc = NULL, *trace_esc = NULL, *uri_esc = NULL, *host_esc = NULL, *cookies_esc = NULL, *post_vars_esc = NULL, *referer_esc = NULL, *sql = NULL;
	int filename_len = 0, msg_len = 0, trace_len = 0, uri_len = 0, host_len = 0, ip_int = 0, cookies_len = 0, post_vars_len = 0, referer_len = 0;
	struct in_addr ip_addr;
	MYSQL *connection;

	MYSQL_INSTANCE_INIT

	if (error_filename) {
		filename_len = strlen(error_filename);
		filename_esc = emalloc(filename_len * 2 + 1);
		filename_len = mysql_real_escape_string(connection, filename_esc, error_filename, filename_len);
	}

	if (msg) {
		msg_len = strlen(msg);
		msg_esc = emalloc(msg_len * 2 + 1);
		msg_len = mysql_real_escape_string(connection, msg_esc, msg, msg_len);
	}

	if (trace) {
		trace_len = strlen(trace);
		trace_esc = emalloc(trace_len * 2 + 1);
		trace_len = mysql_real_escape_string(connection, trace_esc, trace, trace_len);
	}

	if (uri) {
		uri_len = strlen(uri);
		uri_esc = emalloc(uri_len * 2 + 1);
		uri_len = mysql_real_escape_string(connection, uri_esc, uri, uri_len);
	}

	if (host) {
		host_len = strlen(host);
		host_esc = emalloc(host_len * 2 + 1);
		host_len = mysql_real_escape_string(connection, host_esc, host, host_len);
	}

	if (ip && (inet_pton(AF_INET, ip, &ip_addr) == 1)) {
		ip_int = ntohl(ip_addr.s_addr);
	}
	
	if (cookies) {
		cookies_len = strlen(cookies);
		cookies_esc = emalloc(cookies_len * 2 + 1);
		cookies_len = mysql_real_escape_string(connection, cookies_esc, cookies, cookies_len);
	}

	if (post_vars) {
		post_vars_len = strlen(post_vars);
		post_vars_esc = emalloc(post_vars_len * 2 + 1);
		post_vars_len = mysql_real_escape_string(connection, post_vars_esc, post_vars, post_vars_len);
	}

	if (referer) {
		referer_len = strlen(referer);
		referer_esc = emalloc(referer_len * 2 + 1);
		referer_len = mysql_real_escape_string(connection, referer_esc, referer, referer_len);
	}

	sql = emalloc(176 + filename_len + msg_len + trace_len + uri_len + host_len + cookies_len + post_vars_len + referer_len);
	sprintf(
		sql,
		"INSERT INTO event (type, file, line, message, backtrace, uri, host, ip, cookies, post_vars, referer) VALUES (%d, '%s', %u, '%s', '%s', '%s', '%s', %u, '%s', '%s', '%s')",
		type, error_filename ? filename_esc : "", error_lineno, msg ? msg_esc : "", trace ? trace_esc : "", uri ? uri_esc : "", host ? host_esc : "", ip_int, cookies ? cookies_esc : "", post_vars ? post_vars_esc : "", referer ? referer_esc : "");

	APM_DEBUG("[MySQL driver] Sending: %s\n", sql);
	if (mysql_query(connection, sql) != 0)
		APM_DEBUG("[MySQL driver] Error: %s\n", mysql_error(APM_MY_G(event_db)));

	efree(sql);
	efree(filename_esc);
	efree(msg_esc);
	efree(trace_esc);
	efree(uri_esc);
	efree(host_esc);
	efree(cookies_esc);
	efree(post_vars_esc);
	efree(referer_esc);
}
int apm_driver_mysql_minit(int module_number)
{
	REGISTER_INI_ENTRIES();
	return SUCCESS;
}

int apm_driver_mysql_rinit()
{
	return SUCCESS;
}

int apm_driver_mysql_mshutdown()
{
	return SUCCESS;
}

int apm_driver_mysql_rshutdown()
{
	if (APM_MY_G(event_db) != NULL) {
		APM_DEBUG("[MySQL driver] Closing connection\n");
		mysql_close(APM_MY_G(event_db));
		free(APM_MY_G(event_db));
		APM_MY_G(event_db) = NULL;
	}
	return SUCCESS;
}

void apm_driver_mysql_insert_slow_request(float duration, char * script_filename)
{
	char *filename_esc = NULL, *sql = NULL;
	int filename_len = 0;
	MYSQL *connection;

	MYSQL_INSTANCE_INIT

	if (script_filename) {
		filename_len = strlen(script_filename);
		filename_esc = emalloc(filename_len * 2 + 1);
		filename_len = mysql_real_escape_string(connection, filename_esc, script_filename, filename_len);
	}

	sql = emalloc(100 + filename_len);
	sprintf(
		sql,
		"INSERT INTO slow_request (duration, file) VALUES (%f, '%s')",
		USEC_TO_SEC(duration), script_filename ? filename_esc : "");

	APM_DEBUG("[MySQL driver] Sending: %s\n", sql);
	if (mysql_query(connection, sql) != 0)
		APM_DEBUG("[MySQL driver] Error: %s\n", mysql_error(APM_MY_G(event_db)));

	efree(sql);
	efree(filename_esc);
}

/* {{{ proto bool apm_get_mysql_events([, int limit[, int offset[, int order[, bool asc]]]])
   Returns JSON with all events */
PHP_FUNCTION(apm_get_mysql_events)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	long order = APM_ORDER_ID;
	long limit = 25;
	long offset = 0;
	char sql[600];
	zend_bool asc = 0;
	zend_bool first_row = 1;
	struct in_addr myaddr;
	unsigned long n;
	zval zfile, zmsg, zurl;
	smart_str file = {0}, msg = {0}, url = {0};
	MYSQL *connection;
#ifdef HAVE_INET_PTON
    char ip_str[40];
#else
	char *ip_str;
#endif

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|lllb", &limit, &offset, &order, &asc) == FAILURE) {
		return;
	}

	MYSQL_INSTANCE_INIT

	if (order < 1 || order > 6) {
		order = 1;
	}

	sprintf(
		sql,
		"SELECT id, ts, CASE type \
 WHEN 1 THEN 'E_ERROR' \
 WHEN 2 THEN 'E_WARNING' \
 WHEN 4 THEN 'E_PARSE' \
 WHEN 8 THEN 'E_NOTICE' \
 WHEN 16 THEN 'E_CORE_ERROR' \
 WHEN 32 THEN 'E_CORE_WARNING' \
 WHEN 64 THEN 'E_COMPILE_ERROR' \
 WHEN 128 THEN 'E_COMPILE_WARNING' \
 WHEN 256 THEN 'E_USER_ERROR' \
 WHEN 512 THEN 'E_USER_WARNING' \
 WHEN 1024 THEN 'E_USER_NOTICE' \
 WHEN 2048 THEN 'E_STRICT' \
 WHEN 4096 THEN 'E_RECOVERABLE_ERROR' \
 WHEN 8192 THEN 'E_DEPRECATED' \
 WHEN 16384 THEN 'E_USER_DEPRECATED' \
 END, \
file, ip, CONCAT('http://', CASE host WHEN '' THEN '<i>[unknown]</i>' ELSE host END, uri), line, message FROM event ORDER BY %ld %s LIMIT %ld OFFSET %ld", order, asc ? "ASC" : "DESC", limit, offset);

	if (mysql_query(connection, sql) != 0)
		RETURN_FALSE;

	result = mysql_use_result(connection);

	while ((row = mysql_fetch_row(result))) {
		Z_TYPE(zfile) = IS_STRING;
		Z_STRVAL(zfile) = row[3];
		Z_STRLEN(zfile) = strlen(row[3]);
		apm_json_encode(&file, &zfile);
		smart_str_0(&file);

		Z_TYPE(zmsg) = IS_STRING;
		Z_STRVAL(zmsg) = row[7];
		Z_STRLEN(zmsg) = strlen(row[7]);
		apm_json_encode(&msg, &zmsg);
		smart_str_0(&msg);

		Z_TYPE(zurl) = IS_STRING;
		Z_STRVAL(zurl) = row[5];
		Z_STRLEN(zurl) = strlen(row[5]);
		apm_json_encode(&url, &zurl);
		smart_str_0(&url);

		n = strtoul(row[4], NULL, 0);

		myaddr.s_addr = htonl(n);

#ifdef HAVE_INET_PTON
		inet_ntop(AF_INET, &myaddr, ip_str, sizeof(ip_str));
#else
		ip_str = inet_ntoa(myaddr);
#endif

		if (first_row) {
			first_row = 0;
		} else {
			php_printf(",\n");
		}

		php_printf("{\"id\":\"%s\", \"cell\":[\"%s\", \"%s\", \"%s\", %s, %s, \"%s\", \"%s\", %s]}",
					   row[0], row[0], row[1], row[2], url.c, file.c, row[6], ip_str, msg.c);

		smart_str_free(&file);
		smart_str_free(&msg);
		smart_str_free(&url);
	}

	mysql_free_result(result);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool apm_get_mysql_slow_requests([, int limit[, int offset[, int order[, bool asc]]]])
   Returns JSON with all slow requests */
PHP_FUNCTION(apm_get_mysql_slow_requests)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	long order = APM_ORDER_ID;
	long limit = 25;
	long offset = 0;
	char sql[128];
	zend_bool asc = 0;
	zend_bool first_row = 1;
	smart_str file = {0};
	zval zfile;
	MYSQL *connection;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|lllb", &limit, &offset, &order, &asc) == FAILURE) {
		return;
	}

	MYSQL_INSTANCE_INIT

	sprintf(sql, "SELECT id, ts, duration, file FROM slow_request ORDER BY %ld %s LIMIT %ld OFFSET %ld", order, asc ? "ASC" : "DESC", limit, offset);
	if (mysql_query(connection, sql) != 0)
		RETURN_FALSE;

	result = mysql_use_result(connection);

	while ((row = mysql_fetch_row(result))) {
		Z_TYPE(zfile) = IS_STRING;
		Z_STRVAL(zfile) = row[3];
		Z_STRLEN(zfile) = strlen(row[3]);

		apm_json_encode(&file, &zfile);

		smart_str_0(&file);

		if (first_row) {
			first_row = 0;
		} else {
			php_printf(",\n");
		}

		php_printf("{\"id\":\"%s\", \"cell\":[\"%s\", \"%s\", \"%s\", %s]}",
           row[0], row[0], row[1], row[2], file.c);

		smart_str_free(&file);
	}

	mysql_free_result(result);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int apm_get_mysql_events_count()
   Return the number of events */
PHP_FUNCTION(apm_get_mysql_events_count)
{
	long count;

	count = get_table_count("event");
	if (count == -1) {
		RETURN_FALSE;
	}
	RETURN_LONG(count);
}
/* }}} */

/* {{{ proto int apm_get_mysql_slow_requests_count()
   Return the number of slow requests */
PHP_FUNCTION(apm_get_mysql_slow_requests_count)
{
	long count;

	count = get_table_count("slow_request");
	if (count == -1) {
		RETURN_FALSE;
	}
	RETURN_LONG(count);
}
/* }}} */

/* {{{ proto array apm_get_mysql_event_into(int eventID)
   Returns all information available on a request */
PHP_FUNCTION(apm_get_mysql_event_info)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	long id = 0;
	char sql[135];
	apm_event_info info;
	MYSQL *connection;

	info.file = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &id) == FAILURE) {
		return;
	}

	MYSQL_INSTANCE_INIT

	sprintf(sql, "SELECT id, UNIX_TIMESTAMP(ts), type, file, line, message, backtrace, ip, cookies, host, uri, post_vars, referer FROM event WHERE id = %ld", id);
	if (mysql_query(connection, sql) != 0)
		RETURN_FALSE;

	result = mysql_use_result(connection);

	
	if (!(row = mysql_fetch_row(result))) {
		mysql_free_result(result);
		RETURN_FALSE;
	}

	array_init(return_value);

	add_assoc_long(return_value, "timestamp", atoi(row[1]));
	add_assoc_string(return_value, "file", estrdup(row[3]), 1);
	add_assoc_long(return_value, "line", atoi(row[4]));
	add_assoc_long(return_value, "type", atoi(row[2]));
	add_assoc_string(return_value, "message", estrdup(row[5]), 1);
	add_assoc_string(return_value, "stacktrace", estrdup(row[6]), 1);
	add_assoc_long(return_value, "ip", atoi(row[7]));
	add_assoc_string(return_value, "cookies", estrdup(row[8]), 1);
	add_assoc_string(return_value, "host", estrdup(row[9]), 1);
	add_assoc_string(return_value, "uri", estrdup(row[10]), 1);
	add_assoc_string(return_value, "post_vars", estrdup(row[11]), 1);
	add_assoc_string(return_value, "referer", estrdup(row[12]), 1);

	mysql_free_result(result);
}
/* }}} */

/* Returns the number of rows of a table */
static long get_table_count(char * table)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	char sql[64];
	long count;
	MYSQL *connection;

	MYSQL_INSTANCE_INIT_EX(-1)

	sprintf(sql, "SELECT COUNT(*) FROM %s", table);

	if (mysql_query(connection, sql) != 0)
		return -1;

	result = mysql_use_result(connection);

	if (!(row = mysql_fetch_row(result))) {
		return -1;
	}

	count = atol(row[0]);

	mysql_free_result(result);

	return count;
}
