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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <bundle.h>
#include <aul.h>
#include <dlog.h>

#include <appcore-agent.h>
#include <service_app_private.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "TIZEN_N_AGENT"

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
} service_app_context_s;

typedef service_app_context_s *service_app_context_h;

static int service_app_create(void *data);
static int service_app_terminate(void *data);
static int service_app_reset(service_h service, void *data);
static int service_app_low_memory(void *data);
static int service_app_low_battery(void *data);

static void service_app_set_appcore_event_cb(service_app_context_h service_app_context);
static void service_app_unset_appcore_event_cb(void);


EXPORT_API int service_app_main(int argc, char **argv, service_app_event_callback_s *callback, void *user_data)
{
	service_app_context_s service_app_context = {
		.state = SERVICE_APP_STATE_NOT_RUNNING,
		.callback = callback,
		.data = user_data
	};

	struct agentcore_ops appcore_context = {
		.data = &service_app_context,
		.create = service_app_create,
		.terminate = service_app_terminate,
		.service = service_app_reset,
	};

	if (argc <= 0 || argv == NULL || callback == NULL)
	{
		return service_app_error(SERVICE_APP_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);
	}

	if (callback->create == NULL)
	{
		return service_app_error(SERVICE_APP_ERROR_INVALID_PARAMETER, __FUNCTION__, "service_app_create_cb() callback must be registered");
	}

	if (service_app_context.state != SERVICE_APP_STATE_NOT_RUNNING)
	{
		return service_app_error(SERVICE_APP_ERROR_ALREADY_RUNNING, __FUNCTION__, NULL);
	}

	service_app_context.state = SERVICE_APP_STATE_CREATING;

	appcore_agent_main(argc, argv, &appcore_context);

	return SERVICE_APP_ERROR_NONE;
}


EXPORT_API void service_app_exit(void)
{
	appcore_agent_terminate();
}

EXPORT_API void service_app_exit_without_restart(void)
{
	appcore_agent_terminate_without_restart();
}

int service_app_create(void *data)
{
	service_app_context_h service_app_context = data;
	service_app_create_cb create_cb;

	if (service_app_context == NULL)
	{
		return service_app_error(SERVICE_APP_ERROR_INVALID_CONTEXT, __FUNCTION__, NULL);
	}

	service_app_set_appcore_event_cb(service_app_context);

	create_cb = service_app_context->callback->create;

	if (create_cb == NULL || create_cb(service_app_context->data) == false)
	{
		return service_app_error(SERVICE_APP_ERROR_INVALID_CONTEXT, __FUNCTION__, "service_app_create_cb() returns false");
	}

	service_app_context->state = SERVICE_APP_STATE_RUNNING;

	return SERVICE_APP_ERROR_NONE;
}

int service_app_terminate(void *data)
{
	service_app_context_h service_app_context = data;
	service_app_terminate_cb terminate_cb;

	if (service_app_context == NULL)
	{
		return service_app_error(SERVICE_APP_ERROR_INVALID_CONTEXT, __FUNCTION__, NULL);
	}

	terminate_cb = service_app_context->callback->terminate;

	if (terminate_cb != NULL)
	{
		terminate_cb(service_app_context->data);
	}

	service_app_unset_appcore_event_cb();

	return SERVICE_APP_ERROR_NONE;
}


int service_app_reset(service_h service, void *data)
{
	service_app_context_h service_app_context = data;
	service_app_service_cb service_cb;

	if (service_app_context == NULL)
	{
		return service_app_error(SERVICE_APP_ERROR_INVALID_CONTEXT, __FUNCTION__, NULL);
	}

	service_cb = service_app_context->callback->service;

	if (service_cb != NULL)
	{
		service_cb(service, service_app_context->data);
	}

	return SERVICE_APP_ERROR_NONE;
}


int service_app_low_memory(void *data)
{
	service_app_context_h service_app_context = data;
	service_app_low_memory_cb low_memory_cb;

	if (service_app_context == NULL)
	{
		return service_app_error(SERVICE_APP_ERROR_INVALID_CONTEXT, __FUNCTION__, NULL);
	}

	low_memory_cb = service_app_context->callback->low_memory;

	if (low_memory_cb != NULL)
	{
		low_memory_cb(service_app_context->data);
	}

	return SERVICE_APP_ERROR_NONE;
}

int service_app_low_battery(void *data)
{
	service_app_context_h service_app_context = data;
	service_app_low_battery_cb low_battery_cb;

	if (service_app_context == NULL)
	{
		return service_app_error(SERVICE_APP_ERROR_INVALID_CONTEXT, __FUNCTION__, NULL);
	}

	low_battery_cb = service_app_context->callback->low_battery;

	if (low_battery_cb != NULL)
	{
		low_battery_cb(service_app_context->data);
	}

	return SERVICE_APP_ERROR_NONE;
}

void service_app_set_appcore_event_cb(service_app_context_h service_app_context)
{
	if (service_app_context->callback->low_memory != NULL)
	{
		//appcore_set_event_callback(APPCORE_EVENT_LOW_MEMORY, service_app_appcore_low_memory, service_app_context);
	}

	if (service_app_context->callback->low_battery != NULL)
	{
		//appcore_set_event_callback(APPCORE_EVENT_LOW_BATTERY, service_app_appcore_low_battery, service_app_context);
	}
}

void service_app_unset_appcore_event_cb(void)
{
	//appcore_set_event_callback(APPCORE_EVENT_LOW_MEMORY, NULL, NULL);
	//appcore_set_event_callback(APPCORE_EVENT_LOW_BATTERY, NULL, NULL);
}
