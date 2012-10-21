/*
   Copyright (C) 2010 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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
#ifndef __iscsi_h__
#define __iscsi_h__

#include <stdint.h>

#if defined(WIN32)
#define EXTERN __declspec( dllexport )
#else
#define EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct iscsi_context;
struct sockaddr;

/*
 * Syntax for normal and portal/discovery URLs.
 */
#define ISCSI_URL_SYNTAX "\"iscsi://[<username>[%<password>]@]" \
  "<host>[:<port>]/<target-iqn>/<lun>\""
#define ISCSI_PORTAL_URL_SYNTAX "\"iscsi://[<username>[%<password>]@]" \
  "<host>[:<port>]\""


/*
 * The following three functions are used to integrate libiscsi in an event
 * system.
 */
/*
 * Returns the file descriptor that libiscsi uses.
 */
EXTERN int iscsi_get_fd(struct iscsi_context *iscsi);
/*
 * Returns which events that we need to poll for for the iscsi file descriptor.
 */
EXTERN int iscsi_which_events(struct iscsi_context *iscsi);
/*
 * Called to process the events when events become available for the iscsi
 * file descriptor.
 */
EXTERN int iscsi_service(struct iscsi_context *iscsi, int revents);
/*
 * How many commands are in flight.
 */
EXTERN int iscsi_queue_length(struct iscsi_context *iscsi);

/*
 * To set tcp keepalive for the session.
 * Only options supported by given platform (if any) are set.
 */
int iscsi_set_tcp_keepalive(struct iscsi_context *iscsi, int idle, int count, int interval);



struct iscsi_url {
       const char *portal;
       const char *target;
       const char *user;
       const char *passwd;
       int lun;
};

/*
 * This function is used to parse an iSCSI URL into a iscsi_url structure.
 * iSCSI URL format :
 * iscsi://[<username>[%<password>]@]<host>[:<port>]/<target-iqn>/<lun>
 *
 * Function will return a pointer to an iscsi url structure if successful,
 * or it will return NULL and set iscsi_get_error() accrodingly if there was a problem
 * with the URL.
 *
 * CHAP username/password can also be provided via environment variables
 * LIBISCSI_CHAP_USERNAME=ronnie
 * LIBISCSI_CHAP_PASSWORD=password
 *
 * The returned structure is freed by calling iscsi_destroy_url()
 */
EXTERN struct iscsi_url *iscsi_parse_full_url(struct iscsi_context *iscsi, const char *url);
EXTERN void iscsi_destroy_url(struct iscsi_url *iscsi_url);

/*
 * This function is used to parse an iSCSI Portal URL into a iscsi_url structure.
 * iSCSI Portal URL format :
 * iscsi://[<username>[%<password>]@]<host>[:<port>]
 *
 * iSCSI Portal URL is used when describing a discovery session, where the target-iqn and the
 * lun is not yet known.
 *
 * Function will return a pointer to an iscsi url structure if successful,
 * or it will return NULL and set iscsi_get_error() accrodingly if there was a problem
 * with the URL.
 *
 * CHAP username/password can also be provided via environment variables
 * LIBISCSI_CHAP_USERNAME=ronnie
 * LIBISCSI_CHAP_PASSWORD=password
 *
 * The returned structure is freed by calling iscsi_destroy_url()
 */
EXTERN struct iscsi_url *
iscsi_parse_portal_url(struct iscsi_context *iscsi, const char *url);

/*
 * This function returns a description of the last encountered error.
 */
EXTERN const char *iscsi_get_error(struct iscsi_context *iscsi);

/*
 * Create a context for an ISCSI session.
 * Initiator_name is the iqn name we want to identify to the target as.
 *
 * Returns:
 *  0: success
 * <0: error
 */
EXTERN struct iscsi_context *iscsi_create_context(const char *initiator_name);

/*
 * Destroy an existing ISCSI context and tear down any existing connection.
 * Callbacks for any command in flight will be invoked with
 * ISCSI_STATUS_CANCELLED.
 *
 * Returns:
 *  0: success
 * <0: error
 */
EXTERN int iscsi_destroy_context(struct iscsi_context *iscsi);

/*
 * Set an optional alias name to identify with when connecting to the target
 *
 * Returns:
 *  0: success
 * <0: error
 */
EXTERN int iscsi_set_alias(struct iscsi_context *iscsi, const char *alias);

/*
 * Set the iqn name of the taqget to login to.
 * The target name must be set before a normal-login can be initiated.
 * Only discovery-logins are possible without setting the target iqn name.
 *
 * Returns:
 *  0: success
 * <0: error
 */
EXTERN int iscsi_set_targetname(struct iscsi_context *iscsi, const char *targetname);

/*
 * This function returns any target address supplied in a login response when
 * the target has moved.
 */
EXTERN const char *iscsi_get_target_address(struct iscsi_context *iscsi);

/* Type of iscsi sessions. Discovery sessions are used to query for what
 * targets exist behind the portal connected to. Normal sessions are used to
 * log in and do I/O to the SCSI LUNs
 */
enum iscsi_session_type {
	ISCSI_SESSION_DISCOVERY = 1,
	ISCSI_SESSION_NORMAL    = 2
};
/*
 * Set the session type for a scsi context.
 * Session type can only be set/changed before the context
 * is logged in to the target.
 *
 * Returns:
 *  0: success
 * <0: error
 */
EXTERN int iscsi_set_session_type(struct iscsi_context *iscsi,
			   enum iscsi_session_type session_type);


/*
 * Types of header digest we support. Default is NONE
 */
enum iscsi_header_digest {
	ISCSI_HEADER_DIGEST_NONE        = 0,
	ISCSI_HEADER_DIGEST_NONE_CRC32C = 1,
	ISCSI_HEADER_DIGEST_CRC32C_NONE = 2,
	ISCSI_HEADER_DIGEST_CRC32C      = 3,
	ISCSI_HEADER_DIGEST_LAST        = ISCSI_HEADER_DIGEST_CRC32C
};

/*
 * Set the desired header digest for a scsi context.
 * Header digest can only be set/changed before the context
 * is logged in to the target.
 *
 * Returns:
 *  0: success
 * <0: error
 */
EXTERN int iscsi_set_header_digest(struct iscsi_context *iscsi,
			    enum iscsi_header_digest header_digest);

/*
 * Specify the username and password to use for chap authentication
 */
EXTERN int iscsi_set_initiator_username_pwd(struct iscsi_context *iscsi,
    					    const char *user,
					    const char *passwd);

/*
 * check if the context is logged in or not
 */
EXTERN int iscsi_is_logged_in(struct iscsi_context *iscsi);


enum scsi_status {
	SCSI_STATUS_GOOD                 = 0,
	SCSI_STATUS_CHECK_CONDITION      = 2,
	SCSI_STATUS_RESERVATION_CONFLICT = 0x18,
	SCSI_STATUS_REDIRECT             = 0x101,
	SCSI_STATUS_CANCELLED            = 0x0f000000,
	SCSI_STATUS_ERROR                = 0x0f000001
};


/*
 * Generic callback for completion of iscsi_*_async().
 * command_data depends on status.
 */
typedef void (*iscsi_command_cb)(struct iscsi_context *iscsi, int status,
				 void *command_data, void *private_data);



/*
 * Asynchronous call to connect a TCP connection to the target-host/port
 *
 * Returns:
 *  0 if the call was initiated and a connection will be attempted. Result of
 * the connection will be reported through the callback function.
 * <0 if there was an error. The callback function will not be invoked.
 *
 * This command is unique in that the callback can be invoked twice.
 *
 * Callback parameters :
 * status can be either of :
 *    ISCSI_STATUS_GOOD     : Connection was successful. Command_data is NULL.
 *                            In this case the callback will be invoked a
 *                            second time once the connection is torn down.
 *
 *    ISCSI_STATUS_ERROR    : Either failed to establish the connection, or
 *                            an already established connection has failed
 *                            with an error.
 *
 * The callback will NOT be invoked if the session is explicitely torn down
 * through a call to iscsi_disconnect() or iscsi_destroy_context().
 */
EXTERN int iscsi_connect_async(struct iscsi_context *iscsi, const char *portal,
			iscsi_command_cb cb, void *private_data);

/*
 * Synchronous call to connect a TCP connection to the target-host/port
 *
 * Returns:
 *  0 if connected successfully.
 * <0 if there was an error.
 *
 */
EXTERN int iscsi_connect_sync(struct iscsi_context *iscsi, const char *portal);


/*
 * Asynchronous call to connect a lun
 * This function will connect to the portal, login, and verify that the lun
 * is available.
 *
 * Returns:
 *  0 if the call was initiated and a connection will be attempted. Result
 *    of the connection will be reported through the callback function.
 * <0 if there was an error. The callback function will not be invoked.
 *
 * This command is unique in that the callback can be invoked twice.
 *
 * Callback parameters :
 * status can be either of :
 *    ISCSI_STATUS_GOOD     : Connection was successful. Command_data is NULL.
 *                            In this case the callback will be invoked a
 *                            second time once the connection is torn down.
 *
 *    ISCSI_STATUS_ERROR    : Either failed to establish the connection, or
 *                            an already established connection has failed
 *                            with an error.
 *
 * The callback will NOT be invoked if the session is explicitely torn down
 * through a call to iscsi_disconnect() or iscsi_destroy_context().
 */
EXTERN int iscsi_full_connect_async(struct iscsi_context *iscsi, const char *portal,
			     int lun, iscsi_command_cb cb, void *private_data);

/*
 * Synchronous call to connect a lun
 * This function will connect to the portal, login, and verify that the lun
 * is available.
 *
 * Returns:
 *  0 if the cconnect was successful.
 * <0 if there was an error.
 */
EXTERN int iscsi_full_connect_sync(struct iscsi_context *iscsi, const char *portal,
			    int lun);

/*
 * Disconnect a connection to a target.
 * You can not disconnect while being logged in to a target.
 *
 * Returns:
 *  0 disconnect was successful
 * <0 error
 */
EXTERN int iscsi_disconnect(struct iscsi_context *iscsi);

/*
 * Disconnect a connection to a target and try to reconnect.
 *
 * Returns:
 *  0 reconnect was successful
 * <0 error
 */
EXTERN int iscsi_reconnect(struct iscsi_context *iscsi);

/*
 * Asynchronous call to perform an ISCSI login.
 *
 * Returns:
 *  0 if the call was initiated and a login will be attempted. Result of the
 *    login will be reported through the callback function.
 * <0 if there was an error. The callback function will not be invoked.
 *
 * Callback parameters :
 * status can be either of :
 *    ISCSI_STATUS_GOOD     : login was successful. Command_data is always
 *                            NULL.
 *    ISCSI_STATUS_CANCELLED: login was aborted. Command_data is NULL.
 *    ISCSI_STATUS_ERROR    : login failed. Command_data is NULL.
 */
EXTERN int iscsi_login_async(struct iscsi_context *iscsi, iscsi_command_cb cb,
		      void *private_data);

/*
 * Synchronous call to perform an ISCSI login.
 *
 * Returns:
 *  0 if the login was successful
 * <0 if there was an error.
 */
EXTERN int iscsi_login_sync(struct iscsi_context *iscsi);


/*
 * Asynchronous call to perform an ISCSI logout.
 *
 * Returns:
 *  0 if the call was initiated and a logout will be attempted. Result of the
 *    logout will be reported through the callback function.
 * <0 if there was an error. The callback function will not be invoked.
 *
 * Callback parameters :
 * status can be either of :
 *    ISCSI_STATUS_GOOD     : logout was successful. Command_data is always
 *                            NULL.
 *    ISCSI_STATUS_CANCELLED: logout was aborted. Command_data is NULL.
 */
EXTERN int iscsi_logout_async(struct iscsi_context *iscsi, iscsi_command_cb cb,
		       void *private_data);

/*
 * Synchronous call to perform an ISCSI logout.
 *
 * Returns:
 *  0 if the logout was successful
 * <0 if there was an error.
 */
EXTERN int iscsi_logout_sync(struct iscsi_context *iscsi);


/*
 * Asynchronous call to perform an ISCSI discovery.
 *
 * discoveries can only be done on connected and logged in discovery sessions.
 *
 * Returns:
 *  0 if the call was initiated and a discovery  will be attempted. Result
 *    of the logout will be reported through the callback function.
 * <0 if there was an error. The callback function will not be invoked.
 *
 * Callback parameters :
 * status can be either of :
 *    ISCSI_STATUS_GOOD     : Discovery was successful. Command_data is a
 *                            pointer to a iscsi_discovery_address list of
 *                            structures.
 *                            This list of structures is only valid for the
 *                            duration of the callback and all data will be
 *                            freed once the callback returns.
 *    ISCSI_STATUS_CANCELLED: Discovery was aborted. Command_data is NULL.
 */
EXTERN int iscsi_discovery_async(struct iscsi_context *iscsi, iscsi_command_cb cb,
			  void *private_data);

struct iscsi_discovery_address {
       struct iscsi_discovery_address *next;
       const char *target_name;
       const char *target_address;
};

/*
 * Asynchronous call to perform an ISCSI NOP-OUT call
 *
 * Returns:
 *  0 if the call was initiated and a nop-out will be attempted. Result will
 *    be reported through the callback function.
 * <0 if there was an error. The callback function will not be invoked.
 *
 * Callback parameters :
 * status can be either of :
 *    ISCSI_STATUS_GOOD     : NOP-OUT was successful and the server responded
 *                            with a NOP-IN callback_data is a iscsi_data
 *                            structure containing the data returned from
 *                            the server.
 *    ISCSI_STATUS_CANCELLED: Discovery was aborted. Command_data is NULL.
 */
EXTERN int iscsi_nop_out_async(struct iscsi_context *iscsi, iscsi_command_cb cb,
			unsigned char *data, int len, void *private_data);

struct scsi_task;

enum iscsi_task_mgmt_funcs {
     ISCSI_TM_ABORT_TASK        = 0x01,
     ISCSI_TM_ABORT_TASK_SET    = 0x02,
     ISCSI_TM_CLEAR_ACA         = 0x03,
     ISCSI_TM_CLEAR_TASK_SET    = 0x04,
     ISCSI_TM_LUN_RESET         = 0x05,
     ISCSI_TM_TARGET_WARM_RESET = 0x06,
     ISCSI_TM_TARGET_COLD_RESET = 0x07,
     ISCSI_TM_TASK_REASSIGN     = 0x08
};

/*
 * Asynchronous call for task management
 *
 * Returns:
 *  0 if the call was initiated and the task mgmt function will be invoked.
 * the connection will be reported through the callback function.
 * <0 if there was an error. The callback function will not be invoked.
 *
 * Callback parameters :
 * status can be either of :
 *    ISCSI_STATUS_GOOD     : Connection was successful. Command_data is a pointer to uint32_t
 *                            containing the response code as per RFC3720/10.6.1
 *
 *    ISCSI_STATUS_ERROR    : Error.
 *
 * The callback will NOT be invoked if the session is explicitely torn down
 * through a call to iscsi_disconnect() or iscsi_destroy_context().
 *
 * abort_task will also cancel the scsi task. The callback for the scsi task will be invoked with
 *            SCSI_STATUS_CANCELLED
 * abort_task_set, lun_reset, target_warn_reset, target_cold_reset will cancel all tasks. The callback for
 *            all tasks will be invoked with SCSI_STATUS_CANCELLED
 */
EXTERN int
iscsi_task_mgmt_async(struct iscsi_context *iscsi,
		      int lun, enum iscsi_task_mgmt_funcs function,
		      uint32_t ritt, uint32_t rcmdscn,
		      iscsi_command_cb cb, void *private_data);

EXTERN int
iscsi_task_mgmt_abort_task_async(struct iscsi_context *iscsi,
		      struct scsi_task *task,
		      iscsi_command_cb cb, void *private_data);
EXTERN int
iscsi_task_mgmt_abort_task_set_async(struct iscsi_context *iscsi,
		      uint32_t lun,
		      iscsi_command_cb cb, void *private_data);
EXTERN int
iscsi_task_mgmt_lun_reset_async(struct iscsi_context *iscsi,
		      uint32_t lun,
		      iscsi_command_cb cb, void *private_data);
EXTERN int
iscsi_task_mgmt_target_warm_reset_async(struct iscsi_context *iscsi,
		      iscsi_command_cb cb, void *private_data);
EXTERN int
iscsi_task_mgmt_target_cold_reset_async(struct iscsi_context *iscsi,
		      iscsi_command_cb cb, void *private_data);






/* These are the possible status values for the callbacks for scsi commands.
 * The content of command_data depends on the status type.
 *
 * status :
 *   ISCSI_STATUS_GOOD the scsi command completed successfullt on the target.
 *   If this scsi command returns DATA-IN, that data is stored in an scsi_task
 *   structure returned in the command_data parameter. This buffer will be
 *   automatically freed once the callback returns.
 *
 *   ISCSI_STATUS_CHECK_CONDITION the scsi command failed with a scsi sense.
 *   Command_data contains a struct scsi_task. When the callback returns,
 *   this buffer will automatically become freed.
 *
 *   ISCSI_STATUS_CANCELLED the scsi command was aborted. Command_data is
 *   NULL.
 *
 *   ISCSI_STATUS_ERROR the command failed. Command_data is NULL.
 */

struct iscsi_data {
       int size;
       unsigned char *data;
};


/*
 * These functions will set the ISID type and value.
 * By default, contexts will automatically be assigned a 'random'
 * type and value on creation, but this can be overridden
 * by an appplication using these functions.
 *
 * Setting the ISID can only be done before loggin in to the target.
 */
EXTERN int
iscsi_set_isid_oui(struct iscsi_context *iscsi, uint32_t oui, uint32_t qualifier);
EXTERN int
iscsi_set_isid_en(struct iscsi_context *iscsi, uint32_t en, uint32_t qualifier);
EXTERN int
iscsi_set_isid_random(struct iscsi_context *iscsi, uint32_t rnd, uint32_t qualifier);
EXTERN int
iscsi_set_isid_reserved(struct iscsi_context *iscsi);




/*
 * The scsi commands use/return a scsi_task structure when invoked
 * and also through the callback.
 *
 * You must release this structure when you are finished with the task
 * by calling scsi_free_scsi_task().
 * Most of the time this means you should call this function before returning
 * from the callback.
 */

EXTERN int iscsi_scsi_command_async(struct iscsi_context *iscsi, int lun,
			     struct scsi_task *task, iscsi_command_cb cb,
			     struct iscsi_data *data, void *private_data);

/*
 * Async commands for SCSI
 *
 * These async functions return a scsi_task structure, or NULL if the command failed.
 * This structure can be used by task management functions to abort the task or a whole task set.
 */
EXTERN struct scsi_task *
iscsi_reportluns_task(struct iscsi_context *iscsi, int report_type,
			   int alloc_len, iscsi_command_cb cb,
			   void *private_data);
EXTERN struct scsi_task *
iscsi_testunitready_task(struct iscsi_context *iscsi, int lun,
			      iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_inquiry_task(struct iscsi_context *iscsi, int lun, int evpd,
			int page_code, int maxsize, iscsi_command_cb cb,
			void *private_data);
EXTERN struct scsi_task *
iscsi_readcapacity10_task(struct iscsi_context *iscsi, int lun, int lba,
			       int pmi, iscsi_command_cb cb,
			       void *private_data);
EXTERN struct scsi_task *
iscsi_readcapacity16_task(struct iscsi_context *iscsi, int lun,
			  iscsi_command_cb cb,
			  void *private_data);
EXTERN struct scsi_task *
iscsi_get_lba_status_task(struct iscsi_context *iscsi, int lun,
			  uint64_t starting_lba, uint32_t alloc_len,
			  iscsi_command_cb cb,
			  void *private_data);
EXTERN struct scsi_task *
iscsi_synchronizecache10_task(struct iscsi_context *iscsi, int lun,
				   int lba, int num_blocks, int syncnv,
				   int immed, iscsi_command_cb cb,
				   void *private_data);
EXTERN struct scsi_task *
iscsi_synchronizecache16_task(struct iscsi_context *iscsi, int lun,
				   uint64_t lba, uint32_t num_blocks, int syncnv,
				   int immed, iscsi_command_cb cb,
				   void *private_data);
EXTERN struct scsi_task *
iscsi_prefetch10_task(struct iscsi_context *iscsi, int lun,
		      uint32_t lba, int num_blocks,
		      int immed, int group,
		      iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_prefetch16_task(struct iscsi_context *iscsi, int lun,
		      uint64_t lba, int num_blocks,
		      int immed, int group,
		      iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_read6_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		       uint32_t datalen, int blocksize, iscsi_command_cb cb,
		       void *private_data);

EXTERN struct scsi_task *
iscsi_read10_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		  uint32_t datalen, int blocksize,
		  int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		  iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_write10_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_writeverify10_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_read12_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   uint32_t datalen, int blocksize,
		   int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_write12_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_writeverify12_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_read16_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   uint32_t datalen, int blocksize,
		   int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_write16_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_orwrite_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_startstopunit_task(struct iscsi_context *iscsi, int lun,
			 int immed, int pcm, int pc,
			 int no_flush, int loej, int start,
			 iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_preventallow_task(struct iscsi_context *iscsi, int lun,
			int prevent,
			iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_compareandwrite_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_writeverify16_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_verify10_task(struct iscsi_context *iscsi, int lun,
		    unsigned char *data, uint32_t datalen, uint32_t lba,
		    int vprotect, int dpo, int bytchk,
		    int blocksize, iscsi_command_cb cb,
		    void *private_data);
EXTERN struct scsi_task *
iscsi_verify12_task(struct iscsi_context *iscsi, int lun,
		    unsigned char *data, uint32_t datalen, uint32_t lba,
		    int vprotect, int dpo, int bytchk,
		    int blocksize, iscsi_command_cb cb,
		    void *private_data);
EXTERN struct scsi_task *
iscsi_verify16_task(struct iscsi_context *iscsi, int lun,
		    unsigned char *data, uint32_t datalen, uint64_t lba,
		    int vprotect, int dpo, int bytchk,
		    int blocksize, iscsi_command_cb cb,
		    void *private_data);
EXTERN struct scsi_task *
iscsi_writesame10_task(struct iscsi_context *iscsi, int lun,
		       unsigned char *data, uint32_t datalen,
		       uint32_t lba, uint16_t num_blocks,
		       int anchor, int unmap, int pbdata, int lbdata,
		       int wrprotect, int group,
		       iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_writesame16_task(struct iscsi_context *iscsi, int lun,
		       unsigned char *data, uint32_t datalen,
		       uint64_t lba, uint32_t num_blocks,
		       int anchor, int unmap, int pbdata, int lbdata,
		       int wrprotect, int group,
		       iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_modesense6_task(struct iscsi_context *iscsi, int lun, int dbd,
			   int pc, int page_code, int sub_page_code,
			   unsigned char alloc_len, iscsi_command_cb cb,
			   void *private_data);

struct unmap_list {
       uint64_t	  lba;
       uint32_t	  num;
};

EXTERN struct scsi_task *
iscsi_unmap_task(struct iscsi_context *iscsi, int lun, int anchor, int group,
		 struct unmap_list *list, int list_len,
		 iscsi_command_cb cb, void *private_data);

EXTERN struct scsi_task *
iscsi_readtoc_task(struct iscsi_context *iscsi, int lun, int msf, int format, 
		   int track_session, int maxsize, 
		   iscsi_command_cb cb, void *private_data);

EXTERN struct scsi_task *
iscsi_reserve6_task(struct iscsi_context *iscsi, int lun,
		    iscsi_command_cb cb, void *private_data);

EXTERN struct scsi_task *
iscsi_release6_task(struct iscsi_context *iscsi, int lun,
		    iscsi_command_cb cb, void *private_data);


/*
 * Sync commands for SCSI
 */
EXTERN struct scsi_task *
iscsi_scsi_command_sync(struct iscsi_context *iscsi, int lun,
			struct scsi_task *task, struct iscsi_data *data);

EXTERN struct scsi_task *
iscsi_modesense6_sync(struct iscsi_context *iscsi, int lun, int dbd,
		      int pc, int page_code, int sub_page_code,
		      unsigned char alloc_len);

EXTERN struct scsi_task *
iscsi_reportluns_sync(struct iscsi_context *iscsi, int report_type,
		      int alloc_len);

EXTERN struct scsi_task *
iscsi_testunitready_sync(struct iscsi_context *iscsi, int lun);

EXTERN struct scsi_task *
iscsi_inquiry_sync(struct iscsi_context *iscsi, int lun, int evpd,
		   int page_code, int maxsize);

EXTERN struct scsi_task *
iscsi_read6_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		  uint32_t datalen, int blocksize);

EXTERN struct scsi_task *
iscsi_read10_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		  uint32_t datalen, int blocksize,
		  int rdprotect, int dpo, int fua, int fua_nv, int group_number);

EXTERN struct scsi_task *
iscsi_write10_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number);

EXTERN struct scsi_task *
iscsi_writeverify10_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number);

EXTERN struct scsi_task *
iscsi_read12_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		  uint32_t datalen, int blocksize,
		  int rdprotect, int dpo, int fua, int fua_nv, int group_number);

EXTERN struct scsi_task *
iscsi_write12_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number);

EXTERN struct scsi_task *
iscsi_writeverify12_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number);

EXTERN struct scsi_task *
iscsi_read16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		  uint32_t datalen, int blocksize,
		  int rdprotect, int dpo, int fua, int fua_nv, int group_number);

EXTERN struct scsi_task *
iscsi_write16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number);

EXTERN struct scsi_task *
iscsi_orwrite_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number);

EXTERN struct scsi_task *
iscsi_startstopunit_sync(struct iscsi_context *iscsi, int lun,
			 int immed, int pcm, int pc,
			 int no_flush, int loej, int start);

EXTERN struct scsi_task *
iscsi_preventallow_sync(struct iscsi_context *iscsi, int lun,
			int prevent);

EXTERN struct scsi_task *
iscsi_compareandwrite_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number);

EXTERN struct scsi_task *
iscsi_writeverify16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number);

EXTERN struct scsi_task *
iscsi_readcapacity10_sync(struct iscsi_context *iscsi, int lun, int lba,
			  int pmi);

EXTERN struct scsi_task *
iscsi_readcapacity16_sync(struct iscsi_context *iscsi, int lun);

EXTERN struct scsi_task *
iscsi_get_lba_status_sync(struct iscsi_context *iscsi, int lun, uint64_t starting_lba, uint32_t alloc_len);

EXTERN struct scsi_task *
iscsi_synchronizecache10_sync(struct iscsi_context *iscsi, int lun, int lba,
			      int num_blocks, int syncnv, int immed);

EXTERN struct scsi_task *
iscsi_synchronizecache16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
			      uint32_t num_blocks, int syncnv, int immed);

EXTERN struct scsi_task *
iscsi_prefetch10_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		      int num_blocks, int immed, int group);

EXTERN struct scsi_task *
iscsi_prefetch16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		      int num_blocks, int immed, int group);

EXTERN struct scsi_task *
iscsi_verify10_sync(struct iscsi_context *iscsi, int lun,
		    unsigned char *data, uint32_t datalen, uint32_t lba,
		    int vprotect, int dpo, int bytchk,
		    int blocksize);

EXTERN struct scsi_task *
iscsi_verify12_sync(struct iscsi_context *iscsi, int lun,
		    unsigned char *data, uint32_t datalen, uint32_t lba,
		    int vprotect, int dpo, int bytchk,
		    int blocksize);

EXTERN struct scsi_task *
iscsi_verify16_sync(struct iscsi_context *iscsi, int lun,
		    unsigned char *data, uint32_t datalen, uint64_t lba,
		    int vprotect, int dpo, int bytchk,
		    int blocksize);

EXTERN struct scsi_task *
iscsi_writesame10_sync(struct iscsi_context *iscsi, int lun,
		       unsigned char *data, uint32_t datalen,
		       uint32_t lba, uint16_t num_blocks,
		       int anchor, int unmap, int pbdata, int lbdata,
		       int wrprotect, int group);

EXTERN struct scsi_task *
iscsi_writesame16_sync(struct iscsi_context *iscsi, int lun,
		       unsigned char *data, uint32_t datalen,
		       uint64_t lba, uint32_t num_blocks,
		       int anchor, int unmap, int pbdata, int lbdata,
		       int wrprotect, int group);

EXTERN struct scsi_task *
iscsi_unmap_sync(struct iscsi_context *iscsi, int lun, int anchor, int group,
		 struct unmap_list *list, int list_len);

EXTERN struct scsi_task *
iscsi_readtoc_sync(struct iscsi_context *iscsi, int lun, int msf,
		   int format, int track_session, int maxsize);

EXTERN struct scsi_task *
iscsi_reserve6_sync(struct iscsi_context *iscsi, int lun);

EXTERN struct scsi_task *
iscsi_release6_sync(struct iscsi_context *iscsi, int lun);


/*
 * This function is used when  the application wants to specify its own buffers to read the data
 * from the DATA-IN PDUs into.
 * The main use is for SCSI read operations to have them write directly into the application buffers to
 * avoid the two copies that would occur otherwise.
 * First copy from the individual DATA-IN blobs to linearize the buffer and the second in the callback
 * to copy the data from the linearized buffer into the application buffer.
 *
 * This also supports reading into a vector of buffers by calling this function multiple times.
 * The individual buffers will be filled in the same order as they were created.
 *
 * Example:
 *     task = iscsi_read10_task(    ( 2 512byte blocks into two buffers)
 *     scsi_task_add_data_in_buffer(task, first_buffer, 512
 *     scsi_task_add_data_in_buffer(task, second_buffer, 512
 *
 *
 * If you use this function you can not use task->datain in the callback.
 * task->datain.size will be 0 and
 * task->datain.data will be NULL
 */
EXTERN int scsi_task_add_data_in_buffer(struct scsi_task *task, int len, unsigned char *buf);

/*
 * This function is used when you want to cancel a scsi task.
 * The callback for the task will immediately be invoked with SCSI_STATUS_CANCELLED.
 * The cancellation is only local in libiscsi. If the task is already in-flight
 * this call will not cancel the task at the target.
 * To cancel the task also a the target you need to call the task management functions.
 */
EXTERN int
iscsi_scsi_task_cancel(struct iscsi_context *iscsi,
		  struct scsi_task *task);

/*
 * This function is used when you want to cancel all scsi tasks.
 * The callback for the tasks will immediately be invoked with SCSI_STATUS_CANCELLED.
 * The cancellation is only local in libiscsi. If the tasks are already in-flight
 * this call will not cancel the tasks at the target.
 * To cancel the tasks also a the target you need to call the task management functions.
 */
EXTERN void
iscsi_scsi_cancel_all_tasks(struct iscsi_context *iscsi);

#define DPRINTF(iscsi,level,fmt,args...) do { if ((iscsi)->debug >= level) {fprintf(stderr,"libiscsi: ");fprintf(stderr, (fmt), ##args); fprintf(stderr,"\n");} } while (0);

/*
 * This function is to set the debugging level (0=disabled).
 */
EXTERN void
iscsi_set_debug(struct iscsi_context *iscsi, int level);

/*
 * This function is to set the TCP_USER_TIMEOUT option. It has to be called after iscsi
 * context creation. The value given in ms is then applied each time a new socket is created.
 */
EXTERN void 
iscsi_set_tcp_user_timeout(struct iscsi_context *iscsi, int timeout_ms);

/*
 * This function is to set the TCP_KEEPIDLE option. It has to be called after iscsi
 * context creation. 
 */
EXTERN void 
iscsi_set_tcp_keepidle(struct iscsi_context *iscsi, int value);

/*
 * This function is to set the TCP_KEEPCNT option. It has to be called after iscsi
 * context creation. 
 */
EXTERN void 
iscsi_set_tcp_keepcnt(struct iscsi_context *iscsi, int value);

/*
 * This function is to set the TCP_KEEPINTVL option. It has to be called after iscsi
 * context creation. 
 */
EXTERN void 
iscsi_set_tcp_keepintvl(struct iscsi_context *iscsi, int value);


#ifdef __cplusplus
}
#endif

#endif /* __iscsi_h__ */
