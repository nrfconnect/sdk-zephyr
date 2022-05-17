/*
 * Copyright (c) 2021 Vestas Wind Systems A/S
 * Copyright (c) 2018 Alexander Wachter
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CAN_H_
#define ZEPHYR_INCLUDE_DRIVERS_CAN_H_

#include <zephyr/types.h>
#include <zephyr/device.h>
#include <string.h>
#include <sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CAN Interface
 * @defgroup can_interface CAN Interface
 * @ingroup io_interfaces
 * @{
 */

/**
 * @name CAN frame definitions
 * @{
 */

/**
 * @brief Bit mask for a standard (11-bit) CAN identifier.
 */
#define CAN_STD_ID_MASK 0x7FFU
/**
 * @brief Maximum value for a standard (11-bit) CAN identifier.
 */
#define CAN_MAX_STD_ID  CAN_STD_ID_MASK
/**
 * @brief Bit mask for an extended (29-bit) CAN identifier.
 */
#define CAN_EXT_ID_MASK 0x1FFFFFFFU
/**
 * @brief Maximum value for an extended (29-bit) CAN identifier.
 */
#define CAN_MAX_EXT_ID  CAN_EXT_ID_MASK
/**
 * @brief Maximum data length code for CAN 2.0A/2.0B.
 */
#define CAN_MAX_DLC     8U
/**
 * @brief Maximum data length code for CAN-FD.
 */
#define CANFD_MAX_DLC   CONFIG_CANFD_MAX_DLC

/**
 * @cond INTERNAL_HIDDEN
 * Internally calculated maximum data length
 */
#ifndef CONFIG_CANFD_MAX_DLC
#define CAN_MAX_DLEN    8U
#else
#if CONFIG_CANFD_MAX_DLC <= 8
#define CAN_MAX_DLEN    CONFIG_CANFD_MAX_DLC
#elif CONFIG_CANFD_MAX_DLC <= 12
#define CAN_MAX_DLEN    (CONFIG_CANFD_MAX_DLC + (CONFIG_CANFD_MAX_DLC - 8U) * 4U)
#elif CONFIG_CANFD_MAX_DLC == 13
#define CAN_MAX_DLEN    32U
#elif CONFIG_CANFD_MAX_DLC == 14
#define CAN_MAX_DLEN    48U
#elif CONFIG_CANFD_MAX_DLC == 15
#define CAN_MAX_DLEN    64U
#endif
#endif /* CONFIG_CANFD_MAX_DLC */

/** @endcond */

/** @} */

/**
 * @brief Defines the mode of the CAN controller
 */
enum can_mode {
	/** Normal mode. */
	CAN_NORMAL_MODE,
	/** Controller is not allowed to send dominant bits. */
	CAN_SILENT_MODE,
	/** Controller is in loopback mode (receives own frames). */
	CAN_LOOPBACK_MODE,
	/** Combination of loopback and silent modes. */
	CAN_SILENT_LOOPBACK_MODE
};

/**
 * @brief Defines the state of the CAN bus
 */
enum can_state {
	/** Error-active state (RX/TX error count < 96). */
	CAN_ERROR_ACTIVE,
	/** Error-warning state (RX/TX error count < 128). */
	CAN_ERROR_WARNING,
	/** Error-passive state (RX/TX error count < 256). */
	CAN_ERROR_PASSIVE,
	/** Bus-off state (RX/TX error count >= 256). */
	CAN_BUS_OFF,
};

/**
 * @brief Defines if the CAN frame has a standard (11-bit) or extended (29-bit)
 * CAN identifier
 */
enum can_ide {
	/** Standard (11-bit) CAN identifier. */
	CAN_STANDARD_IDENTIFIER,
	/** Extended (29-bit) CAN identifier. */
	CAN_EXTENDED_IDENTIFIER
};

/**
 * @brief Defines if the CAN frame is a data frame or a Remote Transmission Request (RTR) frame
 */
enum can_rtr {
	/** Data frame. */
	CAN_DATAFRAME,
	/** Remote Transmission Request (RTR) frame. */
	CAN_REMOTEREQUEST
};

/**
 * @brief CAN frame structure
 */
struct zcan_frame {
	/** Standard (11-bit) or extended (29-bit) CAN identifier. */
	uint32_t id      : 29;
	/** Frame is in the CAN-FD frame format if set to true. */
	uint32_t fd      : 1;
	/** Remote Transmission Request (RTR) flag. Use @a can_rtr enum for assignment. */
	uint32_t rtr     : 1;
	/** CAN identifier type (standard or extended). Use @a can_ide enum for assignment. */
	uint32_t id_type : 1;
	/** Data Length Code (DLC) indicating data length in bytes. */
	uint8_t dlc;
	/** Baud Rate Switch (BRS). Only valid for CAN-FD. */
	uint8_t brs : 1;
	/** @cond INTERNAL_HIDDEN */
	uint8_t res : 7; /* reserved/padding. */
	/** @endcond */
#if defined(CONFIG_CAN_RX_TIMESTAMP) || defined(__DOXYGEN__)
	/** Captured value of the free-running timer in the CAN controller when
	 * this frame was received. The timer is incremented every bit time and
	 * captured at the start of frame bit (SOF).
	 *
	 * @note @kconfig{CONFIG_CAN_RX_TIMESTAMP} must be selected for this
	 * field to be available.
	 */
	uint16_t timestamp;
#else
	/** @cond INTERNAL_HIDDEN */
	uint8_t res0;  /* reserved/padding. */
	uint8_t res1;  /* reserved/padding. */
	/** @endcond */
#endif
	/** The frame payload data. */
	union {
		uint8_t data[CAN_MAX_DLEN];
		uint32_t data_32[ceiling_fraction(CAN_MAX_DLEN, sizeof(uint32_t))];
	};
};

/**
 * @brief CAN filter structure
 */
struct zcan_filter {
	/** CAN identifier to match. */
	uint32_t id           : 29;
	/** @cond INTERNAL_HIDDEN */
	uint32_t res0         : 1;
	/** @endcond */
	/** Match data frame or Remote Transmission Request (RTR) frame. */
	uint32_t rtr          : 1;
	/** Standard or extended CAN identifier. Use @a can_ide enum for assignment. */
	uint32_t id_type      : 1;
	/** CAN identifier matching mask. If a bit in this mask is 0, the value
	 * of the corresponding bit in the ``id`` field is ignored by the filter.
	 */
	uint32_t id_mask      : 29;
	/** @cond INTERNAL_HIDDEN */
	uint32_t res1         : 1;
	/** @endcond */
	/** Data frame/Remote Transmission Request (RTR) bit matching mask. If
	 * this bit is 0, the value of the ``rtr`` field is ignored by the
	 * filter.
	 */
	uint32_t rtr_mask     : 1;
	/** @cond INTERNAL_HIDDEN */
	uint32_t res2         : 1;
	/** @endcond */
};

/**
 * @brief CAN controller error counters
 */
struct can_bus_err_cnt {
	/** Value of the CAN controller transmit error counter. */
	uint8_t tx_err_cnt;
	/** Value of the CAN controller receive error counter. */
	uint8_t rx_err_cnt;
};

/**
 * @brief CAN bus timing structure
 *
 * This struct is used to pass bus timing values to the configuration and
 * bitrate calculation functions.
 *
 * The propagation segment represents the time of the signal propagation. Phase
 * segment 1 and phase segment 2 define the sampling point. The ``prop_seg`` and
 * ``phase_seg1`` values affect the sampling point in the same way and some
 * controllers only have a register for the sum of those two. The sync segment
 * always has a length of 1 time quantum (see below).
 *
 * @code{.text}
 *
 * +---------+----------+------------+------------+
 * |sync_seg | prop_seg | phase_seg1 | phase_seg2 |
 * +---------+----------+------------+------------+
 *                                   ^
 *                             Sampling-Point
 *
 * @endcode
 *
 * 1 time quantum (tq) has the length of 1/(core_clock / prescaler). The bitrate
 * is defined by the core clock divided by the prescaler and the sum of the
 * segments:
 *
 *   br = (core_clock / prescaler) / (1 + prop_seg + phase_seg1 + phase_seg2)
 *
 * The Synchronization Jump Width (SJW) defines the amount of time quanta the
 * sample point can be moved. The sample point is moved when resynchronization
 * is needed.
 */
struct can_timing {
	/** Synchronisation jump width. */
	uint16_t sjw;
	/** Propagation segment. */
	uint16_t prop_seg;
	/** Phase segment 1. */
	uint16_t phase_seg1;
	/** Phase segment 2. */
	uint16_t phase_seg2;
	/** Prescaler value. */
	uint16_t prescaler;
};

/**
 * @brief Defines the application callback handler function signature
 *
 * @param dev       Pointer to the device structure for the driver instance.
 * @param error     Status of the performed send operation. See the list of
 *                  return values for @a can_send() for value descriptions.
 * @param user_data User data provided when the frame was sent.
 */
typedef void (*can_tx_callback_t)(const struct device *dev, int error, void *user_data);

/**
 * @brief Defines the application callback handler function signature for receiving.
 *
 * @param dev       Pointer to the device structure for the driver instance.
 * @param frame     Received frame.
 * @param user_data User data provided when the filter was added.
 */
typedef void (*can_rx_callback_t)(const struct device *dev, struct zcan_frame *frame,
				  void *user_data);

/**
 * @brief Defines the state change callback handler function signature
 *
 * @param dev       Pointer to the device structure for the driver instance.
 * @param state     State of the CAN controller.
 * @param err_cnt   CAN controller error counter values.
 * @param user_data User data provided the callback was set.
 */
typedef void (*can_state_change_callback_t)(const struct device *dev,
					    enum can_state state,
					    struct can_bus_err_cnt err_cnt,
					    void *user_data);

/**
 * @cond INTERNAL_HIDDEN
 *
 * For internal driver use only, skip these in public documentation.
 */

/**
 * @brief Callback API upon setting CAN bus timing
 * See @a can_set_timing() for argument description
 */
typedef int (*can_set_timing_t)(const struct device *dev,
				const struct can_timing *timing,
				const struct can_timing *timing_data);

/**
 * @brief Callback API upon setting CAN controller mode
 * See @a can_set_mode() for argument description
 */
typedef int (*can_set_mode_t)(const struct device *dev, enum can_mode mode);

/**
 * @brief Callback API upon sending a CAN frame
 * See @a can_send() for argument description
 */
typedef int (*can_send_t)(const struct device *dev,
			  const struct zcan_frame *frame,
			  k_timeout_t timeout, can_tx_callback_t callback,
			  void *user_data);

/**
 * @brief Callback API upon adding an RX filter
 * See @a can_add_rx_callback() for argument description
 */
typedef int (*can_add_rx_filter_t)(const struct device *dev,
				   can_rx_callback_t callback,
				   void *user_data,
				   const struct zcan_filter *filter);

/**
 * @brief Callback API upon removing an RX filter
 * See @a can_remove_rx_filter() for argument description
 */
typedef void (*can_remove_rx_filter_t)(const struct device *dev, int filter_id);

/**
 * @brief Callback API upon recovering the CAN bus
 * See @a can_recover() for argument description
 */
typedef int (*can_recover_t)(const struct device *dev, k_timeout_t timeout);

/**
 * @brief Callback API upon getting the CAN controller state
 * See @a can_get_state() for argument description
 */
typedef int (*can_get_state_t)(const struct device *dev, enum can_state *state,
			       struct can_bus_err_cnt *err_cnt);

/**
 * @brief Callback API upon setting a state change callback
 * See @a can_set_state_change_callback() for argument description
 */
typedef void(*can_set_state_change_callback_t)(const struct device *dev,
					       can_state_change_callback_t callback,
					       void *user_data);

/**
 * @brief Callback API upon getting the CAN core clock rate
 * See @a can_get_core_clock() for argument description
 */
typedef int (*can_get_core_clock_t)(const struct device *dev, uint32_t *rate);

/**
 * @brief Callback API upon getting the maximum number of concurrent CAN RX filters
 * See @a can_get_max_filters() for argument description
 */
typedef int (*can_get_max_filters_t)(const struct device *dev, enum can_ide id_type);

/**
 * @brief Callback API upon getting the maximum supported bitrate
 * See @a can_get_max_bitrate() for argument description
 */
typedef int (*can_get_max_bitrate_t)(const struct device *dev, uint32_t *max_bitrate);

__subsystem struct can_driver_api {
	can_set_mode_t set_mode;
	can_set_timing_t set_timing;
	can_send_t send;
	can_add_rx_filter_t add_rx_filter;
	can_remove_rx_filter_t remove_rx_filter;
#if !defined(CONFIG_CAN_AUTO_BUS_OFF_RECOVERY) || defined(__DOXYGEN__)
	can_recover_t recover;
#endif /* CONFIG_CAN_AUTO_BUS_OFF_RECOVERY */
	can_get_state_t get_state;
	can_set_state_change_callback_t set_state_change_callback;
	can_get_core_clock_t get_core_clock;
	can_get_max_filters_t get_max_filters;
	can_get_max_bitrate_t get_max_bitrate;
	/* Min values for the timing registers */
	struct can_timing timing_min;
	/* Max values for the timing registers */
	struct can_timing timing_max;
#if defined(CONFIG_CAN_FD_MODE) || defined(__DOXYGEN__)
	/* Min values for the timing registers during the data phase */
	struct can_timing timing_min_data;
	/* Max values for the timing registers during the data phase */
	struct can_timing timing_max_data;
#endif /* CONFIG_CAN_FD_MODE */
};

/** @endcond */

#if defined(CONFIG_CAN_STATS) || defined(__DOXYGEN__)

#include <stats/stats.h>

/** @cond INTERNAL_HIDDEN */

STATS_SECT_START(can)
STATS_SECT_ENTRY32(bit0_error)
STATS_SECT_ENTRY32(bit1_error)
STATS_SECT_ENTRY32(stuff_error)
STATS_SECT_ENTRY32(crc_error)
STATS_SECT_ENTRY32(form_error)
STATS_SECT_ENTRY32(ack_error)
STATS_SECT_END;

STATS_NAME_START(can)
STATS_NAME(can, bit0_error)
STATS_NAME(can, bit1_error)
STATS_NAME(can, stuff_error)
STATS_NAME(can, crc_error)
STATS_NAME(can, form_error)
STATS_NAME(can, ack_error)
STATS_NAME_END(can);

/** @endcond */

/**
 * @brief CAN specific device state which allows for CAN device class specific
 * additions
 */
struct can_device_state {
	struct device_state devstate;
	struct stats_can stats;
};

/** @cond INTERNAL_HIDDEN */

/**
 * @brief Get pointer to CAN statistics structure
 */
#define Z_CAN_GET_STATS(dev_)				\
	CONTAINER_OF(dev_->state, struct can_device_state, devstate)->stats

/** @endcond */

/**
 * @brief Increment the bit0 error counter for a CAN device
 *
 * The bit0 error counter is incremented when the CAN controller is unable to
 * transmit a dominant bit.
 *
 * @param dev_ Pointer to the device structure for the driver instance.
 */
#define CAN_STATS_BIT0_ERROR_INC(dev_)			\
	STATS_INC(Z_CAN_GET_STATS(dev_), bit0_error)

/**
 * @brief Increment the bit1 (recessive) error counter for a CAN device
 *
 * The bit1 error counter is incremented when the CAN controller is unable to
 * transmit a recessive bit.
 *
 * @param dev_ Pointer to the device structure for the driver instance.
 */
#define CAN_STATS_BIT1_ERROR_INC(dev_)			\
	STATS_INC(Z_CAN_GET_STATS(dev_), bit1_error)

/**
 * @brief Increment the stuffing error counter for a CAN device
 *
 * The stuffing error counter is incremented when the CAN controller detects a
 * bit stuffing error.
 *
 * @param dev_ Pointer to the device structure for the driver instance.
 */
#define CAN_STATS_STUFF_ERROR_INC(dev_)			\
	STATS_INC(Z_CAN_GET_STATS(dev_), stuff_error)

/**
 * @brief Increment the CRC error counter for a CAN device
 *
 * The CRC error counter is incremented when the CAN controller detects a frame
 * with an invalid CRC.
 *
 * @param dev_ Pointer to the device structure for the driver instance.
 */
#define CAN_STATS_CRC_ERROR_INC(dev_)			\
	STATS_INC(Z_CAN_GET_STATS(dev_), crc_error)

/**
 * @brief Increment the form error counter for a CAN device
 *
 * The form error counter is incremented when the CAN controller detects a
 * fixed-form bit field containing illegal bits.
 *
 * @param dev_ Pointer to the device structure for the driver instance.
 */
#define CAN_STATS_FORM_ERROR_INC(dev_)			\
	STATS_INC(Z_CAN_GET_STATS(dev_), form_error)

/**
 * @brief Increment the acknowledge error counter for a CAN device
 *
 * The acknowledge error counter is incremented when the CAN controller does not
 * monitor a dominant bit in the ACK slot.
 *
 * @param dev_ Pointer to the device structure for the driver instance.
 */
#define CAN_STATS_ACK_ERROR_INC(dev_)			\
	STATS_INC(Z_CAN_GET_STATS(dev_), ack_error)

/** @cond INTERNAL_HIDDEN */

/**
 * @brief Define a statically allocated and section assigned CAN device state
 */
#define Z_CAN_DEVICE_STATE_DEFINE(node_id, dev_name)			\
	static struct can_device_state Z_DEVICE_STATE_NAME(dev_name)	\
	__attribute__((__section__(".z_devstate")));

/**
 * @brief Define a CAN device init wrapper function
 *
 * This does device instance specific initialization of common data (such as stats)
 * and calls the given init_fn
 */
#define Z_CAN_INIT_FN(dev_name, init_fn)				\
	static inline int UTIL_CAT(dev_name, _init)(const struct device *dev) \
	{								\
		struct can_device_state *state =			\
			CONTAINER_OF(dev->state, struct can_device_state, devstate); \
		stats_init(&state->stats.s_hdr, STATS_SIZE_32, 6,	\
			   STATS_NAME_INIT_PARMS(can));			\
		stats_register(dev->name, &(state->stats.s_hdr));	\
		return init_fn(dev);					\
	}

/** @endcond */

/**
 * @brief Like DEVICE_DT_DEFINE() with CAN device specifics.
 *
 * @details Defines a device which implements the CAN API. May generate a custom
 * device_state container struct and init_fn wrapper when needed depending on
 * @kconfig{CONFIG_CAN_STATS}.
 *
 * @param node_id   The devicetree node identifier.
 * @param init_fn   Name of the init function of the driver.
 * @param pm_device PM device resources reference (NULL if device does not use PM).
 * @param data_ptr  Pointer to the device's private data.
 * @param cfg_ptr   The address to the structure containing the configuration
 *                  information for this instance of the driver.
 * @param level     The initialization level. See SYS_INIT() for
 *                  details.
 * @param prio      Priority within the selected initialization level. See
 *                  SYS_INIT() for details.
 * @param api_ptr   Provides an initial pointer to the API function struct
 *                  used by the driver. Can be NULL.
 */
#define CAN_DEVICE_DT_DEFINE(node_id, init_fn, pm_device,		\
			     data_ptr, cfg_ptr, level, prio,		\
			     api_ptr, ...)				\
	Z_CAN_DEVICE_STATE_DEFINE(node_id, Z_DEVICE_DT_DEV_NAME(node_id)); \
	Z_CAN_INIT_FN(Z_DEVICE_DT_DEV_NAME(node_id), init_fn)		\
	Z_DEVICE_DEFINE(node_id, Z_DEVICE_DT_DEV_NAME(node_id),		\
			DEVICE_DT_NAME(node_id),			\
			&UTIL_CAT(Z_DEVICE_DT_DEV_NAME(node_id), _init), \
			pm_device,					\
			data_ptr, cfg_ptr, level, prio,			\
			api_ptr,					\
			&(Z_DEVICE_STATE_NAME(Z_DEVICE_DT_DEV_NAME(node_id)).devstate), \
			__VA_ARGS__)

#else /* CONFIG_CAN_STATS */

#define CAN_STATS_BIT0_ERROR_INC(dev_)
#define CAN_STATS_BIT1_ERROR_INC(dev_)
#define CAN_STATS_STUFF_ERROR_INC(dev_)
#define CAN_STATS_CRC_ERROR_INC(dev_)
#define CAN_STATS_FORM_ERROR_INC(dev_)
#define CAN_STATS_ACK_ERROR_INC(dev_)

#define CAN_DEVICE_DT_DEFINE(node_id, init_fn, pm_device,		\
			     data_ptr, cfg_ptr, level, prio,		\
			     api_ptr, ...)				\
	DEVICE_DT_DEFINE(node_id, init_fn, pm_device,			\
			     data_ptr, cfg_ptr, level, prio,		\
			     api_ptr, __VA_ARGS__)

#endif /* CONFIG_CAN_STATS */

/**
 * @brief Like CAN_DEVICE_DT_DEFINE() for an instance of a DT_DRV_COMPAT compatible
 *
 * @param inst Instance number. This is replaced by <tt>DT_DRV_COMPAT(inst)</tt>
 *             in the call to CAN_DEVICE_DT_DEFINE().
 * @param ...  Other parameters as expected by CAN_DEVICE_DT_DEFINE().
 */
#define CAN_DEVICE_DT_INST_DEFINE(inst, ...)			\
	CAN_DEVICE_DT_DEFINE(DT_DRV_INST(inst), __VA_ARGS__)

/**
 * @name CAN controller configuration
 *
 * @{
 */

/**
 * @brief Get the CAN core clock rate
 *
 * Returns the CAN core clock rate. One time quantum is 1/(core clock rate).
 *
 * @param dev  Pointer to the device structure for the driver instance.
 * @param[out] rate CAN core clock rate in Hz.
 *
 * @return 0 on success, or a negative error code on error
 */
__syscall int can_get_core_clock(const struct device *dev, uint32_t *rate);

static inline int z_impl_can_get_core_clock(const struct device *dev, uint32_t *rate)
{
	const struct can_driver_api *api = (const struct can_driver_api *)dev->api;

	return api->get_core_clock(dev, rate);
}

/**
 * @brief Get maximum supported bitrate
 *
 * Get the maximum supported bitrate for the CAN controller/transceiver combination.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param[out] max_bitrate Maximum supported bitrate in bits/s
 *
 * @retval -EIO General input/output error.
 * @retval -ENOSYS If this function is not implemented by the driver.
 */
__syscall int can_get_max_bitrate(const struct device *dev, uint32_t *max_bitrate);

static inline int z_impl_can_get_max_bitrate(const struct device *dev, uint32_t *max_bitrate)
{
	const struct can_driver_api *api = (const struct can_driver_api *)dev->api;

	if (api->get_max_bitrate == NULL) {
		return -ENOSYS;
	}

	return api->get_max_bitrate(dev, max_bitrate);
}

/**
 * @brief Get the minimum supported timing parameter values.
 *
 * @param dev Pointer to the device structure for the driver instance.
 *
 * @return Pointer to the minimum supported timing parameter values.
 */
__syscall const struct can_timing *can_get_timing_min(const struct device *dev);

static inline const struct can_timing *z_impl_can_get_timing_min(const struct device *dev)
{
	const struct can_driver_api *api = (const struct can_driver_api *)dev->api;

	return &api->timing_min;
}

/**
 * @brief Get the maximum supported timing parameter values.
 *
 * @param dev Pointer to the device structure for the driver instance.
 *
 * @return Pointer to the maximum supported timing parameter values.
 */
__syscall const struct can_timing *can_get_timing_max(const struct device *dev);

static inline const struct can_timing *z_impl_can_get_timing_max(const struct device *dev)
{
	const struct can_driver_api *api = (const struct can_driver_api *)dev->api;

	return &api->timing_max;
}

/**
 * @brief Calculate timing parameters from bitrate and sample point
 *
 * Calculate the timing parameters from a given bitrate in bits/s and the
 * sampling point in permill (1/1000) of the entire bit time. The bitrate must
 * always match perfectly. If no result can be reached for the given parameters,
 * -EINVAL is returned.
 *
 * @note The requested ``sample_pnt`` will not always be matched perfectly. The
 * algorithm calculates the best possible match.
 *
 * @param dev        Pointer to the device structure for the driver instance.
 * @param[out] res   Result is written into the @a can_timing struct provided.
 * @param bitrate    Target bitrate in bits/s.
 * @param sample_pnt Sampling point in permill of the entire bit time.
 *
 * @retval 0 or positive sample point error on success.
 * @retval -EINVAL if there is no solution for the desired values.
 * @retval -EIO if @a can_get_core_clock() is not available.
 */
__syscall int can_calc_timing(const struct device *dev, struct can_timing *res,
			      uint32_t bitrate, uint16_t sample_pnt);

#if defined(CONFIG_CAN_FD_MODE) || defined(__DOXYGEN__)

/**
 * @brief Get the minimum supported timing parameter values for the data phase.
 *
 * Same as @a can_get_timing_min() but for the minimum values for the data phase.
 *
 * @note @kconfig{CONFIG_CAN_FD_MODE} must be selected for this function to be
 * available.
 *
 * @param dev Pointer to the device structure for the driver instance.
 *
 * @return Pointer to the minimum supported timing parameter values, or NULL if
 *         CAN-FD is not supported.
 */
__syscall const struct can_timing *can_get_timing_min_data(const struct device *dev);

static inline const struct can_timing *z_impl_can_get_timing_min_data(const struct device *dev)
{
	const struct can_driver_api *api = (const struct can_driver_api *)dev->api;

	return &api->timing_min_data;
}

/**
 * @brief Get the maximum supported timing parameter values for the data phase.
 *
 * Same as @a can_get_timing_max() but for the maximum values for the data phase.
 *
 * @note @kconfig{CONFIG_CAN_FD_MODE} must be selected for this function to be
 * available.
 *
 * @param dev Pointer to the device structure for the driver instance.
 *
 * @return Pointer to the maximum supported timing parameter values, or NULL if
 *         CAN-FD is not supported.
 */
__syscall const struct can_timing *can_get_timing_max_data(const struct device *dev);

static inline const struct can_timing *z_impl_can_get_timing_max_data(const struct device *dev)
{
	const struct can_driver_api *api = (const struct can_driver_api *)dev->api;

	return &api->timing_max_data;
}

/**
 * @brief Calculate timing parameters for the data phase
 *
 * Same as @a can_calc_timing() but with the maximum and minimum values from the
 * data phase.
 *
 * @note @kconfig{CONFIG_CAN_FD_MODE} must be selected for this function to be
 * available.
 *
 * @param dev        Pointer to the device structure for the driver instance.
 * @param[out] res   Result is written into the @a can_timing struct provided.
 * @param bitrate    Target bitrate for the data phase in bits/s
 * @param sample_pnt Sampling point for the data phase in permille of the entire bit time.
 *
 * @retval 0 or positive sample point error on success.
 * @retval -EINVAL if there is no solution for the desired values.
 * @retval -EIO if @a can_get_core_clock() is not available.
 */
__syscall int can_calc_timing_data(const struct device *dev, struct can_timing *res,
				   uint32_t bitrate, uint16_t sample_pnt);

#endif /* CONFIG_CAN_FD_MODE */

/**
 * @brief Fill in the prescaler value for a given bitrate and timing
 *
 * Fill the prescaler value in the timing struct. The sjw, prop_seg, phase_seg1
 * and phase_seg2 must be given.
 *
 * The returned bitrate error is remainder of the division of the clock rate by
 * the bitrate times the timing segments.
 *
 * @param dev     Pointer to the device structure for the driver instance.
 * @param timing  Result is written into the can_timing struct provided.
 * @param bitrate Target bitrate.
 *
 * @retval 0 or positive bitrate error.
 * @retval Negative error code on error.
 */
int can_calc_prescaler(const struct device *dev, struct can_timing *timing,
		       uint32_t bitrate);

/** Synchronization Jump Width (SJW) value to indicate that the SJW should not
 * be changed by the timing calculation.
 */
#define CAN_SJW_NO_CHANGE 0

/**
 * @brief Configure the bus timing of a CAN controller.
 *
 * If the sjw equals CAN_SJW_NO_CHANGE, the sjw parameter is not changed.
 *
 * @note The parameter ``timing_data`` is only relevant for CAN-FD. If the
 * controller does not support CAN-FD or if @kconfig{CONFIG_CAN_FD_MODE} is not
 * selected, the value of this parameter is ignored.
 *
 * @param dev         Pointer to the device structure for the driver instance.
 * @param timing      Bus timings.
 * @param timing_data Bus timings for data phase (CAN-FD only).
 *
 * @retval 0 If successful.
 * @retval -EIO General input/output error, failed to configure device.
 */
__syscall int can_set_timing(const struct device *dev,
			     const struct can_timing *timing,
			     const struct can_timing *timing_data);

static inline int z_impl_can_set_timing(const struct device *dev,
					const struct can_timing *timing,
					const struct can_timing *timing_data)
{
	const struct can_driver_api *api = (const struct can_driver_api *)dev->api;

	return api->set_timing(dev, timing, timing_data);
}

/**
 * @brief Set the CAN controller to the given operation mode
 *
 * @param dev  Pointer to the device structure for the driver instance.
 * @param mode Operation mode.
 *
 * @retval 0 If successful.
 * @retval -EIO General input/output error, failed to configure device.
 */
__syscall int can_set_mode(const struct device *dev, enum can_mode mode);

static inline int z_impl_can_set_mode(const struct device *dev, enum can_mode mode)
{
	const struct can_driver_api *api = (const struct can_driver_api *)dev->api;

	return api->set_mode(dev, mode);
}

/**
 * @brief Set the bitrate of the CAN controller
 *
 * CAN in Automation (CiA) 301 v4.2.0 recommends a sample point location of
 * 87.5% percent for all bitrates. However, some CAN controllers have
 * difficulties meeting this for higher bitrates.
 *
 * This function defaults to using a sample point of 75.0% for bitrates over 800
 * kbit/s, 80.0% for bitrates over 500 kbit/s, and 87.5% for all other
 * bitrates. This is in line with the sample point locations used by the Linux
 * kernel.
 *
 * @note The parameter ``bitrate_data`` is only relevant for CAN-FD. If the
 * controller does not support CAN-FD or if @kconfig{CONFIG_CAN_FD_MODE} is not
 * selected, the value of this parameter is ignored.

 * @param dev          Pointer to the device structure for the driver instance.
 * @param bitrate      Desired arbitration phase bitrate.
 * @param bitrate_data Desired data phase bitrate.
 *
 * @retval 0 If successful.
 * @retval -ENOTSUP bitrate not supported by CAN controller/transceiver combination
 * @retval -EINVAL bitrate/sample point cannot be met.
 * @retval -EIO General input/output error, failed to set bitrate.
 */
__syscall int can_set_bitrate(const struct device *dev, uint32_t bitrate, uint32_t bitrate_data);

/** @} */

/**
 * @name Transmitting CAN frames
 *
 * @{
 */

/**
 * @brief Queue a CAN frame for transmission on the CAN bus
 *
 * Queue a CAN frame for transmission on the CAN bus with optional timeout and
 * completion callback function.
 *
 * Queued CAN frames are transmitted in order according to the their priority:
 * - The lower the CAN-ID, the higher the priority.
 * - Data frames have higher priority than Remote Transmission Request (RTR)
 *   frames with identical CAN-IDs.
 * - Frames with standard (11-bit) identifiers have higher priority than frames
 *   with extended (29-bit) identifiers with identical base IDs (the higher 11
 *   bits of the extended identifier).
 * - Transmission order for queued frames with the same priority is hardware
 *   dependent.
 *
 * @note If transmitting segmented messages spanning multiple CAN frames with
 * identical CAN-IDs, the sender must ensure to only queue one frame at a time
 * if FIFO order is required.
 *
 * By default, the CAN controller will automatically retry transmission in case
 * of lost bus arbitration or missing acknowledge. Some CAN controllers support
 * disabling automatic retransmissions ("one-shot" mode) via a devicetree
 * property.
 *
 * @param dev       Pointer to the device structure for the driver instance.
 * @param frame     CAN frame to transmit.
 * @param timeout   Timeout waiting for a empty TX mailbox or ``K_FOREVER``.
 * @param callback  Optional callback for when the frame was sent or a
 *                  transmission error occurred. If ``NULL``, this function is
 *                  blocking until frame is sent. The callback must be ``NULL``
 *                  if called from user mode.
 * @param user_data User data to pass to callback function.
 *
 * @retval 0 if successful.
 * @retval -EINVAL if an invalid parameter was passed to the function.
 * @retval -ENETDOWN if the CAN controller is in bus-off state.
 * @retval -EBUSY if CAN bus arbitration was lost (only applicable if automatic
 *                retransmissions are disabled).
 * @retval -EIO if a general transmit error occurred (e.g. missing ACK if
 *              automatic retransmissions are disabled).
 * @retval -EAGAIN on timeout.
 */
__syscall int can_send(const struct device *dev, const struct zcan_frame *frame,
		       k_timeout_t timeout, can_tx_callback_t callback,
		       void *user_data);

static inline int z_impl_can_send(const struct device *dev, const struct zcan_frame *frame,
				  k_timeout_t timeout, can_tx_callback_t callback,
				  void *user_data)
{
	const struct can_driver_api *api = (const struct can_driver_api *)dev->api;

	return api->send(dev, frame, timeout, callback, user_data);
}

/** @} */

/**
 * @name Receiving CAN frames
 *
 * @{
 */

/**
 * @brief Add a callback function for a given CAN filter
 *
 * Add a callback to CAN identifiers specified by a filter. When a received CAN
 * frame matching the filter is received by the CAN controller, the callback
 * function is called in interrupt context.
 *
 * If a frame matches more than one attached filter, the priority of the match
 * is hardware dependent.
 *
 * The same callback function can be used for multiple filters.
 *
 * @param dev       Pointer to the device structure for the driver instance.
 * @param callback  This function is called by the CAN controller driver whenever
 *                  a frame matching the filter is received.
 * @param user_data User data to pass to callback function.
 * @param filter    Pointer to a @a zcan_filter structure defining the filter.
 *
 * @retval filter_id on success.
 * @retval -ENOSPC if there are no free filters.
 */
static inline int can_add_rx_filter(const struct device *dev, can_rx_callback_t callback,
				    void *user_data, const struct zcan_filter *filter)
{
	const struct can_driver_api *api = (const struct can_driver_api *)dev->api;

	return api->add_rx_filter(dev, callback, user_data, filter);
}

/**
 * @brief Statically define and initialize a CAN RX message queue.
 *
 * The message queue's ring buffer contains space for @a max_frames CAN frames.
 *
 * @see can_add_rx_filter_msgq()
 *
 * @param name       Name of the message queue.
 * @param max_frames Maximum number of CAN frames that can be queued.
 */
#define CAN_MSGQ_DEFINE(name, max_frames) \
	K_MSGQ_DEFINE(name, sizeof(struct zcan_frame), max_frames, 4)

/**
 * @brief Wrapper function for adding a message queue for a given filter
 *
 * Wrapper function for @a can_add_rx_filter() which puts received CAN frames
 * matching the filter in a message queue instead of calling a callback.
 *
 * If a frame matches more than one attached filter, the priority of the match
 * is hardware dependent.
 *
 * The same message queue can be used for multiple filters.
 *
 * @note The message queue must be initialized before calling this function and
 * the caller must have appropriate permissions on it.
 *
 * @param dev    Pointer to the device structure for the driver instance.
 * @param msgq   Pointer to the already initialized @a k_msgq struct.
 * @param filter Pointer to a @a zcan_filter structure defining the filter.
 *
 * @retval filter_id on success.
 * @retval -ENOSPC if there are no free filters.
 */
__syscall int can_add_rx_filter_msgq(const struct device *dev, struct k_msgq *msgq,
				     const struct zcan_filter *filter);

/**
 * @brief Remove a CAN RX filter
 *
 * This routine removes a CAN RX filter based on the filter ID returned by @a
 * can_add_rx_filter() or @a can_add_rx_filter_msgq().
 *
 * @param dev       Pointer to the device structure for the driver instance.
 * @param filter_id Filter ID
 */
__syscall void can_remove_rx_filter(const struct device *dev, int filter_id);

static inline void z_impl_can_remove_rx_filter(const struct device *dev, int filter_id)
{
	const struct can_driver_api *api = (const struct can_driver_api *)dev->api;

	return api->remove_rx_filter(dev, filter_id);
}

/**
 * @brief Get maximum number of RX filters
 *
 * Get the maximum number of concurrent RX filters for the CAN controller.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param id_type CAN identifier type (standard or extended).
 *
 * @retval Positive number of maximum concurrent filters.
 * @retval -EIO General input/output error.
 * @retval -ENOSYS If this function is not implemented by the driver.
 */
__syscall int can_get_max_filters(const struct device *dev, enum can_ide id_type);

static inline int z_impl_can_get_max_filters(const struct device *dev, enum can_ide id_type)
{
	const struct can_driver_api *api = (const struct can_driver_api *)dev->api;

	if (api->get_max_filters == NULL) {
		return -ENOSYS;
	}

	return api->get_max_filters(dev, id_type);
}

/** @} */

/**
 * @name CAN bus error reporting and handling
 *
 * @{
 */

/**
 * @brief Get current CAN controller state
 *
 * Returns the current state and optionally the error counter values of the CAN
 * controller.
 *
 * @param dev          Pointer to the device structure for the driver instance.
 * @param[out] state   Pointer to the state destination enum or NULL.
 * @param[out] err_cnt Pointer to the err_cnt destination structure or NULL.
 *
 * @retval 0 If successful.
 * @retval -EIO General input/output error, failed to get state.
 */
__syscall int can_get_state(const struct device *dev, enum can_state *state,
			    struct can_bus_err_cnt *err_cnt);

static inline int z_impl_can_get_state(const struct device *dev, enum can_state *state,
				       struct can_bus_err_cnt *err_cnt)
{
	const struct can_driver_api *api = (const struct can_driver_api *)dev->api;

	return api->get_state(dev, state, err_cnt);
}

/**
 * @brief Recover from bus-off state
 *
 * Recover the CAN controller from bus-off state to error-active state.
 *
 * @note @kconfig{CONFIG_CAN_AUTO_BUS_OFF_RECOVERY} must be deselected for this
 * function to be available.
 *
 * @param dev     Pointer to the device structure for the driver instance.
 * @param timeout Timeout for waiting for the recovery or ``K_FOREVER``.
 *
 * @retval 0 on success.
 * @retval -EAGAIN on timeout.
 */
#if !defined(CONFIG_CAN_AUTO_BUS_OFF_RECOVERY) || defined(__DOXYGEN__)
__syscall int can_recover(const struct device *dev, k_timeout_t timeout);

static inline int z_impl_can_recover(const struct device *dev, k_timeout_t timeout)
{
	const struct can_driver_api *api = (const struct can_driver_api *)dev->api;

	return api->recover(dev, timeout);
}
#else /* CONFIG_CAN_AUTO_BUS_OFF_RECOVERY */
/* This implementation prevents inking errors for auto recovery */
static inline int z_impl_can_recover(const struct device *dev, k_timeout_t timeout)
{
	return 0;
}
#endif /* !CONFIG_CAN_AUTO_BUS_OFF_RECOVERY */

/**
 * @brief Set a callback for CAN controller state change events
 *
 * Set the callback for CAN controller state change events. The callback
 * function will be called in interrupt context.
 *
 * Only one callback can be registered per controller. Calling this function
 * again overrides any previously registered callback.
 *
 * @param dev       Pointer to the device structure for the driver instance.
 * @param callback  Callback function.
 * @param user_data User data to pass to callback function.
 */
static inline void can_set_state_change_callback(const struct device *dev,
						 can_state_change_callback_t callback,
						 void *user_data)
{
	const struct can_driver_api *api = (const struct can_driver_api *)dev->api;

	api->set_state_change_callback(dev, callback, user_data);
}

/** @} */

/**
 * @name CAN utility functions
 *
 * @{
 */

/**
 * @brief Convert from Data Length Code (DLC) to the number of data bytes
 *
 * @param dlc Data Length Code (DLC).
 *
 * @retval Number of bytes.
 */
static inline uint8_t can_dlc_to_bytes(uint8_t dlc)
{
	static const uint8_t dlc_table[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12,
					    16, 20, 24, 32, 48, 64};

	return dlc > 0x0F ? 64 : dlc_table[dlc];
}

/**
 * @brief Convert from number of bytes to Data Length Code (DLC)
 *
 * @param num_bytes Number of bytes.
 *
 * @retval Data Length Code (DLC).
 */
static inline uint8_t can_bytes_to_dlc(uint8_t num_bytes)
{
	return num_bytes <= 8  ? num_bytes :
	       num_bytes <= 12 ? 9 :
	       num_bytes <= 16 ? 10 :
	       num_bytes <= 20 ? 11 :
	       num_bytes <= 24 ? 12 :
	       num_bytes <= 32 ? 13 :
	       num_bytes <= 48 ? 14 :
	       15;
}

/** @} */

/**
 * @name Linux SocketCAN compatibility
 *
 * The following structures and functions provide compatibility with the CAN
 * frame and CAN filter formats used by Linux SocketCAN.
 *
 * @{
 */

/**
 * CAN Identifier structure for Linux SocketCAN compatibility.
 *
 * The fields in this type are:
 *
 * @code{.text}
 *
 * +------+--------------------------------------------------------------+
 * | Bits | Description                                                  |
 * +======+==============================================================+
 * | 0-28 | CAN identifier (11/29 bit)                                   |
 * +------+--------------------------------------------------------------+
 * |  29  | Error message frame flag (0 = data frame, 1 = error message) |
 * +------+--------------------------------------------------------------+
 * |  30  | Remote transmission request flag (1 = RTR frame)             |
 * +------+--------------------------------------------------------------+
 * |  31  | Frame format flag (0 = standard 11 bit, 1 = extended 29 bit) |
 * +------+--------------------------------------------------------------+
 *
 * @endcode
 */
typedef uint32_t canid_t;

/**
 * @brief CAN frame for Linux SocketCAN compatibility.
 */
struct can_frame {
	/** 32-bit CAN ID + EFF/RTR/ERR flags. */
	canid_t can_id;

	/** The data length code (DLC). */
	uint8_t can_dlc;

	/** @cond INTERNAL_HIDDEN */
	uint8_t pad;   /* padding. */
	uint8_t res0;  /* reserved/padding. */
	uint8_t res1;  /* reserved/padding. */
	/** @endcond */

	/** The payload data. */
	uint8_t data[CAN_MAX_DLEN];
};

/**
 * @brief CAN filter for Linux SocketCAN compatibility.
 *
 * A filter is considered a match when `received_can_id & mask == can_id & can_mask`.
 */
struct can_filter {
	/** The CAN identifier to match. */
	canid_t can_id;
	/** The mask applied to @a can_id for matching. */
	canid_t can_mask;
};

/**
 * @brief Translate a @a can_frame struct to a @a zcan_frame struct.
 *
 * @param frame  Pointer to can_frame struct.
 * @param zframe Pointer to zcan_frame struct.
 */
static inline void can_copy_frame_to_zframe(const struct can_frame *frame,
					    struct zcan_frame *zframe)
{
	zframe->id_type = (frame->can_id & BIT(31)) >> 31;
	zframe->rtr = (frame->can_id & BIT(30)) >> 30;
	zframe->id = frame->can_id & BIT_MASK(29);
	zframe->dlc = frame->can_dlc;
	memcpy(zframe->data, frame->data, sizeof(zframe->data));
}

/**
 * @brief Translate a @a zcan_frame struct to a @a can_frame struct.
 *
 * @param zframe Pointer to zcan_frame struct.
 * @param frame  Pointer to can_frame struct.
 */
static inline void can_copy_zframe_to_frame(const struct zcan_frame *zframe,
					    struct can_frame *frame)
{
	frame->can_id = (zframe->id_type << 31) | (zframe->rtr << 30) |	zframe->id;
	frame->can_dlc = zframe->dlc;
	memcpy(frame->data, zframe->data, sizeof(frame->data));
}

/**
 * @brief Translate a @a can_filter struct to a @a zcan_filter struct.
 *
 * @param filter  Pointer to can_filter struct.
 * @param zfilter Pointer to zcan_filter struct.
 */
static inline void can_copy_filter_to_zfilter(const struct can_filter *filter,
					      struct zcan_filter *zfilter)
{
	zfilter->id_type = (filter->can_id & BIT(31)) >> 31;
	zfilter->rtr = (filter->can_id & BIT(30)) >> 30;
	zfilter->id = filter->can_id & BIT_MASK(29);
	zfilter->rtr_mask = (filter->can_mask & BIT(30)) >> 30;
	zfilter->id_mask = filter->can_mask & BIT_MASK(29);
}

/**
 * @brief Translate a @a zcan_filter struct to a @a can_filter struct.
 *
 * @param zfilter Pointer to zcan_filter struct.
 * @param filter  Pointer to can_filter struct.
 */
static inline void can_copy_zfilter_to_filter(const struct zcan_filter *zfilter,
					      struct can_filter *filter)
{
	filter->can_id = (zfilter->id_type << 31) |
		(zfilter->rtr << 30) | zfilter->id;
	filter->can_mask = (zfilter->rtr_mask << 30) |
		(zfilter->id_type << 31) | zfilter->id_mask;
}

/** @} */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#include <syscalls/can.h>

#endif /* ZEPHYR_INCLUDE_DRIVERS_CAN_H_ */
