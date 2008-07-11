#define TWSAPI_GLOBALS
#include "twsapi.h"

#ifdef WINDOWS
#include <winsock2.h>
#include <WS2tcpip.h>
typedef SOCKET socket_t;
#define close closesocket
#define strcasecmp(x,y) _stricmp(x,y)
#define strncasecmp(x,y,z) _strnicmp(x,y,z)
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
#endif

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_TWS_STRINGS 127
#define WORD_SIZE_IN_BITS (8*sizeof(long))
#define WORDS_NEEDED(num, wsize) (!!((num)%(wsize)) + ((num)/(wsize)))
#define ROUND_UP_POW2(num, pow2) (((num) + (pow2)-1) & ~((pow2)-1))
#define INTEGER_MAX_VALUE ((int) ~(1U<<(8*sizeof(int) -1)))
#define DBL_NOTMAX(d) (fabs((d) - DBL_MAX) > DBL_EPSILON)

typedef struct {
    char str[512]; /* maximum conceivable string length */
} tws_string_t;

typedef struct tws_instance {
    void *opaque;
    start_thread_t start_thread;
    external_func_t extfunc;
    socket_t fd;
    char connect_time[60]; /* server reported time */
    unsigned char buf[240]; /* buffer up to 240 chars at a time */
    unsigned int buf_next, buf_last; /* index of next, last chars in buf */
    unsigned int server_version;
    volatile unsigned int connected, started;
    tws_string_t mempool[MAX_TWS_STRINGS];
    unsigned long bitmask[WORDS_NEEDED(MAX_TWS_STRINGS, WORD_SIZE_IN_BITS)];
} tws_instance_t;

static int read_double(tws_instance_t *ti, double *val);
static int read_double_max(tws_instance_t *ti, double *val);
static int read_long(tws_instance_t *ti, long *val);
static int read_int(tws_instance_t *ti, int *val);
static int read_int_max(tws_instance_t *ti, int *val);
static int read_line(tws_instance_t *ti, char *line, long *len);

/* access to these strings is single threaded
 * replace plain bitops with atomic test_and_set_bit/clear_bit + memory barriers
 * if multithreaded access is desired (not applicable at present)
 */
static char *alloc_string(tws_instance_t *ti)
{
    unsigned long j, index, bits;

    for(j = 0; j < MAX_TWS_STRINGS; j++) {
        index = j / WORD_SIZE_IN_BITS;
        if(ti->bitmask[index] == ~0UL) {
            j = ROUND_UP_POW2(j+1, WORD_SIZE_IN_BITS) -1;
            continue;
        }

        bits = 1UL << (j & (WORD_SIZE_IN_BITS - 1));
        if(!(ti->bitmask[index] & bits)) {
            ti->bitmask[index] |= bits;
            return ti->mempool[j].str;
        }
    }
#ifdef TWS_DEBUG
    printf("alloc_string: ran out of strings, will crash shortly\n");
#endif
    return 0;
}

static void free_string(tws_instance_t *ti, void *ptr)
{
    unsigned long j = (unsigned long) ((tws_string_t *) ptr - &ti->mempool[0]),
        index = j / WORD_SIZE_IN_BITS,
        bits = 1UL << (j & (WORD_SIZE_IN_BITS - 1));

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
    c->c_combolegs_descrip = alloc_string(ti);
}

static void destroy_contract(tws_instance_t *ti, tr_contract_t *c)
{
    free_string(ti, c->c_combolegs_descrip);
    free_string(ti, c->c_multiplier);
    free_string(ti, c->c_local_symbol);
    free_string(ti, c->c_right);
    free_string(ti, c->c_currency);
    free_string(ti, c->c_expiry);
    free_string(ti, c->c_primary_exch);
    free_string(ti, c->c_exchange);
    free_string(ti, c->c_sectype);
    free_string(ti, c->c_symbol);
}

static void init_order(tws_instance_t *ti, tr_order_t *o)
{
    memset(o, 0, sizeof *o);

    o->o_good_after_time = alloc_string(ti);
    o->o_good_till_date = alloc_string(ti);
    o->o_fagroup = alloc_string(ti);
    o->o_famethod = alloc_string(ti);
    o->o_fapercentage = alloc_string(ti);
    o->o_faprofile = alloc_string(ti);
    o->o_action = alloc_string(ti);
    o->o_order_type = alloc_string(ti);
    o->o_tif = alloc_string(ti);
    o->o_oca_group = alloc_string(ti);
    o->o_account = alloc_string(ti);
    o->o_open_close = alloc_string(ti);
    o->o_orderref = alloc_string(ti);
    o->o_rule80a = alloc_string(ti);
    o->o_settling_firm = alloc_string(ti);
    o->o_designated_location = alloc_string(ti);
    o->o_delta_neutral_order_type = alloc_string(ti);
    o->o_clearing_account = alloc_string(ti);
    o->o_clearing_intent = alloc_string(ti);
}

static void destroy_order(tws_instance_t *ti, tr_order_t *o)
{
    free_string(ti, o->o_clearing_intent);
    free_string(ti, o->o_clearing_account);
    free_string(ti, o->o_delta_neutral_order_type);
    free_string(ti, o->o_designated_location);
    free_string(ti, o->o_settling_firm);
    free_string(ti, o->o_rule80a);
    free_string(ti, o->o_orderref);
    free_string(ti, o->o_open_close);
    free_string(ti, o->o_account);
    free_string(ti, o->o_oca_group);
    free_string(ti, o->o_tif);
    free_string(ti, o->o_order_type);
    free_string(ti, o->o_action);
    free_string(ti, o->o_faprofile);
    free_string(ti, o->o_fapercentage);
    free_string(ti, o->o_famethod);
    free_string(ti, o->o_fagroup);
    free_string(ti, o->o_good_till_date);
    free_string(ti, o->o_good_after_time);
}

static void init_order_status(tws_instance_t *ti, tr_order_status_t *ost)
{
    memset(ost, 0, sizeof *ost);

    ost->ost_status = alloc_string(ti);
    ost->ost_init_margin = alloc_string(ti);
    ost->ost_maint_margin = alloc_string(ti);
    ost->ost_equity_with_loan = alloc_string(ti);
    ost->ost_commission_currency = alloc_string(ti);
    ost->ost_warning_text = alloc_string(ti);
}

static void destroy_order_status(tws_instance_t *ti, tr_order_status_t *ost)
{
    free_string(ti, ost->ost_warning_text);
    free_string(ti, ost->ost_commission_currency);
    free_string(ti, ost->ost_equity_with_loan);
    free_string(ti, ost->ost_maint_margin);
    free_string(ti, ost->ost_init_margin);
    free_string(ti, ost->ost_status);
}

static void init_contract_details(tws_instance_t *ti, tr_contract_details_t *cd)
{
    struct contract_details_summary *ds = &cd->d_summary;

    memset(cd, 0, sizeof *cd);
    ds->s_symbol = alloc_string(ti);
    ds->s_sectype = alloc_string(ti);
    ds->s_expiry = alloc_string(ti);
    ds->s_right = alloc_string(ti);
    ds->s_exchange = alloc_string(ti);
    ds->s_currency = alloc_string(ti);
    ds->s_local_symbol = alloc_string(ti);
    ds->s_multiplier = alloc_string(ti);

    cd->d_market_name = alloc_string(ti);
    cd->d_trading_class = alloc_string(ti);
    cd->d_order_types = alloc_string(ti);
    cd->d_valid_exchanges = alloc_string(ti);
    cd->d_cusip = alloc_string(ti);
    cd->d_maturity = alloc_string(ti);
    cd->d_issue_date = alloc_string(ti);
    cd->d_ratings = alloc_string(ti);
    cd->d_bond_type = alloc_string(ti);
    cd->d_coupon_type = alloc_string(ti);
    cd->d_desc_append = alloc_string(ti);
    cd->d_next_option_date = alloc_string(ti);
    cd->d_next_option_type = alloc_string(ti);
    cd->d_notes = alloc_string(ti);
}

static void destroy_contract_details(tws_instance_t *ti, tr_contract_details_t *cd)
{
    struct contract_details_summary *ds = &cd->d_summary;

    free_string(ti, cd->d_notes);
    free_string(ti, cd->d_next_option_type);
    free_string(ti, cd->d_next_option_date);
    free_string(ti, cd->d_desc_append);
    free_string(ti, cd->d_coupon_type);
    free_string(ti, cd->d_bond_type);
    free_string(ti, cd->d_ratings);
    free_string(ti, cd->d_issue_date);
    free_string(ti, cd->d_maturity);
    free_string(ti, cd->d_cusip);
    free_string(ti, cd->d_valid_exchanges);
    free_string(ti, cd->d_order_types);
    free_string(ti, cd->d_trading_class);
    free_string(ti, cd->d_market_name);
   
    free_string(ti, ds->s_multiplier);
    free_string(ti, ds->s_local_symbol);
    free_string(ti, ds->s_currency);
    free_string(ti, ds->s_exchange);
    free_string(ti, ds->s_right);
    free_string(ti, ds->s_expiry);
    free_string(ti, ds->s_sectype);
    free_string(ti, ds->s_symbol);
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
    free_string(ti, exec->e_side);
    free_string(ti, exec->e_exchange);
    free_string(ti, exec->e_acct_number);
    free_string(ti, exec->e_time);
    free_string(ti, exec->e_execid);
}


static void receive_tick_price(tws_instance_t *ti)
{
    double price;
    int ival, version, ticker_id, tick_type, size = 0, can_auto_execute = 0, size_tick_type;

    read_int(ti, &ival), version = ival;
    read_int(ti, &ival), ticker_id = ival;
    read_int(ti, &ival), tick_type = ival;
    read_double(ti, &price);

    if(version >= 2)
        read_int(ti, &ival), size = ival;
    
    if(version >= 3)
        read_int(ti, &ival), can_auto_execute = ival;
    
    if(ti->connected)
        event_tick_price(ti->opaque, ticker_id, tick_type, price, can_auto_execute);
    
    if(version >= 2) {
        switch (tick_type) {
            case 1: /* BID */  size_tick_type = 0; break; /* BID_SIZE */
            case 2: /* ASK */  size_tick_type = 3; break; /* ASK_SIZE */
            case 4: /* LAST */ size_tick_type = 5; break; /* LAST_SIZE */
            default: size_tick_type = -1; /* not a tick */
        }
        
        if(size_tick_type != -1)
            if(ti->connected)
                event_tick_size(ti->opaque, ticker_id, size_tick_type, size);
    }
}


static void receive_tick_size(tws_instance_t *ti)
{
    int ival, ticker_id, tick_type, size;
    
    read_int(ti, &ival); /* version unused */
    read_int(ti, &ival), ticker_id = ival;
    read_int(ti, &ival), tick_type = ival;
    read_int(ti, &ival), size = ival;
    
    if(ti->connected)
        event_tick_size(ti->opaque, ticker_id, tick_type, size);
}

static void receive_tick_option_computation(tws_instance_t *ti)
{
    double implied_vol, delta, model_price, pv_dividend;
    int ival, ticker_id, tick_type;

    read_int(ti, &ival /*version*/); /* ignored */
    read_int(ti, &ival), ticker_id = ival;
    read_int(ti, &ival), tick_type = ival;
    read_double(ti, &implied_vol);

    if(implied_vol < 0) /* -1 is the "not yet computed" indicator */
        implied_vol = DBL_MAX;

    read_double(ti, &delta);
    if(fabs(delta) > 1.0)  /* -2 is the "not yet computed" indicator */
        delta = DBL_MAX;

    if(tick_type == MODEL_OPTION) { /* introduced in version == 5 */
        read_double(ti, &model_price);
        read_double(ti, &pv_dividend);
    } else
        model_price = pv_dividend = DBL_MAX;
    
    if(ti->connected)
        event_tick_option_computation(ti->opaque, ticker_id, tick_type, implied_vol, delta, model_price, pv_dividend);   
}

static void receive_tick_generic(tws_instance_t *ti)
{
    double value;
    int ival, ticker_id, tick_type;

    read_int(ti, &ival /*version */); /* ignored */
    read_int(ti, &ival), ticker_id = ival;
    read_int(ti, &ival), tick_type = ival;
    read_double(ti, &value);
    if(ti->connected)
        event_tick_generic(ti->opaque, ticker_id, tick_type, value);
}

static void receive_tick_string(tws_instance_t *ti)
{
    long lval = sizeof ti->mempool;
    char *value = (char *) &ti->mempool[0];
    int ival, ticker_id, tick_type;

    read_int(ti, &ival /*version */); /* ignored */
    read_int(ti, &ival), ticker_id = ival;
    read_int(ti, &ival), tick_type = ival;
    read_line(ti, value, &lval);
    if(ti->connected)
        event_tick_string(ti->opaque, ticker_id, tick_type, value);
}

static void receive_tick_efp(tws_instance_t *ti)
{
    double basis_points, implied_futures_price, dividend_impact, dividends_to_expiry;
    char *formatted_basis_points = alloc_string(ti), *future_expiry = alloc_string(ti);
    long lval;
    int ival, ticker_id, tick_type, hold_days;

    read_int(ti, &ival /*version unused */);
    read_int(ti, &ival); ticker_id = ival;
    read_int(ti, &ival); tick_type = ival;
    read_double(ti, &basis_points);

    lval = sizeof(tws_string_t), read_line(ti, formatted_basis_points, &lval); 
    read_double(ti, &implied_futures_price);
    read_int(ti, &ival); hold_days = ival;
    lval = sizeof(tws_string_t), read_line(ti, future_expiry, &lval); 
    read_double(ti, &dividend_impact);
    read_double(ti, &dividends_to_expiry);

    if(ti->connected)
        event_tick_efp(ti->opaque, ticker_id, tick_type, basis_points, formatted_basis_points, implied_futures_price, hold_days, future_expiry, dividend_impact, dividends_to_expiry);

    free_string(ti, future_expiry);
    free_string(ti, formatted_basis_points);
}

static void receive_order_status(tws_instance_t *ti)
{
    double avg_fill_price, last_fill_price = 0.0;
    long lval;
    char *status = alloc_string(ti), *why_held = alloc_string(ti);
    int ival, version, id, filled, remaining, permid = 0, parentid = 0, clientid = 0;

    read_int(ti, &ival), version = ival;
    read_int(ti, &ival), id = ival;
    lval = sizeof(tws_string_t), read_line(ti, status, &lval);
    read_int(ti, &ival), filled = ival;
    read_int(ti, &ival), remaining = ival;
    read_double(ti, &avg_fill_price);
    
    if(version >= 2)
        read_int(ti, &ival), permid = ival;

    if(version >= 3)
        read_int(ti, &ival), parentid = ival;
    
    if(version >= 4)
        read_double(ti, &last_fill_price);
    
    if(version >= 5)
        read_int(ti, &ival), clientid = ival;

    if(version >= 6) {
        lval = sizeof(tws_string_t), read_line(ti, why_held, &lval);
    }
    
    if(ti->connected)
        event_order_status(ti->opaque, id, status, filled, remaining,
                           avg_fill_price, permid, parentid, last_fill_price, clientid, why_held);

    free_string(ti, why_held);
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
    
    if(version >= 2)
        lval = sizeof(tws_string_t), read_line(ti, account_name, &lval);
    
    if(ti->connected)
        event_update_account_value(ti->opaque, key, val, cur, account_name);

    free_string(ti, account_name);
    free_string(ti, cur);
    free_string(ti, val);
    free_string(ti, key);
}

static void receive_portfolio_value(tws_instance_t *ti)
{
    double market_price, market_value, average_cost = 0.0, unrealized_pnl = 0.0,
        realized_pnl = 0.0;
    tr_contract_t contract;
    long lval;
    char *account_name = alloc_string(ti);
    int ival, version, position;
     
    read_int(ti, &ival), version = ival;
    
    init_contract(ti, &contract);

    if(version >= 6)
        read_int(ti, &contract.c_conid);
    
    lval = sizeof(tws_string_t), read_line(ti, contract.c_symbol, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_sectype, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_expiry, &lval);
    read_double(ti, &contract.c_strike);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_right, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_currency, &lval);
    if(version >= 2)
        lval = sizeof(tws_string_t), read_line(ti, contract.c_local_symbol, &lval);
    
    read_int(ti, &ival), position = ival;
    
    read_double(ti, &market_price);
    read_double(ti, &market_value);
    
    if(version >=3 ) {
        read_double(ti, &average_cost);
        read_double(ti, &unrealized_pnl);
        read_double(ti, &realized_pnl);
    }
    
    if(version >= 4)
        lval = sizeof(tws_string_t), read_line(ti, account_name, &lval);
    
    if(ti->connected)
        event_update_portfolio(ti->opaque, &contract, position,
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
        event_update_account_time(ti->opaque, timestamp);
    
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
    
    lval = sizeof(tws_string_t), read_line(ti, msg, &lval);
    
    if(ti->connected)
        event_error(ti->opaque, id, error_code, msg);

    free_string(ti, msg);
}

static void receive_open_order(tws_instance_t *ti)
{
    tr_contract_t contract;
    tr_order_t order;
    tr_order_status_t ost;
    long lval;
    int ival, version;

    init_contract(ti, &contract);
    init_order(ti, &order);
    init_order_status(ti, &ost);
  
    read_int(ti, &ival), version = ival;
    read_int(ti, &order.o_orderid);

    if(version >- 17)
        read_int(ti, &contract.c_conid);

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
    lval = sizeof(tws_string_t), read_line(ti, order.o_order_type, &lval);
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

        read_int(ti, &ival);
        if(version < 18)
            ; /* "will never happen" comment */
        else
            order.o_outside_rth = ival == 1;

        read_int(ti, &ival); order.o_hidden = ival == 1;
        read_double(ti, &order.o_discretionary_amt);
    }
  
    if(version >= 5)
        lval = sizeof(tws_string_t), read_line(ti, order.o_good_after_time, &lval);
  
    if(version >= 6) {
        /* read and discard deprecated variable */
        char *deprecated_shares_allocation = alloc_string(ti);
        lval = sizeof(tws_string_t), read_line(ti, deprecated_shares_allocation, &lval);
        free_string(ti, deprecated_shares_allocation);
    }

    if(version >= 7) {
        lval = sizeof(tws_string_t), read_line(ti, order.o_fagroup, &lval);
        lval = sizeof(tws_string_t), read_line(ti, order.o_famethod, &lval);
        lval = sizeof(tws_string_t), read_line(ti, order.o_fapercentage, &lval);
        lval = sizeof(tws_string_t), read_line(ti, order.o_faprofile, &lval);
    }

    if(version >= 8)
        lval = sizeof(tws_string_t), read_line(ti, order.o_good_till_date, &lval);
    if(version >= 9) {
        lval = sizeof(tws_string_t), read_line(ti, order.o_rule80a, &lval);
        read_double(ti, &order.o_percent_offset);
        lval = sizeof(tws_string_t), read_line(ti, order.o_settling_firm, &lval);
        read_int(ti, &ival), order.o_short_sale_slot = ival; 
        lval = sizeof(tws_string_t), read_line(ti, order.o_designated_location, &lval);
        read_int(ti, &ival), order.o_auction_strategy = ival;
        read_double(ti, &order.o_starting_price);
        read_double(ti, &order.o_stock_ref_price);
        read_double(ti, &order.o_delta);
        read_double(ti, &order.o_stock_range_lower);
        read_double(ti, &order.o_stock_range_upper);
        read_int(ti, &order.o_display_size);

        if(version < 18) /* "will never happen" comment */
            read_int(ti, &ival); /*order.o_rth_only = !!ival;*/

        read_int(ti, &ival), order.o_block_order = !!ival;
        read_int(ti, &ival), order.o_sweep_to_fill = !!ival;
        read_int(ti, &ival), order.o_all_or_none = !!ival;
        read_int(ti, &order.o_min_qty);
        read_int(ti, &ival), order.o_oca_type = ival;
        read_int(ti, &ival), order.o_etrade_only = !!ival;
        read_int(ti, &ival), order.o_firm_quote_only = !!ival;
        read_double(ti, &order.o_nbbo_price_cap);
    }
    
    if(version >= 10) {
        read_int(ti, &order.o_parentid);
        read_int(ti, &order.o_trigger_method);
    }

    if(version >= 11) {
        read_double(ti, &order.o_volatility);
        read_int(ti, &order.o_volatility_type);
        
        if(version == 11) {
            read_int(ti, &ival); 
            order.o_delta_neutral_order_type = !ival ? (char*) "NONE" : (char *) "MKT"; 
        } else {
            lval = sizeof(tws_string_t), read_line(ti, order.o_delta_neutral_order_type, &lval);
            read_double(ti, &order.o_delta_neutral_aux_price);
        }
        
        read_int(ti, &ival); order.o_continuous_update = !!ival;
        if(ti->server_version == 26) {
            read_double(ti, &order.o_stock_range_lower);
            read_double(ti, &order.o_stock_range_upper);
        }
        
        read_int(ti, &order.o_reference_price_type);
    }

    if(version >= 13)
        read_double(ti, &order.o_trail_stop_price);
    
    if(version >= 14) {
        read_double(ti, &order.o_basis_points);
        read_int(ti, &ival), order.o_basis_points_type = ival;
        lval = sizeof(tws_string_t); read_line(ti, contract.c_combolegs_descrip, &lval);
    }

    if(version >= 15) {
        read_int_max(ti, &order.o_scale_num_components);
        read_int_max(ti, &order.o_scale_component_size);
        read_double_max(ti, &order.o_scale_price_increment);
    }
    
    if(version >= 19) {
        lval = sizeof(tws_string_t); read_line(ti, order.o_clearing_account, &lval);
        lval = sizeof(tws_string_t); read_line(ti, order.o_clearing_intent, &lval);
    }
    
    if(version >= 16) {
        read_int(ti, &ival); order.o_whatif = !!ival;

        lval = sizeof(tws_string_t); read_line(ti, ost.ost_status, &lval);
        lval = sizeof(tws_string_t); read_line(ti, ost.ost_init_margin, &lval);
        lval = sizeof(tws_string_t); read_line(ti, ost.ost_maint_margin, &lval);
        lval = sizeof(tws_string_t); read_line(ti, ost.ost_equity_with_loan, &lval);
        read_double_max(ti, &ost.ost_commission);
        read_double_max(ti, &ost.ost_min_commission);
        read_double_max(ti, &ost.ost_max_commission);
        lval = sizeof(tws_string_t); read_line(ti, ost.ost_commission_currency, &lval);
        lval = sizeof(tws_string_t); read_line(ti, ost.ost_warning_text, &lval);
    }
    
    if(ti->connected)
        event_open_order(ti->opaque, order.o_orderid, &contract, &order, &ost);

    destroy_order_status(ti, &ost);
    destroy_order(ti, &order);
    destroy_contract(ti, &contract);
}

static void receive_next_valid_id(tws_instance_t *ti)
{
    int ival;

    read_int(ti, &ival); /* version */
    read_int(ti, &ival); /* orderid */
    if(ti->connected)
        event_next_valid_id(ti->opaque, /*orderid*/ ival);
}

static void receive_contract_data(tws_instance_t *ti)
{
    tr_contract_details_t cdetails;
    long lval;
    int version;

    init_contract_details(ti, &cdetails);
    read_int(ti, &version);
    
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
    read_int(ti, &cdetails.d_summary.s_conid);
    read_double(ti, &cdetails.d_mintick);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.s_multiplier, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_order_types, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_valid_exchanges, &lval);
    if(version >= 2)
        read_int(ti, &cdetails.d_price_magnifier);

    if(ti->connected)
        event_contract_details(ti->opaque, &cdetails);

    destroy_contract_details(ti, &cdetails);
}

static void receive_bond_contract_data(tws_instance_t *ti)
{
    tr_contract_details_t cdetails;
    long lval;
    int ival, version;

    init_contract_details(ti, &cdetails);
    read_int(ti, &ival), version = ival;
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.s_symbol, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.s_sectype, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_cusip, &lval);
    read_double(ti, &cdetails.d_coupon);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_maturity, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_issue_date, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_ratings, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_bond_type, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_coupon_type, &lval);
    read_int(ti, &ival), cdetails.d_convertible = !!ival;
    read_int(ti, &ival), cdetails.d_callable = !!ival;
    read_int(ti, &ival), cdetails.d_putable = !!ival;
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_desc_append, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.s_exchange, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.s_currency, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_market_name, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_trading_class, &lval);

    read_int(ti, &cdetails.d_summary.s_conid);
    read_double(ti, &cdetails.d_mintick);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_order_types, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_valid_exchanges, &lval);

    if(version >= 2) {
        lval = sizeof(tws_string_t); read_line(ti, cdetails.d_next_option_date, &lval);
        lval = sizeof(tws_string_t); read_line(ti, cdetails.d_next_option_type, &lval);
        read_int(ti, &ival); cdetails.d_next_option_partial = !!ival;
        lval = sizeof(tws_string_t); read_line(ti, cdetails.d_notes, &lval);
    }


    if(ti->connected)
        event_bond_contract_details(ti->opaque, &cdetails);

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
    
    if(version >= 5)
        read_int(ti, &contract.c_conid);

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
    lval = sizeof(tws_string_t), read_line(ti, exec.e_side, &lval);
    read_int(ti, &exec.e_shares);
    read_double(ti, &exec.e_price);
    if(version >= 2)
        read_int(ti, &exec.e_permid);
    
    if(version >= 3)
        read_int(ti, &exec.e_clientid);
    
    if(version >= 4)
        read_int(ti, &exec.e_liquidation);
    
    if(ti->connected)
        event_exec_details(ti->opaque, orderid, &contract, &exec);

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
        event_update_mkt_depth(ti->opaque, id, position, operation,
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
        event_update_mkt_depth_l2(ti->opaque, id, position, mkt_maker,
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
    msg = (char *) &ti->mempool[0];
    
    read_line(ti, msg, &lval); /* news message */
    lval = sizeof originating_exch, read_line(ti, originating_exch, &lval);
    
    if(ti->connected)
        event_update_news_bulletin(ti->opaque, newsmsgid, newsmsgtype,
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
        event_managed_accounts(ti->opaque, acct_list);

    free_string(ti, acct_list);
}

static void receive_fa(tws_instance_t *ti)
{
    long lval = sizeof ti->mempool;
    char *xml = (char *) &ti->mempool[0];
    int ival, fadata_type;

    read_int(ti, &ival); /*version*/
    read_int(ti, &ival), fadata_type = ival;
    read_line(ti, xml, &lval); /* xml */
    if(ti->connected)
        event_receive_fa(ti->opaque, fadata_type, xml);    
}

static void receive_historical_data(void *tws)
{
    double open, high, low, close, wap;
    tws_instance_t *ti = (tws_instance_t *) tws;
    long j, lval, index;
    int ival, version, reqid, item_count, volume, gaps, bar_count;
    char date[60], has_gaps[10], completion[60];

    read_int(ti, &ival), version = ival;
    read_int(ti, &ival), reqid = ival;
    memcpy(completion, "finished-", index = sizeof "finished-" -1);

    if(version >=2) {
        lval = sizeof completion -1 - index;
        if(!read_line(ti, &completion[index], &lval)) {
            index += lval;
            completion[index] = '-';
            completion[++index] = '\0';

            lval = sizeof completion -1 - index;
            if(!read_line(ti, &completion[index], &lval))
                completion[index+lval+1] = '\0';
        }
    }
    read_int(ti, &ival), item_count = ival;

    for(j = 0; j < item_count; j++) {
        lval = sizeof date; read_line(ti, date, &lval);
        read_double(ti, &open);
        read_double(ti, &high);
        read_double(ti, &low);
        read_double(ti, &close);
        read_int(ti, &ival), volume = ival;
        read_double(ti, &wap);
        lval = sizeof has_gaps; read_line(ti, has_gaps, &lval);
        gaps = !strncasecmp(has_gaps, "false", 4) ? 0 : 1;

        if(version >= 3)
            read_int(ti, &ival), bar_count = ival;
        else
            bar_count = -1;

        if(ti->connected)
            event_historical_data(ti->opaque, reqid, date, open, high, low, close, volume, bar_count, wap, gaps);

    }
    /* send end of dataset marker */
    if(ti->connected)
        event_historical_data(ti->opaque, reqid, completion, 0.0, 0.0, 0.0, 0.0, 0, -1, 0.0, 0);    
}

static void receive_scanner_parameters(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    long lval = sizeof ti->mempool;
    char *xml = (char *) &ti->mempool[0];
    int ival;

    read_int(ti, &ival); /*version*/
    read_line(ti, xml, &lval);
    if(ti->connected)
        event_scanner_parameters(ti->opaque, xml);
}

static void receive_scanner_data(void *tws)
{
    tr_contract_details_t cdetails;
    tws_instance_t *ti = (tws_instance_t *) tws;
    char *distance = alloc_string(ti), *benchmark = alloc_string(ti), *projection = alloc_string(ti), *legs_str;
    long lval, j;
    int ival, version, rank, ticker_id, num_elements;

    init_contract_details(ti, &cdetails);

    read_int(ti, &ival), version = ival;
    read_int(ti, &ival), ticker_id = ival;
    read_int(ti, &ival), num_elements = ival;

    for(j = 0; j < num_elements; j++) {
        legs_str = 0;
        read_int(ti, &ival), rank = ival;

        if(version >= 3) {
            read_int(ti, &cdetails.d_summary.s_conid);
        }

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
        lval = sizeof(tws_string_t), read_line(ti, distance, &lval);
        lval = sizeof(tws_string_t), read_line(ti, benchmark, &lval);
        lval = sizeof(tws_string_t), read_line(ti, projection, &lval);

        if(version >= 2) {
            legs_str = alloc_string(ti);
            lval = sizeof(tws_string_t), read_line(ti, legs_str, &lval);
        }

        if(ti->connected)
            event_scanner_data(ti->opaque, ticker_id, rank, &cdetails, distance, benchmark, projection, legs_str);

        if(legs_str)
            free_string(ti, legs_str);
    }

    if(ti->connected)
        event_scanner_data_end(ti->opaque, ticker_id);

    destroy_contract_details(ti, &cdetails);
    free_string(ti, distance);
    free_string(ti, benchmark);
    free_string(ti, projection);
}

static void receive_realtime_bars(void *tws)
{    
    tws_instance_t *ti = (tws_instance_t *) tws;
    long lval, time, volume;
    double open, high, low, close, wap;
    int ival, reqid, count;

    read_int(ti, &ival); /* version unused */
    read_int(ti, &ival), reqid=ival;
    read_long(ti, &lval), time=lval;
    read_double(ti, &open);
    read_double(ti, &high);
    read_double(ti, &low);
    read_double(ti, &close);
    read_long(ti, &lval), volume=lval;
    read_double(ti, &wap);
    read_int(ti, &ival), count = ival;

    if(ti->connected)
        event_realtime_bar(ti->opaque, reqid, time, open, high, low, close, volume, wap, count);
}

static void receive_current_time(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    long time;
    int ival;
    
    read_int(ti, &ival /*version unused */);
    read_long(ti, &time);

    if(ti->connected)
        event_current_time(ti->opaque, time);
}

/* allows for reading events from within the same thread or an externally
 * spawned thread, returns 0 on success, -1 on error,
 */
int tws_event_process(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    int msgid, valid = 1;

    read_int(ti, &msgid);
  
    switch(msgid) {
        case TICK_PRICE: receive_tick_price(ti); break;
        case TICK_SIZE: receive_tick_size(ti); break;
        case TICK_OPTION_COMPUTATION: receive_tick_option_computation(ti); break;
        case TICK_GENERIC: receive_tick_generic(ti); break;
        case TICK_STRING: receive_tick_string(ti); break;
        case TICK_EFP: receive_tick_efp(ti); break;
        case ORDER_STATUS: receive_order_status(ti); break;
        case ACCT_VALUE: receive_acct_value(ti); break;
        case PORTFOLIO_VALUE: receive_portfolio_value(ti); break;
        case ACCT_UPDATE_TIME: receive_acct_update_time(ti); break;
        case ERR_MSG: receive_err_msg(ti); break;
        case OPEN_ORDER: receive_open_order(ti); break;
        case NEXT_VALID_ID: receive_next_valid_id(ti); break;
        case CONTRACT_DATA: receive_contract_data(ti); break;
        case BOND_CONTRACT_DATA: receive_bond_contract_data(ti); break;
        case EXECUTION_DATA: receive_execution_data(ti); break;
        case MARKET_DEPTH: receive_market_depth(ti); break;
        case MARKET_DEPTH_L2: receive_market_depth_l2(ti); break;
        case NEWS_BULLETINS: receive_news_bulletins(ti); break;
        case MANAGED_ACCTS: receive_managed_accts(ti); break;
        case RECEIVE_FA: receive_fa(ti); break;
        case HISTORICAL_DATA: receive_historical_data(ti); break;
        case SCANNER_PARAMETERS: receive_scanner_parameters(ti); break;
        case SCANNER_DATA: receive_scanner_data(ti); break;
        case CURRENT_TIME: receive_current_time(ti); break;
        case REAL_TIME_BARS: receive_realtime_bars(ti); break;
        default: valid = 0; break;
    }

    return valid ? 0 : -1;
}

static void event_loop(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    int msgid, valid;

    ti->started = 1;
    (*ti->extfunc)(0); /* indicate "startup" to callee */

    do {
        read_int(ti, &msgid);
        valid = 1;
      
        switch(msgid) {
            case TICK_PRICE: receive_tick_price(ti); break;
            case TICK_SIZE: receive_tick_size(ti); break;
            case TICK_OPTION_COMPUTATION: receive_tick_option_computation(ti); break;
            case TICK_GENERIC: receive_tick_generic(ti); break;
            case TICK_STRING: receive_tick_string(ti); break;
            case TICK_EFP: receive_tick_efp(ti); break;
            case ORDER_STATUS: receive_order_status(ti); break;
            case ACCT_VALUE: receive_acct_value(ti); break;
            case PORTFOLIO_VALUE: receive_portfolio_value(ti); break;
            case ACCT_UPDATE_TIME: receive_acct_update_time(ti); break;
            case ERR_MSG: receive_err_msg(ti); break;
            case OPEN_ORDER: receive_open_order(ti); break;
            case NEXT_VALID_ID: receive_next_valid_id(ti); break;
            case CONTRACT_DATA: receive_contract_data(ti); break;
            case BOND_CONTRACT_DATA: receive_bond_contract_data(ti); break;
            case EXECUTION_DATA: receive_execution_data(ti); break;
            case MARKET_DEPTH: receive_market_depth(ti); break;
            case MARKET_DEPTH_L2: receive_market_depth_l2(ti); break;
            case NEWS_BULLETINS: receive_news_bulletins(ti); break;
            case MANAGED_ACCTS: receive_managed_accts(ti); break;
            case RECEIVE_FA: receive_fa(ti); break;
            case HISTORICAL_DATA: receive_historical_data(ti); break;
            case SCANNER_PARAMETERS: receive_scanner_parameters(ti); break;
            case SCANNER_DATA: receive_scanner_data(ti); break;
            case CURRENT_TIME: receive_current_time(ti); break;
            case REAL_TIME_BARS: receive_realtime_bars(ti); break;
            default: valid = 0; break;
        }

#ifdef TWS_DEBUG
        printf("\nreceived id=%d, name=%s\n", msgid,
               valid ? tws_incoming_msg_names[msgid-1] : "invalid id");
#endif
    } while(ti->connected);

    (*ti->extfunc)(1); /* indicate termination to callee */
#ifdef TWS_DEBUG
    printf("reader thread exiting\n");
#endif
}

/* caller supplies start_thread method */
void *tws_create(start_thread_t start_thread, void *opaque, external_func_t myfunc)
{
    tws_instance_t *ti = (tws_instance_t *) calloc(1, sizeof *ti);
    if(ti) {
        ti->fd = (socket_t) ~0;
        ti->start_thread = start_thread;
        ti->opaque = opaque;
        ti->extfunc = myfunc;
    }

    return ti;
}

void tws_destroy(void *tws_instance)
{
    tws_instance_t *ti = (tws_instance_t *) tws_instance;

    if(ti->fd != (socket_t) ~0) close(ti->fd);

    /* this is a naive and primitive implementation in order not to
     * utilize more sophisticated native synchronization functions
     */

    while(ti->connected) {
#ifdef TWS_DEBUG
        printf("tws_destroy: waiting for event loop to end\n");
#endif

#ifdef unix
        sleep(1);
#else /* windows */
        Sleep(1000);
#endif  
    }

    free(tws_instance);
}

/* return 1 on error, 0 if successful, it's all right to block
 * str must be null terminated
 */
static int send_str(tws_instance_t *ti, const char str[])
{
    long len = (long) strlen(str) + 1;
    int err = len != send(ti->fd, str, len, 0);

    if(err) {
        close(ti->fd);
        ti->connected = 0;
    }
    return err;
}

static int send_double(tws_instance_t *ti, double *val)
{
    char buf[10*sizeof *val];
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

static int send_int_max(tws_instance_t *ti, int val)
{
    return val != INTEGER_MAX_VALUE ? send_int(ti, val) : send_str(ti, "");
}

static int send_double_max(tws_instance_t *ti, double *val)
{
    return DBL_NOTMAX(*val) ? send_double(ti, val) : send_str(ti, "");
}

static int receive(int fd, void *buf, size_t buflen)
{
    int r;
#ifdef unix
    r = read(fd, buf, buflen); /* workaround for recv not being cancellable on FreeBSD*/
#else
    r = recv(fd, (char *) buf, buflen, 0);
#endif
    return r;
}

/* returns 1 char at a time, kernel not entered most of the time 
 * return -1 on error or EOF
 */
static int read_char(tws_instance_t *ti)
{
    int nread;    

    if(ti->buf_next == ti->buf_last) {
        nread = receive(ti->fd, ti->buf, sizeof ti->buf);
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

/* return -1 on error, 0 if successful, updates *len on success */
static int read_line(tws_instance_t *ti, char *line, long *len)
{
    long j;
    int nread = -1, err = -1;

    line[0] = '\0';
    for(j = 0; j < *len; j++) {
        nread = read_char(ti);
        if(nread < 0) {
#ifdef TWS_DEBUG
            printf("read_line: going out 1, nread=%d\n", nread);
#endif
            goto out;
        }
        line[j] = (char) nread;
        if(line[j] == '\0')
            break;
    }

    if(j == *len) {
#ifdef TWS_DEBUG
        printf("read_line: going out 2 j=%ld\n", j);
#endif
        goto out;
    }

#ifdef TWS_DEBUG
    printf("read_line: i read %s\n", line);
#endif
    *len = j;
    err = 0;
out:
    if(err < 0) {
        if(nread <= 0) {
            close(ti->fd);
            ti->connected = 0;
        } else {
#ifdef TWS_DEBUG
            printf("read_line: corruption happened (string longer than max)\n");
#endif
        }
    }

    return err;
}

static int read_double(tws_instance_t *ti, double *val)
{
    char line[5* sizeof *val];
    long len = sizeof line;
    int err = read_line(ti, line, &len);

    *val = err < 0 ? *dNAN : atof(line);
    return err;
}

static int read_double_max(tws_instance_t *ti, double *val)
{
    char line[5* sizeof *val];
    long len = sizeof line;
    int err = read_line(ti, line, &len);

    if(err < 0)
        *val = *dNAN;
    else
        *val = len ? atof(line) : DBL_MAX;

    return err;
}

/* return <0 on error or 0 if successful */
static int read_int(tws_instance_t *ti, int *val)
{
    char line[3* sizeof *val];
    long len = sizeof line;
    int err = read_line(ti, line, &len);

    /* return an impossibly large negative number on error to fail careless callers*/
    *val = err < 0 ? ~(1 << 30) : atoi(line);
    return err;
}

static int read_int_max(tws_instance_t *ti, int *val)
{
    char line[3* sizeof *val];
    long len = sizeof line;
    int err = read_line(ti, line, &len);

    if(err < 0)
        *val = ~(1<<30);
    else
        *val = len ? atoi(line) : INTEGER_MAX_VALUE;

    return err;
}

/* return -1 on error, 0 if successful */
static int read_long(tws_instance_t *ti, long *val)
{
    char line[3* sizeof *val];
    long len = sizeof line;
    int err = read_line(ti, line, &len);

    /* return an impossibly large negative number on error to fail careless callers*/
    *val = err < 0 ? ~(1 << 30) : atol(line);
    return err;
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


int tws_connect(void *tws, const char host[], unsigned short port, int clientid, resolve_name_t resolve_func)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    const char *hostname;
    unsigned char peer[16];
    struct {
        struct sockaddr_in  addr;
        struct sockaddr_in6 addr6;
    } u;
    long lval, peer_len = sizeof peer;
    int val, err;
    
    if(ti->connected) {
        err = ALREADY_CONNECTED; goto out;
    }

#ifdef WINDOWS
    if(init_winsock())
        goto connect_fail;
#endif

    hostname = host ? host : "127.0.0.1";
    if((*resolve_func)(hostname, peer, &peer_len) < 0)
        goto connect_fail;
    
    ti->fd = socket(4 == peer_len ? PF_INET : PF_INET6, SOCK_STREAM, IPPROTO_IP);
#ifdef WINDOWS
    err = ti->fd == INVALID_SOCKET;
#else
    err = ti->fd < 0;
#endif
    if(err) {
    connect_fail:
        err = CONNECT_FAIL; goto out;
    }

    memset(&u, 0, sizeof u);
    if(4 == peer_len) {
        u.addr.sin_family = PF_INET;
        u.addr.sin_port = htons(port);
        memcpy((void *) &u.addr.sin_addr, peer, peer_len);
    } else { /* must be 16 */
        u.addr6.sin6_family = PF_INET6;
        u.addr6.sin6_port = htons(port);
        memcpy((void *) &u.addr6.sin6_addr, peer, peer_len);
    }


    if(connect(ti->fd, (struct sockaddr *) &u, 4 == peer_len ? sizeof u.addr : sizeof u.addr6) < 0) 
        goto connect_fail;

    if(send_int(ti, TWSCLIENT_VERSION))
        goto connect_fail;

    if(read_int(ti, &val))
        goto connect_fail;

    if(val < 1) {
        err = NO_VALID_ID; goto out;
    }

    ti->server_version = val;
    if(ti->server_version >= 20) {
        lval = sizeof ti->connect_time;
        if(read_line(ti, ti->connect_time, &lval) < 0)
            goto connect_fail;
    }

    if(ti->server_version >= 3)
        if(send_int(ti, clientid))
            goto connect_fail;

    ti->connected = 1;
    if(ti->start_thread)
        if((*ti->start_thread)(event_loop, ti) < 0) 
            goto connect_fail;

    err = 0;
out:
    if(err)
        tws_destroy(ti);

    return err;
}

int tws_connected(void *tws)
{
    return ((tws_instance_t *) tws)->connected;
}

void  tws_disconnect(void *tws)
{
    close(((tws_instance_t *) tws)->fd);
}

int tws_req_scanner_parameters(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < 24) return UPDATE_TWS;
    
    send_int(ti, REQ_SCANNER_PARAMETERS);
    send_int(ti, 1 /*VERSION*/);
    return ti->connected ? 0: FAIL_SEND_REQSCANNERPARAMETERS;
}

int tws_req_scanner_subscription(void *tws, int ticker_id, tr_scanner_subscription_t *s)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < 24) return UPDATE_TWS;
    
    send_int(ti, REQ_SCANNER_SUBSCRIPTION);
    send_int(ti, 3 /*VERSION*/);
    send_int(ti, ticker_id);
    send_int_max(ti, s->scan_number_of_rows);
    send_str(ti, s->scan_instrument);
    send_str(ti, s->scan_location_code);
    send_str(ti, s->scan_code);
    send_double_max(ti, &s->scan_above_price);
    send_double_max(ti, &s->scan_below_price);
    send_int_max(ti, s->scan_above_volume);
    send_double_max(ti, &s->scan_market_cap_above);
    send_double_max(ti, &s->scan_market_cap_below);
    send_str(ti, s->scan_moody_rating_above);
    send_str(ti, s->scan_moody_rating_below);
    send_str(ti, s->scan_sp_rating_above);
    send_str(ti, s->scan_sp_rating_below);
    send_str(ti, s->scan_maturity_date_above);
    send_str(ti, s->scan_maturity_date_below);
    send_double_max(ti, &s->scan_coupon_rate_above);
    send_double_max(ti, &s->scan_coupon_rate_below);
    send_str(ti, s->scan_exclude_convertible);

    if(ti->server_version >= 25) {
        send_int(ti, s->scan_average_option_volume_above);
        send_str(ti, s->scan_scanner_setting_pairs);
    }

    if(ti->server_version >= 27)
        send_str(ti, s->scan_stock_type_filter);

    return ti->connected ? 0 : FAIL_SEND_REQSCANNER;
}

int tws_cancel_scanner_subscription(void *tws, int ticker_id)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < 24) return UPDATE_TWS;

    send_int(ti, CANCEL_SCANNER_SUBSCRIPTION);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, ticker_id);
    return ti->connected ? 0 : FAIL_SEND_CANSCANNER;
}

static void send_combolegs(tws_instance_t *ti, tr_contract_t *contract, int send_open_close)
{
    long j;

    send_int(ti, contract->c_num_combolegs);
    for(j = 0; j < contract->c_num_combolegs; j++) {
        tr_comboleg_t *cl = &contract->c_comboleg[j];

        send_int(ti, cl->co_conid);
        send_int(ti, cl->co_ratio);
        send_str(ti, cl->co_action);
        send_str(ti, cl->co_exchange);
        if(send_open_close)
            send_int(ti, cl->co_open_close);
                        
        if(ti->server_version >= MIN_SERVER_VER_SSHORT_COMBO_LEGS) {
            send_int(ti, cl->co_short_sale_slot);
            send_str(ti, cl->co_designated_location);
        }
    }
}

int tws_req_mkt_data(void *tws, int ticker_id, tr_contract_t *contract, const char generic_tick_list[], long snapshot)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < MIN_SERVER_VER_SNAPSHOT_MKT_DATA && snapshot) {
#ifdef TWS_DEBUG
        printf("tws_req_mkt_data does not support snapshot market data requests\n");
#endif
        return UPDATE_TWS;
    } 

    send_int(ti, REQ_MKT_DATA);
    send_int(ti, 7 /* version */);
    send_int(ti, ticker_id);

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

    if(ti->server_version >= 8 && !strcasecmp(contract->c_sectype, "BAG"))
        send_combolegs(ti, contract, 0);

    if(ti->server_version >= 31)
        send_str(ti, generic_tick_list);

    if(ti->server_version >= MIN_SERVER_VER_SNAPSHOT_MKT_DATA)
        send_int(ti, !!snapshot);

    return ti->connected ? 0 : FAIL_SEND_REQMKT;
}

int tws_req_historical_data(void *tws, int ticker_id, tr_contract_t *contract, const char end_date_time[], const char duration_str[], const char bar_size_setting[], const char what_to_show[], int use_rth, int format_date)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < 16)
        return UPDATE_TWS;
    
    send_int(ti, REQ_HISTORICAL_DATA);
    send_int(ti, 4 /*version*/);
    send_int(ti, ticker_id);

    send_str(ti, contract->c_symbol);
    send_str(ti, contract->c_sectype);
    send_str(ti, contract->c_expiry);
    send_double(ti, &contract->c_strike);
    send_str(ti, contract->c_right);
    send_str(ti, contract->c_multiplier);
    send_str(ti, contract->c_exchange);
    send_str(ti, contract->c_primary_exch);
    send_str(ti, contract->c_currency);
    send_str(ti, contract->c_local_symbol);

    if(ti->server_version >= 31)
        send_int(ti, contract->c_include_expired);

    if(ti->server_version >= 20) {
        send_str(ti, end_date_time);
        send_str(ti, bar_size_setting);
    }
    send_str(ti, duration_str);
    send_int(ti, use_rth);
    send_str(ti, what_to_show);
    if(ti->server_version > 16)
        send_int(ti, format_date);

    send_combolegs(ti, contract, 0);
    return ti->connected ? 0 : FAIL_SEND_REQMKT;
}

int tws_cancel_historical_data(void *tws, int ticker_id)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < 24) return UPDATE_TWS;
    
    send_int(ti, CANCEL_HISTORICAL_DATA);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, ticker_id);
    return ti->connected ? 0 : FAIL_SEND_CANSCANNER;
}

int tws_req_contract_details(void *tws, tr_contract_t *contract)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    /* This feature is only available for versions of TWS >=4 */
    if(ti->server_version < 4)
        return UPDATE_TWS;

    /* send req mkt data msg */
    send_int(ti, REQ_CONTRACT_DATA);
    send_int(ti, 4 /*VERSION*/);

    if(ti->server_version >= MIN_SERVER_VER_CONTRACT_CONID)
        send_int(ti, contract->c_conid);

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
    if(ti->server_version >= 31)
        send_int(ti, contract->c_include_expired);

    return ti->connected ? 0 : FAIL_SEND_REQCONTRACT;
}

int tws_req_mkt_depth(void *tws, int ticker_id, tr_contract_t *contract, int num_rows)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    
    /* This feature is only available for versions of TWS >=6 */
    if(ti->server_version < 6)
        return UPDATE_TWS;
    
    send_int(ti, REQ_MKT_DEPTH);
    send_int(ti, 3 /*VERSION*/);
    send_int(ti, ticker_id);
    
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
    if(ti->server_version >= 19)
        send_int(ti, num_rows);

    return ti->connected ? 0 : FAIL_SEND_REQMKTDEPTH;
}

int tws_cancel_mkt_data(void *tws, int ticker_id)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, CANCEL_MKT_DATA);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, ticker_id);
    return ti->connected ? 0 : FAIL_SEND_CANMKT;
}

int tws_cancel_mkt_depth(void *tws, int ticker_id)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    /* This feature is only available for versions of TWS >=6 */
    if(ti->server_version < 6)
        return UPDATE_TWS;
    
    send_int(ti, CANCEL_MKT_DEPTH);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, ticker_id);

    return ti->connected ? 0 : FAIL_SEND_CANMKTDEPTH;
}

int tws_exercise_options(void *tws, int ticker_id, tr_contract_t *contract, int exercise_action, int exercise_quantity, const char account[], int override)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < 21)
        return UPDATE_TWS;
    
    send_int(ti, EXERCISE_OPTIONS);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, ticker_id);
    send_str(ti, contract->c_symbol);
    send_str(ti, contract->c_sectype);
    send_str(ti, contract->c_expiry);
    send_double(ti, &contract->c_strike);
    send_str(ti, contract->c_right);
    send_str(ti, contract->c_multiplier);
    send_str(ti, contract->c_exchange);
    send_str(ti, contract->c_currency);
    send_str(ti, contract->c_local_symbol);
    send_int(ti, exercise_action);
    send_int(ti, exercise_quantity);
    send_str(ti, account);
    send_int(ti, override);

    return ti->connected ? 0 : FAIL_SEND_REQMKT;
}

int tws_place_order(void *tws, long id, tr_contract_t *contract, tr_order_t *order)
{
    double dmx = DBL_MAX;
    tws_instance_t *ti = (tws_instance_t *) tws;
    int vol26 = 0;

    if(ti->server_version < MIN_SERVER_VER_SCALE_ORDERS) {
        if(order->o_scale_num_components != INTEGER_MAX_VALUE ||
           order->o_scale_component_size != INTEGER_MAX_VALUE ||
           DBL_NOTMAX(order->o_scale_price_increment)) {
#ifdef TWS_DEBUG
            printf("tws_place_order: It does not support Scale orders\n");
#endif
            return UPDATE_TWS;
        }
    }
        
    if(ti->server_version < MIN_SERVER_VER_SSHORT_COMBO_LEGS) {
        if(contract->c_num_combolegs) {
            tr_comboleg_t *cl;
            long j;
            
            for (j = 0; j < contract->c_num_combolegs; j++) {
                cl = &contract->c_comboleg[j];
                
                if(cl->co_short_sale_slot != 0 ||
                   cl->co_designated_location[0] != '\0') {
#ifdef TWS_DEBUG
                    printf("tws_place_order does not support SSHORT flag for combo legs\n");
#endif
                    return UPDATE_TWS;
                }
            }
        }
    }
        
    if(ti->server_version < MIN_SERVER_VER_WHAT_IF_ORDERS) {
        if(order->o_whatif) {
#ifdef TWS_DEBUG
            printf("tws_place_order does not support what-if orders\n");
#endif
            return UPDATE_TWS;
        }
    }

    send_int(ti, PLACE_ORDER);
    send_int(ti, 25 /*VERSION*/);
    send_int(ti, (int) id);
    
    /* send contract fields */
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
    
    /* send main order fields */
    send_str(ti, order->o_action);
    send_int(ti, order->o_total_quantity);
    send_str(ti, order->o_order_type);
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
        send_int(ti, ti->server_version < 38 ? 0 : order->o_outside_rth);
    }
    
    if(ti->server_version >= 7)
        send_int(ti, order->o_hidden);

    /* Send combo legs for BAG requests */
    if(ti->server_version >= 8 && !strcasecmp(contract->c_sectype, "BAG"))
        send_combolegs(ti, contract, 1);
    
    if(ti->server_version >= 9)
        send_str(ti, ""); /* deprecated: shares allocation */
    
    if(ti->server_version >= 10)
        send_double(ti, &order->o_discretionary_amt);
    
    if(ti->server_version >= 11)
        send_str(ti, order->o_good_after_time);
    
    if(ti->server_version >= 12)
        send_str(ti, order->o_good_till_date);
    
    if(ti->server_version >= 13 ) {
        send_str(ti, order->o_fagroup);
        send_str(ti, order->o_famethod);
        send_str(ti, order->o_fapercentage);
        send_str(ti, order->o_faprofile);
    }

    if(ti->server_version >= 18) { /* institutional short sale slot fields.*/
        send_int(ti, order->o_short_sale_slot); /* 0 only for retail, 1 or 2 only for institution.*/
        send_str(ti, order->o_designated_location); /* only populate when order.m_shortSaleSlot = 2.*/
    }

    if(ti->server_version >= 19) {
        vol26 = ti->server_version == 26 && !strcasecmp(order->o_order_type, "VOL");

        send_int(ti, order->o_oca_type);

        if(ti->server_version < 38)
            send_int(ti, 0); /* deprecated: o_rth_only */

        send_str(ti, order->o_rule80a);
        send_str(ti, order->o_settling_firm);
        send_int(ti, order->o_all_or_none);
        send_int_max(ti, order->o_min_qty);
        send_double_max(ti, &order->o_percent_offset);
        send_int(ti, order->o_etrade_only);
        send_int(ti, order->o_firm_quote_only);
        send_double_max(ti, &order->o_nbbo_price_cap);
        send_int_max(ti, order->o_auction_strategy);
        send_double_max(ti, &order->o_starting_price);
        send_double_max(ti, &order->o_stock_ref_price);
        send_double_max(ti, &order->o_delta);
        send_double_max(ti, vol26 ? &dmx : &order->o_stock_range_lower); /* hmm? see below */
        send_double_max(ti, vol26 ? &dmx : &order->o_stock_range_upper); /* hmm? see below */
    }

    if(ti->server_version >= 22)
        send_int(ti, order->o_override_percentage_constraints);

    if(ti->server_version >= 26) { /* Volatility orders */
        send_double_max(ti, &order->o_volatility);
        send_int_max(ti, order->o_volatility_type);
        
        if(ti->server_version < 28)
            send_int(ti, !strcasecmp(order->o_delta_neutral_order_type, "MKT"));
        else {
            send_str(ti, order->o_delta_neutral_order_type);
            send_double_max(ti, &order->o_delta_neutral_aux_price);
        }

        send_int(ti, order->o_continuous_update);
        if(ti->server_version == 26) {
            /* this is a mechanical translation of java code but is it correct? */
            send_double_max(ti, vol26 ? &order->o_stock_range_lower : &dmx);
            send_double_max(ti, vol26 ? &order->o_stock_range_upper : &dmx);
        }

        send_int_max(ti, order->o_reference_price_type);
    }

    if(ti->server_version >= 30)
        send_double_max(ti, &order->o_trail_stop_price);

    if(ti->server_version >= MIN_SERVER_VER_SCALE_ORDERS) {
        send_int_max(ti, order->o_scale_num_components);
        send_int_max(ti, order->o_scale_component_size);
        send_double_max(ti, &order->o_scale_price_increment);
    }
    
    if(ti->server_version >= MIN_SERVER_VER_PTA_ORDERS) {
        send_str(ti, order->o_clearing_account);
        send_str(ti, order->o_clearing_intent);
    }
    
    if(ti->server_version >= MIN_SERVER_VER_WHAT_IF_ORDERS)
        send_int(ti, order->o_whatif);
    
    return ti->connected ? 0 : FAIL_SEND_ORDER;
}

int tws_req_account_updates(void *tws, int subscribe, const char acct_code[])
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, REQ_ACCOUNT_DATA );
    send_int(ti, 2 /*VERSION*/);
    send_int(ti, subscribe);
    
    /* Send the account code. This will only be used for FA clients */
    if(ti->server_version >= 9)
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

int tws_req_current_time(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < 33)
        return UPDATE_TWS;

    send_int(ti, REQ_CURRENT_TIME);
    send_int(ti, 1 /*VERSION*/);

    return ti->connected ? 0 : FAIL_SEND_REQCURRTIME;
}


int tws_request_realtime_bars(void *tws, int ticker_id, tr_contract_t *c, int bar_size, const char what_to_show[], int use_rth)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < 34)
        return UPDATE_TWS;
    
    send_int(ti, REQ_REAL_TIME_BARS);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, ticker_id);
    
    send_str(ti, c->c_symbol);
    send_str(ti, c->c_sectype);
    send_str(ti, c->c_expiry);
    send_double(ti, &c->c_strike);
    send_str(ti, c->c_right);
    send_str(ti, c->c_multiplier);
    send_str(ti, c->c_exchange);
    send_str(ti, c->c_primary_exch);
    send_str(ti, c->c_currency);
    send_str(ti, c->c_local_symbol);
    send_int(ti, bar_size);
    send_str(ti, what_to_show);
    send_int(ti, use_rth);    
    
    return ti->connected ? 0 : FAIL_SEND_REQRTBARS;
}

int tws_cancel_realtime_bars(void *tws, int ticker_id)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < 34)
        return UPDATE_TWS;
    
    send_int(ti, CANCEL_REAL_TIME_BARS);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, ticker_id);

    return ti->connected ? 0 : FAIL_SEND_CANRTBARS;
}

/* returns -1 on error, who knows 0 might be valid? */
int tws_server_version(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    return ti->connected ? (int) ti->server_version : -1;
}

/* this routine is useless if used for synchronization
 * because of clock drift, use NTP for time sync
 */
const char *tws_connection_time(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    return ti->connected ? ti->connect_time : 0;
}
