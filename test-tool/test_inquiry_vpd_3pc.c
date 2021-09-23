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

#include <stdio.h>
#include <stdbool.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-private.h"
#include "iscsi-test-cu.h"

/* Third-party copy descriptor length is a multiple of four */
static int __calc_padding(int len)
{
	return (4 - (len & 3)) & 3;
}

static bool is_command_supported(int code, int action)
{
	static const struct {
		int code;
		int actions[9];
	} commands_list[] = {
		{ 0x83, { 0x1c, 0x01, 0x00, 0x10, 0x11, -1 } },
		{ 0x84, { 0x06, 0x01, 0x03, 0x04, 0x05, 0x00, 0x07, 0x08, -1 } }
	};

	unsigned int i, j;

	for (i = 0; i < ARRAY_SIZE(commands_list); i++) {
		if (commands_list[i].code == code) {
			for (j = 0; j < ARRAY_SIZE(commands_list[i].actions); j++) {
				if (commands_list[i].actions[j] == -1) {
					break;
				}
				if (commands_list[i].actions[j] == action) {
					return true;
				}
			}
		}
	}

	return false;
}

static bool verify_supported_commands(
		struct third_party_copy_supported_commands *supported_commands)
{

	struct third_party_copy_command_support *command;
	int list_length;

	command = supported_commands->commands_supported;
	list_length = supported_commands->commands_supported_list_length;

	if (supported_commands->descriptor_type != THIRD_PARTY_COPY_TYPE_SUPPORTED_COMMANDS) {
		goto failed;
	}
	if (supported_commands->descriptor_length < 1) {
		goto failed;
	}

	/*  It's mandatory command and has to include not empty list commands. */
	if (command == NULL || list_length == 0) {
		goto failed;
	}

	while (command && list_length > 0) {
		int i;

		if (command->service_actions_list_length <= 0) {
			goto failed;
		}

		for (i = 0; i < command->service_actions_list_length; i++) {
			if (is_command_supported(command->operation_code, command->service_actions[i])) {
				break;
			} else {
				goto failed;
			}
		}

		list_length -= command->service_actions_list_length + 2;
		command = command->next;
	}

	if ((command != NULL) || (list_length !=0)) {
		goto failed;
	}

	/* Check padding */
	list_length = (supported_commands->commands_supported_list_length + 1) +
			__calc_padding(supported_commands->commands_supported_list_length + 1);
	if (list_length != supported_commands->descriptor_length) {
		goto failed;
	}

	return true;

failed:
	return false;

}

static bool verify_parameter_data(
		struct third_party_copy_parameter_data *parameter_data)
{
	unsigned int i;

	if (parameter_data->descriptor_type != THIRD_PARTY_COPY_TYPE_PARAMETER_DATA) {
		goto failed;
	}
	if (parameter_data->descriptor_length != 0x001c) {
		goto failed;
	}
	for (i = 0; i < ARRAY_SIZE(parameter_data->reserved_1); i++) {
		if (parameter_data->reserved_1[i] != 0) {
			goto failed;
		}
	}
	if (parameter_data->maximum_cscd_descriptor_count < 2) {
		goto failed;
	}
	if (parameter_data->maximum_segment_descriptor_count < 1) {
		goto failed;
	}
	for (i = 0; i < ARRAY_SIZE(parameter_data->reserved_2); i++) {
		if (parameter_data->reserved_2[i] != 0) {
			goto failed;
		}
	}

	return true;

failed:
	return false;
}

static bool verify_supported_descriptors(
		struct third_party_copy_supported_descriptors *supported_descriptors)
{
	int list_length;

	if (supported_descriptors->descriptor_type != THIRD_PARTY_COPY_TYPE_SUPPORTED_DESCRIPTORS) {
		goto failed;
	}
	if (supported_descriptors->descriptor_length < 1) {
		goto failed;
	}

	/*
	 * Minimum of two descriptor types have to be supported.
	 * One for CSCD descriptor, one for Segment descriptor.
	 */
	if (supported_descriptors->descriptor_list_length < 2) {
		goto failed;
	}

	/* Check padding */
	list_length = (supported_descriptors->descriptor_list_length + 1) +
			__calc_padding(supported_descriptors->descriptor_list_length + 1);
	if (list_length != supported_descriptors->descriptor_length) {
		goto failed;
	}

	return true;

failed:
	return false;
}

static bool verify_supported_cscd_descriptors_id(
	struct third_party_copy_supported_cscd_descriptors_id *supported_cscd_descriptors_id)
{
	int list_length;

	if (supported_cscd_descriptors_id->descriptor_type != THIRD_PARTY_COPY_TYPE_SUPPORTED_CSCD_DESCRIPTORS_ID) {
		goto failed;
	}
	if (supported_cscd_descriptors_id->descriptor_length < 2) {
		goto failed;
	}

	/* The descriptor can include zero list length. */

	/* Check padding */
	list_length = (supported_cscd_descriptors_id->cscd_descriptor_ids_list_length + 1) +
			__calc_padding(supported_cscd_descriptors_id->cscd_descriptor_ids_list_length + 1);
	if (list_length != supported_cscd_descriptors_id->descriptor_length) {
		goto failed;
	}

	return true;

failed:
	return false;
}

static bool verify_general_copy_operations(
		struct third_party_copy_general_copy_operations *general_copy_operations)
{
	unsigned int i;

	if (general_copy_operations->descriptor_type != THIRD_PARTY_COPY_TYPE_GENERAL_COPY_OPERATIONS) {
		goto failed;
	}

	if (general_copy_operations->descriptor_length != 0x0020) {
		goto failed;
	}

	if (general_copy_operations->total_concurrent_copies < 1 ||
			general_copy_operations->total_concurrent_copies > 16384) {
		goto failed;
	}

	if (general_copy_operations->maximum_identified_concurrent_copies > 255) {
		goto failed;
	}

	/*
	 * The maximum_segment_length can be set to zero. It indicates that
	 * the copy manager has no limits on the amount of data.
	 */

	if (general_copy_operations->data_segment_granularity < 0) {
		goto failed;
	}

	/*
	 * The inline_data_granularity can be set to zero. It indicates that
	 * the copy manager doesn't support copying inline data to stream devices.
	 */

	for (i = 0; i < ARRAY_SIZE(general_copy_operations->reserved); i++) {
		if (general_copy_operations->reserved[1] != 0) {
			goto failed;
		}
	}

	return true;

failed:
	return false;
}

void test_inquiry_vpd_third_party_copy(void)
{
	struct scsi_inquiry_supported_pages *sup_inq;
	struct scsi_inquiry_third_party_copy *third_party_inq;
	bool third_party_page_supported = false;
	bool check_status;
	int ret, i;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test of the Third-party Copy VPD page.");

	logging(LOG_VERBOSE, "Get Supported VPD page list.");
	ret = inquiry(sd, &task,
			1, SCSI_INQUIRY_PAGECODE_SUPPORTED_VPD_PAGES, 255,
			EXPECT_STATUS_GOOD);
	CU_ASSERT_EQUAL(ret, 0);
	if (ret != 0) {
		logging(LOG_NORMAL, "[FAILED] Failed to get Supported VPD Pages.");
		goto finished;
	}

	logging(LOG_VERBOSE, "Verify we can unmarshall the DATA-IN buffer.");
	sup_inq = scsi_datain_unmarshall(task);
	CU_ASSERT_NOT_EQUAL(sup_inq, NULL);
	if (sup_inq == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to unmarshall DATA-IN buffer");
		goto finished;
	}

	for (i = 0; i < sup_inq->num_pages; i++) {
		if (sup_inq->pages[i] == SCSI_INQUIRY_PAGECODE_THIRD_PARTY_COPY) {
			third_party_page_supported = true;
			break;
		}
	}
	if (!third_party_page_supported) {
		logging(LOG_NORMAL, "[SKIPPED] Third-party Copy VPD is not implemented.");
		CU_PASS("Third-party Copy VPD is not implemented.");
		goto finished;
	}
	logging(LOG_VERBOSE, "Third-party Copy VPD page is supported.");

	logging(LOG_VERBOSE, "Get the Third-party Copy VPD page.");
	ret = inquiry(sd, &task,
			1, SCSI_INQUIRY_PAGECODE_THIRD_PARTY_COPY, 1024,
			EXPECT_STATUS_GOOD);
	CU_ASSERT_EQUAL(ret, 0);
	if (ret != 0) {
		logging(LOG_NORMAL, "[FAILED] Failed to get Third-party Copy VPD page.");
		goto finished;
	}

	logging(LOG_VERBOSE, "Verify we can unmarshall the DATA-IN buffer.");
	third_party_inq = scsi_datain_unmarshall(task);
	CU_ASSERT_NOT_EQUAL(third_party_inq, NULL);
	if (third_party_inq == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to unmarshall DATA-IN buffer.");
		goto finished;
	}

	logging(LOG_VERBOSE, "Verify Supported Commands descriptor.");
	CU_ASSERT_NOT_EQUAL(third_party_inq->supported_commands, NULL);
	if (third_party_inq->supported_commands == NULL) {
		logging(LOG_NORMAL, "[FAILED] Supported Commands descriptor is not found.");
		goto finished;
	}
	check_status = verify_supported_commands(third_party_inq->supported_commands);
	CU_ASSERT_EQUAL(check_status, true);
	if (check_status != true) {
		logging(LOG_NORMAL, "[FAILED] Supported Commands descriptor failed validation.");
		goto finished;
	}
	logging(LOG_NORMAL, "[OK] Supported Commands descriptor is valid.");

	logging(LOG_VERBOSE, "Verify Parameter Data descriptor.");
	CU_ASSERT_NOT_EQUAL(third_party_inq->parameter_data, NULL);
	if (third_party_inq->parameter_data == NULL) {
		logging(LOG_NORMAL, "[FAILED] Parameter Data descriptor is not found.");
		goto finished;
	}
	check_status = verify_parameter_data(third_party_inq->parameter_data);
	CU_ASSERT_EQUAL(check_status, true);
	if (check_status != true) {
		logging(LOG_NORMAL, "[FAILED] Parameter Data descriptor failed validation.");
		goto finished;
	}
	logging(LOG_NORMAL, "[OK] Parameter Data descriptor is valid.");

	logging(LOG_NORMAL, "Verify Supported Descriptors descriptor.");
	CU_ASSERT_NOT_EQUAL(third_party_inq->supported_descriptors, NULL);
	if (third_party_inq->supported_descriptors == NULL) {
		logging(LOG_NORMAL, "[FAILED] Supported Descriptors descriptor is not found.");
		goto finished;
	}
	check_status = verify_supported_descriptors(third_party_inq->supported_descriptors);
	CU_ASSERT_EQUAL(check_status, true);
	if (check_status != true) {
		logging(LOG_NORMAL, "[FAILED] Supported Descriptors descriptor failed validation.");
		goto finished;
	}
	logging(LOG_NORMAL, "[OK] Supported Descriptors descriptor is valid.");

	logging(LOG_NORMAL, "Verify Supported CSCD Descriptor IDs descriptor.");
	CU_ASSERT_NOT_EQUAL(third_party_inq->supported_cscd_descriptors_id, NULL);
	if (third_party_inq->supported_cscd_descriptors_id == NULL) {
		logging(LOG_NORMAL, "[FAILED] Supported CSCD Descriptor IDs descriptor is not found.");
		goto finished;
	}
	check_status = verify_supported_cscd_descriptors_id(third_party_inq->supported_cscd_descriptors_id);
	CU_ASSERT_EQUAL(check_status, true);
	if (check_status != true) {
		logging(LOG_NORMAL, "[FAILED] Supported CSCD Descriptor IDs descriptor failed validation.");
		goto finished;
	}
	logging(LOG_NORMAL, "[OK] Supported CSCD Descriptor IDs descriptor is valid.");

	logging(LOG_NORMAL, "Verify General Copy Operations descriptor.");
	CU_ASSERT_NOT_EQUAL(third_party_inq->general_copy_operations, NULL);
	if (third_party_inq->general_copy_operations == NULL) {
		logging(LOG_NORMAL, "[FAILED] General Copy Operations descriptor is not found.");
		goto finished;
	}
	check_status = verify_general_copy_operations(third_party_inq->general_copy_operations);
	CU_ASSERT_EQUAL(check_status, true);
	if (check_status != true) {
		logging(LOG_NORMAL, "[FAILED] General Copy Operations descriptor failed validation.");
		goto finished;
	}
	logging(LOG_NORMAL, "[OK] General Copy Operations descriptor is valid.");

finished:
	if (task != NULL) {
		scsi_free_scsi_task(NULL);
		task = NULL;
	}
}