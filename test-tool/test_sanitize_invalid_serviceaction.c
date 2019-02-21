/* 
   Copyright (C) 2013 Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

void
test_sanitize_invalid_serviceaction(void)
{ 
        int i;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test SANITIZE for invalid serviceactions");

        CHECK_FOR_SANITIZE;

        logging(LOG_VERBOSE, "Test all invalid service actions and make sure "
                "they fail with an error");
        for (i = 0; i <= 0x1f; i++) {
                switch (i) {
                case 1:
                case 2:
                case 3:
                case 0x1f:
                        continue;
                }

                logging(LOG_VERBOSE, "Verify that ServiceAction:0x%02d is "
                        "an error.", i);

                SANITIZE(sd, 0, 0, i, 0, NULL,
                         EXPECT_INVALID_FIELD_IN_CDB);
        }
}
