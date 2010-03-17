#include "twsapi.h"

#ifdef unix
#include <pthread.h>
#include <sys/select.h>
#include <netdb.h>
#include <fcntl.h>
#else
#include <process.h>
#include <winsock2.h>
#include <WS2tcpip.h> /* for ipv6 DNS lookups if needed */
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

static void tws_thread_status(int arg)
{
    /* the real reason for having this func
     * is so that array_of_threads[NAME_OF_THIS_ONE] can be set to
     * pthread_self() on startup and 0 on termination, so that
     * the top level thread can cancel one or all if it wants to
     */
    printf("tws reader thread %s\n", arg ? "terminated" : "started");
}

static int mythread_starter(tws_func_t func, void *arg)
{
#ifdef unix
    pthread_t thr;
#endif
    int err;

#ifdef unix
    err = pthread_create(&thr, 0, (void *(*)(void *)) func, arg);
#else
    /* if using _beginthreadex: beware of stack leak if func expected to
     * use Pascal calling convention but declaration of func in another 
     * unit conveys C calling convention, wrap pascal func in C func if 
     * this is the case. Also cleanup is more complex for _beginthreadex.
     */
    err =  _beginthread(func, 2*8192, arg);
#endif

    return err; /* 0 success, -1 error */
}

/* User is responsible for implementing this function
 * name is a null terminated string
 * addr is where the address should be copied on success
 * addr_len contains length of addr buffer and
 * may be decreased on return to caller
 * function returns -1 on failure, 0 on success
 */
static int resolve_name(const void *name, void *addr, long *addr_len)
{
    int error = -1;
#if 1
    struct hostent *h;

    if(*addr_len < 4)
        goto out;

    h = gethostbyname((char *) name); /* does not support ipv6 either, though the API does */
    if(!h)
        goto out;

    memcpy(addr, h->h_addr_list[0], 4);
    *addr_len = 4;
    error = 0;
#else
    struct addrinfo *ai = 0, hints;

    if(*addr_len < 4)
        goto out;

    memset((void *) &hints, 0, sizeof hints);
    hints.ai_family = PF_INET; /* first attempt ipv4 resolution */
    hints.ai_socktype = SOCK_STREAM;
    error = getaddrinfo((char *) name, 0, &hints, &ai);
    if(!error && ai && ai->ai_family == PF_INET) {
        struct sockaddr_in a4;

        memcpy((void *) &a4, &ai->ai_addr[0], sizeof a4);
        memcpy(addr, &a4.sin_addr, 4);
        *addr_len = 4;
        error = 0;
    } else
        error = -1;

    if(ai)
        freeaddrinfo(ai);

    if(!error)
        goto out;

    /* try to get an ipv6 address */
    if(*addr_len < 16) {
        error = -1;
        goto out;
    }

    ai = 0;
    hints.ai_family = PF_INET6;
    error = getaddrinfo((char *) name, 0, &hints, &ai);

    if(!error && ai && ai->ai_family == PF_INET6) {
        struct sockaddr_in6 a6;

        memcpy((void *) &a6, &ai->ai_addr[0], sizeof a6);
        memcpy(addr, &a6.sin6_addr, 16);
        *addr_len = 16;
        error = 0;
    } else
        error = -1;

    if(ai)
        freeaddrinfo(ai);
#endif
out:
    return error;
}

/* find top percentage gainers (US stocks) with price > 5 and volume > 2M */
static void scan_market(void *ti)
{
    /* illustrate new feature as of version 23 */
    tr_scanner_subscription_t s;

    tws_req_scanner_parameters(ti);

    memset(&s, 0, sizeof s);
    s.scan_number_of_rows = -1;
    s.scan_instrument = "STK";
    s.scan_location_code = "STK.US";
    s.scan_code = "TOP_PERC_GAIN"; /* see xml returned by tws_req_scanner_parameters for more choices */

    s.scan_above_price = 5.0;
    s.scan_below_price = s.scan_coupon_rate_above = s.scan_coupon_rate_below = DBL_MAX;
    s.scan_above_volume = 2000*1000;
    s.scan_market_cap_above = s.scan_market_cap_below = DBL_MAX;
    s.scan_moody_rating_above = s.scan_moody_rating_below = s.scan_sp_rating_above = s.scan_sp_rating_below = s.scan_maturity_date_above = s.scan_maturity_date_below = s.scan_exclude_convertible = "";
    s.scan_scanner_setting_pairs = "";
    s.scan_stock_type_filter = "";
    tws_req_scanner_subscription(ti, 1, &s);
}

int main(int argc, char *argv[])
{
    int err, no_thread = 0;
    void *ti;
    tr_contract_t c;

    if(argc != 2) {
    usage:
        printf("Usage: %s spawn_thread|no_thread\n", argv[0]);
        return 1;
    }

    if(!memcmp(argv[1], "spawn_thread", sizeof "spawn_thread" -1))
        ;
    else if(!memcmp(argv[1], "no_thread", sizeof "no_thread" -1))
        no_thread = 1;
    else
        goto usage;

    /* 'no_thread' here could also mean that an externally spawned thread
     * that we did not create will handle IO from TWS, somehow we may happen 
     * to run in the context of that thread
     */
    ti = tws_create(no_thread ? TWS_NO_THREAD : mythread_starter, (void *) 0x12345, tws_thread_status);
    err = tws_connect(ti, 0 , 7496, 1, resolve_name);
    if(err) {
        printf("tws connect returned %d\n", err); exit(1);
    }

    scan_market(ti);

    /* let's get some historical intraday data first */
    memset(&c, 0, sizeof c);
    c.c_symbol = "QQQQ";
    c.c_sectype = "STK";
    c.c_expiry = "";
    c.c_right = "";
    c.c_multiplier = "";
    c.c_exchange = "SMART";
    c.c_primary_exch = "SUPERSOES";
    c.c_currency = "USD";
    c.c_local_symbol = "";
    c.c_secid_type = "";
    c.c_secid = "";

    tws_req_historical_data(ti, 2, &c, /* make date current or retrieval may fail */ "20091005 13:26:44", "1 D", "1 hour", "TRADES", 0, 1); 
    /* now request live data for QQQQ */
    tws_req_mkt_data(ti, 3, &c, "100,101,104,105,106,107,165,225,232,233,236,258", 0);

#if 0  /* flip it to 1, recompile and run at your own risk */
    /* let's place a market order to buy 1 share of QQQQ */
    do {
        tr_order_t o;

        memset(&o, 0, sizeof o);

        o.o_action = "BUY";
        o.o_total_quantity = 1;
        o.o_order_type = "MKT";
        o.o_transmit = 1;

        /* misc default initializations */
        o.o_min_qty = ~(1U<< 31);
        o.o_percent_offset = o.o_nbbo_price_cap = DBL_MAX;
        o.o_starting_price = o.o_stock_ref_price = DBL_MAX;
        o.o_delta = o.o_stock_range_lower = o.o_stock_range_upper = DBL_MAX;
        o.o_volatility = o.o_delta_neutral_aux_price = DBL_MAX;
        o.o_volatility_type = o.o_reference_price_type = ~(1U<< 31);
        o.o_trail_stop_price = DBL_MAX;
        o.o_scale_init_level_size =  o.o_scale_subs_level_size = ~(1U<< 31);
        o.o_scale_price_increment = DBL_MAX;
        o.o_tif = o.o_oca_group = o.o_account = o.o_open_close = "";
        o.o_orderref = o.o_good_after_time = o.o_good_till_date = "";
        o.o_fagroup = o.o_famethod = o.o_fapercentage = o.o_faprofile = "";
        o.o_designated_location = o.o_rule80a = o.o_settling_firm = "";
        o.o_delta_neutral_order_type = o.o_clearing_account = o.o_clearing_intent = "";
        o.o_algo_strategy = "";

        /* see comment in function event_next_valid_id() before placing the order */
        tws_place_order(ti, 1 /* change order_id */, &c, &o);
    } while(0);
#endif /* how to place order example ends */

    /* 3 more illustrations and that's it */
    tws_req_open_orders(ti);
    tws_req_account_updates(ti, 1, "");
    tws_request_realtime_bars(ti, 4, &c, 5, "TRADES", 1);

    if(no_thread) {
        while(0 == tws_event_process(ti));
    } else {
#ifdef unix
        while(1) select(1,0,0,0,0);
#else
        Sleep(INFINITE);
#endif
    }

    tws_destroy(ti);
    return 0;
}
