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
#include <sys/types.h>

#if defined(_WIN32)
#define EXTERN __declspec( dllexport )
#else
#define EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif


struct iscsi_context;
struct sockaddr;
struct scsi_iovec;

/* API VERSION */
#define LIBISCSI_API_VERSION (20170105)

/* FEATURES */
#define LIBISCSI_FEATURE_IOVECTOR (1)
#define LIBISCSI_FEATURE_NOP_COUNTER (1)
#define LIBISCSI_FEATURE_ISER (1)

#define MAX_STRING_SIZE (255)

/*
 * Syntax for normal and portal/discovery URLs.
 */
#define ISCSI_URL_SYNTAX "\"iscsi://[<username>[%<password>]@]" \
  "<host>[:<port>]/<target-iqn>/<lun>\""
#define ISCSI_PORTAL_URL_SYNTAX "\"iscsi://[<username>[%<password>]@]" \
  "<host>[:<port>]\""

enum iscsi_transport_type {
	TCP_TRANSPORT = 0,
	ISER_TRANSPORT = 1
};

EXTERN void iscsi_set_cache_allocations(struct iscsi_context *iscsi, int ca);

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
 *
 * This function can return 0 which means that there are no events to
 * poll for at this time. In that case the application should wait some time
 * before calling iscsi_which_events() again. This could for example happen
 * if we fail to reconnect the TCP session during an automatic session
 * reconnect.
 * When this function returns 0, the application should wait >=100ms
 * before trying again.
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
 * How many commands are queued for dispatch.
 */
EXTERN int iscsi_out_queue_length(struct iscsi_context *iscsi);


/************************************************************
 * Timeout Handling.
 * Libiscsi does not use or interface with any system timers.
 * Instead all timeout processing in libiscsi is done as part
 * of the iscsi_service() processing.
 *
 * This means that if you use the timeout function below you must
 * device your application to call out to iscsi_service() at regular
 * intervals.
 * An easy way to do this is calling iscsi_service(iscsi, 0), i.e.
 * by passing 0 as the revents arguments once every second or so.
 ************************************************************/

/*
 * Set the timeout in seconds after which a task/pdu will timeout.
 * This timeout applies to SCSI task PDUs as well as normal iSCSI
 * PDUs such as login/task management/logout/...
 *
 * Each PDU is assigned its timeout value upon creation and can not be
 * changed afterwards. I.e. When you change the default timeout, it will
 * only affect any commands that are issued in the future but will not
 * affect the timeouts for any commands already in flight.
 *
 * The recommended usecase is to set to a default value for all PDUs
 * and only change the default temporarily when a specific task needs
 * a different timeout.
 *
 * // Set default to 5 seconds for all commands at beginning of program.
 * iscsi_set_timeout(iscsi, 5);
 *
 * ...
 * // SANITIZE command will take long so set it to no tiemout.
 * iscsi_set_timeout(iscsi, 0);
 * iscsi_sanitize_task(iscsi, ...
 * iscsi_set_timeout(iscsi, <set back to original value>);
 * ...
 *
 *
 * Default is 0 == no timeout.
 */
EXTERN int iscsi_set_timeout(struct iscsi_context *iscsi, int timeout);

/*
 * To set tcp keepalive for the session.
 * Only options supported by given platform (if any) are set.
 */
EXTERN int iscsi_set_tcp_keepalive(struct iscsi_context *iscsi, int idle, int count, int interval);

struct iscsi_url {
       char portal[MAX_STRING_SIZE + 1];
       char target[MAX_STRING_SIZE + 1];
       char user[MAX_STRING_SIZE + 1];
       char passwd[MAX_STRING_SIZE + 1];
       char target_user[MAX_STRING_SIZE + 1];
       char target_passwd[MAX_STRING_SIZE + 1];
       int lun;
       struct iscsi_context *iscsi;
       enum iscsi_transport_type transport;
};

/*
 * This function is used to set the desired mode for immediate data.
 * This can be set on a context before it has been logged in to the target
 * and controls how the initiator will try to negotiate the immediate data.
 *
 * Default is for libiscsi to try to negotiate ISCSI_IMMEDIATE_DATA_YES
 */
enum iscsi_immediate_data {
	ISCSI_IMMEDIATE_DATA_NO  = 0,
	ISCSI_IMMEDIATE_DATA_YES = 1
};
EXTERN int iscsi_set_immediate_data(struct iscsi_context *iscsi, enum iscsi_immediate_data immediate_data);

/*
 * This function is used to set the desired mode for initial_r2t
 * This can be set on a context before it has been logged in to the target
 * and controls how the initiator will try to negotiate the initial r2t.
 *
 * Default is for libiscsi to try to negotiate ISCSI_INITIAL_R2T_NO
 */
enum iscsi_initial_r2t {
	ISCSI_INITIAL_R2T_NO  = 0,
	ISCSI_INITIAL_R2T_YES = 1
};
EXTERN int
iscsi_set_initial_r2t(struct iscsi_context *iscsi, enum iscsi_initial_r2t initial_r2t);


/*
 * This function is used to parse an iSCSI URL into a iscsi_url structure.
 * iSCSI URL format :
 * iscsi://[<username>[%<password>]@]<host>[:<port>]/<target-iqn>/<lun>
 *
 * Target names are url encoded with '%' as a special character.
 * Example:
 * "iqn.ronnie.test%3A1234" will be translated to "iqn.ronnie.test:1234"
 *
 * Function will return a pointer to an iscsi url structure if successful,
 * or it will return NULL and set iscsi_get_error() accordingly if there was a problem
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
 * or it will return NULL and set iscsi_get_error() accordingly if there was a problem
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
 *  non-NULL: success
 *  NULL: error
 */
EXTERN struct iscsi_context *iscsi_create_context(const char *initiator_name);

/*
 * Destroy an existing ISCSI context and tear down any existing connection.
 * Callbacks for any command in flight will be invoked with
 * SCSI_STATUS_CANCELLED.
 *
 * Returns:
 *  0: success
 * <0: error
 */
EXTERN int iscsi_destroy_context(struct iscsi_context *iscsi);

/*
 * Sets and initializes the transport type for a context.
 * TCP_TRANSPORT is the default and is available on all platforms.
 * ISER_TRANSPORT is conditionally supported on Linux where available.
 *
 * Returns:
 *  0: success
 * <0: error
 */
EXTERN int iscsi_init_transport(struct iscsi_context *iscsi,
                                enum iscsi_transport_type transport);

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
 * Specify the username and password to use for target chap authentication.
 * Target/bidirectional CHAP is only supported if you also have normal
 * CHAP authentication.
 * You must configure CHAP first using iscsi_set_initiator_username_pwd()
`* before you can set up target authentication.
 */
EXTERN int iscsi_set_target_username_pwd(struct iscsi_context *iscsi,
					 const char *user,
					 const char *passwd);

/*
 * check if the context is logged in or not
 */
EXTERN int iscsi_is_logged_in(struct iscsi_context *iscsi);


enum scsi_status {
	SCSI_STATUS_GOOD                 = 0,
	SCSI_STATUS_CHECK_CONDITION      = 2,
	SCSI_STATUS_CONDITION_MET        = 4,
	SCSI_STATUS_BUSY                 = 8,
	SCSI_STATUS_RESERVATION_CONFLICT = 0x18,
	SCSI_STATUS_TASK_SET_FULL        = 0x28,
	SCSI_STATUS_ACA_ACTIVE           = 0x30,
	SCSI_STATUS_TASK_ABORTED         = 0x40,
	SCSI_STATUS_REDIRECT             = 0x101,
	SCSI_STATUS_CANCELLED            = 0x0f000000,
	SCSI_STATUS_ERROR                = 0x0f000001,
	SCSI_STATUS_TIMEOUT              = 0x0f000002
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
 *    SCSI_STATUS_GOOD     : Connection was successful. Command_data is NULL.
 *                           In this case the callback will be invoked a
 *                           second time once the connection is torn down.
 *
 *    SCSI_STATUS_ERROR    : Either failed to establish the connection, or
 *                           an already established connection has failed
 *                           with an error.
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
 *    SCSI_STATUS_GOOD     : Connection was successful. Command_data is NULL.
 *                           In this case the callback will be invoked a
 *                           second time once the connection is torn down.
 *
 *    SCSI_STATUS_ERROR    : Either failed to establish the connection, or
 *                           an already established connection has failed
 *                           with an error.
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
 * Disconnect a connection to a target and try to reconnect (async version).
 * This call returns immediately and the reconnect is processed in the
 * background. Commands send to this connection will be queued and not
 * processed until we have successfully reconnected.
 *
 * Returns:
 *  0 reconnect was successful
 * <0 error
 */
EXTERN int iscsi_reconnect(struct iscsi_context *iscsi);

/*
 * Disconnect a connection to a target and try to reconnect (sync version).
 * This call will block until the connection is reestablished.
 *
 * Returns:
 *  0 reconnect was successful
 * <0 error
 */
EXTERN int iscsi_reconnect_sync(struct iscsi_context *iscsi);

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
 *    SCSI_STATUS_GOOD     : login was successful. Command_data is always
 *                           NULL.
 *    SCSI_STATUS_CANCELLED: login was aborted. Command_data is NULL.
 *    SCSI_STATUS_ERROR    : login failed. Command_data is NULL.
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
 *    SCSI_STATUS_GOOD     : logout was successful. Command_data is always
 *                           NULL.
 *    SCSI_STATUS_CANCELLED: logout was aborted. Command_data is NULL.
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


struct iscsi_target_portal {
       struct iscsi_target_portal *next;
       const char *portal;
};

struct iscsi_discovery_address {
       struct iscsi_discovery_address *next;
       const char *target_name;
       struct iscsi_target_portal *portals;
};

/*
 * Asynchronous call to perform an ISCSI discovery.
 *
 * discoveries can only be done on connected and logged in discovery sessions.
 *
 * Returns:
 *  0 if the call was initiated and a discovery  will be attempted. Result
 *    will be reported through the callback function.
 * <0 if there was an error. The callback function will not be invoked.
 *
 * Callback parameters :
 * status can be either of :
 *    SCSI_STATUS_GOOD     : Discovery was successful. Command_data is a
 *                           pointer to a iscsi_discovery_address list of
 *                           structures.
 *                           This list of structures is only valid for the
 *                           duration of the callback and all data will be
 *                           freed once the callback returns.
 *    SCSI_STATUS_CANCELLED : Discovery was aborted. Command_data is NULL.
 */
EXTERN int iscsi_discovery_async(struct iscsi_context *iscsi, iscsi_command_cb cb,
			  void *private_data);

/*
 * Synchronous call to perform an ISCSI discovery.
 *
 * discoveries can only be done on connected and logged in discovery sessions.
 *
 * Returns:
 *  NULL if there was an error.
 *  struct iscsi_discovery_address* if the discovery was successfull.
 *    The data returned must be released by calling iscsi_free_discovery_data.
 */
EXTERN struct iscsi_discovery_address *iscsi_discovery_sync(
        struct iscsi_context *iscsi);

/* Free the discovery data structures returned by iscsi_discovery_sync
 */
EXTERN void iscsi_free_discovery_data(struct iscsi_context *iscsi,
                                      struct iscsi_discovery_address *da);

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
 *    SCSI_STATUS_GOOD     : NOP-OUT was successful and the server responded
 *                           with a NOP-IN callback_data is a iscsi_data
 *                           structure containing the data returned from
 *                           the server.
 *    SCSI_STATUS_CANCELLED : Discovery was aborted. Command_data is NULL.
 * 
 * The callback may be NULL if you only want to let libiscsi count the in-flight
 * NOPs.
 */
EXTERN int iscsi_nop_out_async(struct iscsi_context *iscsi, iscsi_command_cb cb,
			unsigned char *data, int len, void *private_data);


/* read out the number of consecutive nop outs that did not receive an answer */
EXTERN int iscsi_get_nops_in_flight(struct iscsi_context *iscsi);

struct scsi_task;
struct scsi_sense;

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

enum iscsi_task_mgmt_response {
	ISCSI_TMR_FUNC_COMPLETE				= 0x0,
	ISCSI_TMR_TASK_DOES_NOT_EXIST			= 0x1,
	ISCSI_TMR_LUN_DOES_NOT_EXIST			= 0x2,
	ISCSI_TMR_TASK_STILL_ALLEGIANT			= 0x3,
	ISCSI_TMR_TASK_ALLEGIANCE_REASS_NOT_SUPPORTED	= 0x4,
	ISCSI_TMR_TMF_NOT_SUPPORTED			= 0x5,
	ISCSI_TMR_FUNC_AUTH_FAILED			= 0x6,
	ISCSI_TMR_FUNC_REJECTED				= 0xFF
};

/*
 * Asynchronous call for task management
 *
 * Returns:
 *  0 if the call was initiated and the task mgmt function will be invoked.
 *    The result will be reported through the callback function.
 * <0 if there was an error. The callback function will not be invoked.
 *
 * Callback parameters :
 * status can be either of :
 *    SCSI_STATUS_GOOD     : Connection was successful. Command_data is a pointer to uint32_t
 *                           containing the response code as per RFC3720/10.6.1
 *
 *    SCSI_STATUS_ERROR     : Error.
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



/*
 * Synchronous calls for task management
 *
 * Returns:
 *  0 success.
 * <0 error.
 */
EXTERN int
iscsi_task_mgmt_sync(struct iscsi_context *iscsi,
		     int lun, enum iscsi_task_mgmt_funcs function,
		     uint32_t ritt, uint32_t rcmdscn);

EXTERN int
iscsi_task_mgmt_abort_task_sync(struct iscsi_context *iscsi, struct scsi_task *task);

EXTERN int
iscsi_task_mgmt_abort_task_set_sync(struct iscsi_context *iscsi, uint32_t lun);

EXTERN int
iscsi_task_mgmt_lun_reset_sync(struct iscsi_context *iscsi, uint32_t lun);

EXTERN int
iscsi_task_mgmt_target_warm_reset_sync(struct iscsi_context *iscsi);

EXTERN int
iscsi_task_mgmt_target_cold_reset_sync(struct iscsi_context *iscsi);




/* These are the possible status values for the callbacks for scsi commands.
 * The content of command_data depends on the status type.
 *
 * status :
 *   SCSI_STATUS_GOOD the scsi command completed successfullt on the target.
 *   If this scsi command returns DATA-IN, that data is stored in an scsi_task
 *   structure returned in the command_data parameter. This buffer will be
 *   automatically freed once the callback returns.
 *
 *   SCSI_STATUS_CHECK_CONDITION the scsi command failed with a scsi sense.
 *   Command_data contains a struct scsi_task. When the callback returns,
 *   this buffer will automatically become freed.
 *
 *   SCSI_STATUS_CANCELLED the scsi command was aborted. Command_data is
 *   NULL.
 *
 *   SCSI_STATUS_ERROR the command failed. Command_data is NULL.
 */

struct iscsi_data {
       size_t size;
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


struct scsi_mode_page;



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
iscsi_readdefectdata10_task(struct iscsi_context *iscsi, int lun,
                            int req_plist, int req_glist,
                            int defect_list_format, uint16_t alloc_len,
                            iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_readdefectdata12_task(struct iscsi_context *iscsi, int lun,
                            int req_plist, int req_glist,
                            int defect_list_format,
                            uint32_t address_descriptor_index,
                            uint32_t alloc_len,
                            iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_sanitize_task(struct iscsi_context *iscsi, int lun,
		    int immed, int ause, int sa, int param_len,
		    struct iscsi_data *data,
		    iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_sanitize_block_erase_task(struct iscsi_context *iscsi, int lun,
			       int immed, int ause,
			       iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_sanitize_crypto_erase_task(struct iscsi_context *iscsi, int lun,
				 int immed, int ause,
				 iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_sanitize_exit_failure_mode_task(struct iscsi_context *iscsi, int lun,
				      int immed, int ause,
				      iscsi_command_cb cb, void *private_data);
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
iscsi_read6_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		       uint32_t datalen, int blocksize, iscsi_command_cb cb,
		       void *private_data, struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_read10_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		  uint32_t datalen, int blocksize,
		  int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		  iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_read10_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		  uint32_t datalen, int blocksize,
		  int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		  iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_write10_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_write10_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_writeverify10_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_writeverify10_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number,
		   iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_read12_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   uint32_t datalen, int blocksize,
		   int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_read12_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   uint32_t datalen, int blocksize,
		   int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_write12_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_write12_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_writeverify12_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_writeverify12_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number,
		   iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_read16_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   uint32_t datalen, int blocksize,
		   int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_read16_iov_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   uint32_t datalen, int blocksize,
		   int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_write16_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_write16_iov_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_writeatomic16_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
			 unsigned char *data, uint32_t datalen, int blocksize,
			 int wrprotect, int dpo, int fua, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_writeatomic16_iov_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
			     unsigned char *data, uint32_t datalen, int blocksize,
			     int wrprotect, int dpo, int fua, int group_number,
			     iscsi_command_cb cb, void *private_data,
			     struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_orwrite_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_orwrite_iov_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		       unsigned char *data, uint32_t datalen, int blocksize,
		       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		       iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov);
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
iscsi_compareandwrite_iov_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
			       unsigned char *data, uint32_t datalen, int blocksize,
			       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
			       iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_writeverify16_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number,
		   iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_writeverify16_iov_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number,
		   iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_verify10_task(struct iscsi_context *iscsi, int lun,
		    unsigned char *data, uint32_t datalen, uint32_t lba,
		    int vprotect, int dpo, int bytchk,
		    int blocksize, iscsi_command_cb cb,
		    void *private_data);
EXTERN struct scsi_task *
iscsi_verify10_iov_task(struct iscsi_context *iscsi, int lun,
			unsigned char *data, uint32_t datalen, uint32_t lba,
			int vprotect, int dpo, int bytchk,
			int blocksize, iscsi_command_cb cb,
			void *private_data, struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_verify12_task(struct iscsi_context *iscsi, int lun,
		    unsigned char *data, uint32_t datalen, uint32_t lba,
		    int vprotect, int dpo, int bytchk,
		    int blocksize, iscsi_command_cb cb,
		    void *private_data);
EXTERN struct scsi_task *
iscsi_verify12_iov_task(struct iscsi_context *iscsi, int lun,
			unsigned char *data, uint32_t datalen, uint32_t lba,
			int vprotect, int dpo, int bytchk,
			int blocksize, iscsi_command_cb cb,
			void *private_data, struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_verify16_task(struct iscsi_context *iscsi, int lun,
		    unsigned char *data, uint32_t datalen, uint64_t lba,
		    int vprotect, int dpo, int bytchk,
		    int blocksize, iscsi_command_cb cb,
		    void *private_data);
EXTERN struct scsi_task *
iscsi_verify16_iov_task(struct iscsi_context *iscsi, int lun,
			unsigned char *data, uint32_t datalen, uint64_t lba,
			int vprotect, int dpo, int bytchk,
			int blocksize, iscsi_command_cb cb,
			void *private_data, struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_writesame10_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		       unsigned char *data, uint32_t datalen,
		       uint16_t num_blocks,
		       int anchor, int unmap, int wrprotect, int group,
		       iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_writesame10_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
			   unsigned char *data, uint32_t datalen,
			   uint16_t num_blocks,
			   int anchor, int unmap, int wrprotect, int group,
			   iscsi_command_cb cb, void *private_data,
			   struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_writesame16_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		       unsigned char *data, uint32_t datalen,
		       uint32_t num_blocks,
		       int anchor, int unmap, int wrprotect, int group,
		       iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_writesame16_iov_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
			   unsigned char *data, uint32_t datalen,
			   uint32_t num_blocks,
			   int anchor, int unmap, int wrprotect, int group,
			   iscsi_command_cb cb, void *private_data,
			   struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_modeselect6_task(struct iscsi_context *iscsi, int lun,
		       int pf, int sp, struct scsi_mode_page *mp,
		       iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_modeselect10_task(struct iscsi_context *iscsi, int lun,
			int pf, int sp, struct scsi_mode_page *mp,
			iscsi_command_cb cb, void *private_data);
EXTERN struct scsi_task *
iscsi_modesense6_task(struct iscsi_context *iscsi, int lun, int dbd,
			   int pc, int page_code, int sub_page_code,
			   unsigned char alloc_len, iscsi_command_cb cb,
			   void *private_data);
EXTERN struct scsi_task *
iscsi_modesense10_task(struct iscsi_context *iscsi, int lun, int llbaa, int dbd,
			   int pc, int page_code, int sub_page_code,
			   unsigned char alloc_len, iscsi_command_cb cb,
			   void *private_data);

struct unmap_list {
       uint64_t	  lba;
       uint32_t	  num;
};

EXTERN struct scsi_task *
iscsi_persistent_reserve_in_task(struct iscsi_context *iscsi, int lun,
				 int sa, uint16_t xferlen,
				 iscsi_command_cb cb, void *private_data);

EXTERN struct scsi_task *
iscsi_persistent_reserve_out_task(struct iscsi_context *iscsi, int lun,
				  int sa, int scope, int type, void *params,
				  iscsi_command_cb cb, void *private_data);

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

EXTERN struct scsi_task *
iscsi_report_supported_opcodes_task(struct iscsi_context *iscsi, int lun,
				    int rctd, int options,
				    int opcode, int sa,
				    uint32_t alloc_len,
				    iscsi_command_cb cb, void *private_data);

EXTERN struct scsi_task *
iscsi_receive_copy_results_task(struct iscsi_context *iscsi, int lun,
				int sa, int list_id, int alloc_len,
				iscsi_command_cb cb, void *private_data);

EXTERN struct scsi_task *
iscsi_extended_copy_task(struct iscsi_context *iscsi, int lun,
			 struct iscsi_data *param_data,
			 iscsi_command_cb cb, void *private_data);

/*
 * Sync commands for SCSI
 */
EXTERN struct scsi_task *
iscsi_scsi_command_sync(struct iscsi_context *iscsi, int lun,
			struct scsi_task *task, struct iscsi_data *data);

EXTERN struct scsi_task *
iscsi_modeselect6_sync(struct iscsi_context *iscsi, int lun,
		       int pf, int sp, struct scsi_mode_page *mp);

EXTERN struct scsi_task *
iscsi_modeselect10_sync(struct iscsi_context *iscsi, int lun,
			int pf, int sp, struct scsi_mode_page *mp);

EXTERN struct scsi_task *
iscsi_modesense6_sync(struct iscsi_context *iscsi, int lun, int dbd,
		      int pc, int page_code, int sub_page_code,
		      unsigned char alloc_len);

EXTERN struct scsi_task *
iscsi_modesense10_sync(struct iscsi_context *iscsi, int lun, int llbaa, int dbd,
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
iscsi_read6_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		     uint32_t datalen, int blocksize,
		     struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_read10_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		  uint32_t datalen, int blocksize,
		  int rdprotect, int dpo, int fua, int fua_nv, int group_number);

EXTERN struct scsi_task *
iscsi_read10_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		      uint32_t datalen, int blocksize,
		      int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		      struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_write10_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number);

EXTERN struct scsi_task *
iscsi_write10_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
			unsigned char *data, uint32_t datalen, int blocksize,
			int wrprotect, int dpo, int fua, int fua_nv, int group_number,
			struct scsi_iovec *iov, int niov);
EXTERN struct scsi_task *
iscsi_writeverify10_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number);

EXTERN struct scsi_task *
iscsi_writeverify10_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
			     unsigned char *data, uint32_t datalen, int blocksize,
			     int wrprotect, int dpo, int bytchk, int group_number,
			     struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_read12_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		  uint32_t datalen, int blocksize,
		  int rdprotect, int dpo, int fua, int fua_nv, int group_number);

EXTERN struct scsi_task *
iscsi_read12_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		      uint32_t datalen, int blocksize,
		      int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		      struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_write12_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number);

EXTERN struct scsi_task *
iscsi_write12_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		       unsigned char *data, uint32_t datalen, int blocksize,
		       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		       struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_writeverify12_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number);

EXTERN struct scsi_task *
iscsi_writeverify12_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
			     unsigned char *data, uint32_t datalen, int blocksize,
			     int wrprotect, int dpo, int bytchk, int group_number,
			     struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_read16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		  uint32_t datalen, int blocksize,
		  int rdprotect, int dpo, int fua, int fua_nv, int group_number);

EXTERN struct scsi_task *
iscsi_read16_iov_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		      uint32_t datalen, int blocksize,
		      int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		      struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_write16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number);

EXTERN struct scsi_task *
iscsi_write16_iov_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		       unsigned char *data, uint32_t datalen, int blocksize,
		       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		       struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_writeatomic16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
			 unsigned char *data, uint32_t datalen, int blocksize,
			 int wrprotect, int dpo, int fua, int group_number);

EXTERN struct scsi_task *
iscsi_writeatomic16_iov_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
			     unsigned char *data, uint32_t datalen, int blocksize,
			     int wrprotect, int dpo, int fua, int group_number,
			     struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_orwrite_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number);

EXTERN struct scsi_task *
iscsi_orwrite_iov_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		       unsigned char *data, uint32_t datalen, int blocksize,
		       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		       struct scsi_iovec *iov, int niov);

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
iscsi_compareandwrite_iov_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
			       unsigned char *data, uint32_t datalen, int blocksize,
			       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
			       struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_writeverify16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number);

EXTERN struct scsi_task *
iscsi_writeverify16_iov_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
			     unsigned char *data, uint32_t datalen, int blocksize,
			     int wrprotect, int dpo, int bytchk, int group_number,
			     struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_readcapacity10_sync(struct iscsi_context *iscsi, int lun, int lba,
			  int pmi);

EXTERN struct scsi_task *
iscsi_readcapacity16_sync(struct iscsi_context *iscsi, int lun);

EXTERN struct scsi_task *
iscsi_readdefectdata10_sync(struct iscsi_context *iscsi, int lun,
                            int req_plist, int req_glist,
                            int defect_list_format, uint16_t alloc_len);

EXTERN struct scsi_task *
iscsi_readdefectdata12_sync(struct iscsi_context *iscsi, int lun,
                            int req_plist, int req_glist,
                            int defect_list_format,
                            uint32_t address_descriptor_index,
                            uint32_t alloc_len);
EXTERN struct scsi_task *
iscsi_get_lba_status_sync(struct iscsi_context *iscsi, int lun, uint64_t starting_lba, uint32_t alloc_len);

EXTERN struct scsi_task *
iscsi_sanitize_sync(struct iscsi_context *iscsi, int lun,
		    int immed, int ause, int sa, int param_len,
		    struct iscsi_data *data);
EXTERN struct scsi_task *
iscsi_sanitize_block_erase_sync(struct iscsi_context *iscsi, int lun,
		    int immed, int ause);
EXTERN struct scsi_task *
iscsi_sanitize_crypto_erase_sync(struct iscsi_context *iscsi, int lun,
		    int immed, int ause);
EXTERN struct scsi_task *
iscsi_sanitize_exit_failure_mode_sync(struct iscsi_context *iscsi, int lun,
		    int immed, int ause);
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
iscsi_verify10_iov_sync(struct iscsi_context *iscsi, int lun,
			unsigned char *data, uint32_t datalen, uint32_t lba,
			int vprotect, int dpo, int bytchk,
			int blocksize, struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_verify12_sync(struct iscsi_context *iscsi, int lun,
		    unsigned char *data, uint32_t datalen, uint32_t lba,
		    int vprotect, int dpo, int bytchk,
		    int blocksize);

EXTERN struct scsi_task *
iscsi_verify12_iov_sync(struct iscsi_context *iscsi, int lun,
			unsigned char *data, uint32_t datalen, uint32_t lba,
			int vprotect, int dpo, int bytchk,
			int blocksize, struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_verify16_sync(struct iscsi_context *iscsi, int lun,
		    unsigned char *data, uint32_t datalen, uint64_t lba,
		    int vprotect, int dpo, int bytchk,
		    int blocksize);

EXTERN struct scsi_task *
iscsi_verify16_iov_sync(struct iscsi_context *iscsi, int lun,
			unsigned char *data, uint32_t datalen, uint64_t lba,
			int vprotect, int dpo, int bytchk,
			int blocksize, struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_writesame10_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		       unsigned char *data, uint32_t datalen,
		       uint16_t num_blocks,
		       int anchor, int unmap, int wrprotect, int group);

EXTERN struct scsi_task *
iscsi_writesame10_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
			   unsigned char *data, uint32_t datalen,
			   uint16_t num_blocks,
			   int anchor, int unmap, int wrprotect, int group,
			   struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_writesame16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		       unsigned char *data, uint32_t datalen,
		       uint32_t num_blocks,
		       int anchor, int unmap, int wrprotect, int group);

EXTERN struct scsi_task *
iscsi_writesame16_iov_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
			   unsigned char *data, uint32_t datalen,
			   uint32_t num_blocks,
			   int anchor, int unmap, int wrprotect, int group,
			   struct scsi_iovec *iov, int niov);

EXTERN struct scsi_task *
iscsi_persistent_reserve_in_sync(struct iscsi_context *iscsi, int lun,
				 int sa, uint16_t xferlen);

EXTERN struct scsi_task *
iscsi_persistent_reserve_out_sync(struct iscsi_context *iscsi, int lun,
				  int sa, int scope, int type, void *params);

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

EXTERN struct scsi_task *
iscsi_report_supported_opcodes_sync(struct iscsi_context *iscsi, int lun,
				    int rctd, int options,
				    int opcode, int sa,
				    uint32_t alloc_len);

EXTERN struct scsi_task *
iscsi_extended_copy_sync(struct iscsi_context *iscsi, int lun,
			 struct iscsi_data *param_data);

EXTERN struct scsi_task *
iscsi_receive_copy_results_sync(struct iscsi_context *iscsi, int lun,
				int sa, int list_id, int alloc_len);

/*
 * These functions are used when the application wants to specify its own buffers to read the data
 * from the DATA-IN PDUs into, or write the data to DATA-OUT PDUs from.
 * The main use is for SCSI READ10/12/16 WRITE10/12/16 operations to have them read/write directly from
 * the applications buffer, avoiding coying the data.
 *
 * This also supports reading into a vector of buffers by calling this function multiple times.
 * The individual buffers will be filled in the same order as they were created.
 *
 * Example READ10:
 *     task = iscsi_read10_task(    ( 2 512byte blocks into two buffers)
 *     scsi_task_add_data_in_buffer(task, first_buffer, 512
 *     scsi_task_add_data_in_buffer(task, second_buffer, 512
 *
 *
 * If you use this function you can not use task->datain in the READ callback.
 * task->datain.size will be 0 and
 * task->datain.data will be NULL
 *
 * Example WRITE10: (write 2 blocks)
 *     static struct scsi_iovec iov[2];
 *
 *     task = iscsi_write10_task(iscsi, lun, 0, NULL, 512, 512, 0, 0, 0, 0, 0, callback, private_data);
 *     iov[0].iov_base = first_buffer;
 *     iov[0].iov_len  = 512;
 *     iov[1].iov_base = second_buffer;
 *     iov[1].iov_len  = 512;
 *     scsi_task_set_iov_out(task, &iov[0], 2);
 */
EXTERN int scsi_task_add_data_in_buffer(struct scsi_task *task, int len, unsigned char *buf);
EXTERN int scsi_task_add_data_out_buffer(struct scsi_task *task, int len, unsigned char *buf);

struct scsi_iovec;
EXTERN void scsi_task_set_iov_out(struct scsi_task *task, struct scsi_iovec *iov, int niov);
EXTERN void scsi_task_set_iov_in(struct scsi_task *task, struct scsi_iovec *iov, int niov);

EXTERN int scsi_task_get_status(struct scsi_task *task, struct scsi_sense *sense);

/*
 * This function is used when you want to cancel a scsi task.
 * The callback for the task will immediately be invoked with SCSI_STATUS_CANCELLED.
 * The cancellation is only local in libiscsi. If the task is already in-flight
 * this call will not cancel the task at the target.
 * To cancel the task also a the target you need to call the task management functions.
 */
EXTERN int
iscsi_scsi_cancel_task(struct iscsi_context *iscsi,
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

/*
 * This function is to set the debugging level where level is
 *
 * 0  = disabled (default)
 * 1  = errors only
 * 2  = connection related info
 * 3  = user set variables
 * 4  = function calls
 * 5  = ...
 * 10 = everything
 */
EXTERN void
iscsi_set_log_level(struct iscsi_context *iscsi, int level);

typedef void (*iscsi_log_fn)(int level, const char *mesage);

/* Set the logging function to use */
EXTERN void iscsi_set_log_fn(struct iscsi_context *iscsi, iscsi_log_fn fn);

/* predefined log function that just writes to stderr */
EXTERN void iscsi_log_to_stderr(int level, const char *message);

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

/*
 * This function is to set the TCP_SYNCNT option. It has to be called after iscsi
 * context creation.
 */
EXTERN void
iscsi_set_tcp_syncnt(struct iscsi_context *iscsi, int value);

/*
 * This function is to set the interface that outbound connections for this socket are bound to.
 * You max specify more than one interface here separated by comma.
 */
EXTERN void
iscsi_set_bind_interfaces(struct iscsi_context *iscsi, char * interfaces);

/*
 * This function is to disable auto reconnect logic.
 *
 *  0 - Disable this feature (auto reconnect)
 *  1 - Enable this feature (no auto reconnect)
 */
EXTERN void
iscsi_set_noautoreconnect(struct iscsi_context *iscsi, int state);


/* This function is to set if we should retry a failed reconnect
   
   count is defined as follows:
    -1 -> retry forever (default)
    0  -> never retry
    n  -> retry n times
*/
EXTERN void
iscsi_set_reconnect_max_retries(struct iscsi_context *iscsi, int count);

/* Set to true to have libiscsi use TESTUNITREADY and consume any/all
   UnitAttentions that may have triggered in the target.
 */
EXTERN void
iscsi_set_no_ua_on_reconnect(struct iscsi_context *iscsi, int state);

#ifdef __cplusplus
}
#endif

#endif /* __iscsi_h__ */
