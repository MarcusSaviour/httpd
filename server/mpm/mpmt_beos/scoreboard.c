#include "httpd.h"
#include "http_log.h"
#include "http_main.h"
#include "http_core.h"
#include "http_config.h"
#include "beosd.h"
#include "http_conf_globals.h"
#include "mpmt_beos.h"
#include "scoreboard.h"

scoreboard *ap_scoreboard_image = NULL;
//static char *ap_server_argv0=NULL;
extern ap_context_t * pconf;

/*****************************************************************
 *
 * Dealing with the scoreboard... a lot of these variables are global
 * only to avoid getting clobbered by the longjmp() that happens when
 * a hard timeout expires...
 *
 * We begin with routines which deal with the file itself... 
 */

void reinit_scoreboard(ap_context_t *p)
{
    ap_assert(!ap_scoreboard_image);
    ap_scoreboard_image = (scoreboard *) malloc(SCOREBOARD_SIZE);
    if (ap_scoreboard_image == NULL) {
        fprintf(stderr, "Ouch! Out of memory reiniting scoreboard!\n");
    }
    memset(ap_scoreboard_image, 0, SCOREBOARD_SIZE);
}

void cleanup_scoreboard(void)
{
    ap_assert(ap_scoreboard_image);
    free(ap_scoreboard_image);
    ap_scoreboard_image = NULL;
}

API_EXPORT(void) reopen_scoreboard(ap_context_t *p)
{
}

API_EXPORT(void) ap_sync_scoreboard_image(void)
{
}

API_EXPORT(int) ap_exists_scoreboard_image(void)
{
    return (ap_scoreboard_image ? 1 : 0);
}

static ap_inline void put_scoreboard_info(int child_num, int thread_num, 
				       thread_score *new_score_rec)
{
    /* XXX - needs to be fixed to account for threads */
}

void update_scoreboard_global(void)
{
}

void increment_counts(int child_num, int thread_num, request_rec *r)
{
    long int bs = 0;
    thread_score *ss;

    ss = &ap_scoreboard_image->servers[child_num][thread_num];

    if (r->sent_bodyct)
	ap_bgetopt(r->connection->client, BO_BYTECT, &bs);

#ifndef NO_TIMES
    times(&ss->times);
#endif
    ss->access_count++;
    ss->my_access_count++;
    ss->conn_count++;
    ss->bytes_served += (unsigned long) bs;
    ss->my_bytes_served += (unsigned long) bs;
    ss->conn_bytes += (unsigned long) bs;

    put_scoreboard_info(child_num, thread_num, ss);

}

API_EXPORT(int) find_child_by_pid(int pid)
{
    int i;
    int max_daemons_limit = ap_get_max_daemons();

    for (i = 0; i < max_daemons_limit; ++i)
	if (ap_scoreboard_image->parent[i].pid == pid)
	    return i;

    return -1;
}

int ap_update_child_status(int child_num, int thread_num, int status, request_rec *r)
{
    int old_status;
    thread_score *ss;
    parent_score *ps;

    if (child_num < 0)
	return -1;

    ss = &ap_scoreboard_image->servers[child_num][thread_num];
    old_status = ss->status;
    ss->status = status;

    ps = &ap_scoreboard_image->parent[child_num];
    
    if ((status == SERVER_READY  || status == SERVER_ACCEPTING)
	&& old_status == SERVER_STARTING) {
        ss->tid = find_thread(NULL);
	ps->worker_threads = ap_threads_per_child;
    }

    if (ap_extended_status) {
	if (status == SERVER_READY || status == SERVER_DEAD) {
	    /*
	     * Reset individual counters
	     */
	    if (status == SERVER_DEAD) {
		ss->my_access_count = 0L;
		ss->my_bytes_served = 0L;
	    }
	    ss->conn_count = (unsigned short) 0;
	    ss->conn_bytes = (unsigned long) 0;
	}
	if (r) {
	    conn_rec *c = r->connection;
	    ap_cpystrn(ss->client, ap_get_remote_host(c, r->per_dir_config,
				  REMOTE_NOLOOKUP), sizeof(ss->client));
	    if (r->the_request == NULL) {
		    ap_cpystrn(ss->request, "NULL", sizeof(ss->request));
	    } else if (r->parsed_uri.password == NULL) {
		    ap_cpystrn(ss->request, r->the_request, sizeof(ss->request));
	    } else {
		/* Don't reveal the password in the server-status view */
		    ap_cpystrn(ss->request, ap_pstrcat(r->pool, r->method, " ",
					       ap_unparse_uri_components(r->pool, &r->parsed_uri, UNP_OMITPASSWORD),
					       r->assbackwards ? NULL : " ", r->protocol, NULL),
				       sizeof(ss->request));
	    }
	    ss->vhostrec =  r->server;
	}
    }
    
    put_scoreboard_info(child_num, thread_num, ss);
    return old_status;
}

void ap_time_process_request(int child_num, int thread_num, int status)
{
    thread_score *ss;

    if (child_num < 0)
	return;

    ss = &ap_scoreboard_image->servers[child_num][thread_num];

    if (status == START_PREQUEST) {
      /*ss->start_time = GetCurrentTime(); ZZZ return time in uS since the 
	epoch. Some platforms do not support gettimeofday. Create a routine 
	to get the current time is some useful units. */
        if (gettimeofday(&ss->start_time, (struct timezone *) 0) < 0) {
            ss->start_time.tv_sec = ss->start_time.tv_usec = 0L;
        }
    }
    else if (status == STOP_PREQUEST) {
      /*ss->stop_time = GetCurrentTime(); 
	ZZZ return time in uS since the epoch */
        
        if (gettimeofday(&ss->stop_time, (struct timezone *) 0) < 0) {
            ss->start_time.tv_sec = ss->start_time.tv_usec = 0L;
        }
    }
    put_scoreboard_info(child_num, thread_num, ss);
}

/* Stub functions until this MPM supports the connection status API */

API_EXPORT(void) ap_update_connection_status(long conn_id, const char *key, \
                                             const char *value)
{
    /* NOP */
}

API_EXPORT(void) ap_reset_connection_status(long conn_id)
{
    /* NOP */
}

