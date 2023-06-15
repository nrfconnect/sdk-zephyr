/*
 * Copyright (c) 2018-2021 mcumgr authors
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/slist.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/device.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/mgmt/handlers.h>
#include <zephyr/sys/iterable_sections.h>
#include <string.h>

#ifdef CONFIG_MCUMGR_MGMT_NOTIFICATION_HOOKS
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#endif

static sys_slist_t mgmt_group_list =
	SYS_SLIST_STATIC_INIT(&mgmt_group_list);

#if defined(CONFIG_MCUMGR_MGMT_NOTIFICATION_HOOKS)
static sys_slist_t mgmt_callback_list =
	SYS_SLIST_STATIC_INIT(&mgmt_callback_list);
#endif

void
mgmt_unregister_group(struct mgmt_group *group)
{
	(void)sys_slist_find_and_remove(&mgmt_group_list, &group->node);
}

const struct mgmt_handler *
mgmt_find_handler(uint16_t group_id, uint16_t command_id)
{
	struct mgmt_group *group = NULL;
	sys_snode_t *snp, *sns;

	/*
	 * Find the group with the specified group id, if one exists
	 * check the handler for the command id and make sure
	 * that is not NULL. If that is not set, look for the group
	 * with a command id that is set
	 */
	SYS_SLIST_FOR_EACH_NODE_SAFE(&mgmt_group_list, snp, sns) {
		struct mgmt_group *loop_group =
			CONTAINER_OF(snp, struct mgmt_group, node);
		if (loop_group->mg_group_id == group_id) {
			if (command_id >= loop_group->mg_handlers_count) {
				break;
			}

			if (!loop_group->mg_handlers[command_id].mh_read &&
				!loop_group->mg_handlers[command_id].mh_write) {
				continue;
			}

			group = loop_group;
			break;
		}
	}

	if (group == NULL) {
		return NULL;
	}

	return &group->mg_handlers[command_id];
}

void
mgmt_register_group(struct mgmt_group *group)
{
	sys_slist_append(&mgmt_group_list, &group->node);
}

#if defined(CONFIG_MCUMGR_MGMT_NOTIFICATION_HOOKS)
void mgmt_callback_register(struct mgmt_callback *callback)
{
	sys_slist_append(&mgmt_callback_list, &callback->node);
}

void mgmt_callback_unregister(struct mgmt_callback *callback)
{
	(void)sys_slist_find_and_remove(&mgmt_callback_list, &callback->node);
}

enum mgmt_cb_return mgmt_callback_notify(uint32_t event, void *data, size_t data_size,
					 int32_t *ret_rc, uint16_t *ret_group)
{
	sys_snode_t *snp, *sns;
	bool failed = false;
	bool abort_more = false;
	uint16_t group = MGMT_EVT_GET_GROUP(event);
	enum mgmt_cb_return return_status = MGMT_CB_OK;

	*ret_rc = MGMT_ERR_EOK;
	*ret_group = 0;

	/*
	 * Search through the linked list for entries that have registered for this event and
	 * notify them, the first handler to return an error code will have this error returned
	 * to the calling function, errors returned by additional handlers will be ignored. If
	 * all notification handlers return MGMT_ERR_EOK then access will be allowed and no error
	 * will be returned to the calling function. The status of if a previous handler has
	 * returned an error is provided to the functions through the failed variable, and a
	 * handler function can set abort_more to true to prevent calling any further handlers.
	 */
	SYS_SLIST_FOR_EACH_NODE_SAFE(&mgmt_callback_list, snp, sns) {
		struct mgmt_callback *loop_group =
			CONTAINER_OF(snp, struct mgmt_callback, node);

		if (loop_group->event_id == MGMT_EVT_OP_ALL ||
		    (MGMT_EVT_GET_GROUP(loop_group->event_id) == group &&
		     (MGMT_EVT_GET_ID(event) & MGMT_EVT_GET_ID(loop_group->event_id)) ==
		     MGMT_EVT_GET_ID(event))) {
			int32_t cached_rc = *ret_rc;
			uint16_t cached_group = *ret_group;
			enum mgmt_cb_return status;

			status = loop_group->callback(event, return_status, &cached_rc,
						      &cached_group, &abort_more, data,
						      data_size);

			__ASSERT((status <= MGMT_CB_ERROR_RET),
				 "Invalid status returned by MCUmgr handler: %d", status);

			if (status != MGMT_CB_OK && failed == false) {
				failed = true;
				return_status = status;
				*ret_rc = cached_rc;

				if (status == MGMT_CB_ERROR_RET) {
					*ret_group = cached_group;
				}
			}

			if (abort_more == true) {
				break;
			}
		}
	}

	return return_status;
}
#endif

/* Processes all registered MCUmgr handlers at start up and registers them */
static int mcumgr_handlers_init(void)
{

	STRUCT_SECTION_FOREACH(mcumgr_handler, handler) {
		if (handler->init) {
			handler->init();
		}
	}

	return 0;
}

SYS_INIT(mcumgr_handlers_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
