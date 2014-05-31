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


#ifndef __TIZEN_APPFW_SERVICE_APP_H__
#define __TIZEN_APPFW_SERVICE_APP_H__

#include <tizen.h>
#include <app_service.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup CAPI_APPLICATION_MODULE
 * @{
 */


/**
 * @brief Enumerations of error code for Application.
 */
typedef enum
{
	SERVICE_APP_ERROR_NONE = TIZEN_ERROR_NONE, /**< Successful */
	SERVICE_APP_ERROR_INVALID_PARAMETER = TIZEN_ERROR_INVALID_PARAMETER, /**< Invalid parameter */
	SERVICE_APP_ERROR_OUT_OF_MEMORY = TIZEN_ERROR_OUT_OF_MEMORY, /**< Out of memory */
	SERVICE_APP_ERROR_INVALID_CONTEXT = TIZEN_ERROR_NOT_PERMITTED, /**< Invalid application context */
	SERVICE_APP_ERROR_NO_SUCH_FILE = TIZEN_ERROR_NO_SUCH_FILE, /**< No such file or directory */
	SERVICE_APP_ERROR_ALREADY_RUNNING = TIZEN_ERROR_ALREADY_IN_PROGRESS, /**< Application is already running */
} service_app_error_e;


/**
 * @brief Called at the start of the agent application.
 *
 * @param[in]	user_data	The user data passed from the callback registration function
 * @return @c true on success, otherwise @c false
 * @pre	service_app_main() will invoke this callback function.
 * @see service_app_main()
 * @see #service_app_event_callback_s
 */
typedef bool (*service_app_create_cb) (void *user_data);


/**
 * @brief   Called once after the main loop of agent application exits.
 *
 * @param[in]	user_data	The user data passed from the callback registration function
 * @see	service_app_main()
 * @see #service_app_event_callback_s
 */
typedef void (*service_app_terminate_cb) (void *user_data);


/**
 * @brief Called when other application send the launch request to the agent application.
 *
 * @param[in]	service	The handle to the service
 * @param[in]	user_data	The user data passed from the callback registration function
 * @see service_app_main()
 * @see #service_app_event_callback_s
 * @see @ref CAPI_SERVICE_MODULE API
 */
typedef void (*service_app_service_cb) (service_h service, void *user_data);


/**
 * @brief   Called when the system memory is running low.
 *
 * @param[in]	user_data	The user data passed from the callback registration function
 * @see	service_app_main()
 * @see #service_app_event_callback_s
 */
typedef void (*service_app_low_memory_cb) (void *user_data);


/**
 * @brief   Called when the battery power is running low.
 *
 * @param[in]	user_data	The user data passed from the callback registration function
 * @see	service_app_main()
 * @see #service_app_event_callback_s
 */
typedef void (*service_app_low_battery_cb) (void *user_data);


/**
 * @brief The structure type to contain the set of callback functions for handling application events.
 * @details It is one of the input parameters of the service_app_efl_main() function.
 *
 * @see service_app_main()
 * @see service_app_create_cb()
 * @see service_app_terminate_cb()
 * @see service_app_service_cb()
 * @see service_app_low_memory_cb()
 * @see service_app_low_battery_cb()
 */
typedef struct
{
	service_app_create_cb create; /**< This callback function is called at the start of the application. */
	service_app_terminate_cb terminate; /**< This callback function is called once after the main loop of application exits. */
	service_app_service_cb service; /**< This callback function is called when other application send the launch request to the application. */
	service_app_low_memory_cb low_memory; /**< The registered callback function is called when the system runs low on memory. */
	service_app_low_battery_cb low_battery; /**< The registered callback function is called when battery is low. */
} service_app_event_callback_s;


/**
 * @brief Runs the main loop of application until service_app_exit() is called
 *
 * @param [in] argc The argument count
 * @param [in] argv The argument vector
 * @param [in] callback The set of callback functions to handle application events
 * @param [in] user_data The user data to be passed to the callback functions
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #SERVICE_APP_ERROR_NONE Successful
 * @retval #SERVICE_APP_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #SERVICE_APP_ERROR_INVALID_CONTEXT The application is illegally launched, not launched by the launch system.
 * @retval #SERVICE_APP_ERROR_ALREADY_RUNNING The main loop already starts
 *
 * @see service_app_create_cb()
 * @see service_app_terminate_cb()
 * @see service_app_service_cb()
 * @see service_app_low_memory_cb()
 * @see service_app_low_battery_cb()
 * @see service_app_exit()
 * @see #service_app_event_callback_s
 */
int service_app_main(int argc, char **argv, service_app_event_callback_s *callback, void *user_data);


/**
 * @brief Exits the main loop of application.
 *
 * @details The main loop of application stops and service_app_terminate_cb() is invoked
 * @see service_app_main()
 * @see service_app_terminate_cb()
 */
void service_app_exit(void);

void service_app_exit_without_restart(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_APPFW_SERVICE_APP_H__ */
