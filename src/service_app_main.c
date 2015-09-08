/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdlib.h>

#include <bundle.h>
#include <aul.h>
#include <dlog.h>
#include <vconf-internal-keys.h>
#include <app_common.h>

#include <Eina.h>

#include <appcore-agent.h>
#include "service_app_private.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "CAPI_APPFW_APPLICATION"

typedef enum {
	SERVICE_APP_STATE_NOT_RUNNING, // The application has been launched or was running but was terminated
	SERVICE_APP_STATE_CREATING, // The application is initializing the resources on service_app_create_cb callback
	SERVICE_APP_STATE_RUNNING, // The application is running in the foreground and background
} service_app_state_e;

typedef struct {
	char *package;
	char *service_app_name;
	service_app_state_e state;
	service_app_event_callback_s *callback;
	void *data;
} svc_app_context_s;

typedef svc_app_context_s *svc_app_context_h;

static int service_app_create(void *data);
static int service_app_terminate(void *data);
static int service_app_reset(app_control_h app_control, void *data);
static int service_app_low_memory(void *event, void *data);
static int service_app_low_battery(void *event, void *data);

static void service_app_set_appcore_event_cb(svc_app_context_h service_app_context);
static void service_app_unset_appcore_event_cb(void);


EXPORT_API int svc_app_main(int argc, char **argv, service_app_event_callback_s *callback, void *user_data)
{
	svc_app_context_s service_app_context = {
		.state = SERVICE_APP_STATE_NOT_RUNNING,
		.callback = callback,
		.data = user_data
	};

	struct agentcore_ops appcore_context = {
		.data = &service_app_context,
		.create = service_app_create,
		.terminate = service_app_terminate,
		.app_control = service_app_reset,
	};

	if (argc <= 0 || argv == NULL || callback == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);
	}

	if (callback->create == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, "service_app_create_cb() callback must be registered");
	}

	if (service_app_context.state != SERVICE_APP_STATE_NOT_RUNNING)
	{
		return service_app_error(APP_ERROR_ALREADY_RUNNING, __FUNCTION__, NULL);
	}

	service_app_context.state = SERVICE_APP_STATE_CREATING;

	appcore_agent_main(argc, argv, &appcore_context);

	return APP_ERROR_NONE;
}

EXPORT_API void svc_app_exit(void)
{
	appcore_agent_terminate();
}

EXPORT_API void service_app_exit_without_restart(void)
{
	appcore_agent_terminate_without_restart();
}

int service_app_create(void *data)
{
	svc_app_context_h service_app_context = data;
	service_app_create_cb create_cb;

	if (service_app_context == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_CONTEXT, __FUNCTION__, NULL);
	}

	service_app_set_appcore_event_cb(service_app_context);

	create_cb = service_app_context->callback->create;

	if (create_cb == NULL || create_cb(service_app_context->data) == false)
	{
		return service_app_error(APP_ERROR_INVALID_CONTEXT, __FUNCTION__, "service_app_create_cb() returns false");
	}

	service_app_context->state = SERVICE_APP_STATE_RUNNING;

	return APP_ERROR_NONE;
}

int service_app_terminate(void *data)
{
	svc_app_context_h service_app_context = data;
	service_app_terminate_cb terminate_cb;

	if (service_app_context == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_CONTEXT, __FUNCTION__, NULL);
	}

	terminate_cb = service_app_context->callback->terminate;

	if (terminate_cb != NULL)
	{
		terminate_cb(service_app_context->data);
	}

	service_app_unset_appcore_event_cb();

	return APP_ERROR_NONE;
}


int service_app_reset(app_control_h app_control, void *data)
{
	svc_app_context_h service_app_context = data;
	service_app_control_cb service_cb;

	if (service_app_context == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_CONTEXT, __FUNCTION__, NULL);
	}

	service_cb = service_app_context->callback->app_control;

	if (service_cb != NULL)
	{
		service_cb(app_control, service_app_context->data);
	}

	return APP_ERROR_NONE;
}


int service_app_low_memory(void *event_info, void *data)
{
	svc_app_context_h service_app_context = data;
	service_app_low_memory_cb low_memory_cb;

	LOGI("service_app_low_memory");

	if (service_app_context == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_CONTEXT, __FUNCTION__, NULL);
	}

	low_memory_cb = service_app_context->callback->low_memory;

	if (low_memory_cb != NULL)
	{
		LOGI("service_app_low_memory: service_app_low_memory_cb() is called");
		low_memory_cb(service_app_context->data);
	}

	return APP_ERROR_NONE;
}

int service_app_low_battery(void *event_info, void *data)
{
	svc_app_context_h service_app_context = data;
	service_app_low_battery_cb low_battery_cb;

	LOGI("service_app_low_battery");

	if (service_app_context == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_CONTEXT, __FUNCTION__, NULL);
	}

	low_battery_cb = service_app_context->callback->low_battery;

	if (low_battery_cb != NULL)
	{
		LOGI("service_app_low_battery: service_app_low_battery_cb() is called");
		low_battery_cb(service_app_context->data);
	}

	return APP_ERROR_NONE;
}

void service_app_set_appcore_event_cb(svc_app_context_h service_app_context)
{
	if (service_app_context->callback->low_memory != NULL)
	{
		appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_LOW_MEMORY, service_app_low_memory, service_app_context);
	}

	if (service_app_context->callback->low_battery != NULL)
	{
		appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_LOW_BATTERY, service_app_low_battery, service_app_context);
	}
}

void service_app_unset_appcore_event_cb(void)
{
	appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_LOW_MEMORY, NULL, NULL);
	appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_LOW_BATTERY, NULL, NULL);
}

#define SERVICE_APP_EVENT_MAX 2
static Eina_List *handler_list[SERVICE_APP_EVENT_MAX] = {NULL, };
static int _initialized = 0;

struct app_event_handler {
	app_event_type_e type;
	app_event_cb cb;
	void *data;
};

struct app_event_info {
	app_event_type_e type;
	void *value;
};

struct service_app_context {
	char *package;
	char *service_app_name;
	service_app_state_e state;
	service_app_lifecycle_callback_s *callback;
	void *data;
};

static void _free_handler_list(void)
{
	int i;
	app_event_handler_h handler;

	for (i = 0; i < SERVICE_APP_EVENT_MAX; i++) {
		EINA_LIST_FREE(handler_list[i], handler)
			free(handler);
	}

	eina_shutdown();
}

static int _service_app_low_memory(void *event_info, void *data)
{
	Eina_List *l;
	app_event_handler_h handler;
	struct app_event_info event;

	LOGI("service_app_low_memory");

	event.type = APP_EVENT_LOW_MEMORY;
	event.value = event_info;

	EINA_LIST_FOREACH(handler_list[APP_EVENT_LOW_MEMORY], l, handler) {
		handler->cb(&event, handler->data);
	}

	return APP_ERROR_NONE;
}

static int _service_app_low_battery(void *event_info, void *data)
{
	Eina_List *l;
	app_event_handler_h handler;
	struct app_event_info event;

	LOGI("service_app_low_battery");

	event.type = APP_EVENT_LOW_BATTERY;
	event.value = event_info;

	EINA_LIST_FOREACH(handler_list[APP_EVENT_LOW_BATTERY], l, handler) {
		handler->cb(&event, handler->data);
	}

	return APP_ERROR_NONE;
}


static void _service_app_set_appcore_event_cb(struct service_app_context *app_context)
{
	appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_LOW_MEMORY, _service_app_low_memory, app_context);
	appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_LOW_BATTERY, _service_app_low_battery, app_context);
}

static void _service_app_unset_appcore_event_cb(void)
{
	appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_LOW_MEMORY, NULL, NULL);
	appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_LOW_BATTERY, NULL, NULL);
}


static int _service_app_create(void *data)
{
	struct service_app_context *app_context = data;
	service_app_create_cb create_cb;

	if (app_context == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_CONTEXT, __FUNCTION__, NULL);
	}

	_service_app_set_appcore_event_cb(app_context);

	create_cb = app_context->callback->create;

	if (create_cb == NULL || create_cb(app_context->data) == false)
	{
		return service_app_error(APP_ERROR_INVALID_CONTEXT, __FUNCTION__, "service_app_create_cb() returns false");
	}

	app_context->state = SERVICE_APP_STATE_RUNNING;

	return APP_ERROR_NONE;
}

static int _service_app_terminate(void *data)
{
	struct service_app_context *app_context = data;
	service_app_terminate_cb terminate_cb;

	if (app_context == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_CONTEXT, __FUNCTION__, NULL);
	}

	terminate_cb = app_context->callback->terminate;

	if (terminate_cb != NULL)
	{
		terminate_cb(app_context->data);
	}

	_service_app_unset_appcore_event_cb();

	if (_initialized)
		_free_handler_list();

	return APP_ERROR_NONE;
}

static int _service_app_reset(app_control_h app_control, void *data)
{
	struct service_app_context *app_context = data;
	service_app_control_cb service_cb;

	if (app_context == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_CONTEXT, __FUNCTION__, NULL);
	}

	service_cb = app_context->callback->app_control;

	if (service_cb != NULL)
	{
		service_cb(app_control, app_context->data);
	}

	return APP_ERROR_NONE;
}

EXPORT_API int service_app_main(int argc, char **argv, service_app_lifecycle_callback_s *callback, void *user_data)
{
	struct service_app_context app_context = {
		.state = SERVICE_APP_STATE_NOT_RUNNING,
		.callback = callback,
		.data = user_data
	};

	struct agentcore_ops appcore_context = {
		.data = &app_context,
		.create = _service_app_create,
		.terminate = _service_app_terminate,
		.app_control = _service_app_reset,
	};

	if (argc <= 0 || argv == NULL || callback == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);
	}

	if (callback->create == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, "service_app_create_cb() callback must be registered");
	}

	if (app_context.state != SERVICE_APP_STATE_NOT_RUNNING)
	{
		return service_app_error(APP_ERROR_ALREADY_RUNNING, __FUNCTION__, NULL);
	}

	app_context.state = SERVICE_APP_STATE_CREATING;

	appcore_agent_main(argc, argv, &appcore_context);

	return APP_ERROR_NONE;
}

EXPORT_API void service_app_exit(void)
{
	appcore_agent_terminate();
}

EXPORT_API int service_app_add_event_handler(app_event_handler_h *event_handler, app_event_type_e event_type, app_event_cb callback, void *user_data)
{
	app_event_handler_h handler;
	Eina_List *l_itr;

	if (!_initialized) {
		eina_init();
		_initialized = 1;
	}

	if (event_handler == NULL || callback == NULL)
		return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	if (event_type < APP_EVENT_LOW_MEMORY || event_type > APP_EVENT_LOW_BATTERY)
		return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	EINA_LIST_FOREACH(handler_list[event_type], l_itr, handler) {
		if (handler->cb == callback)
			return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);
	}

	handler = calloc(1, sizeof(struct app_event_handler));
	if (!handler)
		return service_app_error(APP_ERROR_OUT_OF_MEMORY, __FUNCTION__, NULL);

	handler->type = event_type;
	handler->cb = callback;
	handler->data = user_data;
	handler_list[event_type] = eina_list_append(handler_list[event_type], handler);

	*event_handler = handler;

	return APP_ERROR_NONE;
}

EXPORT_API int service_app_remove_event_handler(app_event_handler_h event_handler)
{
	app_event_handler_h handler;
	app_event_type_e type;
	Eina_List *l_itr;
	Eina_List *l_next;

	if (event_handler == NULL)
		return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	if (!_initialized) {
		LOGI("handler list is not initialzed");
		return APP_ERROR_NONE;
	}

	type = event_handler->type;
	if (type < APP_EVENT_LOW_MEMORY || type > APP_EVENT_LOW_BATTERY)
		return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	EINA_LIST_FOREACH_SAFE(handler_list[type], l_itr, l_next, handler) {
		if (handler == event_handler) {
			free(handler);
			handler_list[type] = eina_list_remove_list(handler_list[type], l_itr);
			return APP_ERROR_NONE;
		}
	}

	return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, "cannot find such handler");
}
