/*
   Utility helper functions.

   Copyright (C) 2023 by zhenwei pi <pizhenwei@bytedance.com>

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
#ifndef __iscsi_utils_h__
#define __iscsi_utils_h__

#ifdef __cplusplus
extern "C" {
#endif

struct iscsi_value_string {
   int value;
   const char *string;
};

const char *iscsi_value_string_find(struct iscsi_value_string *values, int value, const char *not_found);

#ifdef __cplusplus
}
#endif

#endif /* __iscsi_utils_h__ */
