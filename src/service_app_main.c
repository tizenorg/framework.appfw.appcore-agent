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
#include "service_app_extension.h"

#include <unistd.h>


#ifdef LOG_TAG
#undef LOG_TAG
#endif

#ifndef TIZEN_PATH_MAX
#define TIZEN_PATH_MAX 1024
#endif


#define LOG_TAG "CAPI_APPFW_APPLICATION"

typedef enum {
	SERVICE_APP_STATE_NOT_RUNNING, // The application has been launched or was running but was terminated
	SERVICE_APP_STATE_CREATING, // The application is initializing the resources on service_app_create_cb callback
	SERVICE_APP_STATE_RUNNING, // The application is running in the foreground and background
} service_app_state_e;

static int _service_app_get_id(char **id)
{
	static char id_buf[TIZEN_PATH_MAX] = {0, };
	int ret = -1;

	if (id == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);
	}

	if (id_buf[0] == '\0')
	{
		ret = aul_app_get_appid_bypid(getpid(), id_buf, sizeof(id_buf));

		if (ret < 0) {
			return service_app_error(APP_ERROR_INVALID_CONTEXT, __FUNCTION__, "failed to get the application ID");
		}
	}

	if (id_buf[0] == '\0')
	{
		return service_app_error(APP_ERROR_INVALID_CONTEXT, __FUNCTION__, "failed to get the application ID");
	}

	*id = strdup(id_buf);

	if (*id == NULL)
	{
		return service_app_error(APP_ERROR_OUT_OF_MEMORY, __FUNCTION__, NULL);
	}

	return APP_ERROR_NONE;
}

static int _service_appget_package_app_name(const char *appid, char **name)
{
	char *name_token = NULL;

	if (appid == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);
	}

	// com.vendor.name -> name
	name_token = strrchr(appid, '.');

	if (name_token == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_CONTEXT, __FUNCTION__, NULL);
	}

	name_token++;

	*name = strdup(name_token);

	if (*name == NULL)
	{
		return service_app_error(APP_ERROR_OUT_OF_MEMORY, __FUNCTION__, NULL);
	}

	return APP_ERROR_NONE;
}

EXPORT_API void service_app_exit_without_restart(void)
{
	appcore_agent_terminate_without_restart();
}

#define SERVICE_APP_EVENT_MAX 6
static Eina_List *handler_list[SERVICE_APP_EVENT_MAX] = {NULL, };
static int handler_initialized = 0;
static int appcore_agent_initialized = 0;

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
			if (handler)
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

static int _service_app_lang_changed(void *event_info, void *data)
{
	Eina_List *l;
	app_event_handler_h handler;
	struct app_event_info event;

	LOGI("service_app_lang_changed");

	event.type = APP_EVENT_LANGUAGE_CHANGED;
	event.value = event_info;

	EINA_LIST_FOREACH(handler_list[APP_EVENT_LANGUAGE_CHANGED], l, handler) {
		handler->cb(&event, handler->data);
	}

	return APP_ERROR_NONE;
}

static int _service_app_region_changed(void *event_info, void *data)
{
	Eina_List *l;
	app_event_handler_h handler;
	struct app_event_info event;

	if (event_info == NULL) {
		LOGI("receive empy event, ignore it");
		return APP_ERROR_NONE;
	}

	LOGI("service_app_region_changed");

	event.type = APP_EVENT_REGION_FORMAT_CHANGED;
	event.value = event_info;

	EINA_LIST_FOREACH(handler_list[APP_EVENT_REGION_FORMAT_CHANGED], l, handler) {
		handler->cb(&event, handler->data);
	}

	return APP_ERROR_NONE;
}

static int _service_app_appcore_suspended_state_changed(void *event_info, void *data)
{
	Eina_List *l;
	app_event_handler_h handler;
	struct app_event_info event;

	LOGI("_service_app_appcore_suspended_state_changed");
	LOGD("[__SUSPEND__] suspended state: %d (0: suspend, 1: wake)", *(int*)event_info);

	event.type = APP_EVENT_SUSPENDED_STATE_CHANGED;
	event.value = event_info;

	EINA_LIST_FOREACH(handler_list[APP_EVENT_SUSPENDED_STATE_CHANGED], l, handler) {
		handler->cb(&event, handler->data);
	}

	return APP_ERROR_NONE;
}

static void _service_app_appcore_agent_set_event_cb(app_event_type_e event_type)
{
	switch (event_type) {
	case APP_EVENT_LOW_MEMORY:
		appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_LOW_MEMORY, _service_app_low_memory, NULL);
		break;
	case APP_EVENT_LOW_BATTERY:
		appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_LOW_BATTERY, _service_app_low_battery, NULL);
		break;
	case APP_EVENT_LANGUAGE_CHANGED:
		appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_LANG_CHANGE, _service_app_lang_changed, NULL);
		break;
	case APP_EVENT_REGION_FORMAT_CHANGED:
		appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_REGION_CHANGE, _service_app_region_changed, NULL);
		break;
	case APP_EVENT_SUSPENDED_STATE_CHANGED:
		LOGD("[__SUSPEND__]");
		appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_SUSPENDED_STATE_CHANGE, _service_app_appcore_suspended_state_changed, NULL);
		break;
	default:
		break;
	}
}

static void _service_app_appcore_agent_unset_event_cb(app_event_type_e event_type)
{
	switch (event_type) {
	case APP_EVENT_LOW_MEMORY:
		appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_LOW_MEMORY, NULL, NULL);
		break;
	case APP_EVENT_LOW_BATTERY:
		appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_LOW_BATTERY, NULL, NULL);
		break;
	case APP_EVENT_LANGUAGE_CHANGED:
		appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_LANG_CHANGE, NULL, NULL);
		break;
	case APP_EVENT_REGION_FORMAT_CHANGED:
		appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_REGION_CHANGE, NULL, NULL);
		break;
	case APP_EVENT_SUSPENDED_STATE_CHANGED:
		LOGD("[__SUSPEND__]");
		appcore_agent_set_event_callback(APPCORE_AGENT_EVENT_SUSPENDED_STATE_CHANGE, NULL, NULL);
		break;
	default:
		break;
	}
}

static void _service_app_set_appcore_event_cb(void)
{
	_service_app_appcore_agent_set_event_cb(APP_EVENT_LOW_MEMORY);
	_service_app_appcore_agent_set_event_cb(APP_EVENT_LANGUAGE_CHANGED);
	_service_app_appcore_agent_set_event_cb(APP_EVENT_REGION_FORMAT_CHANGED);

	if (eina_list_count(handler_list[APP_EVENT_LOW_BATTERY]) > 0)
		_service_app_appcore_agent_set_event_cb(APP_EVENT_LOW_BATTERY);

	if (eina_list_count(handler_list[APP_EVENT_SUSPENDED_STATE_CHANGED]) > 0)
		_service_app_appcore_agent_set_event_cb(APP_EVENT_SUSPENDED_STATE_CHANGED);
}

static void _service_app_unset_appcore_event_cb(void)
{
	_service_app_appcore_agent_unset_event_cb(APP_EVENT_LOW_MEMORY);
	_service_app_appcore_agent_unset_event_cb(APP_EVENT_LANGUAGE_CHANGED);
	_service_app_appcore_agent_unset_event_cb(APP_EVENT_REGION_FORMAT_CHANGED);

	if (eina_list_count(handler_list[APP_EVENT_LOW_BATTERY]) > 0)
		_service_app_appcore_agent_unset_event_cb(APP_EVENT_LOW_BATTERY);

	if (eina_list_count(handler_list[APP_EVENT_SUSPENDED_STATE_CHANGED]) > 0)
		_service_app_appcore_agent_unset_event_cb(APP_EVENT_SUSPENDED_STATE_CHANGED);
}

static int _service_app_create(void *data)
{
	struct service_app_context *app_context = data;
	service_app_create_cb create_cb;

	if (app_context == NULL)
	{
		return service_app_error(APP_ERROR_INVALID_CONTEXT, __FUNCTION__, NULL);
	}

	appcore_agent_initialized = 1;
	_service_app_set_appcore_event_cb();

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

	if (handler_initialized)
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


	if (_service_app_get_id(&(app_context.package)) == APP_ERROR_NONE)
	{
		if (_service_appget_package_app_name(app_context.package, &(app_context.service_app_name)) != APP_ERROR_NONE)
		{
			free(app_context.package);
			app_context.package = NULL;
		}
	}

	app_context.state = SERVICE_APP_STATE_CREATING;

	appcore_agent_main(argc, argv, &appcore_context);

	if (app_context.service_app_name)
		free(app_context.service_app_name);
	if (app_context.package)
		free(app_context.package);

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

	if (!handler_initialized) {
		eina_init();
		handler_initialized = 1;
	}

	if (event_handler == NULL || callback == NULL)
		return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, "null parameter");

	if (event_type < APP_EVENT_LOW_MEMORY || event_type > APP_EVENT_SUSPENDED_STATE_CHANGED)
		return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, "invalid event type");

	EINA_LIST_FOREACH(handler_list[event_type], l_itr, handler) {
		if (handler->cb == callback)
			return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, "already registered");
	}

	handler = calloc(1, sizeof(struct app_event_handler));
	if (!handler)
		return service_app_error(APP_ERROR_OUT_OF_MEMORY, __FUNCTION__, "insufficient memory");

	handler->type = event_type;
	handler->cb = callback;
	handler->data = user_data;

	if (appcore_agent_initialized && eina_list_count(handler_list[event_type]) == 0)
		_service_app_appcore_agent_set_event_cb(event_type);

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

	if (!handler_initialized) {
		LOGI("handler list is not initialzed");
		return APP_ERROR_NONE;
	}

	type = event_handler->type;
	if (type < APP_EVENT_LOW_MEMORY
			|| type > APP_EVENT_REGION_FORMAT_CHANGED
			|| type == APP_EVENT_DEVICE_ORIENTATION_CHANGED)
		return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	EINA_LIST_FOREACH_SAFE(handler_list[type], l_itr, l_next, handler) {
		if (handler == event_handler) {
			free(handler);
			handler_list[type] = eina_list_remove_list(handler_list[type], l_itr);

			if (appcore_agent_initialized && eina_list_count(handler_list[type]) == 0)
				_service_app_appcore_agent_unset_event_cb(type);

			return APP_ERROR_NONE;
		}
	}

	return service_app_error(APP_ERROR_INVALID_PARAMETER, __FUNCTION__, "cannot find such handler");
}
