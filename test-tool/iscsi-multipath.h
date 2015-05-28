/*
   iscsi test-tool multipath support

   Copyright (C) 2015 David Disseldorp

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef	_ISCSI_MULTIPATH_H_
#define	_ISCSI_MULTIPATH_H_

#define MPATH_MAX_DEVS 2
extern int mp_num_sds;
extern struct scsi_device *mp_sds[MPATH_MAX_DEVS];

#define MPATH_SKIP_IF_UNAVAILABLE(_sds, _num_sds)			\
do {									\
	if (_num_sds <= 1) {						\
		logging(LOG_NORMAL, "[SKIPPED] Multipath unavailable."	\
			" Skipping test");				\
		CU_PASS("[SKIPPED] Multipath unavailable."		\
			" Skipping test");				\
		return;							\
	}								\
} while (0);

int
mpath_check_matching_ids(int num_sds,
			 struct scsi_device **sds);

#endif	/* _ISCSI_MULTIPATH_H_ */
