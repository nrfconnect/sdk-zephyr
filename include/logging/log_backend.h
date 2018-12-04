/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_LOGGING_LOG_BACKEND_H_
#define ZEPHYR_INCLUDE_LOGGING_LOG_BACKEND_H_

#include <logging/log_msg.h>
#include <assert.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Logger backend interface
 * @defgroup log_backend Logger backend interface
 * @ingroup logger
 * @{
 */

/* Forward declaration of the log_backend type. */
struct log_backend;

/**
 * @brief Logger backend API.
 */
struct log_backend_api {
	void (*put)(const struct log_backend *const backend,
		    struct log_msg *msg);

	void (*dropped)(const struct log_backend *const backend, u32_t cnt);
	void (*panic)(const struct log_backend *const backend);
	void (*init)(void);
};

/**
 * @brief Logger backend control block.
 */
struct log_backend_control_block {
	void *ctx;
	u8_t id;
	bool active;
};

/**
 * @brief Logger backend structure.
 */
struct log_backend {
	const struct log_backend_api *api;
	struct log_backend_control_block *cb;
	const char *name;
	bool autostart;
};

extern const struct log_backend __log_backends_start[0];
extern const struct log_backend __log_backends_end[0];

/**
 * @brief Macro for creating a logger backend instance.
 *
 * @param _name		Name of the backend instance.
 * @param _api		Logger backend API.
 * @param _autostart	If true backend is initialized and activated together
 *			with the logger subsystem.
 */
#define LOG_BACKEND_DEFINE(_name, _api, _autostart)			       \
	static struct log_backend_control_block UTIL_CAT(backend_cb_, _name) = \
	{								       \
		.active = false,					       \
		.id = 0,						       \
	};								       \
	static const struct log_backend _name				       \
	__attribute__ ((section(".log_backends"))) __attribute__((used)) =     \
	{								       \
		.api = &_api,						       \
		.cb = &UTIL_CAT(backend_cb_, _name),			       \
		.name = STRINGIFY(_name),				       \
		.autostart = _autostart					       \
	}


/**
 * @brief Put message with log entry to the backend.
 *
 * @param[in] backend  Pointer to the backend instance.
 * @param[in] msg      Pointer to message with log entry.
 */
static inline void log_backend_put(const struct log_backend *const backend,
				   struct log_msg *msg)
{
	__ASSERT_NO_MSG(backend);
	__ASSERT_NO_MSG(msg);
	backend->api->put(backend, msg);
}

/**
 * @brief Notify backend about dropped log messages.
 *
 * Function is optional.
 *
 * @param[in] backend  Pointer to the backend instance.
 * @param[in] cnt      Number of dropped logs since last notification.
 */
static inline void log_backend_dropped(const struct log_backend *const backend,
				       u32_t cnt)
{
	__ASSERT_NO_MSG(backend);

	if (backend->api->dropped) {
		backend->api->dropped(backend, cnt);
	}
}

/**
 * @brief Reconfigure backend to panic mode.
 *
 * @param[in] backend  Pointer to the backend instance.
 */
static inline void log_backend_panic(const struct log_backend *const backend)
{
	__ASSERT_NO_MSG(backend);
	backend->api->panic(backend);
}

/**
 * @brief Set backend id.
 *
 * @note It is used internally by the logger.
 *
 * @param backend  Pointer to the backend instance.
 * @param id       ID.
 */
static inline void log_backend_id_set(const struct log_backend *const backend,
				      u8_t id)
{
	__ASSERT_NO_MSG(backend);
	backend->cb->id = id;
}

/**
 * @brief Get backend id.
 *
 * @note It is used internally by the logger.
 *
 * @param[in] backend  Pointer to the backend instance.
 * @return    Id.
 */
static inline u8_t log_backend_id_get(const struct log_backend *const backend)
{
	__ASSERT_NO_MSG(backend);
	return backend->cb->id;
}

/**
 * @brief Get backend.
 *
 * @param[in] idx  Pointer to the backend instance.
 *
 * @return    Pointer to the backend instance.
 */
static inline const struct log_backend *log_backend_get(u32_t idx)
{
	return &__log_backends_start[idx];
}

/**
 * @brief Get number of backends.
 *
 * @return Number of backends.
 */
static inline int log_backend_count_get(void)
{
	return ((u8_t *)__log_backends_end - (u8_t *)__log_backends_start) /
			sizeof(struct log_backend);
}

/**
 * @brief Activate backend.
 *
 * @param[in] backend  Pointer to the backend instance.
 * @param[in] ctx      User context.
 */
static inline void log_backend_activate(const struct log_backend *const backend,
					void *ctx)
{
	__ASSERT_NO_MSG(backend);
	backend->cb->ctx = ctx;
	backend->cb->active = true;
}

/**
 * @brief Deactivate backend.
 *
 * @param[in] backend  Pointer to the backend instance.
 */
static inline void log_backend_deactivate(
				const struct log_backend *const backend)
{
	__ASSERT_NO_MSG(backend);
	backend->cb->active = false;
}

/**
 * @brief Check state of the backend.
 *
 * @param[in] backend  Pointer to the backend instance.
 *
 * @return True if backend is active, false otherwise.
 */
static inline bool log_backend_is_active(
				const struct log_backend *const backend)
{
	__ASSERT_NO_MSG(backend);
	return backend->cb->active;
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_LOGGING_LOG_BACKEND_H_ */
