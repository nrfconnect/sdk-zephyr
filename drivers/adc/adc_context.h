/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __ADC_CONTEXT_H__
#define __ADC_CONTEXT_H__

#include <adc.h>
#include <atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

struct adc_context;

/*
 * Each driver should provide implementations of the following two functions:
 * - adc_context_start_sampling() that will be called when a sampling (of one
 *   or more channels, depending on the realized sequence) is to be started
 * - adc_context_update_buffer_pointer() that will be called when the sample
 *   buffer pointer should be prepared for writing of next sampling results,
 *   the "repeat_sampling" parameter indicates if the results should be written
 *   in the same place as before (when true) or as consecutive ones (otherwise).
 */
static void adc_context_start_sampling(struct adc_context *ctx);
static void adc_context_update_buffer_pointer(struct adc_context *ctx,
					      bool repeat_sampling);
/*
 * If a given driver uses some dedicated hardware timer to trigger consecutive
 * samplings, it should implement also the following two functions. Otherwise,
 * it should define the ADC_CONTEXT_USES_KERNEL_TIMER macro to enable parts of
 * this module that utilize a standard kernel timer.
 */
static void adc_context_enable_timer(struct adc_context *ctx);
static void adc_context_disable_timer(struct adc_context *ctx);


struct adc_context {
	atomic_t sampling_requested;
#ifdef ADC_CONTEXT_USES_KERNEL_TIMER
	struct k_timer timer;
#endif /* ADC_CONTEXT_USES_KERNEL_TIMER */

	struct k_sem lock;
	struct k_sem sync;
	int status;

#ifdef CONFIG_ADC_ASYNC
	struct k_poll_signal *signal;
	bool asynchronous;
#endif /* CONFIG_ADC_ASYNC */

	const struct adc_sequence *sequence;
	u16_t sampling_index;
};

#ifdef ADC_CONTEXT_USES_KERNEL_TIMER
#define ADC_CONTEXT_INIT_TIMER(_data, _ctx_name) \
	._ctx_name.timer = _K_TIMER_INITIALIZER(_data._ctx_name.timer, \
						adc_context_on_timer_expired, \
						NULL)
#endif /* ADC_CONTEXT_USES_KERNEL_TIMER */

#define ADC_CONTEXT_INIT_LOCK(_data, _ctx_name) \
	._ctx_name.lock = _K_SEM_INITIALIZER(_data._ctx_name.lock, 0, 1)

#define ADC_CONTEXT_INIT_SYNC(_data, _ctx_name) \
	._ctx_name.sync = _K_SEM_INITIALIZER(_data._ctx_name.sync, 0, 1)


static inline void adc_context_request_next_sampling(struct adc_context *ctx)
{
	if (atomic_inc(&ctx->sampling_requested) == 0) {
		adc_context_start_sampling(ctx);
	} else {
		/*
		 * If a sampling was already requested and was not finished yet,
		 * do not start another one from here, this will be done from
		 * adc_context_on_sampling_done() after the current sampling is
		 * complete. Instead, note this fact, and inform the user about
		 * it after the sequence is done.
		 */
		ctx->status = -EIO;
	}
}

#ifdef ADC_CONTEXT_USES_KERNEL_TIMER
static inline void adc_context_enable_timer(struct adc_context *ctx)
{
	u32_t interval_us = ctx->sequence->options->interval_us;
	u32_t interval_ms = ceiling_fraction(interval_us, 1000UL);

	k_timer_start(&ctx->timer, 0, interval_ms);
}

static inline void adc_context_disable_timer(struct adc_context *ctx)
{
	k_timer_stop(&ctx->timer);
}

static void adc_context_on_timer_expired(struct k_timer *timer_id)
{
	struct adc_context *ctx =
		CONTAINER_OF(timer_id, struct adc_context, timer);

	adc_context_request_next_sampling(ctx);
}
#endif /* ADC_CONTEXT_USES_KERNEL_TIMER */


static inline void adc_context_lock(struct adc_context *ctx,
				    bool asynchronous,
				    struct k_poll_signal *signal)
{
	k_sem_take(&ctx->lock, K_FOREVER);

#ifdef CONFIG_ADC_ASYNC
	ctx->asynchronous = asynchronous;
	ctx->signal = signal;
#endif /* CONFIG_ADC_ASYNC */
}

static inline void adc_context_release(struct adc_context *ctx, int status)
{
#ifdef CONFIG_ADC_ASYNC
	if (ctx->asynchronous && (status == 0)) {
		return;
	}
#endif /* CONFIG_ADC_ASYNC */

	k_sem_give(&ctx->lock);
}

static inline void adc_context_unlock_unconditionally(struct adc_context *ctx)
{
	if (!k_sem_count_get(&ctx->lock)) {
		k_sem_give(&ctx->lock);
	}
}

static inline int adc_context_wait_for_completion(struct adc_context *ctx)
{
#ifdef CONFIG_ADC_ASYNC
	if (ctx->asynchronous) {
		return 0;
	}
#endif /* CONFIG_ADC_ASYNC */

	k_sem_take(&ctx->sync, K_FOREVER);
	return ctx->status;
}

static inline void adc_context_complete(struct adc_context *ctx, int status)
{
#ifdef CONFIG_ADC_ASYNC
	if (ctx->asynchronous) {
		if (ctx->signal) {
			k_poll_signal(ctx->signal, status);
		}

		k_sem_give(&ctx->lock);
		return;
	}
#endif /* CONFIG_ADC_ASYNC */

	/*
	 * Override the status only when an error is signaled to this function.
	 * Please note that adc_context_request_next_sampling() might have set
	 * this field.
	 */
	if (status != 0) {
		ctx->status = status;
	}
	k_sem_give(&ctx->sync);
}

static inline void adc_context_start_read(struct adc_context *ctx,
					  const struct adc_sequence *sequence)
{
	ctx->sequence = sequence;
	ctx->status = 0;

	if (ctx->sequence->options) {
		ctx->sampling_index = 0;

		if (ctx->sequence->options->interval_us != 0) {
			atomic_set(&ctx->sampling_requested, 0);
			adc_context_enable_timer(ctx);
			return;
		}
	}

	adc_context_start_sampling(ctx);
}

/*
 * This function should be called after a sampling (of one or more channels,
 * depending on the realized sequence) is done. It calls the defined callback
 * function if required and takes further actions accordingly.
 */
static inline void adc_context_on_sampling_done(struct adc_context *ctx,
						struct device *dev)
{
	if (ctx->sequence->options) {
		adc_sequence_callback callback =
			ctx->sequence->options->callback;
		enum adc_action action;
		bool finish = false;
		bool repeat = false;

		if (callback) {
			action = callback(dev,
					  ctx->sequence,
					  ctx->sampling_index);
		} else {
			action = ADC_ACTION_CONTINUE;
		}

		switch (action) {
		case ADC_ACTION_REPEAT:
			repeat = true;
			break;
		case ADC_ACTION_FINISH:
			finish = true;
			break;
		default: /* ADC_ACTION_CONTINUE */
			if (ctx->sampling_index <
			    ctx->sequence->options->extra_samplings) {
				++ctx->sampling_index;
			} else {
				finish = true;
			}
		}

		if (!finish) {
			adc_context_update_buffer_pointer(ctx, repeat);

			/*
			 * Immediately start the next sampling if working with
			 * a zero interval or if the timer expired again while
			 * the current sampling was in progress.
			 */
			if (ctx->sequence->options->interval_us == 0) {
				adc_context_start_sampling(ctx);
			} else if (atomic_dec(&ctx->sampling_requested) > 1) {
				adc_context_start_sampling(ctx);
			}

			return;
		}

		if (ctx->sequence->options->interval_us != 0) {
			adc_context_disable_timer(ctx);
		}
	}

	adc_context_complete(ctx, 0);
}

#ifdef __cplusplus
}
#endif

#endif /* __ADC_CONTEXT_H__ */
