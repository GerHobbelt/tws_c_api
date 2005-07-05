#include "twsapi.h"

/* this is a quick and dirty example, use concepts here
 * but write your thread starter carefully from scratch.
 */

#ifdef unix
#include <pthread.h>
#else
#include <windows.h>
#include <process.h>
#endif

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
    err = tws_connect(ti, 0, 7496, 1);
    if(err) {
        printf("tws connect returned %d\n", err); exit(1);
    }

    memset(&c, 0, sizeof c);
    c.c_symbol = "msft";
    c.c_sectype = "stk";
    c.c_currency = "usd";
    c.c_exchange = "smart";
    c.c_primary_exch = "";
    c.c_expiry = "";
    c.c_right = "";
    c.c_local_symbol = "";
    c.c_multiplier = "";
    
    tws_req_mkt_data(ti, 1, &c);
#ifdef unix
    while(1) select(1,0,0,0,0);
#else
    Sleep(INFINITE);
#endif
    tws_destroy(ti);
    return 0;
}
