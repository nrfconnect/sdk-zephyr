/*
 * Copyright (c) 2018 Intel Corporation
 * Copyright (c) 2021 Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 * @brief CoAP implementation for Zephyr.
 */

#ifndef ZEPHYR_INCLUDE_NET_COAP_H_
#define ZEPHYR_INCLUDE_NET_COAP_H_

/**
 * @brief COAP library
 * @defgroup coap COAP Library
 * @ingroup networking
 * @{
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <stdbool.h>
#include <zephyr/net/net_ip.h>

#include <zephyr/sys/slist.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set of CoAP packet options we are aware of.
 *
 * Users may add options other than these to their packets, provided
 * they know how to format them correctly. The only restriction is
 * that all options must be added to a packet in numeric order.
 *
 * Refer to RFC 7252, section 12.2 for more information.
 */
enum coap_option_num {
	COAP_OPTION_IF_MATCH = 1,
	COAP_OPTION_URI_HOST = 3,
	COAP_OPTION_ETAG = 4,
	COAP_OPTION_IF_NONE_MATCH = 5,
	COAP_OPTION_OBSERVE = 6,
	COAP_OPTION_URI_PORT = 7,
	COAP_OPTION_LOCATION_PATH = 8,
	COAP_OPTION_URI_PATH = 11,
	COAP_OPTION_CONTENT_FORMAT = 12,
	COAP_OPTION_MAX_AGE = 14,
	COAP_OPTION_URI_QUERY = 15,
	COAP_OPTION_ACCEPT = 17,
	COAP_OPTION_LOCATION_QUERY = 20,
	COAP_OPTION_BLOCK2 = 23,
	COAP_OPTION_BLOCK1 = 27,
	COAP_OPTION_SIZE2 = 28,
	COAP_OPTION_PROXY_URI = 35,
	COAP_OPTION_PROXY_SCHEME = 39,
	COAP_OPTION_SIZE1 = 60,
};

/**
 * @brief Available request methods.
 *
 * To be used when creating a request or a response.
 */
enum coap_method {
	COAP_METHOD_GET = 1,
	COAP_METHOD_POST = 2,
	COAP_METHOD_PUT = 3,
	COAP_METHOD_DELETE = 4,
	COAP_METHOD_FETCH = 5,
	COAP_METHOD_PATCH = 6,
	COAP_METHOD_IPATCH = 7,
};

#define COAP_REQUEST_MASK 0x07

#define COAP_VERSION_1 1U

/**
 * @brief CoAP packets may be of one of these types.
 */
enum coap_msgtype {
	/**
	 * Confirmable message.
	 *
	 * The packet is a request or response the destination end-point must
	 * acknowledge.
	 */
	COAP_TYPE_CON = 0,
	/**
	 * Non-confirmable message.
	 *
	 * The packet is a request or response that doesn't
	 * require acknowledgements.
	 */
	COAP_TYPE_NON_CON = 1,
	/**
	 * Acknowledge.
	 *
	 * Response to a confirmable message.
	 */
	COAP_TYPE_ACK = 2,
	/**
	 * Reset.
	 *
	 * Rejecting a packet for any reason is done by sending a message
	 * of this type.
	 */
	COAP_TYPE_RESET = 3
};

#define coap_make_response_code(class, det) ((class << 5) | (det))

/**
 * @brief Set of response codes available for a response packet.
 *
 * To be used when creating a response.
 */
enum coap_response_code {
	COAP_RESPONSE_CODE_OK = coap_make_response_code(2, 0),
	COAP_RESPONSE_CODE_CREATED = coap_make_response_code(2, 1),
	COAP_RESPONSE_CODE_DELETED = coap_make_response_code(2, 2),
	COAP_RESPONSE_CODE_VALID = coap_make_response_code(2, 3),
	COAP_RESPONSE_CODE_CHANGED = coap_make_response_code(2, 4),
	COAP_RESPONSE_CODE_CONTENT = coap_make_response_code(2, 5),
	COAP_RESPONSE_CODE_CONTINUE = coap_make_response_code(2, 31),
	COAP_RESPONSE_CODE_BAD_REQUEST = coap_make_response_code(4, 0),
	COAP_RESPONSE_CODE_UNAUTHORIZED = coap_make_response_code(4, 1),
	COAP_RESPONSE_CODE_BAD_OPTION = coap_make_response_code(4, 2),
	COAP_RESPONSE_CODE_FORBIDDEN = coap_make_response_code(4, 3),
	COAP_RESPONSE_CODE_NOT_FOUND = coap_make_response_code(4, 4),
	COAP_RESPONSE_CODE_NOT_ALLOWED = coap_make_response_code(4, 5),
	COAP_RESPONSE_CODE_NOT_ACCEPTABLE = coap_make_response_code(4, 6),
	COAP_RESPONSE_CODE_INCOMPLETE = coap_make_response_code(4, 8),
	COAP_RESPONSE_CODE_CONFLICT = coap_make_response_code(4, 9),
	COAP_RESPONSE_CODE_PRECONDITION_FAILED = coap_make_response_code(4, 12),
	COAP_RESPONSE_CODE_REQUEST_TOO_LARGE = coap_make_response_code(4, 13),
	COAP_RESPONSE_CODE_UNSUPPORTED_CONTENT_FORMAT =
						coap_make_response_code(4, 15),
	COAP_RESPONSE_CODE_UNPROCESSABLE_ENTITY = coap_make_response_code(4, 22),
	COAP_RESPONSE_CODE_TOO_MANY_REQUESTS = coap_make_response_code(4, 29),
	COAP_RESPONSE_CODE_INTERNAL_ERROR = coap_make_response_code(5, 0),
	COAP_RESPONSE_CODE_NOT_IMPLEMENTED = coap_make_response_code(5, 1),
	COAP_RESPONSE_CODE_BAD_GATEWAY = coap_make_response_code(5, 2),
	COAP_RESPONSE_CODE_SERVICE_UNAVAILABLE = coap_make_response_code(5, 3),
	COAP_RESPONSE_CODE_GATEWAY_TIMEOUT = coap_make_response_code(5, 4),
	COAP_RESPONSE_CODE_PROXYING_NOT_SUPPORTED =
						coap_make_response_code(5, 5)
};

#define COAP_CODE_EMPTY (0)

#define COAP_TOKEN_MAX_LEN 8UL

/**
 * @brief Set of Content-Format option values for CoAP.
 *
 * To be used when encoding or decoding a Content-Format option.
 */
enum coap_content_format {
	COAP_CONTENT_FORMAT_TEXT_PLAIN = 0, /* charset=urf-8 */
	COAP_CONTENT_FORMAT_APP_LINK_FORMAT = 40,
	COAP_CONTENT_FORMAT_APP_XML = 41,
	COAP_CONTENT_FORMAT_APP_OCTET_STREAM = 42,
	COAP_CONTENT_FORMAT_APP_EXI = 47,
	COAP_CONTENT_FORMAT_APP_JSON = 50,
	COAP_CONTENT_FORMAT_APP_JSON_PATCH_JSON = 51,
	COAP_CONTENT_FORMAT_APP_MERGE_PATCH_JSON = 52,
	COAP_CONTENT_FORMAT_APP_CBOR = 60,
};

/* block option helper */
#define GET_BLOCK_NUM(v)        ((v) >> 4)
#define GET_BLOCK_SIZE(v)       (((v) & 0x7))
#define GET_MORE(v)             (!!((v) & 0x08))

struct coap_observer;
struct coap_packet;
struct coap_pending;
struct coap_reply;
struct coap_resource;

/**
 * @typedef coap_method_t
 * @brief Type of the callback being called when a resource's method is
 * invoked by the remote entity.
 */
typedef int (*coap_method_t)(struct coap_resource *resource,
			     struct coap_packet *request,
			     struct sockaddr *addr, socklen_t addr_len);

/**
 * @typedef coap_notify_t
 * @brief Type of the callback being called when a resource's has observers
 * to be informed when an update happens.
 */
typedef void (*coap_notify_t)(struct coap_resource *resource,
			      struct coap_observer *observer);

/**
 * @brief Description of CoAP resource.
 *
 * CoAP servers often want to register resources, so that clients can act on
 * them, by fetching their state or requesting updates to them.
 */
struct coap_resource {
	/** Which function to be called for each CoAP method */
	coap_method_t get, post, put, del, fetch, patch, ipatch;
	coap_notify_t notify;
	const char * const *path;
	void *user_data;
	sys_slist_t observers;
	int age;
};

/**
 * @brief Represents a remote device that is observing a local resource.
 */
struct coap_observer {
	sys_snode_t list;
	struct sockaddr addr;
	uint8_t token[8];
	uint8_t tkl;
};

/**
 * @brief Representation of a CoAP Packet.
 */
struct coap_packet {
	uint8_t *data; /* User allocated buffer */
	uint16_t offset; /* CoAP lib maintains offset while adding data */
	uint16_t max_len; /* Max CoAP packet data length */
	uint8_t hdr_len; /* CoAP header length */
	uint16_t opt_len; /* Total options length (delta + len + value) */
	uint16_t delta; /* Used for delta calculation in CoAP packet */
#if defined(CONFIG_COAP_KEEP_USER_DATA)
	void *user_data; /* Application specific user data */
#endif
};

struct coap_option {
	uint16_t delta;
#if defined(CONFIG_COAP_EXTENDED_OPTIONS_LEN)
	uint16_t len;
	uint8_t value[CONFIG_COAP_EXTENDED_OPTIONS_LEN_VALUE];
#else
	uint8_t len;
	uint8_t value[12];
#endif
};

/**
 * @typedef coap_reply_t
 * @brief Helper function to be called when a response matches the
 * a pending request.
 */
typedef int (*coap_reply_t)(const struct coap_packet *response,
			    struct coap_reply *reply,
			    const struct sockaddr *from);

/**
 * @brief Represents a request awaiting for an acknowledgment (ACK).
 */
struct coap_pending {
	struct sockaddr addr;
	uint32_t t0;
	uint32_t timeout;
	uint16_t id;
	uint8_t *data;
	uint16_t len;
	uint8_t retries;
};

/**
 * @brief Represents the handler for the reply of a request, it is
 * also used when observing resources.
 */
struct coap_reply {
	coap_reply_t reply;
	void *user_data;
	int age;
	uint16_t id;
	uint8_t token[8];
	uint8_t tkl;
};

/**
 * @brief Returns the version present in a CoAP packet.
 *
 * @param cpkt CoAP packet representation
 *
 * @return the CoAP version in packet
 */
uint8_t coap_header_get_version(const struct coap_packet *cpkt);

/**
 * @brief Returns the type of the CoAP packet.
 *
 * @param cpkt CoAP packet representation
 *
 * @return the type of the packet
 */
uint8_t coap_header_get_type(const struct coap_packet *cpkt);

/**
 * @brief Returns the token (if any) in the CoAP packet.
 *
 * @param cpkt CoAP packet representation
 * @param token Where to store the token, must point to a buffer containing
 *              at least COAP_TOKEN_MAX_LEN bytes
 *
 * @return Token length in the CoAP packet (0 - COAP_TOKEN_MAX_LEN).
 */
uint8_t coap_header_get_token(const struct coap_packet *cpkt, uint8_t *token);

/**
 * @brief Returns the code of the CoAP packet.
 *
 * @param cpkt CoAP packet representation
 *
 * @return the code present in the packet
 */
uint8_t coap_header_get_code(const struct coap_packet *cpkt);

/**
 * @brief Returns the message id associated with the CoAP packet.
 *
 * @param cpkt CoAP packet representation
 *
 * @return the message id present in the packet
 */
uint16_t coap_header_get_id(const struct coap_packet *cpkt);

/**
 * @brief Returns the data pointer and length of the CoAP packet.
 *
 * @param cpkt CoAP packet representation
 * @param len Total length of CoAP payload
 *
 * @return data pointer and length if payload exists
 *         NULL pointer and length set to 0 in case there is no payload
 */
const uint8_t *coap_packet_get_payload(const struct coap_packet *cpkt,
				       uint16_t *len);

/**
 * @brief Parses the CoAP packet in data, validating it and
 * initializing @a cpkt. @a data must remain valid while @a cpkt is used.
 *
 * @param cpkt Packet to be initialized from received @a data.
 * @param data Data containing a CoAP packet, its @a data pointer is
 * positioned on the start of the CoAP packet.
 * @param len Length of the data
 * @param options Parse options and cache its details.
 * @param opt_num Number of options
 *
 * @retval 0 in case of success.
 * @retval -EINVAL in case of invalid input args.
 * @retval -EBADMSG in case of malformed coap packet header.
 * @retval -EILSEQ in case of malformed coap options.
 */
int coap_packet_parse(struct coap_packet *cpkt, uint8_t *data, uint16_t len,
		      struct coap_option *options, uint8_t opt_num);

/**
 * @brief Creates a new CoAP Packet from input data.
 *
 * @param cpkt New packet to be initialized using the storage from @a data.
 * @param data Data that will contain a CoAP packet information
 * @param max_len Maximum allowable length of data
 * @param ver CoAP header version
 * @param type CoAP header type
 * @param token_len CoAP header token length
 * @param token CoAP header token
 * @param code CoAP header code
 * @param id CoAP header message id
 *
 * @return 0 in case of success or negative in case of error.
 */
int coap_packet_init(struct coap_packet *cpkt, uint8_t *data, uint16_t max_len,
		     uint8_t ver, uint8_t type, uint8_t token_len,
		     const uint8_t *token, uint8_t code, uint16_t id);

/**
 * @brief Create a new CoAP Acknowledgment message for given request.
 *
 * This function works like @ref coap_packet_init, filling CoAP header type,
 * CoAP header token, and CoAP header message id fields according to
 * acknowledgment rules.
 *
 * @param cpkt New packet to be initialized using the storage from @a data.
 * @param req CoAP request packet that is being acknowledged
 * @param data Data that will contain a CoAP packet information
 * @param max_len Maximum allowable length of data
 * @param code CoAP header code
 *
 * @return 0 in case of success or negative in case of error.
 */
int coap_ack_init(struct coap_packet *cpkt, const struct coap_packet *req,
		  uint8_t *data, uint16_t max_len, uint8_t code);

/**
 * @brief Returns a randomly generated array of 8 bytes, that can be
 * used as a message's token.
 *
 * @return a 8-byte pseudo-random token.
 */
uint8_t *coap_next_token(void);

/**
 * @brief Helper to generate message ids
 *
 * @return a new message id
 */
uint16_t coap_next_id(void);

/**
 * @brief Return the values associated with the option of value @a
 * code.
 *
 * @param cpkt CoAP packet representation
 * @param code Option number to look for
 * @param options Array of #coap_option where to store the value
 * of the options found
 * @param veclen Number of elements in the options array
 *
 * @return The number of options found in packet matching code,
 * negative on error.
 */
int coap_find_options(const struct coap_packet *cpkt, uint16_t code,
		      struct coap_option *options, uint16_t veclen);

/**
 * @brief Appends an option to the packet.
 *
 * Note: options must be added in numeric order of their codes. Otherwise
 * error will be returned.
 * TODO: Add support for placing options according to its delta value.
 *
 * @param cpkt Packet to be updated
 * @param code Option code to add to the packet, see #coap_option_num
 * @param value Pointer to the value of the option, will be copied to the
 * packet
 * @param len Size of the data to be added
 *
 * @return 0 in case of success or negative in case of error.
 */
int coap_packet_append_option(struct coap_packet *cpkt, uint16_t code,
			      const uint8_t *value, uint16_t len);

/**
 * @brief Converts an option to its integer representation.
 *
 * Assumes that the number is encoded in the network byte order in the
 * option.
 *
 * @param option Pointer to the option value, retrieved by
 * coap_find_options()
 *
 * @return The integer representation of the option
 */
unsigned int coap_option_value_to_int(const struct coap_option *option);

/**
 * @brief Appends an integer value option to the packet.
 *
 * The option must be added in numeric order of their codes, and the
 * least amount of bytes will be used to encode the value.
 *
 * @param cpkt Packet to be updated
 * @param code Option code to add to the packet, see #coap_option_num
 * @param val Integer value to be added
 *
 * @return 0 in case of success or negative in case of error.
 */
int coap_append_option_int(struct coap_packet *cpkt, uint16_t code,
			   unsigned int val);

/**
 * @brief Append payload marker to CoAP packet
 *
 * @param cpkt Packet to append the payload marker (0xFF)
 *
 * @return 0 in case of success or negative in case of error.
 */
int coap_packet_append_payload_marker(struct coap_packet *cpkt);

/**
 * @brief Append payload to CoAP packet
 *
 * @param cpkt Packet to append the payload
 * @param payload CoAP packet payload
 * @param payload_len CoAP packet payload len
 *
 * @return 0 in case of success or negative in case of error.
 */
int coap_packet_append_payload(struct coap_packet *cpkt, const uint8_t *payload,
			       uint16_t payload_len);

/**
 * @brief When a request is received, call the appropriate methods of
 * the matching resources.
 *
 * @param cpkt Packet received
 * @param resources Array of known resources
 * @param options Parsed options from coap_packet_parse()
 * @param opt_num Number of options
 * @param addr Peer address
 * @param addr_len Peer address length
 *
 * @retval 0 in case of success.
 * @retval -ENOTSUP in case of invalid request code.
 * @retval -EPERM in case resource handler is not implemented.
 * @retval -ENOENT in case the resource is not found.
 */
int coap_handle_request(struct coap_packet *cpkt,
			struct coap_resource *resources,
			struct coap_option *options,
			uint8_t opt_num,
			struct sockaddr *addr, socklen_t addr_len);

/**
 * Represents the size of each block that will be transferred using
 * block-wise transfers [RFC7959]:
 *
 * Each entry maps directly to the value that is used in the wire.
 *
 * https://tools.ietf.org/html/rfc7959
 */
enum coap_block_size {
	COAP_BLOCK_16,
	COAP_BLOCK_32,
	COAP_BLOCK_64,
	COAP_BLOCK_128,
	COAP_BLOCK_256,
	COAP_BLOCK_512,
	COAP_BLOCK_1024,
};

/**
 * @brief Helper for converting the enumeration to the size expressed
 * in bytes.
 *
 * @param block_size The block size to be converted
 *
 * @return The size in bytes that the block_size represents
 */
static inline uint16_t coap_block_size_to_bytes(
	enum coap_block_size block_size)
{
	return (1 << (block_size + 4));
}

/**
 * @brief Represents the current state of a block-wise transaction.
 */
struct coap_block_context {
	size_t total_size;
	size_t current;
	enum coap_block_size block_size;
};

/**
 * @brief Initializes the context of a block-wise transfer.
 *
 * @param ctx The context to be initialized
 * @param block_size The size of the block
 * @param total_size The total size of the transfer, if known
 *
 * @return 0 in case of success or negative in case of error.
 */
int coap_block_transfer_init(struct coap_block_context *ctx,
			     enum coap_block_size block_size,
			     size_t total_size);

/**
 * @brief Append BLOCK1 or BLOCK2 option to the packet.
 *
 * If the CoAP packet is a request then BLOCK1 is appended
 * otherwise BLOCK2 is appended.
 *
 * @param cpkt Packet to be updated
 * @param ctx Block context from which to retrieve the
 * information for the block option
 *
 * @return 0 in case of success or negative in case of error.
 */
int coap_append_descriptive_block_option(struct coap_packet *cpkt, struct coap_block_context *ctx);

/**
 * @brief Append BLOCK1 option to the packet.
 *
 * @param cpkt Packet to be updated
 * @param ctx Block context from which to retrieve the
 * information for the Block1 option
 *
 * @return 0 in case of success or negative in case of error.
 */
int coap_append_block1_option(struct coap_packet *cpkt,
			      struct coap_block_context *ctx);

/**
 * @brief Append BLOCK2 option to the packet.
 *
 * @param cpkt Packet to be updated
 * @param ctx Block context from which to retrieve the
 * information for the Block2 option
 *
 * @return 0 in case of success or negative in case of error.
 */
int coap_append_block2_option(struct coap_packet *cpkt,
			      struct coap_block_context *ctx);

/**
 * @brief Append SIZE1 option to the packet.
 *
 * @param cpkt Packet to be updated
 * @param ctx Block context from which to retrieve the
 * information for the Size1 option
 *
 * @return 0 in case of success or negative in case of error.
 */
int coap_append_size1_option(struct coap_packet *cpkt,
			     struct coap_block_context *ctx);

/**
 * @brief Append SIZE2 option to the packet.
 *
 * @param cpkt Packet to be updated
 * @param ctx Block context from which to retrieve the
 * information for the Size2 option
 *
 * @return 0 in case of success or negative in case of error.
 */
int coap_append_size2_option(struct coap_packet *cpkt,
			     struct coap_block_context *ctx);

/**
 * @brief Get the integer representation of a CoAP option.
 *
 * @param cpkt Packet to be inspected
 * @param code CoAP option code
 *
 * @return Integer value >= 0 in case of success or negative in case
 * of error.
 */
int coap_get_option_int(const struct coap_packet *cpkt, uint16_t code);

/**
 * @brief Get the block size, more flag and block number from the
 * CoAP block1 option.
 *
 * @param cpkt Packet to be inspected
 * @param has_more Is set to the value of the more flag
 * @param block_number Is set to the number of the block
 *
 * @return Integer value of the block size in case of success
 * or negative in case of error.
 */
int coap_get_block1_option(const struct coap_packet *cpkt, bool *has_more, uint8_t *block_number);

/**
 * @brief Retrieves BLOCK{1,2} and SIZE{1,2} from @a cpkt and updates
 * @a ctx accordingly.
 *
 * @param cpkt Packet in which to look for block-wise transfers options
 * @param ctx Block context to be updated
 *
 * @return 0 in case of success or negative in case of error.
 */
int coap_update_from_block(const struct coap_packet *cpkt,
			   struct coap_block_context *ctx);

/**
 * @brief Updates @a ctx according to @a option set in @a cpkt
 * so after this is called the current entry indicates the correct
 * offset in the body of data being transferred.
 *
 * @param cpkt Packet in which to look for block-wise transfers options
 * @param ctx Block context to be updated
 * @param option Either COAP_OPTION_BLOCK1 or COAP_OPTION_BLOCK2
 *
 * @return The offset in the block-wise transfer, 0 if the transfer
 * has finished or a negative value in case of an error.
 */
int coap_next_block_for_option(const struct coap_packet *cpkt,
			       struct coap_block_context *ctx,
			       enum coap_option_num option);

/**
 * @brief Updates @a ctx so after this is called the current entry
 * indicates the correct offset in the body of data being
 * transferred.
 *
 * @param cpkt Packet in which to look for block-wise transfers options
 * @param ctx Block context to be updated
 *
 * @return The offset in the block-wise transfer, 0 if the transfer
 * has finished.
 */
size_t coap_next_block(const struct coap_packet *cpkt,
		       struct coap_block_context *ctx);

/**
 * @brief Indicates that the remote device referenced by @a addr, with
 * @a request, wants to observe a resource.
 *
 * @param observer Observer to be initialized
 * @param request Request on which the observer will be based
 * @param addr Address of the remote device
 */
void coap_observer_init(struct coap_observer *observer,
			const struct coap_packet *request,
			const struct sockaddr *addr);

/**
 * @brief After the observer is initialized, associate the observer
 * with an resource.
 *
 * @param resource Resource to add an observer
 * @param observer Observer to be added
 *
 * @return true if this is the first observer added to this resource.
 */
bool coap_register_observer(struct coap_resource *resource,
			    struct coap_observer *observer);

/**
 * @brief Remove this observer from the list of registered observers
 * of that resource.
 *
 * @param resource Resource in which to remove the observer
 * @param observer Observer to be removed
 */
void coap_remove_observer(struct coap_resource *resource,
			  struct coap_observer *observer);

/**
 * @brief Returns the observer that matches address @a addr.
 *
 * @param observers Pointer to the array of observers
 * @param len Size of the array of observers
 * @param addr Address of the endpoint observing a resource
 *
 * @return A pointer to a observer if a match is found, NULL
 * otherwise.
 */
struct coap_observer *coap_find_observer_by_addr(
	struct coap_observer *observers, size_t len,
	const struct sockaddr *addr);

/**
 * @brief Returns the next available observer representation.
 *
 * @param observers Pointer to the array of observers
 * @param len Size of the array of observers
 *
 * @return A pointer to a observer if there's an available observer,
 * NULL otherwise.
 */
struct coap_observer *coap_observer_next_unused(
	struct coap_observer *observers, size_t len);

/**
 * @brief Indicates that a reply is expected for @a request.
 *
 * @param reply Reply structure to be initialized
 * @param request Request from which @a reply will be based
 */
void coap_reply_init(struct coap_reply *reply,
		     const struct coap_packet *request);

/**
 * @brief Initialize a pending request with a request.
 *
 * The request's fields are copied into the pending struct, so @a
 * request doesn't have to live for as long as the pending struct
 * lives, but "data" that needs to live for at least that long.
 *
 * @param pending Structure representing the waiting for a
 * confirmation message, initialized with data from @a request
 * @param request Message waiting for confirmation
 * @param addr Address to send the retransmission
 * @param retries Maximum number of retransmissions of the message.
 *
 * @return 0 in case of success or negative in case of error.
 */
int coap_pending_init(struct coap_pending *pending,
		      const struct coap_packet *request,
		      const struct sockaddr *addr,
		      uint8_t retries);

/**
 * @brief Returns the next available pending struct, that can be used
 * to track the retransmission status of a request.
 *
 * @param pendings Pointer to the array of #coap_pending structures
 * @param len Size of the array of #coap_pending structures
 *
 * @return pointer to a free #coap_pending structure, NULL in case
 * none could be found.
 */
struct coap_pending *coap_pending_next_unused(
	struct coap_pending *pendings, size_t len);

/**
 * @brief Returns the next available reply struct, so it can be used
 * to track replies and notifications received.
 *
 * @param replies Pointer to the array of #coap_reply structures
 * @param len Size of the array of #coap_reply structures
 *
 * @return pointer to a free #coap_reply structure, NULL in case
 * none could be found.
 */
struct coap_reply *coap_reply_next_unused(
	struct coap_reply *replies, size_t len);

/**
 * @brief After a response is received, returns if there is any
 * matching pending request exits. User has to clear all pending
 * retransmissions related to that response by calling
 * coap_pending_clear().
 *
 * @param response The received response
 * @param pendings Pointer to the array of #coap_reply structures
 * @param len Size of the array of #coap_reply structures
 *
 * @return pointer to the associated #coap_pending structure, NULL in
 * case none could be found.
 */
struct coap_pending *coap_pending_received(
	const struct coap_packet *response,
	struct coap_pending *pendings, size_t len);

/**
 * @brief After a response is received, call coap_reply_t handler
 * registered in #coap_reply structure
 *
 * @param response A response received
 * @param from Address from which the response was received
 * @param replies Pointer to the array of #coap_reply structures
 * @param len Size of the array of #coap_reply structures
 *
 * @return Pointer to the reply matching the packet received, NULL if
 * none could be found.
 */
struct coap_reply *coap_response_received(
	const struct coap_packet *response,
	const struct sockaddr *from,
	struct coap_reply *replies, size_t len);

/**
 * @brief Returns the next pending about to expire, pending->timeout
 * informs how many ms to next expiration.
 *
 * @param pendings Pointer to the array of #coap_pending structures
 * @param len Size of the array of #coap_pending structures
 *
 * @return The next #coap_pending to expire, NULL if none is about to
 * expire.
 */
struct coap_pending *coap_pending_next_to_expire(
	struct coap_pending *pendings, size_t len);

/**
 * @brief After a request is sent, user may want to cycle the pending
 * retransmission so the timeout is updated.
 *
 * @param pending Pending representation to have its timeout updated
 *
 * @return false if this is the last retransmission.
 */
bool coap_pending_cycle(struct coap_pending *pending);

/**
 * @brief Cancels the pending retransmission, so it again becomes
 * available.
 *
 * @param pending Pending representation to be canceled
 */
void coap_pending_clear(struct coap_pending *pending);

/**
 * @brief Cancels all pending retransmissions, so they become
 * available again.
 *
 * @param pendings Pointer to the array of #coap_pending structures
 * @param len Size of the array of #coap_pending structures
 */
void coap_pendings_clear(struct coap_pending *pendings, size_t len);

/**
 * @brief Cancels awaiting for this reply, so it becomes available
 * again. User responsibility to free the memory associated with data.
 *
 * @param reply The reply to be canceled
 */
void coap_reply_clear(struct coap_reply *reply);

/**
 * @brief Cancels all replies, so they become available again.
 *
 * @param replies Pointer to the array of #coap_reply structures
 * @param len Size of the array of #coap_reply structures
 */
void coap_replies_clear(struct coap_reply *replies, size_t len);

/**
 * @brief Indicates that this resource was updated and that the @a
 * notify callback should be called for every registered observer.
 *
 * @param resource Resource that was updated
 *
 * @return 0 in case of success or negative in case of error.
 */
int coap_resource_notify(struct coap_resource *resource);

/**
 * @brief Returns if this request is enabling observing a resource.
 *
 * @param request Request to be checked
 *
 * @return True if the request is enabling observing a resource, False
 * otherwise
 */
bool coap_request_is_observe(const struct coap_packet *request);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_NET_COAP_H_ */
