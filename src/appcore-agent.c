/*
 *  service-app-core
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jayoun Lee <airjany@samsung.com>, Sewook Park <sewook7.park@samsung.com>, Jaeho Lee <jaeho81.lee@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


#define _GNU_SOURCE

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <glib.h>

#include "aul.h"
#include "appcore-agent.h"
#include <dlog.h>
#include <vconf.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "APPCORE_AGENT"


#ifndef EXPORT_API
#  define EXPORT_API __attribute__ ((visibility("default")))
#endif

#define _warn_if(expr, fmt, arg...) do { \
		if (expr) { \
			_ERR(fmt, ##arg); \
		} \
	} while (0)

#define _ret_if(expr) do { \
		if (expr) { \
			return; \
		} \
	} while (0)

#define _retv_if(expr, val) do { \
		if (expr) { \
			return (val); \
		} \
	} while (0)

#define _retm_if(expr, fmt, arg...) do { \
		if (expr) { \
			_ERR(fmt, ##arg); \
			return; \
		} \
	} while (0)

#define _retvm_if(expr, val, fmt, arg...) do { \
		if (expr) { \
			_ERR(fmt, ##arg); \
			return (val); \
		} \
	} while (0)


static pid_t _pid;

/**
 * Appcore internal system event
 */
enum sys_event {
	SE_UNKNOWN,
	SE_LOWMEM,
	SE_LOWBAT,
	SE_MAX
};

/**
 * agent internal state
 */
enum agent_state {
	AGS_NONE,
	AGS_CREATED,
	AGS_RUNNING,
	AGS_STOPED,
	AGS_DYING,
};

enum agent_event {
	AGE_UNKNOWN,
	AGE_CREATE,
	AGE_TERMINATE,
	AGE_STOP,
	AGE_REQUEST,
	AGE_MAX
};


static enum appcore_agent_event to_ae[SE_MAX] = {
	APPCORE_AGENT_EVENT_UNKNOWN,	/* SE_UNKNOWN */
	APPCORE_AGENT_EVENT_LOW_MEMORY,	/* SE_LOWMEM */
	APPCORE_AGENT_EVENT_LOW_BATTERY,	/* SE_LOWBAT */
};


enum cb_type {			/* callback */
	_CB_NONE,
	_CB_SYSNOTI,
	_CB_APPNOTI,
	_CB_VCONF,
};

struct evt_ops {
	enum cb_type type;
	union {
		enum appcore_agent_event sys;
		enum agent_event app;
		const char *vkey;
	} key;

	int (*cb_pre) (void *);
	int (*cb) (void *);
	int (*cb_post) (void *);

	int (*vcb_pre) (void *, void *);
	int (*vcb) (void *, void *);
	int (*vcb_post) (void *, void *);
};

struct agent_priv {
	enum agent_state state;

	struct agentcore_ops *ops;
};

static struct agent_priv priv;

struct agent_ops {
	void *data;
	void (*cb_app)(int, void *, bundle *);
};

/**
 * Appcore system event operation
 */
struct sys_op {
	int (*func) (void *);
	void *data;
};

struct agent_appcore {
	int state;

	const struct agent_ops *ops;
	struct sys_op sops[SE_MAX];
};

static struct agent_appcore core;

GMainLoop *mainloop;

extern int service_create_request(bundle *data, service_h *service);
static int __sys_lowmem_post(void *data, void *evt);
static int __sys_lowmem(void *data, void *evt);
static int __sys_lowbatt(void *data, void *evt);


static struct evt_ops evtops[] = {
	{
	 .type = _CB_VCONF,
	 .key.vkey = VCONFKEY_SYSMAN_LOW_MEMORY,
	 .vcb_post = __sys_lowmem_post,
	 .vcb = __sys_lowmem,
	 },
	{
	 .type = _CB_VCONF,
	 .key.vkey = VCONFKEY_SYSMAN_BATTERY_STATUS_LOW,
	 .vcb = __sys_lowbatt,
	 }
};

static void __do_app(enum agent_event event, void *data, bundle * b)
{
	int r = 0;
	struct agent_priv *svc = data;
	service_h service = NULL;

	_ret_if(svc == NULL);

	if (event == AGE_TERMINATE) {
		svc->state = AGS_DYING;
		g_main_loop_quit(mainloop);
		return;
	}

	_ret_if(svc->ops == NULL);

	if (service_create_event(b, &service) != 0)
	{
		return;
	}

	switch (event) {
	case AGE_REQUEST:
		if (svc->ops->service)
			r = svc->ops->service(service, svc->ops->data);
		svc->state = AGS_RUNNING;
		break;
/*	case AGE_STOP:
		if(svc->state == AGS_RUNNING) {
			if (svc->ops->stop)
				r = svc->ops->stop(svc->ops->data);
			svc->state = AGS_STOPED;
		}
		break;
*/	default:
		/* do nothing */
		break;
	}
	service_destroy(service);
}

static struct agent_ops s_ops = {
	.data = &priv,
	.cb_app = __do_app,
};

static int __set_data(struct agent_priv *agent, struct agentcore_ops *ops)
{
	if (ops == NULL) {
		errno = EINVAL;
		return -1;
	}

	agent->ops = ops;

	_pid = getpid();

	return 0;
}

static int __agent_request(void *data, bundle * k)
{
	struct agent_appcore *ac = data;
	_retv_if(ac == NULL || ac->ops == NULL, -1);
	_retv_if(ac->ops->cb_app == NULL, 0);

	ac->ops->cb_app(AGE_REQUEST, ac->ops->data, k);

	return 0;
}

static int __agent_terminate(void *data)
{
	struct agent_appcore *ac = data;

	_retv_if(ac == NULL || ac->ops == NULL, -1);
	_retv_if(ac->ops->cb_app == NULL, 0);

	ac->ops->cb_app(AGE_TERMINATE, ac->ops->data, NULL);

	return 0;
}

static int __sys_do_default(struct appcore *ac, enum sys_event event)
{
	int r;

	switch (event) {
	case SE_LOWBAT:
		/*r = __def_lowbatt(ac);*/
		r = 0;
		break;
	default:
		r = 0;
		break;
	};

	return r;
}

static int __sys_do(struct agent_appcore *ac, enum sys_event event)
{
	struct sys_op *op;

	_retv_if(ac == NULL || event >= SE_MAX, -1);

	op = &ac->sops[event];

	if (op->func == NULL)
		return __sys_do_default(ac, event);

	return op->func(op->data);
}

static int __sys_lowmem_post(void *data, void *evt)
{
#if defined(MEMORY_FLUSH_ACTIVATE)
	struct appcore *ac = data;
	ac->ops->cb_app(AE_LOWMEM_POST, ac->ops->data, NULL);
#else
	malloc_trim(0);
#endif
	return 0;
}

static int __sys_lowmem(void *data, void *evt)
{
	return __sys_do(data, SE_LOWMEM);
}

static int __sys_lowbatt(void *data, void *evt)
{
	keynode_t *key = evt;
	int val;

	val = vconf_keynode_get_int(key);

	/* VCONFKEY_SYSMAN_BAT_CRITICAL_LOW or VCONFKEY_SYSMAN_POWER_OFF */
	if (val <= VCONFKEY_SYSMAN_BAT_CRITICAL_LOW)
		return __sys_do(data, SE_LOWBAT);

	return 0;
}

static void __vconf_do(struct evt_ops *eo, keynode_t * key, void *data)
{
	_ret_if(eo == NULL);

	if (eo->vcb_pre)
		eo->vcb_pre(data, key);

	if (eo->vcb)
		eo->vcb(data, key);

	if (eo->vcb_post)
		eo->vcb_post(data, key);
}

static void __vconf_cb(keynode_t *key, void *data)
{
	int i;
	const char *name;

	name = vconf_keynode_get_name(key);
	_ret_if(name == NULL);

	SECURE_LOGD("[APP %d] vconf changed: %s", _pid, name);

	for (i = 0; i < sizeof(evtops) / sizeof(evtops[0]); i++) {
		struct evt_ops *eo = &evtops[i];

		switch (eo->type) {
		case _CB_VCONF:
			if (!strcmp(name, eo->key.vkey))
				__vconf_do(eo, key, data);
			break;
		default:
			/* do nothing */
			break;
		}
	}
}

static int __add_vconf(struct agent_appcore *ac)
{
	int i;
	int r;

	for (i = 0; i < sizeof(evtops) / sizeof(evtops[0]); i++) {
		struct evt_ops *eo = &evtops[i];

		switch (eo->type) {
		case _CB_VCONF:
			r = vconf_notify_key_changed(eo->key.vkey, __vconf_cb,
						     ac);
			break;
		default:
			/* do nothing */
			break;
		}
	}

	return 0;
}

static int __del_vconf(void)
{
	int i;
	int r;

	for (i = 0; i < sizeof(evtops) / sizeof(evtops[0]); i++) {
		struct evt_ops *eo = &evtops[i];

		switch (eo->type) {
		case _CB_VCONF:
			r = vconf_ignore_key_changed(eo->key.vkey, __vconf_cb);
			break;
		default:
			/* do nothing */
			break;
		}
	}

	return 0;
}

static int __aul_handler(aul_type type, bundle *b, void *data)
{
	int ret;

	switch (type) {
	case AUL_START:
		ret = __agent_request(data, b);
		break;
	case AUL_RESUME:
		break;
/*	case AUL_STOP:
		ret = __service_stop(data);
		break;
*/
	case AUL_TERMINATE:
		ret = __agent_terminate(data);
		break;
	default:
		/* do nothing */
		break;
	}

	return 0;
}

EXPORT_API int appcore_agent_set_event_callback(enum appcore_agent_event event,
					  int (*cb) (void *), void *data)
{
	struct agent_appcore *ac = &core;
	struct sys_op *op;
	enum sys_event se;

	for (se = SE_UNKNOWN; se < SE_MAX; se++) {
		if (event == to_ae[se])
			break;
	}

	if (se == SE_UNKNOWN || se >= SE_MAX) {
		_ERR("Unregistered event");
		errno = EINVAL;
		return -1;
	}

	op = &ac->sops[se];

	op->func = cb;
	op->data = data;

	return 0;
}

EXPORT_API int appcore_agent_init(const struct agent_ops *ops,
			    int argc, char **argv)
{
	int r;

	if (core.state != 0) {
		errno = EALREADY;
		return -1;
	}

	if (ops == NULL || ops->cb_app == NULL) {
		errno = EINVAL;
		return -1;
	}

	r = __add_vconf(&core);
	if (r == -1) {
		_ERR("Add vconf callback failed");
		goto err;
	}

	r = aul_launch_init(__aul_handler, &core);
	if (r < 0) {
		goto err;
	}

	r = aul_launch_argv_handler(argc, argv);
	if (r < 0) {
		goto err;
	}

	core.ops = ops;
	core.state = 1;		/* TODO: use enum value */

	return 0;
 err:
	//__clear(&core);
	return -1;
}


static int __before_loop(struct agent_priv *agent, int argc, char **argv)
{
	int r;

	if (argc == NULL || argv == NULL) {
		errno = EINVAL;
		return -1;
	}

	g_type_init();

	r = appcore_agent_init(&s_ops, argc, argv);
	_retv_if(r == -1, -1);

	if (agent->ops && agent->ops->create) {
		r = agent->ops->create(agent->ops->data);
		if (r == -1) {
			//appcore_exit();
			errno = ECANCELED;
			return -1;
		}
	}
	agent->state = AGS_CREATED;

	return 0;
}

static void __after_loop(struct agent_priv *agent)
{
	priv.state = AGS_DYING;
	if (agent->ops && agent->ops->terminate)
		agent->ops->terminate(agent->ops->data);
}


EXPORT_API int appcore_agent_terminate()
{
	g_main_loop_quit(mainloop);
	return 0;
}


EXPORT_API int appcore_agent_main(int argc, char **argv,
				struct agentcore_ops *ops)
{
	int r;

	r = __set_data(&priv, ops);
	_retv_if(r == -1, -1);

	r = __before_loop(&priv, argc, argv);
	if (r == -1) {
		//__unset_data(&priv);
		return -1;
	}

	mainloop = g_main_loop_new(NULL, FALSE);

	g_main_loop_run(mainloop);


	__after_loop(&priv);

	//__unset_data(&priv);

	return 0;
}
