/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fcb.h"
#include "fcb_priv.h"

int
fcb_rotate(struct fcb *fcb)
{
	struct flash_sector *sector;
	int rc = 0;

	rc = k_mutex_lock(&fcb->f_mtx, K_FOREVER);
	if (rc) {
		return FCB_ERR_ARGS;
	}

	rc = fcb_erase_sector(fcb, fcb->f_oldest);
	if (rc) {
		rc = FCB_ERR_FLASH;
		goto out;
	}
	if (fcb->f_oldest == fcb->f_active.fe_sector) {
		/*
		 * Need to create a new active area, as we're wiping
		 * the current.
		 */
		sector = fcb_getnext_sector(fcb, fcb->f_oldest);
		rc = fcb_sector_hdr_init(fcb, sector, fcb->f_active_id + 1);
		if (rc) {
			goto out;
		}
		fcb->f_active.fe_sector = sector;
		fcb->f_active.fe_elem_off = sizeof(struct fcb_disk_area);
		fcb->f_active_id++;
	}
	fcb->f_oldest = fcb_getnext_sector(fcb, fcb->f_oldest);
out:
	k_mutex_unlock(&fcb->f_mtx);
	return rc;
}
