/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log_output.h>
#include <logging/log_ctrl.h>
#include <logging/log.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>

#define LOG_COLOR_CODE_DEFAULT "\x1B[0m"
#define LOG_COLOR_CODE_RED     "\x1B[1;31m"
#define LOG_COLOR_CODE_YELLOW  "\x1B[1;33m"

#define HEXDUMP_BYTES_IN_LINE 8

#define  DROPPED_COLOR_PREFIX \
	_LOG_EVAL(CONFIG_LOG_BACKEND_SHOW_COLOR, (LOG_COLOR_CODE_RED), ())

#define DROPPED_COLOR_POSTFIX \
	_LOG_EVAL(CONFIG_LOG_BACKEND_SHOW_COLOR, (LOG_COLOR_CODE_DEFAULT), ())

static const char *const severity[] = {
	NULL,
	"err",
	"wrn",
	"inf",
	"dbg"
};

static const char *const colors[] = {
	NULL,
	LOG_COLOR_CODE_RED,     /* err */
	LOG_COLOR_CODE_YELLOW,  /* warn */
	NULL,                   /* info */
	NULL                    /* dbg */
};

static u32_t freq;
static u32_t timestamp_div;

typedef int (*out_func_t)(int c, void *ctx);

extern int _prf(int (*func)(), void *dest, char *format, va_list vargs);
extern void _vprintk(out_func_t out, void *log_output,
		     const char *fmt, va_list ap);

/* The RFC 5424 allows very flexible mapping and suggest the value 0 being the
 * highest severity and 7 to be the lowest (debugging level) severity.
 *
 *    0   Emergency      System is unusable
 *    1   Alert          Action must be taken immediately
 *    2   Critical       Critical conditions
 *    3   Error          Error conditions
 *    4   Warning        Warning conditions
 *    5   Notice         Normal but significant condition
 *    6   Informational  Informational messages
 *    7   Debug          Debug-level messages
 */
static int level_to_rfc5424_severity(u32_t level)
{
	u8_t ret;

	switch (level) {
	case LOG_LEVEL_NONE:
		ret = 7;
		break;
	case LOG_LEVEL_ERR:
		ret =  3;
		break;
	case LOG_LEVEL_WRN:
		ret =  4;
		break;
	case LOG_LEVEL_INF:
		ret =  6;
		break;
	case LOG_LEVEL_DBG:
		ret = 7;
		break;
	default:
		ret = 7;
		break;
	}

	return ret;
}

static int out_func(int c, void *ctx)
{
	const struct log_output *out_ctx =
					(const struct log_output *)ctx;

	out_ctx->buf[out_ctx->control_block->offset] = (u8_t)c;
	out_ctx->control_block->offset++;

	assert(out_ctx->control_block->offset <= out_ctx->size);

	if (out_ctx->control_block->offset == out_ctx->size) {
		log_output_flush(out_ctx);
	}

	return 0;
}

static int print_formatted(const struct log_output *log_output,
			   const char *fmt, ...)
{
	va_list args;
	int length = 0;

	va_start(args, fmt);
#if !defined(CONFIG_NEWLIB_LIBC) && !defined(CONFIG_ARCH_POSIX)
	length = _prf(out_func, (void *)log_output, (char *)fmt, args);
#else
	_vprintk(out_func, (void *)log_output, fmt, args);
#endif
	va_end(args);

	return length;
}

static void buffer_write(log_output_func_t outf, u8_t *buf, size_t len,
			 void *ctx)
{
	int processed;

	do {
		processed = outf(buf, len, ctx);
		len -= processed;
		buf += processed;
	} while (len != 0);
}

void log_output_flush(const struct log_output *log_output)
{
	buffer_write(log_output->func, log_output->buf,
		     log_output->control_block->offset,
		     log_output->control_block->ctx);

	log_output->control_block->offset = 0;
}

static int timestamp_print(struct log_msg *msg,
			   const struct log_output *log_output,
			   u32_t flags)
{
	int length;
	u32_t timestamp = log_msg_timestamp_get(msg);
	bool format =
		(flags & LOG_OUTPUT_FLAG_FORMAT_TIMESTAMP) |
		(flags & LOG_OUTPUT_FLAG_FORMAT_SYSLOG);


	if (!format) {
		length = print_formatted(log_output, "[%08lu] ", timestamp);
	} else if (freq != 0) {
		u32_t remainder;
		u32_t seconds;
		u32_t hours;
		u32_t mins;
		u32_t ms;
		u32_t us;

		timestamp /= timestamp_div;
		seconds = timestamp / freq;
		hours = seconds / 3600;
		seconds -= hours * 3600;
		mins = seconds / 60;
		seconds -= mins * 60;

		remainder = timestamp % freq;
		ms = (remainder * 1000) / freq;
		us = (1000 * (1000 * remainder - (ms * freq))) / freq;

		if (IS_ENABLED(CONFIG_LOG_BACKEND_NET) &&
		    flags & LOG_OUTPUT_FLAG_FORMAT_SYSLOG) {
#if defined(CONFIG_NEWLIB_LIBC)
			char time_str[sizeof("1970-01-01T00:00:00")];
			struct tm *tm;
			time_t time;

			time = seconds;
			tm = gmtime(&time);

			strftime(time_str, sizeof(time_str), "%FT%T", tm);

			length = print_formatted(log_output, "%s.%06dZ ",
						 time_str, ms * 1000 + us);
#else
			length = print_formatted(log_output,
					"1970-01-01T%02d:%02d:%02d.%06dZ ",
					hours, mins, seconds, ms * 1000 + us);
#endif
		} else {
			length = print_formatted(log_output,
						 "[%02d:%02d:%02d.%03d,%03d] ",
						 hours, mins, seconds, ms, us);
		}
	} else {
		length = 0;
	}

	return length;
}

static void color_print(struct log_msg *msg,
			const struct log_output *log_output,
			bool color,
			bool start)
{
	if (color) {
		u32_t level = log_msg_level_get(msg);

		if (colors[level] != NULL) {
			const char *color = start ?
					 colors[level] : LOG_COLOR_CODE_DEFAULT;

			print_formatted(log_output, "%s", color);
		}
	}
}

static void color_prefix(struct log_msg *msg,
			 const struct log_output *log_output,
			 bool color)
{
	color_print(msg, log_output, color, true);
}

static void color_postfix(struct log_msg *msg,
			  const struct log_output *log_output,
			  bool color)
{
	color_print(msg, log_output, color, false);
}


static int ids_print(struct log_msg *msg, const struct log_output *log_output,
		     bool level_on, bool func_on)
{
	u32_t domain_id = log_msg_domain_id_get(msg);
	u32_t source_id = log_msg_source_id_get(msg);
	u32_t level = log_msg_level_get(msg);
	int total = 0;

	if (level_on) {
		total += print_formatted(log_output, "<%s> ", severity[level]);
	}

	total += print_formatted(log_output,
				(func_on &&
				((1 << level) & LOG_FUNCTION_PREFIX_MASK)) ?
				"%s." : "%s: ",
				log_source_name_get(domain_id, source_id));

	return total;
}

static void newline_print(const struct log_output *ctx, u32_t flags)
{
	if (IS_ENABLED(CONFIG_LOG_BACKEND_NET) &&
	    flags & LOG_OUTPUT_FLAG_FORMAT_SYSLOG) {
		return;
	}

	if ((flags & LOG_OUTPUT_FLAG_CRLF_NONE) != 0) {
		return;
	}

	if ((flags & LOG_OUTPUT_FLAG_CRLF_LFONLY) != 0) {
		print_formatted(ctx, "\n");
	} else {
		print_formatted(ctx, "\r\n");
	}
}

static void std_print(struct log_msg *msg,
		      const struct log_output *log_output)
{
	const char *str = log_msg_str_get(msg);
	u32_t nargs = log_msg_nargs_get(msg);
	u32_t *args = alloca(sizeof(u32_t)*nargs);
	int i;

	for (i = 0; i < nargs; i++) {
		args[i] = log_msg_arg_get(msg, i);
	}

	switch (log_msg_nargs_get(msg)) {
	case 0:
		print_formatted(log_output, str);
		break;
	case 1:
		print_formatted(log_output, str, args[0]);
		break;
	case 2:
		print_formatted(log_output, str, args[0], args[1]);
		break;
	case 3:
		print_formatted(log_output, str, args[0], args[1], args[2]);
		break;
	case 4:
		print_formatted(log_output, str, args[0], args[1], args[2],
				args[3]);
		break;
	case 5:
		print_formatted(log_output, str, args[0], args[1], args[2],
				args[3], args[4]);
		break;
	case 6:
		print_formatted(log_output, str, args[0], args[1], args[2],
				args[3], args[4], args[5]);
		break;
	case 7:
		print_formatted(log_output, str, args[0], args[1], args[2],
				args[3], args[4], args[5], args[6]);
		break;
	case 8:
		print_formatted(log_output, str, args[0], args[1], args[2],
				args[3], args[4], args[5], args[6], args[7]);
		break;
	case 9:
		print_formatted(log_output, str, args[0], args[1], args[2],
				args[3], args[4], args[5], args[6],  args[7],
				args[8]);
		break;
	case 10:
		print_formatted(log_output, str, args[0], args[1], args[2],
				args[3], args[4], args[5], args[6],  args[7],
				args[8], args[9]);
		break;
	case 11:
		print_formatted(log_output, str, args[0], args[1], args[2],
				args[3], args[4], args[5], args[6],  args[7],
				args[8], args[9], args[10]);
		break;
	case 12:
		print_formatted(log_output, str, args[0], args[1], args[2],
				args[3], args[4], args[5], args[6],  args[7],
				args[8], args[9], args[10], args[11]);
		break;
	case 13:
		print_formatted(log_output, str, args[0], args[1], args[2],
				args[3], args[4], args[5], args[6],  args[7],
				args[8], args[9], args[10], args[11], args[12]);
		break;
	case 14:
		print_formatted(log_output, str, args[0], args[1], args[2],
				args[3], args[4], args[5], args[6],  args[7],
				args[8], args[9], args[10], args[11], args[12],
				args[13]);
		break;
	case 15:
		print_formatted(log_output, str, args[0], args[1], args[2],
				args[3], args[4], args[5], args[6],  args[7],
				args[8], args[9], args[10], args[11], args[12],
				args[13], args[14]);
		break;
	default:
		/* Unsupported number of arguments. */
		assert(true);
		break;
	}
}

static u32_t hexdump_line_print(struct log_msg *msg,
				const struct log_output *log_output,
				int prefix_offset,
				u32_t offset, u32_t flags)
{
	u8_t buf[HEXDUMP_BYTES_IN_LINE];
	size_t length = sizeof(buf);

	log_msg_hexdump_data_get(msg, buf, &length, offset);

	if (length > 0) {
		newline_print(log_output, flags);

		for (int i = 0; i < prefix_offset; i++) {
			print_formatted(log_output, " ");
		}

		for (int i = 0; i < HEXDUMP_BYTES_IN_LINE; i++) {
			if (i < length) {
				print_formatted(log_output, "%02x ", buf[i]);
			} else {
				print_formatted(log_output, "   ");
			}
		}

		print_formatted(log_output, "|");

		for (int i = 0; i < HEXDUMP_BYTES_IN_LINE; i++) {
			if (i < length) {
				char c = (char)buf[i];

				print_formatted(log_output, "%c",
				      isprint((int)c) ? c : '.');
			} else {
				print_formatted(log_output, " ");
			}
		}
	}

	return length;
}

static void hexdump_print(struct log_msg *msg,
			  const struct log_output *log_output,
			  int prefix_offset, u32_t flags)
{
	u32_t offset = 0U;
	u32_t length;

	print_formatted(log_output, "%s", log_msg_str_get(msg));

	do {
		length = hexdump_line_print(msg, log_output, prefix_offset,
					    offset, flags);

		if (length < HEXDUMP_BYTES_IN_LINE) {
			break;
		}

		offset += length;
	} while (true);
}

static void raw_string_print(struct log_msg *msg,
			     const struct log_output *log_output)
{
	assert(log_output->size);

	size_t offset = 0;
	size_t length;
	bool eol = false;

	do {
		length = log_output->size;
		/* Sting is stored in a hexdump message. */
		log_msg_hexdump_data_get(msg, log_output->buf, &length, offset);
		log_output->control_block->offset = length;

		if (length != 0) {
			eol = (log_output->buf[length - 1] == '\n');
		}

		log_output_flush(log_output);
		offset += length;
	} while (length > 0);

	if (eol) {
		print_formatted(log_output, "\r");
	}
}

static int prefix_print(struct log_msg *msg,
			const struct log_output *log_output,
			u32_t flags, bool func_on)
{
	int length = 0;

	if (!log_msg_is_raw_string(msg)) {
		bool stamp = flags & LOG_OUTPUT_FLAG_TIMESTAMP;
		bool colors_on = flags & LOG_OUTPUT_FLAG_COLORS;
		bool level_on = flags & LOG_OUTPUT_FLAG_LEVEL;

		if (IS_ENABLED(CONFIG_LOG_BACKEND_NET) &&
		    flags & LOG_OUTPUT_FLAG_FORMAT_SYSLOG) {
			/* TODO: As there is no way to figure out the
			 * facility at this point, use a pre-defined value.
			 * Change this to use the real facility of the
			 * logging call when that info is available.
			 */
			static const int facility = 16; /* local0 */

			length += print_formatted(
				log_output,
				"<%d>1 ",
				facility * 8 +
				level_to_rfc5424_severity(
					log_msg_level_get(msg)));
		}

		if (stamp) {
			length += timestamp_print(msg, log_output, flags);
		}

		if (IS_ENABLED(CONFIG_LOG_BACKEND_NET) &&
		    flags & LOG_OUTPUT_FLAG_FORMAT_SYSLOG) {
			length += print_formatted(
				log_output, "%s - - - - ",
				log_output->control_block->hostname ?
				log_output->control_block->hostname :
				"zephyr");

		} else {
			color_prefix(msg, log_output, colors_on);
		length += ids_print(msg, log_output, level_on, func_on);
		}
	}

	return length;
}

static void postfix_print(struct log_msg *msg,
			  const struct log_output *log_output,
			  u32_t flags)
{
	if (!log_msg_is_raw_string(msg)) {
		color_postfix(msg, log_output,
			      (flags & LOG_OUTPUT_FLAG_COLORS));
		newline_print(log_output, flags);
	}
}

void log_output_msg_process(const struct log_output *log_output,
			    struct log_msg *msg,
			    u32_t flags)
{
	int prefix_offset = prefix_print(msg, log_output, flags,
					 log_msg_is_std(msg));

	if (log_msg_is_std(msg)) {
		std_print(msg, log_output);
	} else if (log_msg_is_raw_string(msg)) {
		raw_string_print(msg, log_output);
	} else {
		hexdump_print(msg, log_output, prefix_offset, flags);
	}

	postfix_print(msg, log_output, flags);

	log_output_flush(log_output);
}

void log_output_dropped_process(const struct log_output *log_output, u32_t cnt)
{
	char buf[5];
	int len;
	static const char prefix[] = DROPPED_COLOR_PREFIX "--- ";
	static const char postfix[] =
			" messages dropped ---\r\n" DROPPED_COLOR_POSTFIX;
	log_output_func_t outf = log_output->func;
	struct device *dev = (struct device *)log_output->control_block->ctx;

	cnt = min(cnt, 9999);
	len = snprintf(buf, sizeof(buf), "%d", cnt);

	buffer_write(outf, (u8_t *)prefix, sizeof(prefix) - 1, dev);
	buffer_write(outf, buf, len, dev);
	buffer_write(outf, (u8_t *)postfix, sizeof(postfix) - 1, dev);
}

void log_output_timestamp_freq_set(u32_t frequency)
{
	timestamp_div = 1U;
	/* There is no point to have frequency higher than 1MHz (ns are not
	 * printed) and too high frequency leads to overflows in calculations.
	 */
	while (frequency > 1000000) {
		frequency /= 2;
		timestamp_div *= 2;
	}

	freq = frequency;
}
