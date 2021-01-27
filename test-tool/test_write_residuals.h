/*
   Copyright (C) 2013 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#ifndef _ISCSI_TESTS_WRITE_RESIDUALS_H_
#define _ISCSI_TESTS_WRITE_RESIDUALS_H_

#include <stddef.h>
#include <stdbool.h>

struct residuals_test_data {
  size_t cdb_size;             /* The size of CDB in bytes */

  unsigned int xfer_len;       /* The number of logical blocks of 
                                  data that shall be transferred */

  unsigned int buf_len;        /* Expected Data Transfer Length */

  unsigned int residuals_kind; /* Overflow or Underflow as in 
                                  enum scsi_residual */

  size_t residuals_amount;     /* The amount of residual data in bytes */

  bool check_overwrite;        /* Whether the test checks for overwrite or not */

  const char *log_messages;    /* Test case description */
};

extern bool command_is_implemented;
extern void write_residuals_test (const struct residuals_test_data *tdata);

#endif /* _ISCSI_TESTS_WRITE_RESIDUALS_H_ */
