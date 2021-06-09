/*
* Wire
* Copyright (C) 2016 Wire Swiss GmbH
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <re/re.h>
#include "avs_aucodec.h"
#include "avs_log.h"
#include "avs_uuid.h"
#include "avs_zapi.h"
#include "avs_rest.h"
#include "avs_conf_pos.h"
#include "avs_icall.h"
#include "avs_msystem.h"
#include "avs_flowmgr.h"
#if USE_AVSLIB
#include "avs_voe.h"
#endif
#include "flowmgr.h"


int flowmgr_auplay_changed(struct flowmgr *fm, enum flowmgr_auplay aplay)
{
	const char *dev;
	int err = ENOSYS;
	(void)fm;

	switch (aplay) {

	case FLOWMGR_AUPLAY_EARPIECE:
		dev = "earpiece";
		break;

	case FLOWMGR_AUPLAY_SPEAKER:
		dev = "speaker";
		break;

	case FLOWMGR_AUPLAY_BT:
		dev = "bt";
		break;

	case FLOWMGR_AUPLAY_LINEOUT:
		dev = "lineout";
		break;

	case FLOWMGR_AUPLAY_SPDIF:
		dev = "spdif";
		break;

	case FLOWMGR_AUPLAY_HEADSET:
		dev = "headset";
		break;
            
	default:
		return EINVAL;
	}

#if USE_AVSLIB
	err = voe_set_auplay(dev);
#else
	(void)dev;
#endif

	return err;
}


