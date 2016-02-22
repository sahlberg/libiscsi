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

#ifndef        _ISCSI_MULTIPATH_H_
#define        _ISCSI_MULTIPATH_H_

#define MPATH_MAX_DEVS 2
extern int mp_num_sds;
extern struct scsi_device *mp_sds[MPATH_MAX_DEVS];

int
mpath_check_matching_ids(int num_sds,
                         struct scsi_device **sds);
int
mpath_count_iscsi(int num_sds,
                  struct scsi_device **sds);
int
mpath_sd2_get_or_clone(struct scsi_device *sd1, struct scsi_device **_sd2);
void
mpath_sd2_put(struct scsi_device *sd2);

#define MPATH_SKIP_IF_UNAVAILABLE(_sds, _num_sds)                        \
do {                                                                        \
        if (_num_sds <= 1) {                                                \
                logging(LOG_NORMAL, "[SKIPPED] Multipath unavailable."        \
                        " Skipping test");                                \
                CU_PASS("[SKIPPED] Multipath unavailable."                \
                        " Skipping test");                                \
                return;                                                        \
        }                                                                \
} while (0);

#define MPATH_SKIP_UNLESS_ISCSI(_sds, _num_sds)                                \
do {                                                                        \
        if (mpath_count_iscsi(_num_sds, _sds) != _num_sds) {                \
                logging(LOG_NORMAL, "[SKIPPED] Non-iSCSI multipath."        \
                        " Skipping test");                                \
                CU_PASS("[SKIPPED] Non-iSCSI multipath."                \
                        " Skipping test");                                \
                return;                                                        \
        }                                                                \
} while (0);

#endif        /* _ISCSI_MULTIPATH_H_ */
