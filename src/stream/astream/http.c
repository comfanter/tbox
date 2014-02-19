/*!The Treasure Box Library
 * 
 * TBox is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * TBox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with TBox; 
 * If not, see <a href="http://www.gnu.org/licenses/"> http://www.gnu.org/licenses/</a>
 * 
 * Copyright (C) 2009 - 2012, ruki All rights reserved.
 *
 * @author		ruki
 * @file		http.c
 *
 */

/* ///////////////////////////////////////////////////////////////////////
 * trace
 */
//#define TB_TRACE_IMPL_TAG 				"ahttp"

/* ///////////////////////////////////////////////////////////////////////
 * includes
 */
#include "prefix.h"
#include "../../asio/asio.h"
#include "../../platform/platform.h"

/* ///////////////////////////////////////////////////////////////////////
 * types
 */

// the http stream type
typedef struct __tb_astream_http_t
{
	// the base
	tb_astream_t 				base;

	// the http 
	tb_handle_t 				http;

	// the size
	tb_atomic64_t 				size;

	// the offset
	tb_atomic64_t 				offset;

	// the func
	union
	{
		tb_astream_open_func_t 	open;
		tb_astream_read_func_t 	read;
		tb_astream_writ_func_t 	writ;
		tb_astream_seek_func_t 	seek;
		tb_astream_sync_func_t 	sync;
		tb_astream_task_func_t 	task;

	} 							func;

	// the priv
	tb_pointer_t 				priv;

}tb_astream_http_t;

/* ///////////////////////////////////////////////////////////////////////
 * implementation
 */
static __tb_inline__ tb_astream_http_t* tb_astream_http_cast(tb_astream_t* astream)
{
	tb_assert_and_check_return_val(astream && astream->type == TB_ASTREAM_TYPE_HTTP, tb_null);
	return (tb_astream_http_t*)astream;
}
static tb_bool_t tb_astream_http_open_func(tb_handle_t http, tb_size_t state, tb_http_status_t const* status, tb_pointer_t priv)
{
	// check
	tb_assert_and_check_return_val(http && status, tb_false);

	// the stream
	tb_astream_http_t* hstream = (tb_astream_http_t*)priv;
	tb_assert_and_check_return_val(hstream && hstream->func.open, tb_false);

	// opened
	tb_atomic_set(&hstream->base.opened, 1);

	// save size
	tb_hize_t size = (!status->bgzip && !status->bdeflate)? status->document_size : 0;
	tb_atomic64_set(&hstream->size, size);

	// save offset
	if (size) tb_atomic64_set0(&hstream->offset);

	// done func
	return hstream->func.open((tb_astream_t*)hstream, state, hstream->priv);
}
static tb_bool_t tb_astream_http_open(tb_astream_t* astream, tb_astream_open_func_t func, tb_pointer_t priv)
{
	// check
	tb_astream_http_t* hstream = tb_astream_http_cast(astream);
	tb_assert_and_check_return_val(hstream && hstream->http && func, tb_false);

	// save func and priv
	hstream->priv 		= priv;
	hstream->func.open 	= func;

	// init size and offset
	tb_atomic64_set0(&hstream->size);
	tb_atomic64_set(&hstream->offset, -1);
 
	// post open
	return tb_aicp_http_open(hstream->http, tb_astream_http_open_func, astream);
}
static tb_bool_t tb_astream_http_read_func(tb_handle_t http, tb_size_t state, tb_byte_t const* data, tb_size_t real, tb_size_t size, tb_pointer_t priv)
{
	// check
	tb_assert_and_check_return_val(http && data, tb_false);

	// the stream
	tb_astream_http_t* hstream = (tb_astream_http_t*)priv;
	tb_assert_and_check_return_val(hstream && hstream->func.read, tb_false);

	// save offset
	if (state == TB_ASTREAM_STATE_OK) 
	{
		// try offset += real
		tb_hong_t offset = tb_atomic64_fetch_and_add(&hstream->offset, real);
		
		// not seeked? reset offset
		if (offset < 0) tb_atomic64_set(&hstream->offset, -1);
	}

	// done func
	return hstream->func.read((tb_astream_t*)hstream, state, data, real, size, hstream->priv);
}
static tb_bool_t tb_astream_http_read(tb_astream_t* astream, tb_size_t delay, tb_size_t maxn, tb_astream_read_func_t func, tb_pointer_t priv)
{
	// check
	tb_astream_http_t* hstream = tb_astream_http_cast(astream);
	tb_assert_and_check_return_val(hstream && hstream->http && func, tb_false);

	// save func and priv
	hstream->priv 		= priv;
	hstream->func.read 	= func;

	// post read
	return tb_aicp_http_read_after(hstream->http, delay, maxn, tb_astream_http_read_func, astream);
}
static tb_bool_t tb_astream_http_writ_func(tb_handle_t http, tb_size_t state, tb_byte_t const* data, tb_size_t real, tb_size_t size, tb_pointer_t priv)
{
	// check
	tb_assert_and_check_return_val(http, tb_false);

	// the stream
	tb_astream_http_t* hstream = (tb_astream_http_t*)priv;
	tb_assert_and_check_return_val(hstream && hstream->func.writ, tb_false);

	// save offset
	if (state == TB_ASTREAM_STATE_OK) 
	{
		// try offset += real
		tb_hong_t offset = tb_atomic64_fetch_and_add(&hstream->offset, real);
		
		// not seeked? reset offset
		if (offset < 0) tb_atomic64_set(&hstream->offset, -1);
	}

	// done func
	return hstream->func.writ((tb_astream_t*)hstream, state, data, real, size, hstream->priv);
}
static tb_bool_t tb_astream_http_writ(tb_astream_t* astream, tb_size_t delay, tb_byte_t const* data, tb_size_t size, tb_astream_writ_func_t func, tb_pointer_t priv)
{
	// check
	tb_astream_http_t* hstream = tb_astream_http_cast(astream);
	tb_assert_and_check_return_val(hstream && hstream->http && data && size && func, tb_false);

	// save func and priv
	hstream->priv 		= priv;
	hstream->func.writ 	= func;

	// post writ
	return tb_aicp_http_writ_after(hstream->http, delay, data, size, tb_astream_http_writ_func, astream);
}
static tb_bool_t tb_astream_http_seek_func(tb_handle_t http, tb_size_t state, tb_hize_t offset, tb_pointer_t priv)
{
	// check
	tb_assert_and_check_return_val(http, tb_false);

	// the stream
	tb_astream_http_t* hstream = (tb_astream_http_t*)priv;
	tb_assert_and_check_return_val(hstream && hstream->func.seek, tb_false);

	// save offset
	if (state == TB_ASTREAM_STATE_OK) tb_atomic64_set(&hstream->offset, offset);

	// done func
	return hstream->func.seek((tb_astream_t*)hstream, state, offset, hstream->priv);
}
static tb_bool_t tb_astream_http_seek(tb_astream_t* astream, tb_hize_t offset, tb_astream_seek_func_t func, tb_pointer_t priv)
{
	// check
	tb_astream_http_t* hstream = tb_astream_http_cast(astream);
	tb_assert_and_check_return_val(hstream && hstream->http && func, tb_false);

	// save func and priv
	hstream->priv 		= priv;
	hstream->func.seek 	= func;

	// post seek
	return tb_aicp_http_seek(hstream->http, offset, tb_astream_http_seek_func, astream);
}
static tb_bool_t tb_astream_http_sync_func(tb_handle_t http, tb_size_t state, tb_pointer_t priv)
{
	// check
	tb_assert_and_check_return_val(http, tb_false);

	// the stream
	tb_astream_http_t* hstream = (tb_astream_http_t*)priv;
	tb_assert_and_check_return_val(hstream && hstream->func.sync, tb_false);

	// done func
	return hstream->func.sync((tb_astream_t*)hstream, state, hstream->priv);
}
static tb_bool_t tb_astream_http_sync(tb_astream_t* astream, tb_bool_t bclosing, tb_astream_sync_func_t func, tb_pointer_t priv)
{
	// check
	tb_astream_http_t* hstream = tb_astream_http_cast(astream);
	tb_assert_and_check_return_val(hstream && hstream->http && func, tb_false);

	// save func and priv
	hstream->priv 		= priv;
	hstream->func.sync 	= func;

	// post sync
	return tb_aicp_http_sync(hstream->http, bclosing, tb_astream_http_sync_func, astream);
}
static tb_bool_t tb_astream_http_task_func(tb_handle_t http, tb_size_t state, tb_pointer_t priv)
{
	// check
	tb_assert_and_check_return_val(http, tb_false);

	// the stream
	tb_astream_http_t* hstream = (tb_astream_http_t*)priv;
	tb_assert_and_check_return_val(hstream && hstream->func.task, tb_false);

	// done func
	return hstream->func.task((tb_astream_t*)hstream, state, hstream->priv);
}
static tb_bool_t tb_astream_http_task(tb_astream_t* astream, tb_size_t delay, tb_astream_task_func_t func, tb_pointer_t priv)
{
	// check
	tb_astream_http_t* hstream = tb_astream_http_cast(astream);
	tb_assert_and_check_return_val(hstream && hstream->http && func, tb_false);

	// save func and priv
	hstream->priv 		= priv;
	hstream->func.task 	= func;

	// post task
	return tb_aicp_http_task(hstream->http, delay, tb_astream_http_task_func, astream);
}
static tb_void_t tb_astream_http_kill(tb_astream_t* astream)
{	
	// check
	tb_astream_http_t* hstream = tb_astream_http_cast(astream);
	tb_assert_and_check_return(hstream);

	// kill it
	if (hstream->http) tb_aicp_http_kill(hstream->http);
}
static tb_void_t tb_astream_http_clos(tb_astream_t* astream, tb_bool_t bcalling)
{	
	// check
	tb_astream_http_t* hstream = tb_astream_http_cast(astream);
	tb_assert_and_check_return(hstream);

	// close it
	if (hstream->http) tb_aicp_http_clos(hstream->http, bcalling);

	// clear size and offset
	tb_atomic64_set0(&hstream->size);
	tb_atomic64_set(&hstream->offset, -1);
}
static tb_void_t tb_astream_http_exit(tb_astream_t* astream, tb_bool_t bcalling)
{	
	// check
	tb_astream_http_t* hstream = tb_astream_http_cast(astream);
	tb_assert_and_check_return(hstream);

	// exit it
	if (hstream->http) tb_aicp_http_exit(hstream->http, bcalling);
	hstream->http = tb_null;
}
static tb_bool_t tb_astream_http_ctrl(tb_astream_t* astream, tb_size_t ctrl, tb_va_list_t args)
{
	// check
	tb_astream_http_t* hstream = tb_astream_http_cast(astream);
	tb_assert_and_check_return_val(hstream && hstream->http, tb_false);

	// done
	switch (ctrl)
	{
	case TB_ASTREAM_CTRL_GET_SIZE:
		{
			// check
			tb_assert_and_check_return_val(tb_atomic_get(&astream->opened) && hstream->http, tb_false);

			// get size
			tb_hize_t* psize = (tb_hize_t*)tb_va_arg(args, tb_hize_t*);
			tb_assert_and_check_return_val(psize, tb_false);
			*psize = (tb_hize_t)tb_atomic64_get(&hstream->size);
			return tb_true;
		}
	case TB_ASTREAM_CTRL_GET_OFFSET:
		{
			// check
			tb_assert_and_check_return_val(tb_atomic_get(&astream->opened) && hstream->http, tb_false);

			// get offset
			tb_hong_t* poffset = (tb_hong_t*)tb_va_arg(args, tb_hong_t*);
			tb_assert_and_check_return_val(poffset, tb_false);
			*poffset = (tb_hong_t)tb_atomic64_get(&hstream->offset);
			return tb_true;
		}
	case TB_ASTREAM_CTRL_SET_URL:
		{
			// url
			tb_char_t const* url = (tb_char_t const*)tb_va_arg(args, tb_char_t const*);
			tb_assert_and_check_return_val(url, tb_false);
		
			// set url
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_SET_URL, url);
		}
		break;
	case TB_ASTREAM_CTRL_GET_URL:
		{
			// purl
			tb_char_t const** purl = (tb_char_t const**)tb_va_arg(args, tb_char_t const**);
			tb_assert_and_check_return_val(purl, tb_false);
	
			// get url
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_GET_URL, purl);
		}
		break;
	case TB_ASTREAM_CTRL_SET_HOST:
		{
			// host
			tb_char_t const* host = (tb_char_t const*)tb_va_arg(args, tb_char_t const*);
			tb_assert_and_check_return_val(host, tb_false);
	
			// set host
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_SET_HOST, host);
		}
		break;
	case TB_ASTREAM_CTRL_GET_HOST:
		{
			// phost
			tb_char_t const** phost = (tb_char_t const**)tb_va_arg(args, tb_char_t const**);
			tb_assert_and_check_return_val(phost, tb_false); 

			// get host
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_GET_HOST, phost);
		}
		break;
	case TB_ASTREAM_CTRL_SET_PORT:
		{
			// port
			tb_size_t port = (tb_size_t)tb_va_arg(args, tb_size_t);
			tb_assert_and_check_return_val(port, tb_false);
	
			// set port
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_SET_PORT, port);
		}
		break;
	case TB_ASTREAM_CTRL_GET_PORT:
		{
			// pport
			tb_size_t* pport = (tb_size_t*)tb_va_arg(args, tb_size_t*);
			tb_assert_and_check_return_val(pport, tb_false);
	
			// get port
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_GET_PORT, pport);
		}
		break;
	case TB_ASTREAM_CTRL_SET_PATH:
		{
			// path
			tb_char_t const* path = (tb_char_t const*)tb_va_arg(args, tb_char_t const*);
			tb_assert_and_check_return_val(path, tb_false);
	
			// set path
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_SET_PATH, path);
		}
		break;
	case TB_ASTREAM_CTRL_GET_PATH:
		{
			// ppath
			tb_char_t const** ppath = (tb_char_t const**)tb_va_arg(args, tb_char_t const**);
			tb_assert_and_check_return_val(ppath, tb_false);
	
			// get path
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_GET_PATH, ppath);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_SET_METHOD:
		{
			// method
			tb_size_t method = (tb_size_t)tb_va_arg(args, tb_size_t);
	
			// set method
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_SET_METHOD, method);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_GET_METHOD:
		{
			// pmethod
			tb_size_t* pmethod = (tb_size_t*)tb_va_arg(args, tb_size_t*);
			tb_assert_and_check_return_val(pmethod, tb_false);
	
			// get method
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_GET_METHOD, pmethod);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_SET_HEAD:
		{
			// key
			tb_char_t const* key = (tb_char_t const*)tb_va_arg(args, tb_char_t const*);
			tb_assert_and_check_return_val(key, tb_false);

 			// val
			tb_char_t const* val = (tb_char_t const*)tb_va_arg(args, tb_char_t const*);
			tb_assert_and_check_return_val(val, tb_false);
	
			// set head
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_SET_HEAD, key, val);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_GET_HEAD:
		{
			// key
			tb_char_t const* key = (tb_char_t const*)tb_va_arg(args, tb_char_t const*);
			tb_assert_and_check_return_val(key, tb_false);

			// pval
			tb_char_t const** pval = (tb_char_t const**)tb_va_arg(args, tb_char_t const**);
			tb_assert_and_check_return_val(pval, tb_false);
	
			// get head
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_GET_HEAD, key, pval);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_SET_HEAD_FUNC:
		{
			// head_func
			tb_http_head_func_t head_func = (tb_http_head_func_t)tb_va_arg(args, tb_http_head_func_t);

			// set head_func
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_SET_HEAD_FUNC, head_func);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_GET_HEAD_FUNC:
		{
			// phead_func
			tb_http_head_func_t* phead_func = (tb_http_head_func_t*)tb_va_arg(args, tb_http_head_func_t*);
			tb_assert_and_check_return_val(phead_func, tb_false);

			// get head_func
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_GET_HEAD_FUNC, phead_func);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_SET_HEAD_PRIV:
		{
			// head_priv
			tb_pointer_t head_priv = (tb_pointer_t)tb_va_arg(args, tb_pointer_t);

			// set head_priv
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_SET_HEAD_PRIV, head_priv);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_GET_HEAD_PRIV:
		{
			// phead_priv
			tb_pointer_t* phead_priv = (tb_pointer_t*)tb_va_arg(args, tb_pointer_t*);
			tb_assert_and_check_return_val(phead_priv, tb_false);

			// get head_priv
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_GET_HEAD_PRIV, phead_priv);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_SET_RANGE:
		{
			tb_hize_t bof = (tb_hize_t)tb_va_arg(args, tb_hize_t);
			tb_hize_t eof = (tb_hize_t)tb_va_arg(args, tb_hize_t);
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_SET_RANGE, bof, eof);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_GET_RANGE:
		{
			// pbof
			tb_hize_t* pbof = (tb_hize_t*)tb_va_arg(args, tb_hize_t*);
			tb_assert_and_check_return_val(pbof, tb_false);

			// peof
			tb_hize_t* peof = (tb_hize_t*)tb_va_arg(args, tb_hize_t*);
			tb_assert_and_check_return_val(peof, tb_false);

			// ok
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_GET_RANGE, pbof, peof);
		}
		break;
	case TB_ASTREAM_CTRL_SET_SSL:
		{
			// bssl
			tb_bool_t bssl = (tb_bool_t)tb_va_arg(args, tb_bool_t);
	
			// set ssl
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_SET_SSL, bssl);
		}
		break;
	case TB_ASTREAM_CTRL_GET_SSL:
		{
			// pssl
			tb_bool_t* pssl = (tb_bool_t*)tb_va_arg(args, tb_bool_t*);
			tb_assert_and_check_return_val(pssl, tb_false);

			// get ssl
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_GET_SSL, pssl);
		}
		break;
	case TB_ASTREAM_CTRL_SET_TIMEOUT:
		{
			// timeout
			tb_size_t timeout = (tb_size_t)tb_va_arg(args, tb_size_t);
			tb_assert_and_check_return_val(timeout, tb_false);
	
			// set timeout
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_SET_TIMEOUT, timeout);
		}
		break;
	case TB_ASTREAM_CTRL_GET_TIMEOUT:
		{
			// ptimeout
			tb_size_t* ptimeout = (tb_size_t*)tb_va_arg(args, tb_size_t*);
			tb_assert_and_check_return_val(ptimeout, tb_false);
	
			// get timeout
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_GET_TIMEOUT, ptimeout);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_SET_POST_SIZE:
		{
			// post
			tb_hize_t post = (tb_hize_t)tb_va_arg(args, tb_hize_t);

			// set post
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_SET_POST_SIZE, post);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_GET_POST_SIZE:
		{
			// ppost
			tb_hize_t* ppost = (tb_hize_t*)tb_va_arg(args, tb_hize_t*);
			tb_assert_and_check_return_val(ppost, tb_false);

			// get post
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_GET_POST_SIZE, ppost);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_SET_AUTO_UNZIP:
		{
			// bunzip
			tb_bool_t bunzip = (tb_bool_t)tb_va_arg(args, tb_bool_t);

			// set bunzip
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_SET_AUTO_UNZIP, bunzip);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_GET_AUTO_UNZIP:
		{
			// pbunzip
			tb_bool_t* pbunzip = (tb_bool_t*)tb_va_arg(args, tb_bool_t*);
			tb_assert_and_check_return_val(pbunzip, tb_false);

			// get bunzip
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_GET_AUTO_UNZIP, pbunzip);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_SET_REDIRECT:
		{
			// redirect
			tb_size_t redirect = (tb_size_t)tb_va_arg(args, tb_size_t);

			// set redirect
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_SET_REDIRECT, redirect);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_GET_REDIRECT:
		{
			// predirect
			tb_size_t* predirect = (tb_size_t*)tb_va_arg(args, tb_size_t*);
			tb_assert_and_check_return_val(predirect, tb_false);

			// get redirect
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_GET_REDIRECT, predirect);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_SET_VERSION:
		{
			// version
			tb_size_t version = (tb_size_t)tb_va_arg(args, tb_size_t);

			// set version
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_SET_VERSION, version);
		}
		break;
	case TB_ASTREAM_CTRL_HTTP_GET_VERSION:
		{
			// pversion
			tb_size_t* pversion = (tb_size_t*)tb_va_arg(args, tb_size_t*);
			tb_assert_and_check_return_val(pversion, tb_false);

			// get version
			return tb_aicp_http_option(hstream->http, TB_HTTP_OPTION_GET_VERSION, pversion);
		}
		break;
	default:
		break;
	}
	return tb_false;
}

/* ///////////////////////////////////////////////////////////////////////
 * interfaces
 */
tb_astream_t* tb_astream_init_http(tb_aicp_t* aicp)
{
	// check
	tb_assert_and_check_return_val(aicp, tb_null);

	// make stream
	tb_astream_http_t* hstream = (tb_astream_http_t*)tb_malloc0(sizeof(tb_astream_http_t));
	tb_assert_and_check_return_val(hstream, tb_null);

	// init stream
	if (!tb_astream_init((tb_astream_t*)hstream, aicp, TB_ASTREAM_TYPE_HTTP)) goto fail;
	hstream->base.open 		= tb_astream_http_open;
	hstream->base.read 		= tb_astream_http_read;
	hstream->base.writ 		= tb_astream_http_writ;
	hstream->base.seek 		= tb_astream_http_seek;
	hstream->base.sync 		= tb_astream_http_sync;
	hstream->base.task 		= tb_astream_http_task;
	hstream->base.kill 		= tb_astream_http_kill;
	hstream->base.clos 		= tb_astream_http_clos;
	hstream->base.exit 		= tb_astream_http_exit;
	hstream->base.ctrl 		= tb_astream_http_ctrl;

	// ok
	return (tb_astream_t*)hstream;

fail:
	if (hstream) tb_astream_exit((tb_astream_t*)hstream, tb_false);
	return tb_null;
}
tb_astream_t* tb_astream_init_from_http(tb_aicp_t* aicp, tb_char_t const* host, tb_size_t port, tb_char_t const* path, tb_bool_t bssl)
{
	// check
	tb_assert_and_check_return_val(aicp && host && port && path, tb_null);

	// init http stream
	tb_astream_t* hstream = tb_astream_init_http(aicp);
	tb_assert_and_check_return_val(hstream, tb_null);

	// ctrl
	if (!tb_astream_ctrl(hstream, TB_ASTREAM_CTRL_SET_HOST, host)) goto fail;
	if (!tb_astream_ctrl(hstream, TB_ASTREAM_CTRL_SET_PORT, port)) goto fail;
	if (!tb_astream_ctrl(hstream, TB_ASTREAM_CTRL_SET_PATH, path)) goto fail;
	if (!tb_astream_ctrl(hstream, TB_ASTREAM_CTRL_SET_SSL, bssl)) goto fail;
	
	// ok
	return hstream;
fail:
	if (hstream) tb_astream_exit(hstream, tb_false);
	return tb_null;
}
