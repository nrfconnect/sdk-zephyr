/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_filter, CONFIG_NET_FILTER_LOG_LEVEL);

#include <zephyr/net/net_core.h>
#include <zephyr/spinlock.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_filter.h>
#include <zephyr/sys/util.h>

struct nf_hook_entries {
	sys_slist_t hook_root;
	struct k_spinlock lock;
};

struct nf_hook_entry {
	sys_snode_t node;
	nf_hook_fn_t *hook_fn;
	int priority;
};

#ifdef CONFIG_NET_IPV4
static struct nf_hook_entries hooks_ipv4[NF_IP_NUMHOOKS];
#endif

#ifdef CONFIG_NET_IPV6
static struct nf_hook_entries hooks_ipv6[NF_IP_NUMHOOKS];
#endif

static struct nf_hook_entries *nf_hook_entry_head(uint8_t pf, unsigned int hooknum)
{
	switch (pf) {
	case PF_INET:
	#ifdef CONFIG_NET_IPV4
		if (ARRAY_SIZE(hooks_ipv4) <= hooknum) {
			return NULL;
		}
		return &hooks_ipv4[hooknum];
	#endif
		break;
	case PF_INET6:
	#ifdef CONFIG_NET_IPV6
		if (ARRAY_SIZE(hooks_ipv6) <= hooknum) {
			return NULL;
		}
		return &hooks_ipv6[hooknum];
	#endif
		break;
	default:
		return NULL;
	}

	return NULL;
}

static void nf_lock_and_remove_hook(struct nf_hook_entries *entry,
				    const struct nf_hook_cfg *cfg)
{
	struct nf_hook_entry *hook;
	struct nf_hook_entry *tmp;
	sys_snode_t *prev_node = NULL;
	k_spinlock_key_t key = k_spin_lock(&entry->lock);

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&entry->hook_root, hook, tmp, node) {
		if ((cfg->hook_fn == hook->hook_fn) && (cfg->priority == hook->priority)) {
			sys_slist_remove(&entry->hook_root, prev_node, &hook->node);
			k_free(hook);
			NET_DBG("Unregistered hook %p [priority: %d]", hook->hook_fn, hook->priority);
		} else {
			prev_node = &hook->node;
		}
	}
	k_spin_unlock(&entry->lock, key);
}

static void nf_lock_and_register_hook(struct nf_hook_entries *entry,
				      struct nf_hook_entry *hook)
{
	k_spinlock_key_t key = k_spin_lock(&entry->lock);

	if (sys_slist_is_empty(&entry->hook_root)) {
		/* List is empty, just append hook. */
		sys_slist_append(&entry->hook_root, &hook->node);
	} else {
		struct nf_hook_entry *cur_hook;
		struct nf_hook_entry *prev_hook = NULL;
		SYS_SLIST_FOR_EACH_CONTAINER(&entry->hook_root, cur_hook, node) {
			if (cur_hook->priority > hook->priority) {
				break;
			}
			prev_hook = cur_hook;
		}

		if (!prev_hook) {
			/* List is not empty and first hook has higher priority, so throw in
			   new hook in front of it... */
			sys_slist_prepend(&entry->hook_root, &hook->node);
		} else {
			/* ...or between hooks otherwise. */
			sys_slist_insert(&entry->hook_root, &prev_hook->node, &hook->node);
		}
	}
	k_spin_unlock(&entry->lock, key);
}

enum net_verdict nf_hook(uint8_t pf, unsigned int hooknum, struct net_pkt *pkt)
{
	__ASSERT_NO_MSG(pkt != NULL);
	struct nf_hook_entries *entry_head = nf_hook_entry_head(pf, hooknum);
	enum net_verdict result = NET_CONTINUE;

	if (entry_head && !sys_slist_is_empty(&entry_head->hook_root)) {
		struct nf_hook_entry *hook;

		k_spinlock_key_t key = k_spin_lock(&entry_head->lock);
		SYS_SLIST_FOR_EACH_CONTAINER(&entry_head->hook_root, hook, node) {
			if (hook->hook_fn) {
				result = hook->hook_fn(pkt);
			}
			NET_DBG("Hook %p result %d", hook->hook_fn, result);
			if (result != NET_CONTINUE) {
				break;
			}
		}
		k_spin_unlock(&entry_head->lock, key);
		return result;
	}

	NET_DBG("No hooks");
	return result;
}

int nf_register_net_hook(const struct nf_hook_cfg *cfg)
{
	if (!cfg || !cfg->hook_fn) {
		NET_WARN("Invalid argument");
		return -EINVAL;
	}

	struct nf_hook_entries *entry_head = nf_hook_entry_head(cfg->pf, cfg->hooknum);

	if (!entry_head) {
		NET_WARN("No entry");
		return -ENOENT;
	}

	struct nf_hook_entry *new_hook = k_malloc(sizeof(struct nf_hook_entry));
	if (!new_hook) {
		NET_ERR("Failed to allocate memory");
		return -ENOMEM;
	}

	new_hook->hook_fn = cfg->hook_fn;
	new_hook->priority = cfg->priority;
	NET_DBG("Register new hook %p [priority: %d]", cfg->hook_fn, cfg->priority);

	nf_lock_and_register_hook(entry_head, new_hook);

	return 0;
}

int nf_register_net_hooks(const struct nf_hook_cfg *cfg, unsigned int count)
{
	int result = 0;
	unsigned int it = 0;

	NET_DBG("Register new %d hooks", count);

	for (it = 0; it < count; it++) {
		result = nf_register_net_hook(&cfg[it]);
		if (result) {
			goto error;
		}
	}
	return result;

error:
	if (it > 0) {
		nf_unregister_net_hooks(cfg, count);
	}
	return result;
}

int nf_unregister_net_hook(const struct nf_hook_cfg *cfg)
{
	if (!cfg || !cfg->hook_fn) {
		NET_WARN("Invalid argument");
		return -EINVAL;
	}

	struct nf_hook_entries *entry_head = nf_hook_entry_head(cfg->pf, cfg->hooknum);

	if (!entry_head || sys_slist_is_empty(&entry_head->hook_root)) {
		NET_WARN("No entry");
		return -ENOENT;
	}

	nf_lock_and_remove_hook(entry_head, cfg);

	return 0;
}

void nf_unregister_net_hooks(const struct nf_hook_cfg *cfg, unsigned int count)
{
	NET_DBG("Unregister %d hooks", count);

	for (unsigned int it = 0; it < count; it++) {
		(void)nf_unregister_net_hook(&cfg[it]);
	}
}
