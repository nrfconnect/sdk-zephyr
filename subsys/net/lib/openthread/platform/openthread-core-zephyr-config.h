/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *   This file includes Zephyr compile-time configuration constants
 *   for OpenThread.
 */

#ifndef OPENTHREAD_CORE_ZEPHYR_CONFIG_H_
#define OPENTHREAD_CORE_ZEPHYR_CONFIG_H_

#include <autoconf.h>
#include <devicetree.h>
#include <toolchain.h>

/**
 * @def OPENTHREAD_CONFIG_PLATFORM_ASSERT_MANAGEMENT
 *
 * The assert is managed by platform defined logic when this flag is set.
 *
 */
#ifndef OPENTHREAD_CONFIG_PLATFORM_ASSERT_MANAGEMENT
#define OPENTHREAD_CONFIG_PLATFORM_ASSERT_MANAGEMENT 1
#endif

/**
 * @def OPENTHREAD_CONFIG_NUM_MESSAGE_BUFFERS
 *
 * The number of message buffers in the buffer pool.
 *
 */
#ifdef CONFIG_OPENTHREAD_NUM_MESSAGE_BUFFERS
#define OPENTHREAD_CONFIG_NUM_MESSAGE_BUFFERS                                  \
	CONFIG_OPENTHREAD_NUM_MESSAGE_BUFFERS
#endif

/**
 * @def OPENTHREAD_CONFIG_MAX_STATECHANGE_HANDLERS
 *
 * The maximum number of state-changed callback handlers
 * (set using `otSetStateChangedCallback()`).
 *
 */
#ifdef CONFIG_OPENTHREAD_MAX_STATECHANGE_HANDLERS
#define OPENTHREAD_CONFIG_MAX_STATECHANGE_HANDLERS                             \
	CONFIG_OPENTHREAD_MAX_STATECHANGE_HANDLERS
#endif

/**
 * @def OPENTHREAD_CONFIG_TMF_ADDRESS_CACHE_ENTRIES
 *
 * The number of EID-to-RLOC cache entries.
 *
 */
#ifdef CONFIG_OPENTHREAD_TMF_ADDRESS_CACHE_ENTRIES
#define OPENTHREAD_CONFIG_TMF_ADDRESS_CACHE_ENTRIES                            \
	CONFIG_OPENTHREAD_TMF_ADDRESS_CACHE_ENTRIES
#endif

/**
 * @def OPENTHREAD_CONFIG_LOG_PREPEND_LEVEL
 *
 * Define to prepend the log level to all log messages.
 *
 */
#ifdef CONFIG_OPENTHREAD_LOG_PREPEND_LEVEL_ENABLE
#define OPENTHREAD_CONFIG_LOG_PREPEND_LEVEL 1
#endif

/**
 * @def OPENTHREAD_CONFIG_MAC_SOFTWARE_ACK_TIMEOUT_ENABLE
 *
 * Define to 1 to enable software ACK timeout logic.
 *
 */
#ifdef CONFIG_OPENTHREAD_MAC_SOFTWARE_ACK_TIMEOUT_ENABLE
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_ACK_TIMEOUT_ENABLE 1
#endif

/**
 * @def OPENTHREAD_CONFIG_MAC_SOFTWARE_RETRANSMIT_ENABLE
 *
 * Define to 1 to enable software retransmission logic.
 *
 */
#ifdef CONFIG_OPENTHREAD_MAC_SOFTWARE_RETRANSMIT_ENABLE
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_RETRANSMIT_ENABLE 1
#endif

/**
 * @def OPENTHREAD_CONFIG_MAC_SOFTWARE_CSMA_BACKOFF_ENABLE
 *
 * Define to 1 if you want to enable software CSMA-CA backoff logic.
 *
 */
#ifdef CONFIG_OPENTHREAD_MAC_SOFTWARE_CSMA_BACKOFF_ENABLE
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_CSMA_BACKOFF_ENABLE 1
#endif

/**
 * @def OPENTHREAD_CONFIG_MLE_INFORM_PREVIOUS_PARENT_ON_REATTACH
 *
 * Define as 1 for a child to inform its previous parent when it attaches to a new parent.
 *
 */
#ifdef CONFIG_OPENTHREAD_MLE_INFORM_PREVIOUS_PARENT_ON_REATTACH
#define OPENTHREAD_CONFIG_MLE_INFORM_PREVIOUS_PARENT_ON_REATTACH 1
#endif

/**
 * @def OPENTHREAD_CONFIG_PARENT_SEARCH_ENABLE
 *
 * Define as 1 to enable periodic parent search feature.
 *
 */
#ifdef CONFIG_OPENTHREAD_PARENT_SEARCH
#define OPENTHREAD_CONFIG_PARENT_SEARCH_ENABLE 1

/**
 * @def OPENTHREAD_CONFIG_PARENT_SEARCH_CHECK_INTERVAL
 *
 * Specifies the interval in seconds for a child to check the trigger condition
 * to perform a parent search.
 *
 */
#define OPENTHREAD_CONFIG_PARENT_SEARCH_CHECK_INTERVAL                         \
	CONFIG_OPENTHREAD_PARENT_SEARCH_CHECK_INTERVAL

/**
 * @def OPENTHREAD_CONFIG_PARENT_SEARCH_BACKOFF_INTERVAL
 *
 * Specifies the backoff interval in seconds for a child to not perform a parent
 * search after triggering one.
 *
 */
#define OPENTHREAD_CONFIG_PARENT_SEARCH_BACKOFF_INTERVAL                       \
	CONFIG_OPENTHREAD_PARENT_SEARCH_BACKOFF_INTERVAL

/**
 * @def OPENTHREAD_CONFIG_PARENT_SEARCH_RSS_THRESHOLD
 *
 * Specifies the RSS threshold used to trigger a parent search.
 *
 */
#define OPENTHREAD_CONFIG_PARENT_SEARCH_RSS_THRESHOLD                          \
	CONFIG_OPENTHREAD_PARENT_SEARCH_RSS_THRESHOLD
#endif

/**
 * @def OPENTHREAD_CONFIG_MAC_SOFTWARE_TX_TIMING_ENABLE
 *
 * Define to 1 to enable software transmission target time logic.
 *
 */
#ifndef OPENTHREAD_CONFIG_MAC_SOFTWARE_TX_TIMING_ENABLE
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_TX_TIMING_ENABLE                        \
	(OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2)
#endif

/**
 * @def OPENTHREAD_CONFIG_MAC_SOFTWARE_RX_TIMING_ENABLE
 *
 * Define to 1 to enable software reception target time logic.
 *
 */
#ifndef OPENTHREAD_CONFIG_MAC_SOFTWARE_RX_TIMING_ENABLE
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_RX_TIMING_ENABLE                        \
	(OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2)
#endif

/**
 * @def OPENTHREAD_CONFIG_MAC_HEADER_IE_SUPPORT
 *
 * Define as 1 to support IEEE 802.15.4-2015 Header IE (Information Element) generation and parsing,
 * it must be set to support following features:
 *    1. Time synchronization service feature (i.e., OPENTHREAD_CONFIG_TIME_SYNC_ENABLE is set).
 *    2. Thread 1.2.
 *
 * @note If it's enabled, platform must support interrupt context and concurrent access AES.
 *
 */
#ifndef OPENTHREAD_CONFIG_MAC_HEADER_IE_SUPPORT
#if OPENTHREAD_CONFIG_TIME_SYNC_ENABLE ||                                      \
	(OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2)
#define OPENTHREAD_CONFIG_MAC_HEADER_IE_SUPPORT 1
#else
#define OPENTHREAD_CONFIG_MAC_HEADER_IE_SUPPORT 0
#endif
#endif

/**
 * @def OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE
 *
 * Define to 1 if you want to enable microsecond backoff timer implemented
 * in platform.
 *
 */
#define OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE                                               \
	(CONFIG_OPENTHREAD_CSL_RECEIVER &&                                                         \
	 (OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2))

/* Zephyr does not use OpenThread's heap. mbedTLS will use heap memory allocated
 * by Zephyr. Here, we use some dummy values to prevent OpenThread warnings.
 */

/**
 * @def OPENTHREAD_CONFIG_HEAP_SIZE
 *
 * The size of heap buffer when DTLS is enabled.
 *
 */
#define OPENTHREAD_CONFIG_HEAP_INTERNAL_SIZE (4 * sizeof(void *))

/**
 * @def OPENTHREAD_CONFIG_HEAP_SIZE_NO_DTLS
 *
 * The size of heap buffer when DTLS is disabled.
 *
 */
#define OPENTHREAD_CONFIG_HEAP_INTERNAL_SIZE_NO_DTLS (4 * sizeof(void *))

/* Disable software srouce address matching. */

/**
 * @def RADIO_CONFIG_SRC_MATCH_SHORT_ENTRY_NUM
 *
 * The number of short source address table entries.
 *
 */
#define RADIO_CONFIG_SRC_MATCH_SHORT_ENTRY_NUM 0

/**
 * @def OPENTHREAD_CONFIG_PLATFORM_INFO
 *
 * The platform-specific string to insert into the OpenThread version string.
 *
 */
#ifdef CONFIG_OPENTHREAD_CONFIG_PLATFORM_INFO
#define OPENTHREAD_CONFIG_PLATFORM_INFO CONFIG_OPENTHREAD_CONFIG_PLATFORM_INFO
#endif /* CONFIG_OPENTHREAD_CONFIG_PLATFORM_INFO */

/**
 * @def OPENTHREAD_CONFIG_MLE_MAX_CHILDREN
 *
 * The maximum number of children.
 *
 */
#ifdef CONFIG_OPENTHREAD_MAX_CHILDREN
#define OPENTHREAD_CONFIG_MLE_MAX_CHILDREN CONFIG_OPENTHREAD_MAX_CHILDREN
#endif /* CONFIG_OPENTHREAD_MAX_CHILDREN */

/**
 * @def OPENTHREAD_CONFIG_MLE_IP_ADDRS_PER_CHILD
 *
 * The maximum number of supported IPv6 address registrations per child.
 *
 */
#ifdef CONFIG_OPENTHREAD_MAX_IP_ADDR_PER_CHILD
#define OPENTHREAD_CONFIG_MLE_IP_ADDRS_PER_CHILD \
	CONFIG_OPENTHREAD_MAX_IP_ADDR_PER_CHILD
#endif /* CONFIG_OPENTHREAD_MAX_IP_ADDR_PER_CHILD */

/**
 * @def RADIO_CONFIG_SRC_MATCH_EXT_ENTRY_NUM
 *
 * The number of extended source address table entries.
 *
 */
#define RADIO_CONFIG_SRC_MATCH_EXT_ENTRY_NUM 0

/**
 * @def OPENTHREAD_CONFIG_RADIO_LINK_IEEE_802_15_4_ENABLE
 *
 * Set to 1 to enable support for IEEE802.15.4 radio link.
 *
 */
#ifdef CONFIG_OPENTHREAD_RADIO_LINK_IEEE_802_15_4_ENABLE
#define OPENTHREAD_CONFIG_RADIO_LINK_IEEE_802_15_4_ENABLE \
	CONFIG_OPENTHREAD_RADIO_LINK_IEEE_802_15_4_ENABLE
#endif /* CONFIG_OPENTHREAD_RADIO_LINK_IEEE_802_15_4_ENABLE */

/**
 * @def OPENTHREAD_CONFIG_RADIO_LINK_TREL_ENABLE
 *
 * Set to 1 to enable support for Thread Radio Encapsulation Link (TREL).
 *
 */
#ifdef CONFIG_OPENTHREAD_RADIO_LINK_TREL_ENABLE
#define OPENTHREAD_CONFIG_RADIO_LINK_TREL_ENABLE \
	CONFIG_OPENTHREAD_RADIO_LINK_TREL_ENABLE
#endif /* CONFIG_OPENTHREAD_RADIO_LINK_TREL_ENABLE */

/**
 * @def OPENTHREAD_CONFIG_CSL_SAMPLE_WINDOW
 *
 * CSL sample window in units of 10 symbols.
 *
 */
#ifdef CONFIG_OPENTHREAD_CSL_SAMPLE_WINDOW
#define OPENTHREAD_CONFIG_CSL_SAMPLE_WINDOW \
	CONFIG_OPENTHREAD_CSL_SAMPLE_WINDOW
#endif /* CONFIG_OPENTHREAD_CSL_SAMPLE_WINDOW */

/**
 * @def OPENTHREAD_CONFIG_CSL_RECEIVE_TIME_AHEAD
 *
 * For some reasons, CSL receivers wake up a little later than expected. This
 * variable specifies how much time that CSL receiver would wake up earlier
 * than the expected sample window. The time is in unit of microseconds.
 *
 */
#ifdef CONFIG_OPENTHREAD_CSL_RECEIVE_TIME_AHEAD
#define OPENTHREAD_CONFIG_CSL_RECEIVE_TIME_AHEAD \
	CONFIG_OPENTHREAD_CSL_RECEIVE_TIME_AHEAD
#endif /* CONFIG_OPENTHREAD_CSL_RECEIVE_TIME_AHEAD */

/**
 * @def OPENTHREAD_CONFIG_CSL_MIN_RECEIVE_ON
 *
 * The minimum CSL receive window (in microseconds) required to receive an IEEE 802.15.4 frame.
 *
 */
#ifdef CONFIG_OPENTHREAD_CSL_MIN_RECEIVE_ON
#define OPENTHREAD_CONFIG_CSL_MIN_RECEIVE_ON CONFIG_OPENTHREAD_CSL_MIN_RECEIVE_ON
#endif /* CONFIG_OPENTHREAD_CSL_MIN_RECEIVE_ON */

/**
 * @def OPENTHREAD_CONFIG_MAC_SOFTWARE_TX_SECURITY_ENABLE
 *
 * Set to 1 to enable software transmission security logic.
 *
 */
#ifdef CONFIG_OPENTHREAD_MAC_SOFTWARE_TX_SECURITY_ENABLE
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_TX_SECURITY_ENABLE                                          \
	CONFIG_OPENTHREAD_MAC_SOFTWARE_TX_SECURITY_ENABLE
#endif /* CONFIG_OPENTHREAD_MAC_SOFTWARE_TX_SECURITY_ENABLE */

/**
 * @def OPENTHREAD_CONFIG_CLI_MAX_LINE_LENGTH
 *
 * The maximum size of the CLI line in bytes.
 *
 */
#ifdef CONFIG_OPENTHREAD_CLI_MAX_LINE_LENGTH
#define OPENTHREAD_CONFIG_CLI_MAX_LINE_LENGTH CONFIG_OPENTHREAD_CLI_MAX_LINE_LENGTH
#endif /* CONFIG_OPENTHREAD_CLI_MAX_LINE_LENGTH */

/**
 * @def OPENTHREAD_CONFIG_CLI_PROMPT_ENABLE
 *
 * Enable CLI prompt.
 *
 * When enabled, the CLI will print prompt on the output after processing a command.
 * Otherwise, no prompt is added to the output.
 *
 */
#define OPENTHREAD_CONFIG_CLI_PROMPT_ENABLE 0

/**
 * @def OPENTHREAD_CONFIG_IP6_MAX_EXT_UCAST_ADDRS
 *
 * The maximum number of supported IPv6 addresses allows to be externally added.
 *
 */
#ifdef CONFIG_OPENTHREAD_IP6_MAX_EXT_UCAST_ADDRS
#define OPENTHREAD_CONFIG_IP6_MAX_EXT_UCAST_ADDRS CONFIG_OPENTHREAD_IP6_MAX_EXT_UCAST_ADDRS
#endif /* CONFIG_OPENTHREAD_IP6_MAX_EXT_UCAST_ADDRS */

/**
 * @def OPENTHREAD_CONFIG_IP6_MAX_EXT_MCAST_ADDRS
 *
 * The maximum number of supported IPv6 multicast addresses allows to be externally added.
 *
 */
#ifdef CONFIG_OPENTHREAD_IP6_MAX_EXT_MCAST_ADDRS
#define OPENTHREAD_CONFIG_IP6_MAX_EXT_MCAST_ADDRS CONFIG_OPENTHREAD_IP6_MAX_EXT_MCAST_ADDRS
#endif /* CONFIG_OPENTHREAD_IP6_MAX_EXT_MCAST_ADDRS */

/**
 * @def OPENTHREAD_CONFIG_TCP_ENABLE
 *
 * Enable TCP.
 *
 */
#define OPENTHREAD_CONFIG_TCP_ENABLE IS_ENABLED(CONFIG_OPENTHREAD_TCP_ENABLE)

/**
 * @def OPENTHREAD_CONFIG_CLI_TCP_ENABLE
 *
 * Enable TCP in the CLI tool.
 *
 */
#define OPENTHREAD_CONFIG_CLI_TCP_ENABLE IS_ENABLED(CONFIG_OPENTHREAD_CLI_TCP_ENABLE)

/**
 * @def OPENTHREAD_CONFIG_CRYPTO_LIB
 *
 * Selects crypto backend library for OpenThread.
 *
 */
#ifdef CONFIG_OPENTHREAD_CRYPTO_PSA
#define OPENTHREAD_CONFIG_CRYPTO_LIB OPENTHREAD_CONFIG_CRYPTO_LIB_PSA
#endif

/**
 * @def OPENTHREAD_CONFIG_PLATFORM_KEY_REFERENCES_ENABLE
 *
 * Set to 1 if you want to enable key reference usage support.
 *
 */
#ifdef CONFIG_OPENTHREAD_PLATFORM_KEY_REFERENCES_ENABLE
#define OPENTHREAD_CONFIG_PLATFORM_KEY_REFERENCES_ENABLE 1
#endif

#endif  /* OPENTHREAD_CORE_ZEPHYR_CONFIG_H_ */
