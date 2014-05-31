#include <stdio.h>
#include <dlog.h>
#include <service_app.h>

#undef LOG_TAG
#define LOG_TAG "SERVICE_APP_TEST"

static bool __service_app_create(void *data)
{
	LOGD("__service_app_create");

	// Todo: add your code here.

	return true;
}

static void __service_app_terminate(void *data)
{
	LOGD("__service_app_terminate");

	// Todo: add your code here.

	return;
}

static void __service_app_service(service_h service, void *data)
{
	LOGD("__service_app_service");
	char *test_data = (char*)data;

	LOGD("requset : %s", test_data);

	// Todo: add your code here.

	return;
}

static void __service_app_low_memory_cb(void *data)
{
	LOGD("__service_app_low_memory_cb");

	// Todo: add your code here.

	service_app_exit();

	return;
}

static void __service_app_low_battery_cb(void *data)
{
	LOGD("__service_app_low_memory_cb");

	// Todo: add your code here.

	service_app_exit();

	return;
}

int main(int argc, char* argv[])
{
	char ad[50] = "test data";
	service_app_event_callback_s ops;

	LOGD("main");
	memset(&ops, 0x00, sizeof(service_app_event_callback_s));

	ops.create = (service_app_create_cb)__service_app_create;
	ops.terminate = (service_app_terminate_cb)__service_app_terminate;
	ops.service = (service_app_service_cb)__service_app_service;
	ops.low_memory = (service_app_low_memory_cb)__service_app_low_memory_cb;
	ops.low_battery = (service_app_low_battery_cb)__service_app_low_battery_cb;

	return service_app_main(argc, argv, &ops, ad);
}

