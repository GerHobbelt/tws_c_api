#define TWSAPI_GLOBALS
#include "twsapi.h"

#ifdef WINDOWS
#include <winsock2.h>
typedef SOCKET socket_t;
#define close closesocket
#else /* unix assumed */
typedef int socket_t;
#endif

#ifdef unix
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#define MAX_TWS_STRINGS 63
#define WORD_SIZE_IN_BITS (8*sizeof(long))
typedef struct {
    char str[1024]; /* maximum conceivable string length */
} tws_string_t;

typedef struct tws_instance {
    void *opaque_user_defined;
    start_thread_t start_thread;
    socket_t fd;
    unsigned char buf[200]; /* buffer up to 200 chars at a time */
    unsigned int buf_next, buf_last; /* index of next, last chars in buf */
    unsigned int server_version;
    volatile unsigned int connected:1;
    tws_string_t mempool[MAX_TWS_STRINGS];
    unsigned long bitmask[1 + MAX_TWS_STRINGS / WORD_SIZE_IN_BITS];
} tws_instance_t;

static int read_double(tws_instance_t *ti, double *val);
static int read_int(tws_instance_t *ti, int *val);
static int read_line(tws_instance_t *ti, char *line, long *len);

/* access to these strings is single threaded
 * replace plain bitops with atomic test_and_set_bit/clear_bit + memory barriers
 * if multithreaded access is desired (not applicable at present)
 */
static char *alloc_string(tws_instance_t *ti)
{
    register unsigned long j, index, bits;

    for(j = 0; j < MAX_TWS_STRINGS; j++) {
        index = j / WORD_SIZE_IN_BITS;
        if(ti->bitmask[index] == ~0UL)
            continue;

        bits = 1 << (j & (WORD_SIZE_IN_BITS - 1));
        if(!(ti->bitmask[index] & bits)) {
            ti->bitmask[index] |= bits;
            ti->mempool[j].str[0] = '\0';
            return ti->mempool[j].str;
        }
    }
#ifdef DEBUG
    printf("alloc_string: ran out of strings, will crash shortly\n");
#endif
    return 0;
}

static void free_string(tws_instance_t *ti, void *ptr)
{
    unsigned long j = (unsigned long) ((tws_string_t *) ptr - &ti->mempool[0]);
    unsigned long index = j / WORD_SIZE_IN_BITS;
    unsigned long bits = 1 << (j & (WORD_SIZE_IN_BITS - 1));

    ti->bitmask[index] &= ~bits;
}

static void init_contract(tws_instance_t *ti, tr_contract_t *c)
{
    memset(c, 0, sizeof *c);
    c->c_symbol = alloc_string(ti);
    c->c_sectype = alloc_string(ti);
    c->c_exchange = alloc_string(ti);
    c->c_primary_exch = alloc_string(ti);
    c->c_expiry = alloc_string(ti);
    c->c_currency = alloc_string(ti);
    c->c_right = alloc_string(ti);
    c->c_local_symbol = alloc_string(ti);
    c->c_multiplier = alloc_string(ti);
}

static void destroy_contract(tws_instance_t *ti, tr_contract_t *c)
{
    free_string(ti, c->c_symbol);
    free_string(ti, c->c_sectype);
    free_string(ti, c->c_exchange);
    free_string(ti, c->c_primary_exch);
    free_string(ti, c->c_expiry);
    free_string(ti, c->c_currency);
    free_string(ti, c->c_right);
    free_string(ti, c->c_local_symbol);
    free_string(ti, c->c_multiplier);
}

static void init_order(tws_instance_t *ti, tr_order_t *o)
{
    memset(o, 0, sizeof *o);
    o->o_good_after_time = alloc_string(ti);
    o->o_good_till_date = alloc_string(ti);
    o->o_shares_allocation = alloc_string(ti);
    o->o_fagroup = alloc_string(ti);
    o->o_famethod = alloc_string(ti);
    o->o_fapercentage = alloc_string(ti);
    o->o_faprofile = alloc_string(ti);
    o->o_action = alloc_string(ti);
    o->o_ordertype = alloc_string(ti);
    o->o_tif = alloc_string(ti);
    o->o_oca_group = alloc_string(ti);
    o->o_account = alloc_string(ti);
    o->o_open_close = alloc_string(ti);
    o->o_orderref = alloc_string(ti);
}

static void destroy_order(tws_instance_t *ti, tr_order_t *o)
{
    free_string(ti, o->o_good_after_time);
    free_string(ti, o->o_good_till_date);
    free_string(ti, o->o_shares_allocation);
    free_string(ti, o->o_fagroup);
    free_string(ti, o->o_famethod);
    free_string(ti, o->o_fapercentage);
    free_string(ti, o->o_faprofile);
    free_string(ti, o->o_action);
    free_string(ti, o->o_ordertype);
    free_string(ti, o->o_tif);
    free_string(ti, o->o_oca_group);
    free_string(ti, o->o_account);
    free_string(ti, o->o_open_close);
    free_string(ti, o->o_orderref);
}

static void init_contract_details(tws_instance_t *ti, tr_contract_details_t *cd)
{
    memset(cd, 0, sizeof *cd);
    cd->d_summary.s_symbol = alloc_string(ti);
    cd->d_summary.s_sectype = alloc_string(ti);
    cd->d_summary.s_expiry = alloc_string(ti);
    cd->d_summary.s_right = alloc_string(ti);
    cd->d_summary.s_exchange = alloc_string(ti);
    cd->d_summary.s_currency = alloc_string(ti);
    cd->d_summary.s_local_symbol = alloc_string(ti);

    cd->d_market_name = alloc_string(ti);
    cd->d_trading_class = alloc_string(ti);
    cd->d_multiplier = alloc_string(ti);
    cd->d_order_types = alloc_string(ti);
    cd->d_valid_exchanges = alloc_string(ti);
}

static void destroy_contract_details(tws_instance_t *ti, tr_contract_details_t *cd)
{
    free_string(ti, cd->d_summary.s_symbol);
    free_string(ti, cd->d_summary.s_sectype);
    free_string(ti, cd->d_summary.s_expiry);
    free_string(ti, cd->d_summary.s_right);
    free_string(ti, cd->d_summary.s_exchange);
    free_string(ti, cd->d_summary.s_currency);
    free_string(ti, cd->d_summary.s_local_symbol);

    free_string(ti, cd->d_market_name);
    free_string(ti, cd->d_trading_class);
    free_string(ti, cd->d_multiplier);
    free_string(ti, cd->d_order_types);
    free_string(ti, cd->d_valid_exchanges);
}

static void init_execution(tws_instance_t *ti, tr_execution_t *exec)
{
    memset(exec, 0, sizeof *exec);

    exec->e_execid = alloc_string(ti);
    exec->e_time = alloc_string(ti);
    exec->e_acct_number = alloc_string(ti);
    exec->e_exchange = alloc_string(ti);
    exec->e_side = alloc_string(ti);
}

static void destroy_execution(tws_instance_t *ti, tr_execution_t *exec)
{
    free_string(ti, exec->e_execid);
    free_string(ti, exec->e_time);
    free_string(ti, exec->e_acct_number);
    free_string(ti, exec->e_exchange);
    free_string(ti, exec->e_side);
}


static void receive_tick_price(tws_instance_t *ti)
{
    double price;
    int ival, version, tickerid, tick_type, size = 0, can_auto_execute = 0, size_tick_type, connected;

    read_int(ti, &ival), version = ival;
    read_int(ti, &ival), tickerid = ival;
    read_int(ti, &ival), tick_type = ival;
    read_double(ti, &price);

    if(version >= 2)
        read_int(ti, &ival), size = ival;
    
    if(version >= 3)
        read_int(ti, &ival), can_auto_execute = ival;
    
    connected = ti->connected;
    if(connected)
        event_tick_price(ti->opaque_user_defined, tickerid, tick_type, price, can_auto_execute);
    
    if(version >= 2) {
        switch (tick_type) {
            case 1: /* BID */  size_tick_type = 0; break; /* BID_SIZE */
            case 2: /* ASK */  size_tick_type = 3; break; /* ASK_SIZE */
            case 4: /* LAST */ size_tick_type = 5; break; /* LAST_SIZE */
            default: size_tick_type = -1; /* not a tick */
        }
        
        if(size_tick_type != -1)
            if(connected)
                event_tick_size(ti->opaque_user_defined, tickerid, size_tick_type, size);
    }
}


static void receive_tick_size(tws_instance_t *ti)
{
    int ival, tickerid, tick_type, size;
    
    read_int(ti, &ival); /* version unused */
    read_int(ti, &ival), tickerid = ival;
    read_int(ti, &ival), tick_type = ival;
    read_int(ti, &ival), size = ival;
    
    if(ti->connected)
        event_tick_size(ti->opaque_user_defined, tickerid, tick_type, size);
}

static void receive_order_status(tws_instance_t *ti)
{
    double dval, avg_fill_price, last_fill_price = 0.0;
    long lval;
    char *status = alloc_string(ti);
    int ival, version, id, filled, remaining, permid = 0, parentid = 0, clientid = 0;

    read_int(ti, &ival), version = ival;
    read_int(ti, &ival), id = ival;
    lval = sizeof(tws_string_t), read_line(ti, status, &lval);
    read_int(ti, &ival), filled = ival;
    read_int(ti, &ival), remaining = ival;
    read_double(ti, &dval), avg_fill_price = dval;
    
    if(version >= 2)
        read_int(ti, &ival), permid = ival;

    if(version >= 3)
        read_int(ti, &ival), parentid = ival;
    
    if(version >= 4)
        read_double(ti, &dval), last_fill_price = dval;
    
    if(version >= 5)
        read_int(ti, &ival), clientid = ival;
    
    if(ti->connected)
        event_order_status(ti->opaque_user_defined, id, status, filled, remaining,
                           avg_fill_price, permid, parentid, last_fill_price, clientid);

    free_string(ti, status);
}

static void receive_acct_value(tws_instance_t *ti)
{
    char *key = alloc_string(ti), *val = alloc_string(ti), *cur = alloc_string(ti),
        *account_name = alloc_string(ti);
    long lval;
    int ival, version;

    read_int(ti, &ival), version = ival;
    lval = sizeof(tws_string_t), read_line(ti, key, &lval);
    lval = sizeof(tws_string_t), read_line(ti, val, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cur, &lval);
    
    account_name[0] = '\0';
    if(version >= 2)
        lval = sizeof(tws_string_t), read_line(ti, account_name, &lval);
    
    if(ti->connected)
        event_update_account_value(ti->opaque_user_defined, key, val, cur, account_name);

    free_string(ti, account_name);
    free_string(ti, cur);
    free_string(ti, val);
    free_string(ti, key);
}

static void receive_portfolio_value(tws_instance_t *ti)
{
    double dval, market_price, market_value, average_cost = 0.0, unrealized_pnl = 0.0,
        realized_pnl = 0.0;
    tr_contract_t contract;
    long lval;
    char *account_name = alloc_string(ti);
    int ival, version, position;
    
    read_int(ti, &ival), version = ival;
    
    init_contract(ti, &contract);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_symbol, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_sectype, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_expiry, &lval);
    read_double(ti, &contract.c_strike);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_right, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_currency, &lval);
    if (version >= 2)
        lval = sizeof(tws_string_t), read_line(ti, contract.c_local_symbol, &lval);
    
    read_int(ti, &ival), position = ival;
    
    read_double(ti, &dval), market_price = dval;
    read_double(ti, &dval), market_value = dval;
    
    if(version >=3 ) {
        read_double(ti, &dval), average_cost = dval;
        read_double(ti, &dval), unrealized_pnl = dval;
        read_double(ti, &dval), realized_pnl = dval;
    }
    
    account_name[0] = '\0';
    if(version >= 4)
        lval = sizeof(tws_string_t), read_line(ti, account_name, &lval);
    
    if(ti->connected)
        event_update_portfolio(ti->opaque_user_defined, &contract, position,
                               market_price, market_value, average_cost,
                               unrealized_pnl, realized_pnl, account_name);

    free_string(ti, account_name);
    destroy_contract(ti, &contract);
}

static void receive_acct_update_time(tws_instance_t *ti)
{
    char *timestamp = alloc_string(ti);
    long lval;
    int ival;
    
    read_int(ti, &ival); /* version unused */
    lval = sizeof(tws_string_t), read_line(ti, timestamp, &lval);
    
    if(ti->connected)
        event_update_account_time(ti->opaque_user_defined, timestamp);
    
    free_string(ti, timestamp);
}

static void receive_err_msg(tws_instance_t *ti)
{
    char *msg = alloc_string(ti);
    long lval;
    int ival, version, id = 0, error_code = 0;
    
    read_int(ti, &ival), version = ival;
    
    if(version >= 2) {
        read_int(ti, &ival), id = ival;
        read_int(ti, &ival), error_code = ival;
    }
    
    lval = sizeof(tws_instance_t), read_line(ti, msg, &lval);
    
    if(ti->connected)
        event_error(ti->opaque_user_defined, id, error_code, msg);

    free_string(ti, msg);
}

static void receive_open_order(tws_instance_t *ti)
{
    tr_contract_t contract;
    tr_order_t order;
    long lval;
    int ival, version;

    init_contract(ti, &contract);
    init_order(ti, &order);
  
    read_int(ti, &ival), version = ival;
    read_int(ti, &order.o_orderid);
  
    lval = sizeof(tws_string_t), read_line(ti, contract.c_symbol, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_sectype, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_expiry, &lval);
    read_double(ti, &contract.c_strike);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_right, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_exchange, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_currency, &lval);
  
    if(version >= 2)
        lval = sizeof(tws_string_t), read_line(ti, contract.c_local_symbol, &lval);
  
    lval = sizeof(tws_string_t), read_line(ti, order.o_action, &lval);
    read_int(ti, &order.o_total_quantity);
    lval = sizeof(tws_string_t), read_line(ti, order.o_ordertype, &lval);
    read_double(ti, &order.o_lmt_price);
    read_double(ti, &order.o_aux_price);
    lval = sizeof(tws_string_t), read_line(ti, order.o_tif, &lval);
    lval = sizeof(tws_string_t), read_line(ti, order.o_oca_group, &lval);
    lval = sizeof(tws_string_t), read_line(ti, order.o_account, &lval);
    lval = sizeof(tws_string_t), read_line(ti, order.o_open_close, &lval);
    read_int(ti, &order.o_origin);
    lval = sizeof(tws_string_t), read_line(ti, order.o_orderref, &lval);
  
    if(version >= 3)
        read_int(ti, &order.o_clientid);
  
    if(version >= 4) {
        read_int(ti, &order.o_permid);
        read_int(ti, &ival); order.o_ignore_rth = ival == 1;
        read_int(ti, &ival); order.o_hidden = ival == 1;
        read_double(ti, &order.o_discretionary_amt);
    }
  
    if(version >= 5)
        lval = sizeof(tws_string_t), read_line(ti, order.o_good_after_time, &lval);
  
    if(version >= 6)
        lval = sizeof(tws_string_t), read_line(ti, order.o_shares_allocation, &lval);
  
    if(version >= 7) {
        lval = sizeof(tws_string_t), read_line(ti, order.o_fagroup, &lval);
        lval = sizeof(tws_string_t), read_line(ti, order.o_famethod, &lval);
        lval = sizeof(tws_string_t), read_line(ti, order.o_fapercentage, &lval);
        lval = sizeof(tws_string_t), read_line(ti, order.o_faprofile, &lval);
    }
  
    if(ti->connected)
        event_open_order(ti->opaque_user_defined, order.o_orderid, &contract, &order);

    destroy_order(ti, &order);
    destroy_contract(ti, &contract);
}

static void receive_next_valid_id(tws_instance_t *ti)
{
    int ival;

    read_int(ti, &ival); /* version */
    read_int(ti, &ival); /* orderid */
    if(ti->connected)
        event_next_valid_id(ti->opaque_user_defined, /*orderid*/ ival);
}

static void receive_contract_data(tws_instance_t *ti)
{
    tr_contract_details_t cdetails;
    long lval;
    int ival;

    init_contract_details(ti, &cdetails);
    read_int(ti, &ival); /* version */
    
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.s_symbol, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.s_sectype, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.s_expiry, &lval);
    read_double(ti, &cdetails.d_summary.s_strike);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.s_right, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.s_exchange, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.s_currency, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.s_local_symbol, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_market_name, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_trading_class, &lval);
    read_int(ti, &cdetails.d_conid);
    read_double(ti, &cdetails.d_mintick);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_multiplier, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_order_types, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_valid_exchanges, &lval);

    if(ti->connected)
        event_contract_details(ti->opaque_user_defined, &cdetails);

    destroy_contract_details(ti, &cdetails);
}

static void receive_execution_data(tws_instance_t *ti)
{
    tr_contract_t contract;
    tr_execution_t exec;
    long lval;
    int ival, version, orderid;

    init_contract(ti, &contract);
    init_execution(ti, &exec);

    read_int(ti, &ival), version = ival;
    read_int(ti, &ival), orderid = ival;
    lval = sizeof(tws_string_t), read_line(ti, contract.c_symbol, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_sectype, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_expiry, &lval);
    read_double(ti, &contract.c_strike);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_right, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_exchange, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_currency, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_local_symbol, &lval);

    exec.e_orderid = orderid;
    lval = sizeof(tws_string_t), read_line(ti, exec.e_execid, &lval);
    lval = sizeof(tws_string_t), read_line(ti, exec.e_time, &lval);
    lval = sizeof(tws_string_t), read_line(ti, exec.e_acct_number, &lval);
    lval = sizeof(tws_string_t), read_line(ti, exec.e_exchange, &lval);
    lval = sizeof exec.e_side, read_line(ti, exec.e_side, &lval);
    read_int(ti, &exec.e_shares);
    read_double(ti, &exec.e_price);
    if(version >= 2)
        read_int(ti, &exec.e_permid);
    
    if(version >= 3)
        read_int(ti, &exec.e_clientid);
    
    if(version >= 4)
        read_int(ti, &exec.e_liquidation);
    
    if(ti->connected)
        event_exec_details(ti->opaque_user_defined, orderid, &contract, &exec);

    destroy_contract(ti, &contract);
    destroy_execution(ti, &exec);
}

static void receive_market_depth(tws_instance_t *ti)
{
    double price;
    int ival, id, position, operation, side, size;

    read_int(ti, &ival); /* version */
    read_int(ti, &ival), id = ival;
    read_int(ti, &ival), position = ival;
    read_int(ti, &ival), operation = ival;
    read_int(ti, &ival), side = ival;
    read_double(ti, &price);
    read_int(ti, &ival), size = ival;

    if(ti->connected)
        event_update_mkt_depth(ti->opaque_user_defined, id, position, operation,
                               side, price, size);
}

static void receive_market_depth_l2(tws_instance_t *ti)
{
    double price;
    char *mkt_maker = alloc_string(ti);
    long lval;
    int ival, id, position, operation, side, size;

    read_int(ti, &ival); /*version*/
    read_int(ti, &ival), id = ival;
    read_int(ti, &ival), position = ival;
    
    lval = sizeof(tws_string_t), read_line(ti, mkt_maker, &lval);
    read_int(ti, &ival), operation = ival;
    read_int(ti, &ival), side = ival;
    read_double(ti, &price);
    read_int(ti, &ival), size = ival;
    
    if(ti->connected)
        event_update_mkt_depth_l2(ti->opaque_user_defined, id, position, mkt_maker,
                                  operation, side, price, size);

    free_string(ti, mkt_maker);
}

static void receive_news_bulletins(tws_instance_t *ti)
{
    char *msg, originating_exch[60];
    long lval;
    int ival, newsmsgid, newsmsgtype;

    read_int(ti, &ival); /*version*/
    read_int(ti, &ival), newsmsgid = ival;
    read_int(ti, &ival), newsmsgtype = ival;
    
    lval = sizeof ti->mempool;
    msg = (char *) &ti->mempool[0]; msg[0] = '\0';
    
    read_line(ti, msg, &lval); /* news message */
    lval = sizeof originating_exch, read_line(ti, originating_exch, &lval);
    
    if(ti->connected)
        event_update_news_bulletin(ti->opaque_user_defined, newsmsgid, newsmsgtype,
                                   msg, originating_exch);
}

static void receive_managed_accts(tws_instance_t *ti)
{
    long lval;
    char *acct_list = alloc_string(ti);
    int ival;
    
    read_int(ti, &ival); /*version*/
    lval = sizeof(tws_string_t), read_line(ti, acct_list, &lval); /* accounts list */
    
    if(ti->connected)
        event_managed_accounts(ti->opaque_user_defined, acct_list);

    free_string(ti, acct_list);
}

static void receive_receive_fa(tws_instance_t *ti)
{
    long lval = sizeof ti->mempool;
    char *xml = (char *) &ti->mempool[0];
    int ival, fadata_type;

    xml[0] = '\0';
    read_int(ti, &ival); /*version*/
    read_int(ti, &ival), fadata_type = ival;
    read_line(ti, xml, &lval); /* xml */
    if(ti->connected)
        event_receive_fa(ti->opaque_user_defined, fadata_type, xml);    
}

static void event_loop(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    int msgid;

    do {
        read_int(ti, &msgid);
        
        if(msgid < 1 || msgid >= no_more_messages) {
#ifdef DEBUG
            printf("read_int: invalid msgid received %d\n", msgid);
#endif
            break;
        }
        
#ifdef DEBUG
        printf("\nreceived %s\n", tws_incoming_msg_names[msgid-1]);
#endif
      
        switch(msgid) {
            case TICK_PRICE:
                receive_tick_price(ti); break;
        
            case TICK_SIZE:
                receive_tick_size(ti); break;
        
            case ORDER_STATUS:
                receive_order_status(ti); break;
        
            case ACCT_VALUE:
                receive_acct_value(ti); break;
        
            case PORTFOLIO_VALUE:
                receive_portfolio_value(ti); break;
        
            case ACCT_UPDATE_TIME:
                receive_acct_update_time(ti); break;
        
            case ERR_MSG:
                receive_err_msg(ti); break;
        
            case OPEN_ORDER:
                receive_open_order(ti); break;
        
            case NEXT_VALID_ID:
                receive_next_valid_id(ti); break;
        
            case CONTRACT_DATA:
                receive_contract_data(ti); break;
        
            case EXECUTION_DATA:
                receive_execution_data(ti); break;
        
            case MARKET_DEPTH:
                receive_market_depth(ti); break;
        
            case MARKET_DEPTH_L2:
                receive_market_depth_l2(ti); break;
        
            case NEWS_BULLETINS:
                receive_news_bulletins(ti); break;
        
            case MANAGED_ACCTS:
                receive_managed_accts(ti); break;
        
            case RECEIVE_FA:     
                receive_receive_fa(ti); break;
        
            default: ; /*provably can't happen */
        }
    } while(ti->connected);

#ifdef DEBUG
    printf("reader thread exiting\n");
#endif
}

/* caller supplies start_thread method */
void *tws_create(start_thread_t start_thread, void *opaque_user_defined)
{
    tws_instance_t *ti = calloc(1, sizeof *ti);
    if(ti) {
        ti->fd = (socket_t) ~0;
        ti->start_thread = start_thread;
        ti->opaque_user_defined = opaque_user_defined;
    }

    return ti;
}

void tws_destroy(void *tws_instance)
{
    tws_instance_t *ti = (tws_instance_t *) tws_instance;
    close(ti->fd);
    free(tws_instance);
}

#if 0
/*
 * returns -1 on fatal error, 0 if socket not readable/writeable
 * 1 if socket is readable
 * 2 if socket is writeable
 * 3 if it is both readable and writeable
 * if block_forever is true then tws_poll sleeps, otherwise returns immediately
 */
static int tws_poll(void *tws_instance, long block_forever)
{
    fd_set rd, wr;
    struct timeval tmout;
    tws_instance_t *ti = (tws_instance_t *) tws_instance;
    int err;

    while(1) {
        if(!block_forever) {
            tmout.tv_sec = 0;
            tmout.tv_usec = 0;
        }
        FD_ZERO(&rd); FD_ZERO(&wr);
        FD_SET(ti->fd, &rd); FD_SET(ti->fd, &wr);
        
        err = select(1 + ti->fd, &rd, &wr, 0, block_forever ? 0 : &tmout);
        if(err == -1) {
#ifdef WINDOWS
            if(WSAGetLastError() == WSAEINTR)
#else /* unix */
                if(errno == EINTR)          
#endif
                    continue;

            return err;
        }

        break;
    }

    if(err > 0) {
        err = 0;
        err |= FD_ISSET(ti->fd, &rd) ? TWS_CAN_READ : 0;
        err |= FD_ISSET(ti->fd, &wr) ? TWS_CAN_WRITE : 0;
    }

    return err;
}
#endif

/* return 1 on error, 0 if successful, it's all right to block
 * str must be null terminated
 */
static int send_str(tws_instance_t *ti, const char str[])
{
    long len = (long) strlen(str) + 1;
    int err;
    err = len != send(ti->fd, str, len, 0);

    if(err) {
        close(ti->fd);
        ti->connected = 0;
    }
    return err;
}

static int send_double(tws_instance_t *ti, double *val)
{
    char buf[5*(sizeof *val)/2 + 8];
    long len;
    int err = 1;

    len = sprintf(buf, "%.7lf", *val);
    if(len++ < 0)
        goto out;

    if(len != send(ti->fd, buf, len, 0)) {
        close(ti->fd);
        ti->connected = 0;
        goto out;
    }

    err = 0;
out:
    return err;
}

/* return 1 on error, 0 if successful, it's all right to block */
static int send_int(tws_instance_t *ti, int val)
{
    char buf[5*(sizeof val)/2 + 2];
    long len;
    int err = 1;

    len = sprintf(buf, "%d", val);
    if(len++ < 0)
        goto out;

    if(len != send(ti->fd, buf, len, 0)) {
        close(ti->fd);
        ti->connected = 0;
        goto out;
    }
    err = 0;
out:
    return err;
}

/* returns 1 char at a time, kernel not entered most of the time 
 * return -1 on error or EOF
 */
static int read_char(tws_instance_t *ti)
{
    int nread;    

    if(ti->buf_next == ti->buf_last) {
	nread = recv(ti->fd, ti->buf, sizeof ti->buf, 0);
	if(nread <= 0) {
	    nread = -1;
	    goto out;
	}
	ti->buf_last = nread;
	ti->buf_next = 0;
    }

    nread = ti->buf[ti->buf_next++];
out: return nread;
}

/* return -1 on error, 0 if successful */
static int read_line(tws_instance_t *ti, char *line, long *len)
{
    long j;
    int nread = -1, err = -1;

    for(j = 0; j < *len; j++) {
	nread = read_char(ti);
        if(nread < 0) {
#ifdef DEBUG
            printf("read_line: going out 1, nread=%d\n", nread);
#endif
            goto out;
        }
	line[j] = (char) nread;
        if(line[j] == '\0')
            break;
    }

    if(j == *len) {
#ifdef DEBUG
        printf("read_line: going out 2 j=%ld\n", j);
#endif
        goto out;
    }

#ifdef DEBUG
    printf("read_line: i read %s\n", line);
#endif
    err = 0;
out:
    if(err < 0) {
        if(nread <= 0) {
            close(ti->fd);
            ti->connected = 0;
        } else {
#ifdef DEBUG
            printf("read_line: corruption happened\n");
#endif
        }
    }

    return err;
}

static int read_double(tws_instance_t *ti, double *val)
{
    char line[5*(sizeof *val)/2 + 2];
    long len = sizeof line;
    int err = read_line(ti, line, &len);

    if(err < 0) {
        *val = *dNAN;
        return err;
    }

    *val = atof(line);
    return 0;
}

/* return -1 on error, 0 if successful */
static int read_int(tws_instance_t *ti, int *val)
{
    char line[5*(sizeof *val)/2 + 2];
    long len = sizeof line;
    int err = read_line(ti, line, &len);

    if(err < 0) {
        *val = -1000;
        return err;
    }

    *val = atoi(line);
    return 0;
}

#ifdef WINDOWS
/* returns 1 on error, 0 if successful */
static int init_winsock()
{
    WORD wVersionRequested = MAKEWORD(1, 0);
    WSADATA wsaData;
    
    return !!WSAStartup(wVersionRequested, &wsaData );  
}
#endif

int tws_connect(void *tws, const char host[], unsigned short port, int clientid)
{
    tws_instance_t *ti = tws;
    const char *hostname;
    struct hostent *h;
    int val;
    struct sockaddr_in addr;
    int err;
    
    if(ti->connected) {
        err = ALREADY_CONNECTED; goto out;
    }

#ifdef WINDOWS
    if(init_winsock())
        goto connect_fail;
#endif
    
    ti->fd = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
#ifdef WINDOWS
    err = ti->fd == INVALID_SOCKET;
#else
    err = ti->fd < 0;
#endif
    if(err) {
    connect_fail:
        err = CONNECT_FAIL; goto out;
    }
    
    hostname = host ? host : "127.0.0.1";
    /* use this instead of getaddrinfo since old headers might not have it */
    h = gethostbyname(hostname); /* beware: not thread safe, may cause corruption */
    if(!h)
        goto connect_fail;

    memset(&addr, 0, sizeof addr);
    addr.sin_family = PF_INET;
    memcpy((void *) &addr.sin_addr, h->h_addr_list[0], sizeof addr.sin_addr);
    addr.sin_port = htons(port);
    if(connect(ti->fd, (struct sockaddr *) &addr, sizeof addr) < 0)
        goto connect_fail;

    if(send_int(ti, TWSCLIENT_VERSION))
        goto connect_fail;

    if(read_int(ti, &val))
        goto connect_fail;

    if(val < 1) {
        err = NO_VALID_ID; goto out;
    }

    ti->server_version = val;
    if(ti->server_version >= 3)
        if(send_int(ti, clientid))
            goto connect_fail;

    if((*ti->start_thread)(event_loop, ti) < 0)
        goto connect_fail;

    ti->connected = 1;
    err = 0;
out:
    return err;
}

void  tws_disconnect(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    close(ti->fd);
}

int tws_req_mkt_data(void *tws, long ticker_id, tr_contract_t *contract)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    register long j;

    send_int(ti, REQ_MKT_DATA);
    send_int(ti, 5 /* version */);
    send_int(ti, (unsigned int) ticker_id);

    send_str(ti, contract->c_symbol);
    send_str(ti, contract->c_sectype);
    send_str(ti, contract->c_expiry);
    send_double(ti, &contract->c_strike);
    send_str(ti, contract->c_right);

    if(ti->server_version >= 15)
        send_str(ti, contract->c_multiplier);

    send_str(ti, contract->c_exchange);
    
    if(ti->server_version >= 14)
        send_str(ti, contract->c_primary_exch);

    send_str(ti, contract->c_currency);
    if(ti->server_version >= 2)
        send_str(ti, contract->c_local_symbol);

    if(ti->server_version >= 8) {
        send_int(ti, contract->c_num_combolegs);
        for(j = 0; j < contract->c_num_combolegs; j++) {
            send_int(ti, contract->c_comboleg[j].co_conid);
            send_int(ti, contract->c_comboleg[j].co_ratio);
            send_str(ti, contract->c_comboleg[j].co_action);
            send_str(ti, contract->c_comboleg[j].co_exchange);
        }
    }

    return ti->connected ? 0 : FAIL_SEND_REQMKT;
}

int req_contract_details(void *tws, tr_contract_t *contract)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    /* This feature is only available for versions of TWS >=4 */
    if(ti->server_version < 4)
        return UPDATE_TWS;

    /* send req mkt data msg */
    send_int(ti, REQ_CONTRACT_DATA);
    send_int(ti, 2 /*VERSION*/);
    
    send_str(ti, contract->c_symbol);
    send_str(ti, contract->c_sectype);
    send_str(ti, contract->c_expiry);
    send_double(ti, &contract->c_strike);
    send_str(ti, contract->c_right);

    if(ti->server_version >= 15)
        send_str(ti, contract->c_multiplier);

    send_str(ti, contract->c_exchange);
    send_str(ti, contract->c_currency);
    send_str(ti, contract->c_local_symbol);

    return ti->connected ? 0 : FAIL_SEND_REQCONTRACT;
}

int tws_req_mkt_depth(void *tws, long tickerid, tr_contract_t *contract)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    
    /* This feature is only available for versions of TWS >=6 */
    if(ti->server_version < 6)
        return UPDATE_TWS;
    
    send_int(ti, REQ_MKT_DEPTH);
    send_int(ti, 2 /*VERSION*/);     /*final int VERSION = 2;*/
    send_int(ti, (int) tickerid);
    
    send_str(ti, contract->c_symbol);
    send_str(ti, contract->c_sectype);
    send_str(ti, contract->c_expiry);
    send_double(ti, &contract->c_strike);
    send_str(ti, contract->c_right);
    if(ti->server_version >= 15)
        send_str(ti, contract->c_multiplier);

    send_str(ti, contract->c_exchange);
    send_str(ti, contract->c_currency);
    send_str(ti, contract->c_local_symbol);
    
    return ti->connected ? 0 : FAIL_SEND_REQMKTDEPTH;
}

int tws_cancel_mkt_data(void *tws, long tickerid)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, CANCEL_MKT_DATA);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, (int) tickerid);
    return ti->connected ? 0 : FAIL_SEND_CANMKT;
}

int tws_cancel_mkt_depth(void *tws, long tickerid)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    /* This feature is only available for versions of TWS >=6 */
    if(ti->server_version < 6)
        return UPDATE_TWS;
    
    send_int(ti, CANCEL_MKT_DEPTH);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, (int) tickerid);

    return ti->connected ? 0 : FAIL_SEND_CANMKTDEPTH;
}


int tws_place_order(void *tws, long id, tr_contract_t *contract, tr_order_t *order)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    register long j;

    send_int(ti, PLACE_ORDER);
    send_int(ti, 15 /*VERSION*/);
    send_int(ti, (int) id);
    
    /* send contract fields */
    send_str(ti, contract->c_symbol);
    send_str(ti, contract->c_sectype);
    send_str(ti, contract->c_expiry);
    send_double(ti, &contract->c_strike);
    send_str(ti, contract->c_right);
    if (ti->server_version >= 15)
        send_str(ti, contract->c_multiplier);

    send_str(ti, contract->c_exchange);
    if(ti->server_version >= 14)
        send_str(ti, contract->c_primary_exch);

    send_str(ti, contract->c_currency);
    if(ti->server_version >= 2)
        send_str(ti, contract->c_local_symbol);
    
    /* send main order fields */
    send_str(ti, order->o_action);
    send_int(ti, order->o_total_quantity);
    send_str(ti, order->o_ordertype);
    send_double(ti, &order->o_lmt_price);
    send_double(ti, &order->o_aux_price);
    
    /* send extended order fields */
    send_str(ti, order->o_tif);
    send_str(ti, order->o_oca_group);
    send_str(ti, order->o_account);
    send_str(ti, order->o_open_close);
    send_int(ti, order->o_origin);
    send_str(ti, order->o_orderref);
    send_int(ti, order->o_transmit);
    if(ti->server_version >= 4)
        send_int(ti, order->o_parentid);
    
    if(ti->server_version >= 5 ) {
        send_int(ti, order->o_block_order);
        send_int(ti, order->o_sweep_to_fill);
        send_int(ti, order->o_display_size);
        send_int(ti, order->o_trigger_method);
        send_int(ti, order->o_ignore_rth);
    }
    
    if(ti->server_version >= 7)
        send_int(ti, order->o_hidden);

    /* Send combo legs for BAG requests */
    if(ti->server_version >= 8 && !strcasecmp(contract->c_sectype, "BAG")) {
        send_int(ti, contract->c_num_combolegs);

        for(j = 0; j < contract->c_num_combolegs; j++) {
            send_int(ti, contract->c_comboleg[j].co_conid);
            send_int(ti, contract->c_comboleg[j].co_ratio);
            send_str(ti, contract->c_comboleg[j].co_action);
            send_str(ti, contract->c_comboleg[j].co_exchange);
            send_int(ti, contract->c_comboleg[j].co_open_close);
        }
    }
    
    if(ti->server_version >= 9)
        send_str(ti, order->o_shares_allocation); /* deprecated */
    
    if(ti->server_version >= 10)
        send_double(ti, &order->o_discretionary_amt);
    
    if(ti->server_version >= 11)
        send_str(ti, order->o_good_after_time);
    
    if(ti->server_version >= 12)
        send_str(ti, order->o_good_till_date);
    
    if (ti->server_version >= 13 ) {
        send_str(ti, order->o_fagroup);
        send_str(ti, order->o_famethod);
        send_str(ti, order->o_fapercentage);
        send_str(ti, order->o_faprofile);
    }
    
    return ti->connected ? 0 : FAIL_SEND_ORDER;
}

int tws_req_account_updates(void *tws, int subscribe, const char acct_code[])
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, REQ_ACCOUNT_DATA );
    send_int(ti, 2 /*VERSION*/);
    send_int(ti, subscribe);
    
    /* Send the account code. This will only be used for FA clients */
    if (ti->server_version >= 9)
        send_str(ti, acct_code);
    
    return ti->connected ? 0 : FAIL_SEND_ACCT;
}

int tws_req_executions(void *tws, tr_exec_filter_t *filter)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, REQ_EXECUTIONS);
    send_int(ti, 2 /*VERSION*/);
    
    /* Send the execution rpt filter data */
    if(ti->server_version >= 9) {
        send_int(ti, filter->f_clientid);
        send_str(ti, filter->f_acctcode);
        
        /* Note that the valid format for m_time is "yyyymmdd-hh:mm:ss" */
        send_str(ti, filter->f_time);
        send_str(ti, filter->f_symbol);
        send_str(ti, filter->f_sectype);
        send_str(ti, filter->f_exchange);
        send_str(ti, filter->f_side);
    }

    return ti->connected ? 0 : FAIL_SEND_EXEC;
}

int tws_cancel_order(void *tws, long order_id)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    /* send cancel order msg */
    send_int(ti, CANCEL_ORDER);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, (int) order_id);

    return ti->connected ? 0 : FAIL_SEND_ORDER;
}

int tws_req_open_orders(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, REQ_OPEN_ORDERS);
    send_int(ti, 1 /*VERSION*/);

    return ti->connected ? 0 : FAIL_SEND_OORDER;
}

int tws_req_ids(void *tws, int numids)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, REQ_IDS);
    send_int(ti, 1 /* VERSION */);
    send_int(ti, numids);

    return ti->connected ? 0 : FAIL_SEND_CORDER;
}

int tws_req_news_bulletins(void *tws, int allmsgs)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, REQ_NEWS_BULLETINS);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, allmsgs);

    return ti->connected ? 0 : FAIL_SEND_CORDER;
}

int tws_cancel_news_bulletins(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    
    send_int(ti, CANCEL_NEWS_BULLETINS);
    send_int(ti, 1 /*VERSION*/);

    return ti->connected ? 0 : FAIL_SEND_CORDER;
}

int tws_set_server_log_level(void *tws, int level)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, SET_SERVER_LOGLEVEL);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, level);

    return ti->connected ? 0 : FAIL_SEND_SERVER_LOG_LEVEL;
}

int tws_req_auto_open_orders(void *tws, int auto_bind)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    
    send_int(ti, REQ_AUTO_OPEN_ORDERS);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, auto_bind);

    return ti->connected ? 0 : FAIL_SEND_OORDER;
}

int tws_req_all_open_orders(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    /* send req all open orders msg */
    send_int(ti, REQ_ALL_OPEN_ORDERS);
    send_int(ti, 1 /*VERSION*/);

    return ti->connected ? 0 : FAIL_SEND_OORDER;
}

int tws_req_managed_accts(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    /* send req FA managed accounts msg */
    send_int(ti, REQ_MANAGED_ACCTS);
    send_int(ti, 1 /*VERSION*/);

    return ti->connected ? 0 : FAIL_SEND_OORDER;
}

int tws_request_fa(void *tws, long fa_data_type)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    /* This feature is only available for versions of TWS >= 13 */
    if(ti->server_version < 13)
        return UPDATE_TWS;
    
    send_int(ti, REQ_FA);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, (int) fa_data_type);

    return ti->connected ? 0 : FAIL_SEND_FA_REQUEST;
}

int tws_replace_fa(void *tws, long fa_data_type, const char xml[])
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    
    /* This feature is only available for versions of TWS >= 13 */
    if(ti->server_version < 13)
        return UPDATE_TWS;
    
    send_int(ti, REPLACE_FA );
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, (int) fa_data_type);
    send_str(ti, xml);
    
    return ti->connected ? 0 : FAIL_SEND_FA_REPLACE;
}
