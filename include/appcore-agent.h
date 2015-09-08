/*
 *  app-core
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



#ifndef __AGENT_APPCORE_H__
#define __AGENT_APPCORE_H__


#include <libintl.h>
#include <bundle.h>

#include "app_control.h"


#ifdef __cplusplus
extern "C" {
#endif

struct agentcore_ops {
	void *data;
	    /**< Callback data */
	int (*create) (void *); /**< This callback function is called at the start of the application. */
	int (*terminate) (void *); /**< This callback function is called once after the main loop of application exits. */
	int (*app_control) (app_control_h, void *); /**< This callback function is called when other application send the launch request to the application. */

	void *reserved[6];
		   /**< Reserved */
};

enum appcore_agent_event {
	APPCORE_AGENT_EVENT_UNKNOWN,
		       /**< Unknown event */
	APPCORE_AGENT_EVENT_LOW_MEMORY,
			  /**< Low memory */
	APPCORE_AGENT_EVENT_LOW_BATTERY,
			   /**< Low battery */
};

int appcore_agent_main(int argc, char **argv, struct agentcore_ops *ops);

int appcore_agent_terminate();

int appcore_agent_terminate_without_restart();

int appcore_agent_set_event_callback(enum appcore_agent_event event,
					  int (*cb) (void *, void *), void *data);

#ifdef __cplusplus
}
#endif

#endif				/* __AGENT_APPCORE_H__ */
