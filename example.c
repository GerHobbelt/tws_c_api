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


int main()
{
    int err;
    void *ti;
    tr_contract_t c;

    ti = tws_create(mythread_starter, (void *) 0x12345);
    err = tws_connect(ti, 0 , 7496, 1);
    if(err) {
        printf("tws connect returned %d\n", err); exit(1);
    }

    /* let's get some historical intraday data first */
    memset(&c, 0, sizeof c);
    c.c_symbol = "MSFT";
    c.c_sectype = "STK";
    c.c_expiry = "";
    c.c_right = "";
    c.c_multiplier = "";
    c.c_exchange = "SUPERSOES";
    c.c_primary_exch = "";
    c.c_currency = "USD";
    c.c_local_symbol = "";
    /* it seems that if parameter 6 is less than 9 (=30 min) data is never received */
    tws_req_historical_data(ti, 1, &c, /* MAKE date current or retrieval will fail */ "20051001 13:26:44",
                            "4 D", 9, "TRADES", 0, 1); 

    /* now request live data for msft */
    c.c_exchange = "SMART";
    tws_req_mkt_data(ti, 2, &c);

#ifdef unix
    while(1) select(1,0,0,0,0);
#else
    Sleep(INFINITE);
#endif
    tws_destroy(ti);
    return 0;
}
