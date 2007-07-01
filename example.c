#include "twsapi.h"

/* this is a quick and dirty example, use concepts here
 * but write your thread starter carefully from scratch.
 */

#ifdef unix
#include <pthread.h>
#include <sys/select.h>
#else
#include <windows.h>
#include <process.h>
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

int mythread_starter(tws_func_t func, void *arg)
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
    err = _beginthread(func, 2*8192, arg);
#endif

    return err; /* 0 success, -1 error */
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

int main()
{
    int err;
    void *ti;
    tr_contract_t c;

    ti = tws_create(mythread_starter, (void *) 0x12345, tws_thread_status);
    err = tws_connect(ti, 0 , 7496, 1);
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

    tws_req_historical_data(ti, 2, &c, /* MAKE date current or retrieval will fail */ "20070629 13:26:44", "1 D", "1 hour", "TRADES", 0, 1); 

    /* now request live data for QQQQ */
    tws_req_mkt_data(ti, 3, &c, "100,101,104,106,162,165,221,225");

#ifdef unix
    while(1) select(1,0,0,0,0);
#else
    Sleep(INFINITE);
#endif
    tws_destroy(ti);
    return 0;
}
