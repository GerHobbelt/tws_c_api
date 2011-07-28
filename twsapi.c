#define TWSAPI_GLOBALS
#include "twsapi.h"

#if defined(WINDOWS) || defined(_WIN32)
#include <string.h>
#include <limits.h>
#if defined(_MSC_VER)
#define strcasecmp(x,y) _stricmp(x,y)
#define strncasecmp(x,y,z) _strnicmp(x,y,z)
#endif
#else /* unix assumed */
#endif

#ifdef unix
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#endif

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_TWS_STRINGS 127
#define WORD_SIZE_IN_BITS (8*sizeof(unsigned long))
#define WORDS_NEEDED(num, wsize) (!!((num)%(wsize)) + ((num)/(wsize)))
#define ROUND_UP_POW2(num, pow2) (((num) + (pow2)-1) & ~((pow2)-1))
#define INTEGER_MAX_VALUE ((int) ~(1U<<(8*sizeof(int) -1)))
#define DBL_NOTMAX(d) (fabs((d) - DBL_MAX) > DBL_EPSILON)
#define IS_EMPTY(str)  ((str)[0] == '\0')

typedef struct {
    char str[512]; /* maximum conceivable string length */
} tws_string_t;

typedef struct tws_instance {
    void *opaque;
    tws_transmit_func_t *transmit;
    tws_receive_func_t *receive;
    tws_flush_func_t *flush;
    tws_open_func_t *open;
    tws_close_func_t *close;

    char connect_time[60]; /* server reported time */
    unsigned char tx_buf[512]; /* buffer up to 512 chars at a time for transmission */
    unsigned int tx_buf_next; /* index of next empty char slot in tx_buf */
    unsigned char buf[240]; /* buffer up to 240 chars at a time */
    unsigned int buf_next, buf_last; /* index of next, last chars in buf */
    unsigned int server_version;
    volatile unsigned int connected;
    tws_string_t mempool[MAX_TWS_STRINGS];
    unsigned long bitmask[WORDS_NEEDED(MAX_TWS_STRINGS, WORD_SIZE_IN_BITS)];
} tws_instance_t;

static int read_double(tws_instance_t *ti, double *val);
static int read_double_max(tws_instance_t *ti, double *val);
static int read_long(tws_instance_t *ti, long *val);
static int read_int(tws_instance_t *ti, int *val);
static int read_int_max(tws_instance_t *ti, int *val);
static int read_line(tws_instance_t *ti, char *line, size_t *len);
static int read_line_of_arbitrary_length(tws_instance_t *ti, char **val, size_t initial_space);

static void reset_io_buffers(tws_instance_t *ti);

/* access to these strings is single threaded
 * replace plain bitops with atomic test_and_set_bit/clear_bit + memory barriers
 * if multithreaded access is desired (not applicable at present)
 */
static char *alloc_string(tws_instance_t *ti)
{
    unsigned int index;
    unsigned long bits;
    unsigned int j;

    for(j = 0; j < MAX_TWS_STRINGS; j++) {
        index = j / WORD_SIZE_IN_BITS;
        if(ti->bitmask[index] == ~0UL) {
            j = ROUND_UP_POW2(j+1, WORD_SIZE_IN_BITS) -1;
            continue;
        }

        bits = 1UL << (j & (WORD_SIZE_IN_BITS - 1));
        if(!(ti->bitmask[index] & bits)) {
            ti->bitmask[index] |= bits;
            // and initially start with an empty string value (aids legibility while debugging):
            ti->mempool[j].str[0] = 0;
            return ti->mempool[j].str;
        }
    }

#ifdef TWS_DEBUG
    printf("alloc_string: ran out of strings, will crash shortly\n");
#endif

    // close the connection when we run out of pool memory to prevent the currently processed message from making it through to the handler
    tws_disconnect(ti);

    return 0;
}

static void free_string(tws_instance_t *ti, void *ptr)
{
    if (ptr)
    {
        unsigned int j = (unsigned int) ((tws_string_t *) ptr - &ti->mempool[0]);
        unsigned int index = j / WORD_SIZE_IN_BITS;
        unsigned long bits = 1UL << (j & (WORD_SIZE_IN_BITS - 1));

        ti->bitmask[index] &= ~bits;
    }
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
    c->c_secid_type = alloc_string(ti);
    c->c_secid = alloc_string(ti);
}

static void destroy_contract(tws_instance_t *ti, tr_contract_t *c)
{
    free_string(ti, c->c_secid);
    free_string(ti, c->c_secid_type);
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

    o->o_algo_strategy = alloc_string(ti);
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

    o->o_exempt_code = -1;
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
    free_string(ti, o->o_algo_strategy);

    if (o->o_algo_params && o->o_algo_params_count)
    {
        int i;

        for (i = 0; i < o->o_algo_params_count; i++)
        {
            free_string(ti, o->o_algo_params[i].t_tag);
            free_string(ti, o->o_algo_params[i].t_val);
        }
        free(o->o_algo_params);
    }
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
    memset(cd, 0, sizeof *cd);
    init_contract(ti, &cd->d_summary);

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
    cd->d_long_name = alloc_string(ti);
    cd->d_contract_month = alloc_string(ti);
    cd->d_industry = alloc_string(ti);
    cd->d_category = alloc_string(ti);
    cd->d_subcategory = alloc_string(ti);
    cd->d_timezone_id = alloc_string(ti);
    cd->d_trading_hours = alloc_string(ti);
    cd->d_liquid_hours = alloc_string(ti);
}

static void destroy_contract_details(tws_instance_t *ti, tr_contract_details_t *cd)
{
    free_string(ti, cd->d_liquid_hours);
    free_string(ti, cd->d_trading_hours);
    free_string(ti, cd->d_timezone_id);
    free_string(ti, cd->d_subcategory);
    free_string(ti, cd->d_category);
    free_string(ti, cd->d_industry);
    free_string(ti, cd->d_contract_month);
    free_string(ti, cd->d_long_name);
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

    destroy_contract(ti, &cd->d_summary);
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

static void init_under_comp(tws_instance_t *ti, under_comp_t *und)
{
    memset(und, 0, sizeof(*und));
}

static void destroy_under_comp(tws_instance_t *ti, under_comp_t *und)
{
}

void tws_init_tr_comboleg(void *tws, tr_comboleg_t *cl)
{
    memset(cl, 0, sizeof(*cl));

    cl->co_exempt_code = -1;
}


static void receive_tick_price(tws_instance_t *ti)
{
    double price;
    int ival, version, ticker_id;
    tr_tick_type_t tick_type;
    int size = 0, can_auto_execute = 0;
    tr_tick_type_t size_tick_type;

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
            case BID:  size_tick_type = BID_SIZE; break;
            case ASK:  size_tick_type = ASK_SIZE; break;
            case LAST: size_tick_type = LAST_SIZE; break;
            default: size_tick_type = TICK_UNDEFINED; break; /* not a tick */
        }

        if(size_tick_type != TICK_UNDEFINED)
            if(ti->connected)
                event_tick_size(ti->opaque, ticker_id, size_tick_type, size);
    }
}


static void receive_tick_size(tws_instance_t *ti)
{
    int ival, ticker_id;
    tr_tick_type_t tick_type;
    int size;

    read_int(ti, &ival); /* version unused */
    read_int(ti, &ival), ticker_id = ival;
    read_int(ti, &ival), tick_type = ival;
    read_int(ti, &ival), size = ival;

    if(ti->connected)
        event_tick_size(ti->opaque, ticker_id, tick_type, size);
}

static void receive_tick_option_computation(tws_instance_t *ti)
{
    double implied_vol, delta;
    double opt_price = DBL_MAX;
    double pv_dividend = DBL_MAX;
    double gamma = DBL_MAX;
    double vega = DBL_MAX;
    double theta = DBL_MAX;
    double und_price = DBL_MAX;
    int ival, ticker_id;
    tr_tick_type_t tick_type;
    int version;

    read_int(ti, &ival), version = ival;
    read_int(ti, &ival), ticker_id = ival;
    read_int(ti, &ival), tick_type = ival;
    read_double(ti, &implied_vol);
    if(implied_vol < 0) /* -1 is the "not yet computed" indicator */
        implied_vol = DBL_MAX;

    read_double(ti, &delta);
    if(fabs(delta) > 1.0)  /* -2 is the "not yet computed" indicator */
        delta = DBL_MAX;

    if (version >= 6 || tick_type == MODEL_OPTION) { // introduced in version == 5
        read_double(ti, &opt_price);
        if (opt_price < 0) { // -1 is the "not yet computed" indicator
            opt_price = DBL_MAX;
        }
        read_double(ti, &pv_dividend);
        if (pv_dividend < 0) { // -1 is the "not yet computed" indicator
            pv_dividend = DBL_MAX;
        }
    }
    if (version >= 6) {
        read_double(ti, &gamma);
        if (fabs(gamma) > 1) { // -2 is the "not yet computed" indicator
            gamma = DBL_MAX;
        }
        read_double(ti, &vega);
        if (fabs(vega) > 1) { // -2 is the "not yet computed" indicator
            vega = DBL_MAX;
        }
        read_double(ti, &theta);
        if (fabs(theta) > 1) { // -2 is the "not yet computed" indicator
            theta = DBL_MAX;
        }
        read_double(ti, &und_price);
        if (und_price < 0) { // -1 is the "not yet computed" indicator
            und_price = DBL_MAX;
        }
    }

    if(ti->connected)
        event_tick_option_computation(ti->opaque, ticker_id, tick_type, implied_vol, delta, opt_price, pv_dividend, gamma, vega, theta, und_price);
}

static void receive_tick_generic(tws_instance_t *ti)
{
    double value;
    int ival, ticker_id;
    tr_tick_type_t tick_type;

    read_int(ti, &ival /*version */); /* ignored */
    read_int(ti, &ival), ticker_id = ival;
    read_int(ti, &ival), tick_type = ival;
    read_double(ti, &value);
    if(ti->connected)
        event_tick_generic(ti->opaque, ticker_id, tick_type, value);
}

static void receive_tick_string(tws_instance_t *ti)
{
    char *str;
    char *ticker_value;
    int ival, ticker_id;
    tr_tick_type_t tick_type;

    read_int(ti, &ival /*version */); /* ignored */
    read_int(ti, &ival), ticker_id = ival;
    read_int(ti, &ival), tick_type = ival;

    ticker_value = str = alloc_string(ti);
    read_line_of_arbitrary_length(ti, &ticker_value, sizeof(tws_string_t));

    if(ti->connected) {
        event_tick_string(ti->opaque, ticker_id, tick_type, ticker_value);
    }

    if (ticker_value != str)
        free(ticker_value);
    free_string(ti, str);
}

static void receive_tick_efp(tws_instance_t *ti)
{
    double basis_points, implied_futures_price, dividend_impact, dividends_to_expiry;
    char *formatted_basis_points = alloc_string(ti), *future_expiry = alloc_string(ti);
    size_t lval;
    int ival, ticker_id;
    tr_tick_type_t tick_type;
    int hold_days;

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
    size_t lval;
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
    size_t lval;
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
    size_t lval;
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
    if(version >= 7) {
        lval = sizeof(tws_string_t), read_line(ti, contract.c_multiplier, &lval);
        lval = sizeof(tws_string_t), read_line(ti, contract.c_primary_exch, &lval);
    }

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

    if(version == 6 && ti->server_version == 39)
        lval = sizeof(tws_string_t), read_line(ti, contract.c_primary_exch, &lval);

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
    size_t lval;
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
    size_t lval;
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
    under_comp_t und;
    size_t lval;
    int ival, version;

    init_contract(ti, &contract);
    init_under_comp(ti, &und);
    contract.c_undercomp = &und;
    init_order(ti, &order);
    init_order_status(ti, &ost);

    read_int(ti, &ival), version = ival;
    read_int(ti, &order.o_orderid);

    if(version >= 17) {
        read_int(ti, &contract.c_conid);
    }

    lval = sizeof(tws_string_t), read_line(ti, contract.c_symbol, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_sectype, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_expiry, &lval);
    read_double(ti, &contract.c_strike);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_right, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_exchange, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_currency, &lval);

    if(version >= 2) {
        lval = sizeof(tws_string_t), read_line(ti, contract.c_local_symbol, &lval);
    }

    lval = sizeof(tws_string_t), read_line(ti, order.o_action, &lval);
    read_int(ti, &order.o_total_quantity);
    lval = sizeof(tws_string_t), read_line(ti, order.o_order_type, &lval);
    read_double(ti, &order.o_lmt_price);
    read_double(ti, &order.o_aux_price);
    lval = sizeof(tws_string_t), read_line(ti, order.o_tif, &lval);
    lval = sizeof(tws_string_t), read_line(ti, order.o_oca_group, &lval);
    lval = sizeof(tws_string_t), read_line(ti, order.o_account, &lval);
    lval = sizeof(tws_string_t), read_line(ti, order.o_open_close, &lval);
    read_int(ti, &ival), order.o_origin = ival;
    lval = sizeof(tws_string_t), read_line(ti, order.o_orderref, &lval);

    if(version >= 3)
        read_int(ti, &order.o_clientid);

    if(version >= 4) {
        read_int(ti, &order.o_permid);

        read_int(ti, &ival);
        if(version < 18)
            ; /* "will never happen" comment */
        else
            order.o_outside_rth = (ival == 1);

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

    if(version >= 8) {
        lval = sizeof(tws_string_t), read_line(ti, order.o_good_till_date, &lval);
    }

    if(version >= 9) {
        lval = sizeof(tws_string_t), read_line(ti, order.o_rule80a, &lval);
        read_double(ti, &order.o_percent_offset);
        lval = sizeof(tws_string_t), read_line(ti, order.o_settling_firm, &lval);
        read_int(ti, &ival), order.o_short_sale_slot = ival;
        lval = sizeof(tws_string_t), read_line(ti, order.o_designated_location, &lval);
        if (ti->server_version == 51) {
            read_int(ti, &ival); // exemptCode
        }
        else if (version >= 23) {
            read_int(ti, &ival); order.o_exempt_code = ival;
        }
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
            strcpy(order.o_delta_neutral_order_type, (!ival ? "NONE" : "MKT"));
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
        if (version >= 20) {
            read_int_max(ti, &order.o_scale_init_level_size);
            read_int_max(ti, &order.o_scale_subs_level_size);
        } else {
            read_int_max(ti, &ival); /* unused */
            read_int_max(ti, &order.o_scale_init_level_size);
        }

        read_double_max(ti, &order.o_scale_price_increment);
    }

    if(version >= 19) {
        lval = sizeof(tws_string_t); read_line(ti, order.o_clearing_account, &lval);
        lval = sizeof(tws_string_t); read_line(ti, order.o_clearing_intent, &lval);
    }

    if(version >= 22)
        read_int(ti, &ival), order.o_not_held = !!ival;

    if(version >= 20) {
        read_int(ti, &ival);
        if (!!ival) {
            read_int(ti, &ival), contract.c_undercomp->u_conid = ival;
            read_double(ti, &contract.c_undercomp->u_delta);
            read_double(ti, &contract.c_undercomp->u_price);
        }
    }

    if(version >= 21) {
        lval = sizeof(tws_string_t); read_line(ti, order.o_algo_strategy, &lval);

        if (!IS_EMPTY(order.o_algo_strategy)) {
            read_int(ti, &order.o_algo_params_count);

            if (order.o_algo_params_count > 0) {
                order.o_algo_params = calloc(order.o_algo_params_count, sizeof(*order.o_algo_params));
                if(order.o_algo_params) {
                    int j;
                    for (j = 0; j < order.o_algo_params_count; j++) {
                        order.o_algo_params[j].t_tag = alloc_string(ti);
                        order.o_algo_params[j].t_val = alloc_string(ti);
                        lval = sizeof(tws_string_t); read_line(ti, order.o_algo_params[j].t_tag, &lval);
                        lval = sizeof(tws_string_t); read_line(ti, order.o_algo_params[j].t_val, &lval);
                    }
                } else {
#ifdef TWS_DEBUG
                    printf("receive_open_order: memory allocation failure\n");
#endif
                    tws_disconnect(ti);
                }
            }
        }
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
    size_t lval;
    int version, req_id = -1;

    init_contract_details(ti, &cdetails);
    read_int(ti, &version);

    if(version >= 3)
        read_int(ti, &req_id);

    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_symbol, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_sectype, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_expiry, &lval);
    read_double(ti, &cdetails.d_summary.c_strike);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_right, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_exchange, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_currency, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_local_symbol, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_market_name, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_trading_class, &lval);
    read_int(ti, &cdetails.d_summary.c_conid);
    read_double(ti, &cdetails.d_mintick);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_multiplier, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_order_types, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_valid_exchanges, &lval);

    if(version >= 2)
        read_int(ti, &cdetails.d_price_magnifier);

    if(version >= 4)
        read_int(ti, &cdetails.d_under_conid);

    if(version >= 5) {
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_long_name, &lval);
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_primary_exch, &lval);
    }

    if(version >= 6) {
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_contract_month, &lval);
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_industry, &lval);
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_category, &lval);
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_subcategory, &lval);
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_timezone_id, &lval);
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_trading_hours, &lval);
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_liquid_hours, &lval);
    }

    if(ti->connected)
        event_contract_details(ti->opaque, req_id, &cdetails);

    destroy_contract_details(ti, &cdetails);
}

static void receive_bond_contract_data(tws_instance_t *ti)
{
    tr_contract_details_t cdetails;
    size_t lval;
    int ival, version, req_id = -1;

    init_contract_details(ti, &cdetails);
    read_int(ti, &ival), version = ival;

    if(version >= 3)
        read_int(ti, &ival), req_id = ival;

    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_symbol, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_sectype, &lval);
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
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_exchange, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_currency, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_market_name, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_trading_class, &lval);

    read_int(ti, &cdetails.d_summary.c_conid);
    read_double(ti, &cdetails.d_mintick);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_order_types, &lval);
    lval = sizeof(tws_string_t), read_line(ti, cdetails.d_valid_exchanges, &lval);

    if(version >= 2) {
        lval = sizeof(tws_string_t); read_line(ti, cdetails.d_next_option_date, &lval);
        lval = sizeof(tws_string_t); read_line(ti, cdetails.d_next_option_type, &lval);
        read_int(ti, &ival); cdetails.d_next_option_partial = !!ival;
        lval = sizeof(tws_string_t); read_line(ti, cdetails.d_notes, &lval);
    }

    if(version >= 4)
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_long_name, &lval);

    if(ti->connected)
        event_bond_contract_details(ti->opaque, req_id, &cdetails);

    destroy_contract_details(ti, &cdetails);
}

static void receive_execution_data(tws_instance_t *ti)
{
    tr_contract_t contract;
    tr_execution_t exec;
    size_t lval;
    int ival, version, orderid, reqid = -1;

    init_contract(ti, &contract);
    init_execution(ti, &exec);

    read_int(ti, &ival), version = ival;

    if(version >= 7)
        read_int(ti, &ival), reqid = ival;

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

    if (version >= 6) {
        read_int(ti, &exec.e_cum_qty);
        read_double(ti, &exec.e_avg_price);
    }

    if(ti->connected)
        event_exec_details(ti->opaque, reqid, &contract, &exec);

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
    size_t lval;
    int ival, id, position, operation, side, size;

    read_int(ti, &ival); /*version*/
    read_int(ti, &ival), id = ival;
    read_int(ti, &ival), position = ival;

    lval = sizeof(tws_string_t), read_line(ti, mkt_maker, &lval);
    read_int(ti, &ival), operation = ival;
    read_int(ti, &ival), side = ival;
    read_double(ti, &price);
    read_int(ti, &ival), size = ival;

    if(ti->connected) {
        event_update_mkt_depth_l2(ti->opaque, id, position, mkt_maker,
                                  operation, side, price, size);
    }

    free_string(ti, mkt_maker);
}

static void receive_news_bulletins(tws_instance_t *ti)
{
    char *msg, *originating_exch, *str;
    size_t lval;
    int ival, newsmsgid, newsmsgtype;

    read_int(ti, &ival); /*version*/
    read_int(ti, &ival), newsmsgid = ival;
    read_int(ti, &ival), newsmsgtype = ival;

    msg = str = alloc_string(ti);
    read_line_of_arbitrary_length(ti, &msg, sizeof(tws_string_t)); /* news message */

    originating_exch = alloc_string(ti);
    lval = sizeof(tws_string_t), read_line(ti, originating_exch, &lval);

    if(ti->connected) {
        event_update_news_bulletin(ti->opaque, newsmsgid, newsmsgtype,
                                   msg, originating_exch);
    }

    if (msg != str)
        free(msg);
    free_string(ti, str);
    free_string(ti, originating_exch);
}

static void receive_managed_accts(tws_instance_t *ti)
{
    size_t lval;
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
    char *str;
    char *xml;
    int ival, fadata_type;

    read_int(ti, &ival); /*version*/
    read_int(ti, &ival), fadata_type = ival;

    xml = str = alloc_string(ti);
    read_line_of_arbitrary_length(ti, &xml, sizeof(tws_string_t)); /* xml */

    if(ti->connected) {
        event_receive_fa(ti->opaque, fadata_type, xml);
    }

    if (xml != str)
        free(xml);
    free_string(ti, str);
}

static void receive_historical_data(void *tws)
{
    double open, high, low, close, wap;
    tws_instance_t *ti = (tws_instance_t *) tws;
    int j;
    size_t lval;
    int ival, version, reqid, item_count, volume, gaps, bar_count;
    char *date = alloc_string(ti), *has_gaps = alloc_string(ti), *completion_from = alloc_string(ti), *completion_to = alloc_string(ti);

    read_int(ti, &ival), version = ival;
    read_int(ti, &ival), reqid = ival;

    if(version >= 2) {
        lval = sizeof(tws_string_t); read_line(ti, completion_from, &lval);
        lval = sizeof(tws_string_t); read_line(ti, completion_to, &lval);
    }
    read_int(ti, &ival), item_count = ival;

    for(j = 0; j < item_count; j++) {
        lval = sizeof(tws_string_t); read_line(ti, date, &lval);
        read_double(ti, &open);
        read_double(ti, &high);
        read_double(ti, &low);
        read_double(ti, &close);
        read_int(ti, &ival), volume = ival;
        read_double(ti, &wap);
        lval = sizeof(tws_string_t); read_line(ti, has_gaps, &lval);
        gaps = !!strncasecmp(has_gaps, "false", 5);

        if(version >= 3)
            read_int(ti, &ival), bar_count = ival;
        else
            bar_count = -1;

        if(ti->connected)
            event_historical_data(ti->opaque, reqid, date, open, high, low, close, volume, bar_count, wap, gaps);

    }
    /* send end of dataset marker */
    if(ti->connected)
        event_historical_data_end(ti->opaque, reqid, completion_from, completion_to);

    free_string(ti, date);
    free_string(ti, has_gaps);
    free_string(ti, completion_from);
    free_string(ti, completion_to);
}

static void receive_scanner_parameters(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    char *xml;
    int ival;

    read_int(ti, &ival); /*version*/

    // we expect to receive a very large XML string here, so don't even try to make the effort for smaller strings.
    // In my case, I see an incoming XML string of ~ 193K so we instruct the function to start with a 200K buffer
    // on the heap to speed it up a little bit by (most probably) not requiring any realloc ~ large memcopy
    // operation in there.
    xml = NULL;
    read_line_of_arbitrary_length(ti, &xml, 200 * 1024);

    if(ti->connected) {
        event_scanner_parameters(ti->opaque, xml);
    }

    free(xml);
}

static void receive_scanner_data(void *tws)
{
    tr_contract_details_t cdetails;
    tws_instance_t *ti = (tws_instance_t *) tws;
    char *distance = alloc_string(ti), *benchmark = alloc_string(ti), *projection = alloc_string(ti), *legs_str;
    size_t lval;
    int j;
    int ival, version, rank, ticker_id, num_elements;

    init_contract_details(ti, &cdetails);

    read_int(ti, &ival), version = ival;
    read_int(ti, &ival), ticker_id = ival;
    read_int(ti, &ival), num_elements = ival;

    for(j = 0; j < num_elements; j++) {
        legs_str = 0;
        read_int(ti, &ival), rank = ival;

        if(version >= 3) {
            read_int(ti, &cdetails.d_summary.c_conid);
        }

        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_symbol, &lval);
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_sectype, &lval);
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_expiry, &lval);
        read_double(ti, &cdetails.d_summary.c_strike);
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_right, &lval);
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_exchange, &lval);
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_currency, &lval);
        lval = sizeof(tws_string_t), read_line(ti, cdetails.d_summary.c_local_symbol, &lval);
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

static void receive_fundamental_data(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    int ival, req_id;
    char *str;
    char *data;

    read_int(ti, &ival); /* version ignored */
    read_int(ti, &ival), req_id = ival;

    data = str = alloc_string(ti);
    read_line_of_arbitrary_length(ti, &data, sizeof(tws_string_t));

    if(ti->connected) {
        event_fundamental_data(ti->opaque, req_id, data);
    }

    if (data != str)
        free(data);
    free_string(ti, str);
}

static void receive_contract_data_end(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    int ival, req_id;

    read_int(ti, &ival); /* version ignored */
    read_int(ti, &ival), req_id = ival;

    if(ti->connected)
        event_contract_details_end(ti->opaque, req_id);
}

static void receive_open_order_end(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    int ival;

    read_int(ti, &ival); /* version ignored */
    if(ti->connected)
        event_open_order_end(ti->opaque);
}

static void receive_acct_download_end(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    char acct_name[200];
    size_t lval = sizeof acct_name;
    int ival;

    read_int(ti, &ival); /* version ignored */
    read_line(ti, acct_name, &lval);

    if(ti->connected)
        event_acct_download_end(ti->opaque, acct_name);
}

static void receive_execution_data_end(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    int ival, reqid;

    read_int(ti, &ival); /* version ignored */
    read_int(ti, &ival); reqid = ival;

    if(ti->connected)
        event_exec_details_end(ti->opaque, reqid);
}

static void receive_delta_neutral_validation(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    under_comp_t und;
    int ival, reqid;

    read_int(ti, &ival); /* version ignored */
    read_int(ti, &ival); reqid = ival;

    read_int(ti, &und.u_conid);
    read_double(ti, &und.u_delta);
    read_double(ti, &und.u_price);

    if(ti->connected)
        event_delta_neutral_validation(ti->opaque, reqid, &und);
}

static void receive_tick_snapshot_end(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    int ival, reqid;

    read_int(ti, &ival); /* version ignored */
    read_int(ti, &ival); reqid = ival;

    if(ti->connected)
        event_tick_snapshot_end(ti->opaque, reqid);
}

/* allows for reading events from within the same thread or an externally
 * spawned thread, returns 0 on success, -1 on error,
 */
int tws_event_process(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    int ival;
    int valid = 1;
    enum tws_incoming_ids msgcode;

    read_int(ti, &ival);
    msgcode = (enum tws_incoming_ids)ival;

    switch(msgcode)
    {
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
        case FUNDAMENTAL_DATA: receive_fundamental_data(ti); break;
        case CONTRACT_DATA_END: receive_contract_data_end(ti); break;
        case OPEN_ORDER_END: receive_open_order_end(ti); break;
        case ACCT_DOWNLOAD_END: receive_acct_download_end(ti); break;
        case EXECUTION_DATA_END: receive_execution_data_end(ti); break;
        case DELTA_NEUTRAL_VALIDATION: receive_delta_neutral_validation(ti); break;
        case TICK_SNAPSHOT_END: receive_tick_snapshot_end(ti); break;
        default: valid = 0; break;
    }

#ifdef TWS_DEBUG
    printf("\nreceived id=%d, name=%s\n", (int)msgcode,
           valid ? tws_incoming_msg_names[msgcode - 1] : "invalid id");
#endif
    return valid ? 0 : -1;
}

/* caller supplies start_thread method */
void *tws_create(void *opaque, tws_transmit_func_t *transmit, tws_receive_func_t *receive, tws_flush_func_t *flush, tws_open_func_t *open, tws_close_func_t *close)
{
    tws_instance_t *ti = (tws_instance_t *) calloc(1, sizeof *ti);
    if(ti)
    {
        ti->opaque = opaque;
        ti->transmit = transmit;
        ti->receive = receive;
        ti->flush = flush;
        ti->open = open;
        ti->close = close;

        reset_io_buffers(ti);
    }

    return ti;
}

void tws_destroy(void *tws_instance)
{
    tws_disconnect(tws_instance);

    free(tws_instance);
}

/* perform output buffering */
static int send_blob(tws_instance_t *ti, const char *src, size_t srclen)
{
    size_t len = sizeof(ti->tx_buf) - ti->tx_buf_next;
    int err = 0;

    if (ti->connected) {
        while (len < srclen) {
            err = ((int)ti->tx_buf_next != ti->transmit(ti->opaque, ti->tx_buf, ti->tx_buf_next));
            if(err) {
                tws_disconnect(ti);
                return err;
            }
            len = sizeof(ti->tx_buf);
            if (len > srclen) {
                len = srclen;
            }
            memcpy(ti->tx_buf, src, len);
            srclen -= len;
            src += len;
            ti->tx_buf_next = len;
        }

        if (srclen > 0)
        {
            memcpy(ti->tx_buf + ti->tx_buf_next, src, srclen);
            ti->tx_buf_next += srclen;
        }
    }

    return err;
}

static int flush_message(tws_instance_t *ti)
{
    int err = 0;

    if (ti->connected) {
        if (ti->tx_buf_next > 0) {
            err = ((int)ti->tx_buf_next != ti->transmit(ti->opaque, ti->tx_buf, ti->tx_buf_next));
            if(err) {
                tws_disconnect(ti);
                goto out;
            }
        }
        /* now that all lingering message data has been transmitted, signal end of message by requesting a TX/flush: */
        err = ti->flush(ti->opaque);
        if(err) {
            tws_disconnect(ti);
            goto out;
        }
    }
    ti->tx_buf_next = 0;
out:
    return err;
}

/* return 1 on error, 0 if successful, it's all right to block
 * str must be null terminated
 */
static int send_str(tws_instance_t *ti, const char str[])
{
    const char *s = (str ? str : "");
    int len = (int)strlen(s) + 1;
    int err = send_blob(ti, s, len);

    return err;
}

static int send_double(tws_instance_t *ti, double val)
{
    char buf[10*sizeof val];
    int len;
    int err = 1;

    len = sprintf(buf, "%.7lf", val);
    if(len++ < 0)
        goto out;

    err = send_blob(ti, buf, len);
out:
    return err;
}

/* return 1 on error, 0 if successful, it's all right to block */
static int send_int(tws_instance_t *ti, int val)
{
    char buf[5*(sizeof val)/2 + 2];
    int len;
    int err = 1;

    len = sprintf(buf, "%d", val);
    if(len++ < 0)
        goto out;

    err = send_blob(ti, buf, len);
out:
    return err;
}

/* return 1 on error, 0 if successful, it's all right to block */
static int send_boolean(tws_instance_t *ti, int val)
{
    return send_blob(ti, (val ? "1" : "0"), 2);
}

static int send_int_max(tws_instance_t *ti, int val)
{
    return val != INTEGER_MAX_VALUE ? send_int(ti, val) : send_str(ti, "");
}

static int send_double_max(tws_instance_t *ti, double val)
{
    return DBL_NOTMAX(val) ? send_double(ti, val) : send_str(ti, "");
}

/* returns 1 char at a time, kernel not entered most of the time
 * return -1 on error or EOF
 */
static int read_char(tws_instance_t *ti)
{
    int nread;

    if (ti->connected) {
        if(ti->buf_next == ti->buf_last) {
            nread = ti->receive(ti->opaque, ti->buf, (unsigned int)sizeof ti->buf);
            if(nread <= 0) {
                nread = -1;
                goto out;
            }
            ti->buf_last = nread;
            ti->buf_next = 0;
        }

        nread = ti->buf[ti->buf_next++];
    }
    else {
        nread = -1;
    }
out:
    return nread;
}

/* return -1 on error, 0 if successful, updates *len on success */
static int read_line(tws_instance_t *ti, char *line, size_t *len)
{
    size_t j;
    int nread = -1, err = -1;

    if (line == NULL) {
#ifdef TWS_DEBUG
        printf("read_line: line buffer is NULL\n");
#endif
        goto out;
    }

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
        if(nread > 0) {
#ifdef TWS_DEBUG
            printf("read_line: corruption happened (string longer than max)\n");
#endif
        }
        // always close the connection in buffer overflow / error conditions; the next element fetch will be corrupt anyway and this way we prevent nasty surprises downrange.
        tws_disconnect(ti);
    }

    return err;
}

/*
When fetching a parameter string value of arbitrary length, we return a pointer to
space allocated on the heap (we only use the string memory pool for shorter strings
as that one is size limited and (in its entirity!) too small for several messages.

We don't want to take a buffer overflow risk like that any more, so we fix this by
allowing arbitrary string length for this parameter type only.
*/
static int read_line_of_arbitrary_length(tws_instance_t *ti, char **val, size_t alloc_size)
{
    size_t j;
    char *line;
    int nread = -1, err = -1;
    int malloced = 0;

    line = *val;
    *val = NULL;

    if (line == NULL || alloc_size == 0) {
        // guestimate reasonable initial buffer size: we expect large inputs so we start pretty big:
        if (alloc_size < 65536)
            alloc_size = 65536;
        line = malloc(alloc_size);
        malloced = 1;
        if (line == NULL)
        {
#ifdef TWS_DEBUG
            printf("read_line_of_arbitrary_length: going out 0, heap alloc failure\n");
#endif
            goto out;
        }
    }

    line[0] = '\0';
    for(j = 0; ; j++) {
        if (j + 1 >= alloc_size) {
            alloc_size += 65536;
            if (malloced)
            {
                line = realloc(line, alloc_size);
            }
            else
            {
                line = malloc(alloc_size);
            }
            malloced = 1;
            if (line == NULL) {
#ifdef TWS_DEBUG
                printf("read_line_of_arbitrary_length: going out 1, heap alloc failure\n");
#endif
                goto out;
            }
        }
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

#ifdef TWS_DEBUG
    printf("read_line: i read %s\n", line);
#endif
    *val = line;
    err = 0;
out:
    if(err < 0) {
        if(nread > 0) {
#ifdef TWS_DEBUG
            printf("read_line: corruption happened (string longer than max)\n");
#endif
        }
        // always close the connection in buffer overflow conditions; the next element fetch will be corrupt anyway and this way we prevent nasty surprises downrange.
        tws_disconnect(ti);

        // and free the allocated buffer when an error occurred: it is NOT passed back to the caller
        if (malloced && line)
        {
            free(line);
        }
        //assert(*val == NULL);
    }

    return err;
}

static int read_double(tws_instance_t *ti, double *val)
{
    char line[5* sizeof *val];
    size_t len = sizeof line;
    int err = read_line(ti, line, &len);

    *val = err < 0 ? *dNAN : atof(line);
    return err;
}

static int read_double_max(tws_instance_t *ti, double *val)
{
    char line[5* sizeof *val];
    size_t len = sizeof line;
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
    size_t len = sizeof line;
    int err = read_line(ti, line, &len);

    /* return an impossibly large negative number on error to fail careless callers*/
    *val = err < 0 ? ~(1 << 30) : atoi(line);
    return err;
}

static int read_int_max(tws_instance_t *ti, int *val)
{
    char line[3* sizeof *val];
    size_t len = sizeof line;
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
    char line[3 * sizeof *val];
    size_t len = sizeof line;
    int err = read_line(ti, line, &len);

    /* return an impossibly large negative number on error to fail careless callers*/
    *val = err < 0 ? ~(1 << 30) : atol(line);
    return err;
}

static void reset_io_buffers(tws_instance_t *ti)
{
    /* WARNING: reset the output buffer to NIL fill when we send a connect message: this flushes any data lingering from a previously failed transmit on a previous connect */
    ti->tx_buf_next = 0;
    /* also reset the RECEIVE BUFFER to an 'empty' state! */
    ti->buf_last = 0;
    ti->buf_next = 0;
}


/*
similar to IB/TWS Java method:

    public synchronized void eConnect(Socket socket, int clientId) throws IOException {
*/
int tws_connect(void *tws, int client_id)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    size_t lval;
    int val, err;

    if(ti->connected) {
        err = ALREADY_CONNECTED; goto out;
    }

    reset_io_buffers(ti);

    err = ti->open(ti->opaque);
    if (err != 0) {
        goto out;
    }
    // turn this 'is connected' flag ON so that the read/send methods in here will work as expected.
    ti->connected = 1;

    if(send_int(ti, TWSCLIENT_VERSION)) {
        err = CONNECT_FAIL; goto out;
    }
    flush_message(ti);

    if(read_int(ti, &val)) {
        err = CONNECT_FAIL; goto out;
    }

    if(val < 1) {
        err = NO_VALID_ID;
        tws_disconnect(ti);
        goto out;
    }

    ti->server_version = val;
    if(ti->server_version >= 20) {
        lval = sizeof ti->connect_time;
        if(read_line(ti, ti->connect_time, &lval) < 0) {
            err = CONNECT_FAIL; goto out;
        }
    }

    if(ti->server_version >= 3) {
        if(send_int(ti, client_id)) {
            err = CONNECT_FAIL; goto out;
        }
        flush_message(ti);
    }

    err = 0;
out:
    if(err) {
        // do NOT 'destroy' the tws instance for reasons of symmetry: that sort of thing should only happen when tws_create() fails!
        //
        // Any error which is fatal to the connection, will already have triggered a tws_disconnect() by now (e.g. errors in send_int() and read_int())
    }
    return err;
}

int tws_connected(void *tws)
{
    return ((tws_instance_t *) tws)->connected;
}

void  tws_disconnect(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if (ti->connected) {
        ti->close(ti->opaque);
    }
    ti->connected = 0;

    reset_io_buffers(ti);
}

/*
similar to IB/TWS Java method:

    public synchronized void reqScannerParameters() {
*/
int tws_req_scanner_parameters(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < 24) return UPDATE_TWS;

    send_int(ti, REQ_SCANNER_PARAMETERS);
    send_int(ti, 1 /*VERSION*/);

    flush_message(ti);

    return ti->connected ? 0: FAIL_SEND_REQSCANNERPARAMETERS;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqScannerSubscription( int tickerId,
        ScannerSubscription subscription) {
*/
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
    send_double_max(ti, s->scan_above_price);
    send_double_max(ti, s->scan_below_price);
    send_int_max(ti, s->scan_above_volume);
    send_double_max(ti, s->scan_market_cap_above);
    send_double_max(ti, s->scan_market_cap_below);
    send_str(ti, s->scan_moody_rating_above);
    send_str(ti, s->scan_moody_rating_below);
    send_str(ti, s->scan_sp_rating_above);
    send_str(ti, s->scan_sp_rating_below);
    send_str(ti, s->scan_maturity_date_above);
    send_str(ti, s->scan_maturity_date_below);
    send_double_max(ti, s->scan_coupon_rate_above);
    send_double_max(ti, s->scan_coupon_rate_below);
    send_str(ti, s->scan_exclude_convertible);

    if(ti->server_version >= 25) {
        send_int(ti, s->scan_average_option_volume_above);
        send_str(ti, s->scan_scanner_setting_pairs);
    }

    if(ti->server_version >= 27)
        send_str(ti, s->scan_stock_type_filter);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_REQSCANNER;
}

/*
similar to IB/TWS Java method:

    public synchronized void cancelScannerSubscription( int tickerId) {
*/
int tws_cancel_scanner_subscription(void *tws, int ticker_id)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < 24) return UPDATE_TWS;

    send_int(ti, CANCEL_SCANNER_SUBSCRIPTION);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, ticker_id);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_CANSCANNER;
}

typedef enum
{
    COMBO_FOR_REQUEST_MARKET_DATA,
    COMBO_FOR_REQUEST_HIST_DATA,
    COMBO_FOR_PLACE_ORDER,
} send_combolegs_mode;

static void send_combolegs(tws_instance_t *ti, tr_contract_t *contract, send_combolegs_mode mode)
{
    int j;

    send_int(ti, contract->c_num_combolegs);
    for(j = 0; j < contract->c_num_combolegs; j++) {
        tr_comboleg_t *cl = &contract->c_comboleg[j];

        send_int(ti, cl->co_conid);
        send_int(ti, cl->co_ratio);
        send_str(ti, cl->co_action);
        send_str(ti, cl->co_exchange);
        if (mode != COMBO_FOR_REQUEST_MARKET_DATA && mode != COMBO_FOR_REQUEST_HIST_DATA)
        {
            send_int(ti, cl->co_open_close);

            if(ti->server_version >= MIN_SERVER_VER_SSHORT_COMBO_LEGS)
            {
                send_int(ti, cl->co_short_sale_slot);
                send_str(ti, cl->co_designated_location);
            }
            if (ti->server_version >= MIN_SERVER_VER_SSHORTX_OLD)
            {
                send_int(ti, cl->co_exempt_code);
            }
        }
    }
}

/*
similar to IB/TWS Java method:

    public synchronized void reqMktData(int tickerId, Contract contract,
            String genericTickList, boolean snapshot) {
*/
int tws_req_mkt_data(void *tws, int ticker_id, tr_contract_t *contract, const char generic_tick_list[], int snapshot)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < MIN_SERVER_VER_SNAPSHOT_MKT_DATA && snapshot) {
#ifdef TWS_DEBUG
        printf("tws_req_mkt_data does not support snapshot market data requests\n");
#endif
        return UPDATE_TWS;
    }

    if(ti->server_version < MIN_SERVER_VER_UNDER_COMP) {
        if(contract->c_undercomp) {
#ifdef TWS_DEBUG
            printf("tws_req_mkt_data does not support delta neutral orders\n");
#endif
            return UPDATE_TWS;
        }
    }

    if (ti->server_version < MIN_SERVER_VER_REQ_MKT_DATA_CONID) {
        if (contract->c_conid > 0) {
#ifdef TWS_DEBUG
            printf("tws_req_mkt_data does not support conId parameter\n");
#endif
            return UPDATE_TWS;
        }
    }

    send_int(ti, REQ_MKT_DATA);
    send_int(ti, 9 /* version */);
    send_int(ti, ticker_id);

    if (ti->server_version >= MIN_SERVER_VER_REQ_MKT_DATA_CONID) {
        send_int(ti, contract->c_conid);
    }
    send_str(ti, contract->c_symbol);
    send_str(ti, contract->c_sectype);
    send_str(ti, contract->c_expiry);
    send_double(ti, contract->c_strike);
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
        send_combolegs(ti, contract, COMBO_FOR_REQUEST_MARKET_DATA);

    if (ti->server_version >= MIN_SERVER_VER_UNDER_COMP) {
        if(contract->c_undercomp) {
            send_int(ti, 1);
            send_int(ti, contract->c_undercomp->u_conid);
            send_double(ti, contract->c_undercomp->u_delta);
            send_double(ti, contract->c_undercomp->u_price);
        } else {
            send_int(ti, 0);
        }
    }

    if(ti->server_version >= 31)
    {
        /*
         * Note: Even though SHORTABLE tick type support only
         *       started with server version 33 it would be relatively
         *       expensive to expose this restriction here.
         *
         *       Therefore we are relying on TWS doing validation.
         */
        send_str(ti, generic_tick_list);
    }

    if(ti->server_version >= MIN_SERVER_VER_SNAPSHOT_MKT_DATA)
        send_boolean(ti, snapshot);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_REQMKT;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqHistoricalData( int tickerId, Contract contract,
                                                String endDateTime, String durationStr,
                                                String barSizeSetting, String whatToShow,
                                                int useRTH, int formatDate) {
*/
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
    send_double(ti, contract->c_strike);
    send_str(ti, contract->c_right);
    send_str(ti, contract->c_multiplier);
    send_str(ti, contract->c_exchange);
    send_str(ti, contract->c_primary_exch);
    send_str(ti, contract->c_currency);
    send_str(ti, contract->c_local_symbol);

    if(ti->server_version >= 31)
        send_boolean(ti, contract->c_include_expired);

    if(ti->server_version >= 20) {
        send_str(ti, end_date_time);
        send_str(ti, bar_size_setting);
    }
    send_str(ti, duration_str);
    send_int(ti, use_rth);
    send_str(ti, what_to_show);
    if(ti->server_version > 16)
        send_int(ti, format_date);

    if(ti->server_version >= 8 && !strcasecmp(contract->c_sectype, "BAG"))
        send_combolegs(ti, contract, COMBO_FOR_REQUEST_HIST_DATA);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_REQHISTDATA;
}

/*
similar to IB/TWS Java method:

    public synchronized void cancelHistoricalData( int tickerId ) {
*/
int tws_cancel_historical_data(void *tws, int ticker_id)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < 24) return UPDATE_TWS;

    send_int(ti, CANCEL_HISTORICAL_DATA);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, ticker_id);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_CANHISTDATA;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqContractDetails(int reqId, Contract contract)
*/
int tws_req_contract_details(void *tws, int reqid, tr_contract_t *contract)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    /* This feature is only available for versions of TWS >=4 */
    if(ti->server_version < 4)
        return UPDATE_TWS;

    if(ti->server_version < MIN_SERVER_VER_SEC_ID_TYPE) {
        if(!IS_EMPTY(contract->c_secid_type) || !IS_EMPTY(contract->c_secid)) {
#ifdef TWS_DEBUG
            printf("tws_req_contract_details: it does not support secIdType and secId parameters.");
#endif
            return UPDATE_TWS;
        }
    }

    /* send req mkt data msg */
    send_int(ti, REQ_CONTRACT_DATA);
    send_int(ti, 6 /*VERSION*/);

    if(ti->server_version >= MIN_SERVER_VER_CONTRACT_DATA_CHAIN)
        send_int(ti, reqid);

    if(ti->server_version >= MIN_SERVER_VER_CONTRACT_CONID)
        send_int(ti, contract->c_conid);

    send_str(ti, contract->c_symbol);
    send_str(ti, contract->c_sectype);
    send_str(ti, contract->c_expiry);
    send_double(ti, contract->c_strike);
    send_str(ti, contract->c_right);

    if(ti->server_version >= 15)
        send_str(ti, contract->c_multiplier);

    send_str(ti, contract->c_exchange);
    send_str(ti, contract->c_currency);
    send_str(ti, contract->c_local_symbol);
    if(ti->server_version >= 31)
        send_boolean(ti, contract->c_include_expired);

    if(ti->server_version >= MIN_SERVER_VER_SEC_ID_TYPE) {
        send_str(ti, contract->c_secid_type);
        send_str(ti, contract->c_secid);
    }

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_REQCONTRACT;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqMktDepth( int tickerId, Contract contract, int numRows)
*/
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
    send_double(ti, contract->c_strike);
    send_str(ti, contract->c_right);
    if(ti->server_version >= 15)
        send_str(ti, contract->c_multiplier);

    send_str(ti, contract->c_exchange);
    send_str(ti, contract->c_currency);
    send_str(ti, contract->c_local_symbol);
    if(ti->server_version >= 19)
        send_int(ti, num_rows);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_REQMKTDEPTH;
}

/*
similar to IB/TWS Java method:

    public synchronized void cancelMktData( int tickerId) {
*/
int tws_cancel_mkt_data(void *tws, int ticker_id)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, CANCEL_MKT_DATA);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, ticker_id);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_CANMKT;
}

/*
similar to IB/TWS Java method:

    public synchronized void cancelMktDepth( int tickerId) {
*/
int tws_cancel_mkt_depth(void *tws, int ticker_id)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    /* This feature is only available for versions of TWS >=6 */
    if(ti->server_version < 6)
        return UPDATE_TWS;

    send_int(ti, CANCEL_MKT_DEPTH);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, ticker_id);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_CANMKTDEPTH;
}

/*
similar to IB/TWS Java method:

    public synchronized void exerciseOptions( int tickerId, Contract contract,
                                              int exerciseAction, int exerciseQuantity,
                                              String account, int override) {
*/
int tws_exercise_options(void *tws, int ticker_id, tr_contract_t *contract, int exercise_action, int exercise_quantity, const char account[], int exc_override)
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
    send_double(ti, contract->c_strike);
    send_str(ti, contract->c_right);
    send_str(ti, contract->c_multiplier);
    send_str(ti, contract->c_exchange);
    send_str(ti, contract->c_currency);
    send_str(ti, contract->c_local_symbol);
    send_int(ti, exercise_action);
    send_int(ti, exercise_quantity);
    send_str(ti, account);
    send_int(ti, exc_override);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_EXCERCISE_OPTIONS;
}

/*
similar to IB/TWS Java method:

    public synchronized void placeOrder( int id, Contract contract, Order order) {
*/
int tws_place_order(void *tws, int id, tr_contract_t *contract, tr_order_t *order)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    int vol26 = 0, version;

    if(ti->server_version < MIN_SERVER_VER_SCALE_ORDERS) {
        if(order->o_scale_init_level_size != INTEGER_MAX_VALUE ||
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
            int j;

            for (j = 0; j < contract->c_num_combolegs; j++) {
                cl = &contract->c_comboleg[j];

                if(cl->co_short_sale_slot != 0 ||
                    !IS_EMPTY(cl->co_designated_location)) {
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

    if(ti->server_version < MIN_SERVER_VER_UNDER_COMP) {
        if(contract->c_undercomp) {
#ifdef TWS_DEBUG
            printf("tws_place_order does not support delta-neutral orders\n");
#endif
            return UPDATE_TWS;
        }
    }

    if(ti->server_version < MIN_SERVER_VER_SCALE_ORDERS2) {
        if(order->o_scale_subs_level_size != INTEGER_MAX_VALUE) {
#ifdef TWS_DEBUG
            printf("tws_place_order does not support Subsequent Level Size for Scale orders\n");
#endif
            return UPDATE_TWS;
        }
    }

    if(ti->server_version < MIN_SERVER_VER_ALGO_ORDERS) {
        if (!IS_EMPTY(order->o_algo_strategy)) {
#ifdef TWS_DEBUG
            printf("tws_place_order: It does not support algo orders\n");
#endif
            return UPDATE_TWS;
        }
    }

    if(ti->server_version < MIN_SERVER_VER_NOT_HELD) {
        if (order->o_not_held) {
#ifdef TWS_DEBUG
            printf("tws_place_order: It does not support notHeld parameter\n");
#endif
            return UPDATE_TWS;
        }
    }

    if (ti->server_version < MIN_SERVER_VER_SEC_ID_TYPE) {
        if(!IS_EMPTY(contract->c_secid_type) || !IS_EMPTY(contract->c_secid)) {
#ifdef TWS_DEBUG
            printf("tws_place_order: It does not support secIdType and secId parameters\n");
#endif
            return UPDATE_TWS;
        }
    }

    if (ti->server_version < MIN_SERVER_VER_PLACE_ORDER_CONID) {
        if (contract->c_conid > 0) {
#ifdef TWS_DEBUG
            printf("tws_place_order: It does not support conId parameter\n");
#endif
            return UPDATE_TWS;
        }
    }

    if (ti->server_version < MIN_SERVER_VER_SSHORTX) {
        if (order->o_exempt_code != -1) {
#ifdef TWS_DEBUG
            printf("tws_place_order: It does not support exemptCode parameter\n");
#endif
            return UPDATE_TWS;
        }
    }

    if (ti->server_version < MIN_SERVER_VER_SSHORTX) {
        if (contract->c_comboleg && contract->c_num_combolegs) {
            tr_comboleg_t *cl;
            int i;
            for (i = 0; i < contract->c_num_combolegs; ++i) {
                cl = &contract->c_comboleg[i];
                if (cl->co_exempt_code != -1) {
#ifdef TWS_DEBUG
                    printf("tws_place_order: It does not support exemptCode parameter\n");
#endif
                    return UPDATE_TWS;
                }
            }
        }
    }

    send_int(ti, PLACE_ORDER);
    version = ti->server_version < MIN_SERVER_VER_NOT_HELD ? 27 : 31;
    send_int(ti, version);
    send_int(ti, id);

    /* send contract fields */
    if (ti->server_version >= MIN_SERVER_VER_PLACE_ORDER_CONID) {
        send_int(ti, contract->c_conid);
    }
    send_str(ti, contract->c_symbol);
    send_str(ti, contract->c_sectype);
    send_str(ti, contract->c_expiry);
    send_double(ti, contract->c_strike);
    send_str(ti, contract->c_right);
    if(ti->server_version >= 15)
        send_str(ti, contract->c_multiplier);

    send_str(ti, contract->c_exchange);
    if(ti->server_version >= 14)
        send_str(ti, contract->c_primary_exch);

    send_str(ti, contract->c_currency);
    if(ti->server_version >= 2)
        send_str(ti, contract->c_local_symbol);

    if(ti->server_version >= MIN_SERVER_VER_SEC_ID_TYPE){
        send_str(ti, contract->c_secid_type);
        send_str(ti, contract->c_secid);
    }

    /* send main order fields */
    send_str(ti, order->o_action);
    send_int(ti, order->o_total_quantity);
    send_str(ti, order->o_order_type);
    send_double(ti, order->o_lmt_price);
    send_double(ti, order->o_aux_price);

    /* send extended order fields */
    send_str(ti, order->o_tif);
    send_str(ti, order->o_oca_group);
    send_str(ti, order->o_account);
    send_str(ti, order->o_open_close);
    send_int(ti, order->o_origin);
    send_str(ti, order->o_orderref);
    send_boolean(ti, order->o_transmit);
    if(ti->server_version >= 4)
        send_int(ti, order->o_parentid);

    if(ti->server_version >= 5 ) {
        send_boolean(ti, order->o_block_order);
        send_boolean(ti, order->o_sweep_to_fill);
        send_int(ti, order->o_display_size);
        send_int(ti, order->o_trigger_method);
        send_boolean(ti, ti->server_version < 38 ? 0 : order->o_outside_rth);
    }

    if(ti->server_version >= 7)
        send_boolean(ti, order->o_hidden);

    /* Send combo legs for BAG requests */
    if(ti->server_version >= 8 && !strcasecmp(contract->c_sectype, "BAG"))
        send_combolegs(ti, contract, COMBO_FOR_PLACE_ORDER);

    if(ti->server_version >= 9)
        send_str(ti, ""); /* deprecated: shares allocation */

    if(ti->server_version >= 10)
        send_double(ti, order->o_discretionary_amt);

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

    if (ti->server_version >= MIN_SERVER_VER_SSHORTX_OLD) {
        send_int(ti, order->o_exempt_code);
    }

    if(ti->server_version >= 19) {
        vol26 = (ti->server_version == 26 && !strcasecmp(order->o_order_type, "VOL"));

        send_int(ti, order->o_oca_type);

        if(ti->server_version < 38)
            send_int(ti, 0); /* deprecated: o_rth_only */

        send_str(ti, order->o_rule80a);
        send_str(ti, order->o_settling_firm);
        send_boolean(ti, order->o_all_or_none);
        send_int_max(ti, order->o_min_qty);
        send_double_max(ti, order->o_percent_offset);
        send_boolean(ti, order->o_etrade_only);
        send_boolean(ti, order->o_firm_quote_only);
        send_double_max(ti, order->o_nbbo_price_cap);
        send_int_max(ti, order->o_auction_strategy);
        send_double_max(ti, order->o_starting_price);
        send_double_max(ti, order->o_stock_ref_price);
        send_double_max(ti, order->o_delta);
        /* Volatility orders had specific watermark price attribs in server version 26 */
        send_double_max(ti, vol26 ? DBL_MAX : order->o_stock_range_lower);
        send_double_max(ti, vol26 ? DBL_MAX : order->o_stock_range_upper);
    }

    if(ti->server_version >= 22)
        send_boolean(ti, order->o_override_percentage_constraints);

    if(ti->server_version >= 26) { /* Volatility orders */
        send_double_max(ti, order->o_volatility);
        send_int_max(ti, order->o_volatility_type);

        if(ti->server_version < 28) {
            send_boolean(ti, !strcasecmp(order->o_delta_neutral_order_type, "MKT"));
        }
        else {
            send_str(ti, order->o_delta_neutral_order_type);
            send_double_max(ti, order->o_delta_neutral_aux_price);
        }

        send_boolean(ti, order->o_continuous_update);
        /* Volatility orders had specific watermark price attribs in server version 26 */
        if(ti->server_version == 26) {
            /* this is a mechanical translation of java code but is it correct? */
            send_double_max(ti, vol26 ? order->o_stock_range_lower : DBL_MAX);
            send_double_max(ti, vol26 ? order->o_stock_range_upper : DBL_MAX);
        }

        send_int_max(ti, order->o_reference_price_type);
    }

    if(ti->server_version >= 30) /* TRAIL_STOP_LIMIT stop price */
        send_double_max(ti, order->o_trail_stop_price);

    if(ti->server_version >= MIN_SERVER_VER_SCALE_ORDERS) {
        if(ti->server_version >= MIN_SERVER_VER_SCALE_ORDERS2) {
            send_int_max(ti, order->o_scale_init_level_size);
            send_int_max(ti, order->o_scale_subs_level_size);
        } else {
            send_str(ti, "");
            send_int_max(ti, order->o_scale_init_level_size);
        }

        send_double_max(ti, order->o_scale_price_increment);
    }

    if(ti->server_version >= MIN_SERVER_VER_PTA_ORDERS) {
        send_str(ti, order->o_clearing_account);
        send_str(ti, order->o_clearing_intent);
    }

    if(ti->server_version >= MIN_SERVER_VER_NOT_HELD)
        send_boolean(ti, order->o_not_held);

    if(ti->server_version >= MIN_SERVER_VER_UNDER_COMP) {
        if(contract->c_undercomp) {
            send_int(ti, 1);
            send_int(ti, contract->c_undercomp->u_conid);
            send_double(ti, contract->c_undercomp->u_delta);
            send_double(ti, contract->c_undercomp->u_price);
        } else {
            send_int(ti, 0);
        }
    }

    if(ti->server_version >= MIN_SERVER_VER_ALGO_ORDERS) {
        send_str(ti, order->o_algo_strategy);
        if(!IS_EMPTY(order->o_algo_strategy)) {
            send_int(ti, order->o_algo_params_count);
            if(order->o_algo_params_count > 0) {
                int j;
                if (order->o_algo_params == NULL) {
#ifdef TWS_DEBUG
                    printf("tws_place_order: Algo Params array has not been properly set up: array is NULL\n");
#endif
                    // we may already have sent part of the constructed message so play it safe and discard the connection!
                    tws_disconnect(ti);
                }
                else {
                    for(j = 0; j < order->o_algo_params_count; j++) {
                        if (order->o_algo_params[j].t_tag == NULL) {
#ifdef TWS_DEBUG
                            printf("tws_place_order: Algo Params array has not been properly set up: tag is NULL\n");
#endif
                            // we may already have sent part of the constructed message so play it safe and discard the connection!
                            tws_disconnect(ti);
                            break;
                        }
                        send_str(ti, order->o_algo_params[j].t_tag);
                        send_str(ti, order->o_algo_params[j].t_val);
                    }
                }
            }
        }
    }

    if(ti->server_version >= MIN_SERVER_VER_WHAT_IF_ORDERS)
        send_boolean(ti, order->o_whatif);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_ORDER;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqAccountUpdates(boolean subscribe, String acctCode) {
*/
int tws_req_account_updates(void *tws, int subscribe, const char acct_code[])
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, REQ_ACCOUNT_DATA );
    send_int(ti, 2 /*VERSION*/);
    send_boolean(ti, subscribe);

    /* Send the account code. This will only be used for FA clients */
    if(ti->server_version >= 9)
        send_str(ti, acct_code);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_ACCT;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqExecutions(int reqId, ExecutionFilter filter) {
*/
int tws_req_executions(void *tws, int reqid, tr_exec_filter_t *filter)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, REQ_EXECUTIONS);
    send_int(ti, 3 /*VERSION*/);
    if(ti->server_version >= MIN_SERVER_VER_EXECUTION_DATA_CHAIN)
        send_int(ti, reqid);

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

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_EXEC;
}

/*
similar to IB/TWS Java method:

    public synchronized void cancelOrder( int id) {
*/
int tws_cancel_order(void *tws, int order_id)
{
    tws_instance_t *ti = (tws_instance_t *) tws;
    /* send cancel order msg */
    send_int(ti, CANCEL_ORDER);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, order_id);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_CORDER;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqOpenOrders() {
*/
int tws_req_open_orders(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, REQ_OPEN_ORDERS);
    send_int(ti, 1 /*VERSION*/);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_OORDER;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqIds( int numIds) {
*/
int tws_req_ids(void *tws, int numids)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, REQ_IDS);
    send_int(ti, 1 /* VERSION */);
    send_int(ti, numids);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_REQIDS;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqNewsBulletins( boolean allMsgs) {
*/
int tws_req_news_bulletins(void *tws, int allmsgs)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, REQ_NEWS_BULLETINS);
    send_int(ti, 1 /*VERSION*/);
    send_boolean(ti, allmsgs);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_BULLETINS;
}

/*
similar to IB/TWS Java method:

    public synchronized void cancelNewsBulletins() {
*/
int tws_cancel_news_bulletins(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, CANCEL_NEWS_BULLETINS);
    send_int(ti, 1 /*VERSION*/);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_CBULLETINS;
}

/*
similar to IB/TWS Java method:

    public synchronized void setServerLogLevel(int logLevel) {
*/
int tws_set_server_log_level(void *tws, int level)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, SET_SERVER_LOGLEVEL);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, level);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_SERVER_LOG_LEVEL;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqAutoOpenOrders(boolean bAutoBind)
*/
int tws_req_auto_open_orders(void *tws, int auto_bind)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    send_int(ti, REQ_AUTO_OPEN_ORDERS);
    send_int(ti, 1 /*VERSION*/);
    send_boolean(ti, auto_bind);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_OORDER;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqAllOpenOrders() {
*/
int tws_req_all_open_orders(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    /* send req all open orders msg */
    send_int(ti, REQ_ALL_OPEN_ORDERS);
    send_int(ti, 1 /*VERSION*/);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_OORDER;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqManagedAccts() {
*/
int tws_req_managed_accts(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    /* send req FA managed accounts msg */
    send_int(ti, REQ_MANAGED_ACCTS);
    send_int(ti, 1 /*VERSION*/);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_OORDER;
}

/*
similar to IB/TWS Java method:

    public synchronized void requestFA( int faDataType ) {
*/
int tws_request_fa(void *tws, int fa_data_type)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    /* This feature is only available for versions of TWS >= 13 */
    if(ti->server_version < 13)
        return UPDATE_TWS;

    send_int(ti, REQ_FA);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, fa_data_type);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_FA_REQUEST;
}

/*
similar to IB/TWS Java method:

    public synchronized void replaceFA( int faDataType, String xml ) {
*/
int tws_replace_fa(void *tws, int fa_data_type, const char xml[])
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    /* This feature is only available for versions of TWS >= 13 */
    if(ti->server_version < 13)
        return UPDATE_TWS;

    send_int(ti, REPLACE_FA );
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, fa_data_type);
    send_str(ti, xml);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_FA_REPLACE;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqCurrentTime() {
*/
int tws_req_current_time(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < 33) {
        return UPDATE_TWS;
    }

    send_int(ti, REQ_CURRENT_TIME);
    send_int(ti, 1 /*VERSION*/);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_REQCURRTIME;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqFundamentalData(int reqId, Contract contract,
            String reportType) {
*/
int tws_req_fundamental_data(void *tws, int reqid, tr_contract_t *contract, char report_type[])
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < MIN_SERVER_VER_FUNDAMENTAL_DATA) {
#ifdef TWS_DEBUG
        printf("tws_req_fundamental_data does not support fundamental data requests");
#endif
        return UPDATE_TWS;
    }

    send_int(ti, REQ_FUNDAMENTAL_DATA);
    send_int(ti, 1 /* version */);
    send_int(ti, reqid);
    send_str(ti, contract->c_symbol);
    send_str(ti, contract->c_sectype);
    send_str(ti, contract->c_exchange);
    send_str(ti, contract->c_primary_exch);
    send_str(ti, contract->c_currency);
    send_str(ti, contract->c_local_symbol);
    send_str(ti, report_type);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_REQFUNDDATA;
}

/*
similar to IB/TWS Java method:

    public synchronized void cancelFundamentalData(int reqId) {
*/
int tws_cancel_fundamental_data(void *tws, int reqid)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < MIN_SERVER_VER_FUNDAMENTAL_DATA) {
#ifdef TWS_DEBUG
        printf("tws_cancel_fundamental data does not support fundamental data requests.");
#endif
        return UPDATE_TWS;
    }

    send_int(ti, CANCEL_FUNDAMENTAL_DATA);
    send_int(ti, 1 /*version*/);
    send_int(ti, reqid);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_CANFUNDDATA;
}

/*
similar to IB/TWS Java method:

    public synchronized void calculateImpliedVolatility(int reqId, Contract contract,
            double optionPrice, double underPrice) {
*/
int tws_calculate_implied_volatility(void *tws, int reqid, tr_contract_t *contract, double option_price, double under_price)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if (ti->server_version < MIN_SERVER_VER_REQ_CALC_IMPLIED_VOLAT) {
#ifdef TWS_DEBUG
        printf("tws_calculate_implied_volatility: It does not support calculate implied volatility requests\n");
#endif
        return UPDATE_TWS;
    }

    // send calculate implied volatility msg
    send_int(ti, REQ_CALC_IMPLIED_VOLAT);
    send_int(ti, 1 /*version*/);
    send_int(ti, reqid);

    // send contract fields
    send_int(ti, contract->c_conid);
    send_str(ti, contract->c_symbol);
    send_str(ti, contract->c_sectype);
    send_str(ti, contract->c_expiry);
    send_double(ti, contract->c_strike);
    send_str(ti, contract->c_right);
    send_str(ti, contract->c_multiplier);
    send_str(ti, contract->c_exchange);
    send_str(ti, contract->c_primary_exch);
    send_str(ti, contract->c_currency);
    send_str(ti, contract->c_local_symbol);

    send_double(ti, option_price);
    send_double(ti, under_price);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_REQCALCIMPLIEDVOLAT;
}

/*
similar to IB/TWS Java method:

    public synchronized void cancelCalculateImpliedVolatility(int reqId) {
*/
int tws_cancel_calculate_implied_volatility(void *tws, int reqid)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if (ti->server_version < MIN_SERVER_VER_CANCEL_CALC_IMPLIED_VOLAT) {
#ifdef TWS_DEBUG
        printf("tws_cancel_calculate_implied_volatility: It does not support calculate implied volatility cancellation\n");
#endif
        return UPDATE_TWS;
    }

    // send cancel calculate implied volatility msg
    send_int(ti, CANCEL_CALC_IMPLIED_VOLAT);
    send_int(ti, 1 /*version*/);
    send_int(ti, reqid);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_CANCALCIMPLIEDVOLAT;
}

/*
similar to IB/TWS Java method:

    public synchronized void calculateOptionPrice(int reqId, Contract contract,
            double volatility, double underPrice) {
*/
int tws_calculate_option_price(void *tws, int reqid, tr_contract_t *contract, double volatility, double under_price)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if (ti->server_version < MIN_SERVER_VER_REQ_CALC_OPTION_PRICE) {
#ifdef TWS_DEBUG
        printf("tws_calculate_option_price: It does not support calculate option price requests\n");
#endif
        return UPDATE_TWS;
    }

    // send calculate option price msg
    send_int(ti, REQ_CALC_OPTION_PRICE);
    send_int(ti, 1 /*version*/);
    send_int(ti, reqid);

    // send contract fields
    send_int(ti, contract->c_conid);
    send_str(ti, contract->c_symbol);
    send_str(ti, contract->c_sectype);
    send_str(ti, contract->c_expiry);
    send_double(ti, contract->c_strike);
    send_str(ti, contract->c_right);
    send_str(ti, contract->c_multiplier);
    send_str(ti, contract->c_exchange);
    send_str(ti, contract->c_primary_exch);
    send_str(ti, contract->c_currency);
    send_str(ti, contract->c_local_symbol);

    send_double(ti, volatility);
    send_double(ti, under_price);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_REQCALCOPTIONPRICE;
}

/*
similar to IB/TWS Java method:

    public synchronized void cancelCalculateOptionPrice(int reqId) {
*/
int tws_cancel_calculate_option_price(void *tws, int reqid)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if (ti->server_version < MIN_SERVER_VER_CANCEL_CALC_OPTION_PRICE) {
#ifdef TWS_DEBUG
        printf("tws_cancel_calculate_option_price: It does not support calculate option price cancellation\n");
#endif
        return UPDATE_TWS;
    }

    // send cancel calculate option price msg
    send_int(ti, CANCEL_CALC_OPTION_PRICE);
    send_int(ti, 1 /*version*/);
    send_int(ti, reqid);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_CANCALCOPTIONPRICE;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqGlobalCancel() {
*/
int tws_req_global_cancel(void *tws)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if (ti->server_version < MIN_SERVER_VER_REQ_GLOBAL_CANCEL) {
#ifdef TWS_DEBUG
        printf("tws_req_global_cancel: It does not support globalCancel requests\n");
#endif
        return UPDATE_TWS;
    }

    // send request global cancel msg
    send_int(ti, REQ_GLOBAL_CANCEL);
    send_int(ti, 1 /*version*/);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_REQGLOBALCANCEL;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqRealTimeBars(int tickerId, Contract contract, int barSize, String whatToShow, boolean useRTH) {
*/
int tws_request_realtime_bars(void *tws, int ticker_id, tr_contract_t *c, int bar_size, const char what_to_show[], int use_rth)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < MIN_SERVER_VER_REAL_TIME_BARS)
        return UPDATE_TWS;

    send_int(ti, REQ_REAL_TIME_BARS);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, ticker_id);

    send_str(ti, c->c_symbol);
    send_str(ti, c->c_sectype);
    send_str(ti, c->c_expiry);
    send_double(ti, c->c_strike);
    send_str(ti, c->c_right);
    send_str(ti, c->c_multiplier);
    send_str(ti, c->c_exchange);
    send_str(ti, c->c_primary_exch);
    send_str(ti, c->c_currency);
    send_str(ti, c->c_local_symbol);
    send_int(ti, bar_size);
    send_str(ti, what_to_show);
    send_boolean(ti, use_rth);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_REQRTBARS;
}

/*
similar to IB/TWS Java method:

    public void cancelRealTimeBars(int tickerId) {
*/
int tws_cancel_realtime_bars(void *tws, int ticker_id)
{
    tws_instance_t *ti = (tws_instance_t *) tws;

    if(ti->server_version < MIN_SERVER_VER_REAL_TIME_BARS)
        return UPDATE_TWS;

    send_int(ti, CANCEL_REAL_TIME_BARS);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, ticker_id);

    flush_message(ti);

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

const struct twsclient_errmsg *tws_strerror(int errcode)
{
    static const struct twsclient_errmsg unknown_err = {
        500, "Unknown TWS failure code."
    };
    int i;

    for (i = 0; i < sizeof(twsclient_err_indication) / sizeof(twsclient_err_indication[0]); i++)
    {
        if (twsclient_err_indication[i].err_code == errcode)
        {
            return &twsclient_err_indication[i];
        }
    }
    return &unknown_err;
}

