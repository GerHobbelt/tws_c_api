#include "twsapi.h"

/* this is a quick and dirty example, use concepts here
 * but write your thread starter carefully from scratch.
 */

#ifdef unix
#include <pthread.h>
#else
#include <windows.h>
#endif

int mythread_starter(tws_func_t func, void *arg)
{
#ifdef unix
    pthread_t thr;
#else
    unsigned thr;
#endif
    int err;

#ifdef unix
    err = pthread_create(&thr, 0, (void *(*)(void *)) func, arg);
#else
    /* beware of stack leak if func expected to use Pascal calling convention
     * but declaration of func in another unit conveys C calling convention.
     * wrap pascal func in C func if this is the case.
     */
    err = _beginthreadex(0, 2*8192, func, arg, 0, (unsigned *)&thr);
#endif

    return err;
}


int main()
{
    int err;
    void *ti;
    tr_contract_t c;

    ti = tws_create(mythread_starter, 0x12345);
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
