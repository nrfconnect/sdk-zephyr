/**
 * Copyright (c) 2021-2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_SUBSYS_BLUETOOTH_AUDIO_CSIP_H_
#define ZEPHYR_SUBSYS_BLUETOOTH_AUDIO_CSIP_H_

/**
 * @brief Coordinated Set Identification Profile (CSIP)
 *
 * @defgroup bt_gatt_csip Coordinated Set Identification Profile (CSIP)
 *
 * @ingroup bluetooth
 * @{
 * *
 * [Experimental] Users should note that the APIs can change as a part of ongoing development.
 */

#include <zephyr/bluetooth/conn.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Recommended timer for member discovery */
#define BT_CSIP_SET_COORDINATOR_DISCOVER_TIMER_VALUE        K_SECONDS(10)

#if defined(CONFIG_BT_CSIP_SET_COORDINATOR)
#define BT_CSIP_SET_COORDINATOR_MAX_CSIS_INSTANCES CONFIG_BT_CSIP_SET_COORDINATOR_MAX_CSIS_INSTANCES
#else
#define BT_CSIP_SET_COORDINATOR_MAX_CSIS_INSTANCES 0
#endif /* CONFIG_BT_CSIP_SET_COORDINATOR */

/** Accept the request to read the SIRK as plaintext */
#define BT_CSIP_READ_SIRK_REQ_RSP_ACCEPT        0x00
/** Accept the request to read the SIRK, but return encrypted SIRK */
#define BT_CSIP_READ_SIRK_REQ_RSP_ACCEPT_ENC    0x01
/** Reject the request to read the SIRK */
#define BT_CSIP_READ_SIRK_REQ_RSP_REJECT        0x02
/** SIRK is available only via an OOB procedure */
#define BT_CSIP_READ_SIRK_REQ_RSP_OOB_ONLY      0x03

/** Size of the Set Identification Resolving Key (SIRK) */
#define BT_CSIP_SET_SIRK_SIZE                   16

/** Size of the Resolvable Set Identifier (RSI) */
#define BT_CSIP_RSI_SIZE                        6

/* Coordinate Set Identification Service Error codes */
/** Service is already locked */
#define BT_CSIP_ERROR_LOCK_DENIED               0x80
/** Service is not locked */
#define BT_CSIP_ERROR_LOCK_RELEASE_DENIED       0x81
/** Invalid lock value */
#define BT_CSIP_ERROR_LOCK_INVAL_VALUE          0x82
/** SIRK only available out-of-band */
#define BT_CSIP_ERROR_SIRK_OOB_ONLY             0x83
/** Client is already owner of the lock */
#define BT_CSIP_ERROR_LOCK_ALREADY_GRANTED      0x84

/**
 * @brief Helper to declare bt_data array including RSI
 *
 * This macro is mainly for creating an array of struct bt_data
 * elements which is then passed to e.g. @ref bt_le_ext_adv_start().
 *
 * @param _rsi Pointer to the RSI value
 */
#define BT_CSIP_DATA_RSI(_rsi) BT_DATA(BT_DATA_CSIS_RSI, _rsi, BT_CSIP_RSI_SIZE)

/** @brief Opaque Coordinated Set Identification Service instance. */
struct bt_csip_set_member_svc_inst;

/** Callback structure for the Coordinated Set Identification Service */
struct bt_csip_set_member_cb {
	/**
	 * @brief Callback whenever the lock changes on the server.
	 *
	 * @param conn      The connection to the client that changed the lock.
	 *                  NULL if server changed it, either by calling
	 *                  bt_csip_set_member_lock() or by timeout.
	 * @param svc_inst  Pointer to the Coordinated Set Identification
	 *                  Service.
	 * @param locked    Whether the lock was locked or released.
	 *
	 */
	void (*lock_changed)(struct bt_conn *conn,
			     struct bt_csip_set_member_svc_inst *svc_inst,
			     bool locked);

	/**
	 * @brief Request from a peer device to read the sirk.
	 *
	 * If this callback is not set, all clients will be allowed to read
	 * the SIRK unencrypted.
	 *
	 * @param conn      The connection to the client that requested to read
	 *                  the SIRK.
	 * @param svc_inst  Pointer to the Coordinated Set Identification
	 *                  Service.
	 *
	 * @return A BT_CSIP_READ_SIRK_REQ_RSP_* response code.
	 */
	uint8_t (*sirk_read_req)(struct bt_conn *conn,
				 struct bt_csip_set_member_svc_inst *svc_inst);
};

/** Register structure for Coordinated Set Identification Service */
struct bt_csip_set_member_register_param {
	/**
	 * @brief Size of the set.
	 *
	 * If set to 0, the set size characteristic won't be initialized.
	 */
	uint8_t set_size;

	/**
	 * @brief The unique Set Identity Resolving Key (SIRK)
	 *
	 * This shall be unique between different sets, and shall be the same
	 * for each set member for each set.
	 */
	uint8_t set_sirk[BT_CSIP_SET_SIRK_SIZE];

	/**
	 * @brief Boolean to set whether the set is lockable by clients
	 *
	 * Setting this to false will disable the lock characteristic.
	 */
	bool lockable;

	/**
	 * @brief Rank of this device in this set.
	 *
	 * If the lockable parameter is set to true, this shall be > 0 and
	 * <= to the set_size. If the lockable parameter is set to false, this
	 * may be set to 0 to disable the rank characteristic.
	 */
	uint8_t rank;

	/** Pointer to the callback structure. */
	struct bt_csip_set_member_cb *cb;

#if CONFIG_BT_CSIP_SET_MEMBER_MAX_INSTANCE_COUNT > 1
	/**
	 * @brief Parent service pointer
	 *
	 * Mandatory parent service pointer if this CSIS instance is included
	 * by another service. All CSIS instances when
	 * @kconfig{CONFIG_BT_CSIP_SET_MEMBER_MAX_INSTANCE_COUNT} is above 1
	 * shall be included by another service, as per the
	 * Coordinated Set Identification Profile (CSIP).
	 */
	const struct bt_gatt_service *parent;
#endif /* CONFIG_BT_CSIP_SET_MEMBER_MAX_INSTANCE_COUNT > 1 */
};

/**
 * @brief Get the service declaration attribute.
 *
 * The first service attribute can be included in any other GATT service.
 *
 * @param svc_inst   Pointer to the Coordinated Set Identification Service.
 *
 * @return The first CSIS attribute instance.
 */
void *bt_csip_set_member_svc_decl_get(const struct bt_csip_set_member_svc_inst *svc_inst);

/**
 * @brief Register a Coordinated Set Identification Service instance.
 *
 * This will register and enable the service and make it discoverable by
 * clients.
 *
 * This shall only be done as a server.
 *
 * @param      param     Coordinated Set Identification Service register
 *                       parameters.
 * @param[out] svc_inst  Pointer to the registered Coordinated Set
 *                       Identification Service.
 *
 * @return 0 if success, errno on failure.
 */
int bt_csip_set_member_register(const struct bt_csip_set_member_register_param *param,
				struct bt_csip_set_member_svc_inst **svc_inst);

/**
 * @brief Print the SIRK to the debug output
 *
 * @param svc_inst   Pointer to the Coordinated Set Identification Service.
 */
void bt_csip_set_member_print_sirk(const struct bt_csip_set_member_svc_inst *svc_inst);

/**
 * @brief Generate the Resolvable Set Identifier (RSI) value.
 *
 * This will generate RSI for given @p svc_inst instance.
 *
 * @param svc_inst  Pointer to the Coordinated Set Identification Service.
 * @param rsi       Pointer to the 6-octet newly generated RSI data in little-endian.
 *
 * @return int		0 if on success, errno on error.
 */
int bt_csip_set_member_generate_rsi(const struct bt_csip_set_member_svc_inst *svc_inst,
				    uint8_t rsi[BT_CSIP_RSI_SIZE]);

/**
 * @brief Locks a specific Coordinated Set Identification Service instance on the server.
 *
 * @param svc_inst  Pointer to the Coordinated Set Identification Service.
 * @param lock      If true lock the set, if false release the set.
 * @param force     This argument only have meaning when @p lock is false
 *                  (release) and will force release the lock, regardless of who
 *                  took the lock.
 *
 * @return 0 on success, GATT error on error.
 */
int bt_csip_set_member_lock(struct bt_csip_set_member_svc_inst *svc_inst,
			    bool lock, bool force);

/** Information about a specific set */
struct bt_csip_set_coordinator_set_info {
	/**
	 * @brief The 16 octet set Set Identity Resolving Key (SIRK)
	 *
	 * The Set SIRK may not be exposed by the server over Bluetooth, and
	 * may require an out-of-band solution.
	 */
	uint8_t set_sirk[BT_CSIP_SET_SIRK_SIZE];

	/**
	 * @brief The size of the set
	 *
	 * Will be 0 if not exposed by the server.
	 */
	uint8_t set_size;

	/**
	 * @brief The rank of the set on on the remote device
	 *
	 * Will be 0 if not exposed by the server.
	 */
	uint8_t rank;

	/** Whether or not the set can be locked on this device */
	bool lockable;
};

/**
 * @brief Struct representing a coordinated set instance on a remote device
 *
 * The values in this struct will be populated during discovery of sets
 * (bt_csip_set_coordinator_discover()).
 */
struct bt_csip_set_coordinator_csis_inst {
	struct bt_csip_set_coordinator_set_info info;

	/** Internally used pointer value */
	void *svc_inst;
};

/** Struct representing a remote device as a set member */
struct bt_csip_set_coordinator_set_member {
	/** Array of Coordinated Set Identification Service instances for the remote device */
	struct bt_csip_set_coordinator_csis_inst insts[BT_CSIP_SET_COORDINATOR_MAX_CSIS_INSTANCES];
};

/**
 * @typedef bt_csip_set_coordinator_discover_cb
 * @brief Callback for discovering Coordinated Set Identification Services.
 *
 * @param conn      Pointer to the remote device.
 * @param member    Pointer to the set member.
 * @param err       0 on success, or an errno value on error.
 * @param set_count Number of sets on the member.
 */
typedef void (*bt_csip_set_coordinator_discover_cb)(
	struct bt_conn *conn,
	const struct bt_csip_set_coordinator_set_member *member,
	int err, size_t set_count);

/**
 * @brief Initialise the csip_set_coordinator instance for a connection. This will do a
 * discovery on the device and prepare the instance for following commands.
 *
 * @param conn Pointer to remote device to perform discovery on.
 *
 * @return int Return 0 on success, or an errno value on error.
 */
int bt_csip_set_coordinator_discover(struct bt_conn *conn);

/**
 * @typedef bt_csip_set_coordinator_lock_set_cb
 * @brief Callback for locking a set across one or more devices
 *
 * @param err       0 on success, or an errno value on error.
 */
typedef void (*bt_csip_set_coordinator_lock_set_cb)(int err);

/**
 * @typedef bt_csip_set_coordinator_lock_changed_cb
 * @brief Callback when the lock value on a set of a connected device changes.
 *
 * @param inst    The Coordinated Set Identification Service instance that was
 *                changed.
 * @param locked  Whether the lock is locked or release.
 *
 * @return int Return 0 on success, or an errno value on error.
 */
typedef void (*bt_csip_set_coordinator_lock_changed_cb)(
	struct bt_csip_set_coordinator_csis_inst *inst, bool locked);

/**
 * @typedef bt_csip_set_coordinator_ordered_access_cb_t
 * @brief Callback for bt_csip_set_coordinator_ordered_access()
 *
 * If any of the set members supplied to bt_csip_set_coordinator_ordered_access() is
 * in the locked state, this will be called with @p locked true and @p member
 * will be the locked member, and the ordered access procedure is cancelled.
 * Likewise, if any error occurs, the procedure will also be aborted.
 *
 * @param set_info  Pointer to the a specific set_info struct.
 * @param err       Error value. 0 on success, GATT error or errno on fail.
 * @param locked    Whether the lock is locked or release.
 * @param member    The locked member if @p locked is true, otherwise NULL.
 */
typedef void (*bt_csip_set_coordinator_ordered_access_cb_t)(
	const struct bt_csip_set_coordinator_set_info *set_info,
	int err, bool locked,
	struct bt_csip_set_coordinator_set_member *member);

struct bt_csip_set_coordinator_cb {
	/* Set callbacks */
	bt_csip_set_coordinator_lock_set_cb             lock_set;
	bt_csip_set_coordinator_lock_set_cb             release_set;
	bt_csip_set_coordinator_lock_changed_cb         lock_changed;

	/* Device specific callbacks */
	bt_csip_set_coordinator_discover_cb             discover;
	bt_csip_set_coordinator_ordered_access_cb_t     ordered_access;

	/** Internally used field for list handling */
	sys_snode_t _node;
};

/**
 * @brief Check if advertising data indicates a set member
 *
 * @param set_sirk The SIRK of the set to check against
 * @param data     The advertising data
 *
 * @return true if the advertising data indicates a set member, false otherwise
 */
bool bt_csip_set_coordinator_is_set_member(const uint8_t set_sirk[BT_CSIP_SET_SIRK_SIZE],
					   struct bt_data *data);

/**
 * @brief Registers callbacks for csip_set_coordinator.
 *
 * @param cb   Pointer to the callback structure.
 *
 * @return Return 0 on success, or an errno value on error.
 */
int bt_csip_set_coordinator_register_cb(struct bt_csip_set_coordinator_cb *cb);

/**
 * @brief Callback function definition for bt_csip_set_coordinator_ordered_access()
 *
 * @param set_info   Pointer to the a specific set_info struct.
 * @param members    Array of members ordered by rank. The procedure shall be
 *                   done on the members in ascending order.
 * @param count      Number of members in @p members.
 *
 * @return true if the procedures can be successfully done, or false to stop the
 *         procedure.
 */
typedef bool (*bt_csip_set_coordinator_ordered_access_t)(
	const struct bt_csip_set_coordinator_set_info *set_info,
	struct bt_csip_set_coordinator_set_member *members[],
	size_t count);

/**
 * @brief Access Coordinated Set devices in an ordered manner as a client
 *
 * This function will read the lock state of all devices and if all devices are
 * in the unlocked state, then @p cb will be called with the same members as
 * provided by @p members, but where the members are ordered by rank
 * (if present). Once this procedure is finished or an error occurs,
 * @ref bt_csip_set_coordinator_cb.ordered_access will be called.
 *
 * This procedure only works if all the members have the lock characterstic,
 * and all either has rank = 0 or unique ranks.
 *
 * If any of the members are in the locked state, the procedure will be
 * cancelled.
 *
 * This can only be done on members that are bonded.
 *
 * @param members   Array of set members to access.
 * @param count     Number of set members in @p members.
 * @param set_info  Pointer to the a specific set_info struct, as a member may
 *                  be part of multiple sets.
 * @param cb        The callback function to be called for each member.
 */
int bt_csip_set_coordinator_ordered_access(
	const struct bt_csip_set_coordinator_set_member *members[],
	uint8_t count,
	const struct bt_csip_set_coordinator_set_info *set_info,
	bt_csip_set_coordinator_ordered_access_t cb);

/**
 * @brief Lock an array of set members
 *
 * The members will be locked starting from lowest rank going up.
 *
 * TODO: If locking fails, the already locked members will not be unlocked.
 *
 * @param members   Array of set members to lock.
 * @param count     Number of set members in @p members.
 * @param set_info  Pointer to the a specific set_info struct, as a member may
 *                  be part of multiple sets.
 *
 * @return Return 0 on success, or an errno value on error.
 */
int bt_csip_set_coordinator_lock(const struct bt_csip_set_coordinator_set_member **members,
				 uint8_t count,
				 const struct bt_csip_set_coordinator_set_info *set_info);

/**
 * @brief Release an array of set members
 *
 * The members will be released starting from highest rank going down.
 *
 * @param members   Array of set members to lock.
 * @param count     Number of set members in @p members.
 * @param set_info  Pointer to the a specific set_info struct, as a member may
 *                  be part of multiple sets.
 *
 * @return Return 0 on success, or an errno value on error.
 */
int bt_csip_set_coordinator_release(const struct bt_csip_set_coordinator_set_member **members,
				    uint8_t count,
				    const struct bt_csip_set_coordinator_set_info *set_info);


#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_SUBSYS_BLUETOOTH_AUDIO_CSIP_H_ */
