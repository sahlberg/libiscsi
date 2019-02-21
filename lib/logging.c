/*
   Copyright (C) 2012 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined(_WIN32)
#include "win32/win32_compat.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"

void
iscsi_log_to_stderr(int level, const char *message)
{
	fprintf(stderr, "libiscsi:%d %s\n", level, message);
}

void
iscsi_set_log_fn(struct iscsi_context *iscsi, iscsi_log_fn fn)
{
	iscsi->log_fn = fn;
}

void
iscsi_log_message(struct iscsi_context *iscsi, int level, const char *format, ...)
{
        va_list ap;
	static char message[1024];
	int ret;

	if (iscsi->log_fn == NULL) {
		return;
	}

        va_start(ap, format);
	ret = vsnprintf(message, 1024, format, ap);
        va_end(ap);

	if (ret < 0) {
		return;
	}

	if (iscsi->target_name[0]) {
		static char message2[1282];

		snprintf(message2, 1282, "%s [%s]", message, iscsi->target_name);
		iscsi->log_fn(level, message2);
	}
	else
		iscsi->log_fn(level, message);
}


