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
#include <stdarg.h>

#define MAX_TWS_STRINGS 127
#define WORD_SIZE_IN_BITS (8*sizeof(unsigned long))
#define WORDS_NEEDED(num, wsize) (!!((num)%(wsize)) + ((num)/(wsize)))
#define ROUND_UP_POW2(num, pow2) (((num) + (pow2)-1) & ~((pow2)-1))
#define INTEGER_MAX_VALUE ((int) ~(1U<<(8*sizeof(int) -1)))
#define DBL_NOTMAX(d) (fabs((d) - DBL_MAX) > DBL_EPSILON)
#define IS_EMPTY(str)  (!(str) || ((str)[0] == '\0'))
#define DEFAULT_RX_BUFFERSIZE  4096

#if !defined(TRUE)
#undef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif

#include "twsapi-debug.h"




typedef struct {
    char str[512]; /* maximum conceivable string length */
} tws_string_t;

struct tws_instance {
    void *opaque;
    tws_transmit_func_t *transmit;
    tws_receive_func_t *receive;
    tws_flush_func_t *flush;
    tws_open_func_t *open;
    tws_close_func_t *close;

	tws_transmit_element_func_t *tx_observe;
	tws_receive_element_func_t *rx_observe;

    char connect_time[60]; /* server reported time */
    unsigned char tx_buf[512]; /* buffer up to 512 chars at a time for transmission */
    unsigned int tx_buf_next; /* index of next empty char slot in tx_buf */
    unsigned char buf[DEFAULT_RX_BUFFERSIZE]; /* buffer up to N chars at a time */
    unsigned int buf_next, buf_last; /* index of next, last chars in buf */
    unsigned int server_version;
    volatile unsigned int connected;
    tws_string_t mempool[MAX_TWS_STRINGS];
    unsigned long bitmask[WORDS_NEEDED(MAX_TWS_STRINGS, WORD_SIZE_IN_BITS)];
};

static int read_double(tws_instance_t *ti, double *val);
static int read_double_max(tws_instance_t *ti, double *val);
static int read_long(tws_instance_t *ti, long *val);
static int read_int(tws_instance_t *ti, int *val);
static int read_int_max(tws_instance_t *ti, int *val);
static int read_line(tws_instance_t *ti, char *line, size_t *len);
static int read_line_of_arbitrary_length(tws_instance_t *ti, char **val, size_t initial_space);

static void reset_io_buffers(tws_instance_t *ti);

/* access to these strings is single threaded
 * replace plain bit ops with atomic test_and_set_bit/clear_bit + memory barriers
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

    TWS_DEBUG_PRINTF((ti->opaque, "alloc_string: ran out of strings, will crash shortly\n"));

    // close the connection when we run out of pool memory to prevent the currently processed message from making it through to the handler
    tws_disconnect(ti);

    return 0;
}

static void free_string(tws_instance_t *ti, const void *ptr)
{
    if (ptr)
    {
        unsigned int j = (unsigned int) ((const tws_string_t *) ptr - &ti->mempool[0]);

		// do NOT try to free a string which didn't originate from the pool!
		if (j >= 0 && j < MAX_TWS_STRINGS)
		{
			unsigned int index = j / WORD_SIZE_IN_BITS;
			unsigned long bits = 1UL << (j & (WORD_SIZE_IN_BITS - 1));

			ti->bitmask[index] &= ~bits;
		}
    }
}


char *tws_strcpy(char *tws_string_ref, const char *src)
{
	size_t srclen;

	if (!tws_string_ref) 
		return NULL;
	if (!src) 
		src = "";
	srclen = strlen(src);
	if (srclen > sizeof(tws_string_t) - 1) // len-1 to compensate for NUL sentinel
		srclen = sizeof(tws_string_t) - 1;
	// Do not use strncpy() as it'll spend useless time on NUL-filling the dst buffer entirely! 
	// Still it doesn't do anything useful like /always/ NUL-terminating the copied string,
	// hence we use memmove() -- which also allows copying from/to the same buffer.
	memmove(tws_string_ref, src, srclen);
	tws_string_ref[srclen] = 0;

	return tws_string_ref;
}


void tws_init_contract(tws_instance_t *ti, tr_contract_t *c)
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

void tws_destroy_contract(tws_instance_t *ti, tr_contract_t *c)
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

void tws_init_order(tws_instance_t *ti, tr_order_t *o)
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
    o->o_hedge_type = alloc_string(ti);
    o->o_hedge_param = alloc_string(ti);
    o->o_delta_neutral_settling_firm = alloc_string(ti);
    o->o_delta_neutral_clearing_account = alloc_string(ti);
    o->o_delta_neutral_clearing_intent = alloc_string(ti);

	o->o_lmt_price = DBL_MAX;
	o->o_aux_price = DBL_MAX;
	strcpy(o->o_open_close, "O");
	o->o_origin = CUSTOMER;
	o->o_transmit = TRUE;
	o->o_exempt_code = -1;
	o->o_min_qty = INTEGER_MAX_VALUE;
	o->o_percent_offset = DBL_MAX;
	o->o_nbbo_price_cap = DBL_MAX;
	o->o_starting_price = DBL_MAX;
	o->o_stock_ref_price = DBL_MAX;
	o->o_delta = DBL_MAX;
	o->o_stock_range_lower = DBL_MAX;
	o->o_stock_range_upper = DBL_MAX;
	o->o_volatility = DBL_MAX;
	o->o_volatility_type = INTEGER_MAX_VALUE;
	o->o_delta_neutral_aux_price = DBL_MAX;
	o->o_reference_price_type = INTEGER_MAX_VALUE;
	o->o_trail_stop_price = DBL_MAX;
	o->o_trailing_percent = DBL_MAX;
	o->o_basis_points = DBL_MAX;
	o->o_basis_points_type = INTEGER_MAX_VALUE;
	o->o_scale_init_level_size = INTEGER_MAX_VALUE;
	o->o_scale_subs_level_size = INTEGER_MAX_VALUE;
	o->o_scale_price_increment = DBL_MAX;
	o->o_scale_price_adjust_value = DBL_MAX;
	o->o_scale_price_adjust_interval = INTEGER_MAX_VALUE;
	o->o_scale_profit_offset = DBL_MAX;
	o->o_scale_init_position = INTEGER_MAX_VALUE;
	o->o_scale_init_fill_qty = INTEGER_MAX_VALUE;
}

void tws_destroy_order(tws_instance_t *ti, tr_order_t *o)
{
    free_string(ti, o->o_delta_neutral_clearing_intent);
    free_string(ti, o->o_delta_neutral_clearing_account);
    free_string(ti, o->o_delta_neutral_settling_firm);
    free_string(ti, o->o_hedge_param);
    free_string(ti, o->o_hedge_type);
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

	if (o->o_smart_combo_routing_params && o->o_smart_combo_routing_params_count)
    {
        int i;

        for (i = 0; i < o->o_smart_combo_routing_params_count; i++)
        {
            free_string(ti, o->o_smart_combo_routing_params[i].t_tag);
            free_string(ti, o->o_smart_combo_routing_params[i].t_val);
        }
        free(o->o_smart_combo_routing_params);
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
    tws_init_contract(ti, &cd->d_summary);

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

    tws_destroy_contract(ti, &cd->d_summary);
}

static void init_execution(tws_instance_t *ti, tr_execution_t *exec)
{
    memset(exec, 0, sizeof *exec);

    exec->e_execid = alloc_string(ti);
    exec->e_time = alloc_string(ti);
    exec->e_acct_number = alloc_string(ti);
    exec->e_exchange = alloc_string(ti);
    exec->e_side = alloc_string(ti);
    exec->e_orderref = alloc_string(ti);
}

static void destroy_execution(tws_instance_t *ti, tr_execution_t *exec)
{
    free_string(ti, exec->e_orderref);
    free_string(ti, exec->e_side);
    free_string(ti, exec->e_exchange);
    free_string(ti, exec->e_acct_number);
    free_string(ti, exec->e_time);
    free_string(ti, exec->e_execid);
}

void tws_init_tr_comboleg(tws_instance_t *ti, tr_comboleg_t *cl)
{
    memset(cl, 0, sizeof(*cl));
    cl->co_action = alloc_string(ti);
    cl->co_exchange = alloc_string(ti);
    cl->co_designated_location = alloc_string(ti);

    cl->co_exempt_code = -1;
}

void tws_destroy_tr_comboleg(tws_instance_t *ti, tr_comboleg_t *cl)
{
    free_string(ti, cl->co_action);
    free_string(ti, cl->co_exchange);
    free_string(ti, cl->co_designated_location);
}

void tws_init_exec_filter(tws_instance_t *ti, tr_exec_filter_t *filter)
{
    memset(filter, 0, sizeof(*filter));
    filter->f_acctcode = alloc_string(ti);
    filter->f_time = alloc_string(ti);
    filter->f_symbol = alloc_string(ti);
    filter->f_sectype = alloc_string(ti);
    filter->f_exchange = alloc_string(ti);
    filter->f_side = alloc_string(ti);
}

void tws_destroy_exec_filter(tws_instance_t *ti, tr_exec_filter_t *filter)
{
    free_string(ti, filter->f_acctcode);
    free_string(ti, filter->f_time);
    free_string(ti, filter->f_symbol);
    free_string(ti, filter->f_sectype);
    free_string(ti, filter->f_exchange);
    free_string(ti, filter->f_side);
}

void tws_init_scanner_subscription(tws_instance_t *ti, tr_scanner_subscription_t *ss)
{
    memset(ss, 0, sizeof(*ss));

	// set sensible values instead of the ones set by the original IB/TWS code:
	// http://www.traderslaboratory.com/forums/automated-trading/7870-interactive-brokers-api-market-scanners.html

    ss->scan_above_price = 0; // DBL_MAX;
    ss->scan_below_price = DBL_MAX;
    ss->scan_coupon_rate_above = 0; // DBL_MAX;
    ss->scan_coupon_rate_below = DBL_MAX;
    ss->scan_market_cap_above = 0; // DBL_MAX; 
    ss->scan_market_cap_below = DBL_MAX;

    ss->scan_exclude_convertible = alloc_string(ti);
    ss->scan_instrument = alloc_string(ti);
    ss->scan_location_code = alloc_string(ti);
    ss->scan_maturity_date_above = alloc_string(ti);
    ss->scan_maturity_date_below = alloc_string(ti);
    ss->scan_moody_rating_above = alloc_string(ti);
    ss->scan_moody_rating_below = alloc_string(ti);
    ss->scan_code = alloc_string(ti);
    ss->scan_sp_rating_above = alloc_string(ti);
    ss->scan_sp_rating_below = alloc_string(ti);
    ss->scan_scanner_setting_pairs = alloc_string(ti);
    ss->scan_stock_type_filter = alloc_string(ti);

    ss->scan_above_volume = 0; // INTEGER_MAX_VALUE;
    ss->scan_number_of_rows = -1;
    ss->scan_average_option_volume_above = 0; // INTEGER_MAX_VALUE;
}

void tws_destroy_scanner_subscription(tws_instance_t *ti, tr_scanner_subscription_t *ss)
{
    free_string(ti, ss->scan_exclude_convertible);
    free_string(ti, ss->scan_instrument);
    free_string(ti, ss->scan_location_code);
    free_string(ti, ss->scan_maturity_date_above);
    free_string(ti, ss->scan_maturity_date_below);
    free_string(ti, ss->scan_moody_rating_above);
    free_string(ti, ss->scan_moody_rating_below);
    free_string(ti, ss->scan_code);
    free_string(ti, ss->scan_sp_rating_above);
    free_string(ti, ss->scan_sp_rating_below);
    free_string(ti, ss->scan_scanner_setting_pairs);
    free_string(ti, ss->scan_stock_type_filter);
}

void tws_init_tag_value(tws_instance_t *ti, tr_tag_value_t *t)
{
    memset(t, 0, sizeof(*t));
    t->t_tag = alloc_string(ti);
    t->t_val = alloc_string(ti);
}

void tws_destroy_tag_value(tws_instance_t *ti, tr_tag_value_t *t)
{
    free_string(ti, t->t_tag);
    free_string(ti, t->t_val);
}

void tws_init_order_combo_leg(tws_instance_t *ti, tr_order_combo_leg_t *ocl)
{
    memset(ocl, 0, sizeof(*ocl));
	ocl->cl_price = DBL_MAX;
}

void tws_destroy_order_combo_leg(tws_instance_t *ti, tr_order_combo_leg_t *ocl)
{
}

void tws_init_under_comp(tws_instance_t *ti, under_comp_t *u)
{
    memset(u, 0, sizeof(*u));
}

void tws_destroy_under_comp(tws_instance_t *ti, under_comp_t *u)
{
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
    read_int(ti, &ival), tick_type = (tr_tick_type_t)ival;
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
    read_int(ti, &ival), tick_type = (tr_tick_type_t)ival;
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
    read_int(ti, &ival), tick_type = (tr_tick_type_t)ival;
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
    read_int(ti, &ival), tick_type = (tr_tick_type_t)ival;
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
    read_int(ti, &ival), tick_type = (tr_tick_type_t)ival;

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
    read_int(ti, &ival); tick_type = (tr_tick_type_t)ival;
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

    tws_init_contract(ti, &contract);

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
    tws_destroy_contract(ti, &contract);
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

    tws_init_contract(ti, &contract);
    tws_init_under_comp(ti, &und);
    contract.c_undercomp = &und;
    tws_init_order(ti, &order);
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
    if (version < 29) { 
	    read_double(ti, &order.o_lmt_price);
    }
    else {
	    read_double_max(ti, &order.o_lmt_price);
    }
    if (version < 30) { 
	    read_double(ti, &order.o_aux_price);
    }
    else {
	    read_double_max(ti, &order.o_aux_price);
    }
    lval = sizeof(tws_string_t), read_line(ti, order.o_tif, &lval);
    lval = sizeof(tws_string_t), read_line(ti, order.o_oca_group, &lval);
    lval = sizeof(tws_string_t), read_line(ti, order.o_account, &lval);
    lval = sizeof(tws_string_t), read_line(ti, order.o_open_close, &lval);
    read_int(ti, &ival), order.o_origin = (tr_origin_t)ival;
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
        read_double_max(ti, &order.o_percent_offset);
        lval = sizeof(tws_string_t), read_line(ti, order.o_settling_firm, &lval);
        read_int(ti, &ival), order.o_short_sale_slot = ival;
        lval = sizeof(tws_string_t), read_line(ti, order.o_designated_location, &lval);
        if (ti->server_version == 51) {
            read_int(ti, &ival); // exemptCode
        }
        else if (version >= 23) {
            read_int(ti, &ival); order.o_exempt_code = ival;
        }
        read_int(ti, &ival), order.o_auction_strategy = (tr_auction_strategy_t)ival;
        read_double_max(ti, &order.o_starting_price);
        read_double_max(ti, &order.o_stock_ref_price);
        read_double_max(ti, &order.o_delta);
        read_double_max(ti, &order.o_stock_range_lower);
        read_double_max(ti, &order.o_stock_range_upper);
        read_int(ti, &order.o_display_size);

        if(version < 18) /* "will never happen" comment */
            read_int(ti, &ival); /*order.o_rth_only = !!ival;*/

        read_int(ti, &ival), order.o_block_order = !!ival;
        read_int(ti, &ival), order.o_sweep_to_fill = !!ival;
        read_int(ti, &ival), order.o_all_or_none = !!ival;
        read_int_max(ti, &order.o_min_qty);
        read_int(ti, &ival), order.o_oca_type = (tr_oca_type_t)ival;
        read_int(ti, &ival), order.o_etrade_only = !!ival;
        read_int(ti, &ival), order.o_firm_quote_only = !!ival;
        read_double_max(ti, &order.o_nbbo_price_cap);
    }

    if(version >= 10) {
        read_int(ti, &order.o_parentid);
        read_int(ti, &order.o_trigger_method);
    }

    if(version >= 11) {
        read_double_max(ti, &order.o_volatility);
        read_int(ti, &order.o_volatility_type);

        if(version == 11) {
            read_int(ti, &ival);
            strcpy(order.o_delta_neutral_order_type, (!ival ? "NONE" : "MKT"));
        } else {
            lval = sizeof(tws_string_t), read_line(ti, order.o_delta_neutral_order_type, &lval);
            read_double_max(ti, &order.o_delta_neutral_aux_price);

            if (version >= 27 && !IS_EMPTY(order.o_delta_neutral_order_type)) {
	            read_int(ti, &ival); order.o_delta_neutral_con_id = ival;
				lval = sizeof(tws_string_t), read_line(ti, order.o_delta_neutral_settling_firm, &lval);
				lval = sizeof(tws_string_t), read_line(ti, order.o_delta_neutral_clearing_account, &lval);
				lval = sizeof(tws_string_t), read_line(ti, order.o_delta_neutral_clearing_intent, &lval);
            }
        }

        read_int(ti, &ival); order.o_continuous_update = !!ival;
        if(ti->server_version == 26) {
            read_double(ti, &order.o_stock_range_lower);
            read_double(ti, &order.o_stock_range_upper);
        }

        read_int(ti, &order.o_reference_price_type);
    }

    if(version >= 13) {
        read_double_max(ti, &order.o_trail_stop_price);
	}

    if (version >= 30) {
        read_double_max(ti, &order.o_trailing_percent);
    }

    if(version >= 14) {
        read_double_max(ti, &order.o_basis_points);
        read_int_max(ti, &ival), order.o_basis_points_type = ival;
        lval = sizeof(tws_string_t); read_line(ti, contract.c_combolegs_descrip, &lval);
    }
                
    if (version >= 29) {
        read_int(ti, &ival); order.o_combo_legs_count = ival;
        if (order.o_combo_legs_count > 0) {
			int j;

            contract.c_comboleg = (tr_comboleg_t *)calloc(order.o_combo_legs_count, sizeof(*contract.c_comboleg));
            for (j = 0; j < order.o_combo_legs_count; j++) {
				tr_comboleg_t *leg = &contract.c_comboleg[j];
				
				tws_init_tr_comboleg(ti, leg);
				read_int(ti, &ival); leg->co_conid = ival;
				read_int(ti, &ival); leg->co_ratio = ival;
				lval = sizeof(tws_string_t); read_line(ti, leg->co_action, &lval);
				lval = sizeof(tws_string_t); read_line(ti, leg->co_exchange, &lval);
				read_int(ti, &ival); leg->co_open_close = (tr_comboleg_type_t)ival;
				read_int(ti, &ival); leg->co_short_sale_slot = ival;
				lval = sizeof(tws_string_t); read_line(ti, leg->co_designated_location, &lval);
				read_int(ti, &ival); leg->co_exempt_code = ival;
            }
        }
        
		read_int(ti, &ival); order.o_combo_legs_count = ival;
        if (order.o_combo_legs_count > 0) {
			int j;

            order.o_combo_legs = (tr_order_combo_leg_t *)calloc(order.o_combo_legs_count, sizeof(*order.o_combo_legs));
            for (j = 0; j < order.o_combo_legs_count; j++) {
				tr_order_combo_leg_t *leg = &order.o_combo_legs[j];

				read_double_max(ti, &leg->cl_price);
            }
        }
    }
                
    if (version >= 26) {
		read_int(ti, &order.o_smart_combo_routing_params_count);
        if (order.o_smart_combo_routing_params_count > 0) {
            order.o_smart_combo_routing_params = (tr_tag_value_t *)calloc(order.o_smart_combo_routing_params_count, sizeof(*order.o_smart_combo_routing_params));
            if(order.o_smart_combo_routing_params) {
                int j;
                for (j = 0; j < order.o_smart_combo_routing_params_count; j++) {
                    order.o_smart_combo_routing_params[j].t_tag = alloc_string(ti);
                    order.o_smart_combo_routing_params[j].t_val = alloc_string(ti);
                    lval = sizeof(tws_string_t); read_line(ti, order.o_smart_combo_routing_params[j].t_tag, &lval);
                    lval = sizeof(tws_string_t); read_line(ti, order.o_smart_combo_routing_params[j].t_val, &lval);
                }
            } else {
                TWS_DEBUG_PRINTF((ti->opaque, "receive_open_order: memory allocation failure\n"));
                tws_disconnect(ti);
            }
        }
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
                
    if (version >= 28 && order.o_scale_price_increment > 0.0 && DBL_NOTMAX(order.o_scale_price_increment)) {
        read_double_max(ti, &order.o_scale_price_adjust_value);
        read_int_max(ti, &order.o_scale_price_adjust_interval);
        read_double_max(ti, &order.o_scale_profit_offset);
        read_int(ti, &ival); order.o_scale_auto_reset = !!ival;
        read_int_max(ti, &order.o_scale_init_position);
        read_int_max(ti, &order.o_scale_init_fill_qty);
        read_int(ti, &ival); order.o_scale_random_percent = !!ival;
    }

	if(version >= 24) {
        lval = sizeof(tws_string_t); read_line(ti, order.o_hedge_type, &lval);
		if (!IS_EMPTY(order.o_hedge_type)) {
	        lval = sizeof(tws_string_t); read_line(ti, order.o_hedge_param, &lval);
		}
	}

    if (version >= 25) {
        read_int(ti, &ival), order.o_opt_out_smart_routing = !!ival;
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
                order.o_algo_params = (tr_tag_value_t *)calloc(order.o_algo_params_count, sizeof(*order.o_algo_params));
                if(order.o_algo_params) {
                    int j;
                    for (j = 0; j < order.o_algo_params_count; j++) {
                        order.o_algo_params[j].t_tag = alloc_string(ti);
                        order.o_algo_params[j].t_val = alloc_string(ti);
                        lval = sizeof(tws_string_t); read_line(ti, order.o_algo_params[j].t_tag, &lval);
                        lval = sizeof(tws_string_t); read_line(ti, order.o_algo_params[j].t_val, &lval);
                    }
                } else {
                    TWS_DEBUG_PRINTF((ti->opaque, "receive_open_order: memory allocation failure\n"));
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
    tws_destroy_order(ti, &order);
    tws_destroy_contract(ti, &contract);
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

	if(version >= 8) {
		lval = sizeof(tws_string_t), read_line(ti, cdetails.d_ev_rule, &lval);
		read_double(ti, &cdetails.d_ev_multiplier);
	}

	if(version >= 7) {
		read_int(ti, &cdetails.d_sec_id_list_count);
		if (cdetails.d_sec_id_list_count > 0) {
			cdetails.d_sec_id_list = (tr_tag_value_t *)calloc(cdetails.d_sec_id_list_count, sizeof(*cdetails.d_sec_id_list));
			if(cdetails.d_sec_id_list) {
				int j;
				for (j = 0; j < cdetails.d_sec_id_list_count; j++) {
					cdetails.d_sec_id_list[j].t_tag = alloc_string(ti);
					cdetails.d_sec_id_list[j].t_val = alloc_string(ti);
					lval = sizeof(tws_string_t); read_line(ti, cdetails.d_sec_id_list[j].t_tag, &lval);
					lval = sizeof(tws_string_t); read_line(ti, cdetails.d_sec_id_list[j].t_val, &lval);
				}
			} else {
				TWS_DEBUG_PRINTF((ti->opaque, "receive_contract_data: memory allocation failure\n"));
				tws_disconnect(ti);
			}
		}
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

	if(version >= 6) {
		lval = sizeof(tws_string_t), read_line(ti, cdetails.d_ev_rule, &lval);
		read_double(ti, &cdetails.d_ev_multiplier);
	}

	if(version >= 5) {
		read_int(ti, &cdetails.d_sec_id_list_count);
		if (cdetails.d_sec_id_list_count > 0) {
			cdetails.d_sec_id_list = (tr_tag_value_t *)calloc(cdetails.d_sec_id_list_count, sizeof(*cdetails.d_sec_id_list));
			if(cdetails.d_sec_id_list) {
				int j;
				for (j = 0; j < cdetails.d_sec_id_list_count; j++) {
					cdetails.d_sec_id_list[j].t_tag = alloc_string(ti);
					cdetails.d_sec_id_list[j].t_val = alloc_string(ti);
					lval = sizeof(tws_string_t); read_line(ti, cdetails.d_sec_id_list[j].t_tag, &lval);
					lval = sizeof(tws_string_t); read_line(ti, cdetails.d_sec_id_list[j].t_val, &lval);
				}
			} else {
				TWS_DEBUG_PRINTF((ti->opaque, "receive_bond_contract_data: memory allocation failure\n"));
				tws_disconnect(ti);
			}
		}
	}

    if(ti->connected)
        event_bond_contract_details(ti->opaque, req_id, &cdetails);

    destroy_contract_details(ti, &cdetails);
}

static void receive_execution_data(tws_instance_t *ti)
{
    tr_contract_t contract;
    tr_execution_t exec;
    size_t lval;
    int ival, version, orderid, req_id = -1;

    tws_init_contract(ti, &contract);
    init_execution(ti, &exec);

    read_int(ti, &ival), version = ival;

    if(version >= 7)
        read_int(ti, &ival), req_id = ival;

    read_int(ti, &ival), orderid = ival;

    if(version >= 5)
        read_int(ti, &contract.c_conid);

    lval = sizeof(tws_string_t), read_line(ti, contract.c_symbol, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_sectype, &lval);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_expiry, &lval);
    read_double(ti, &contract.c_strike);
    lval = sizeof(tws_string_t), read_line(ti, contract.c_right, &lval);
	if(version >= 9) {
	    lval = sizeof(tws_string_t), read_line(ti, contract.c_multiplier, &lval);
	}
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

    if (version >= 8) {
	    lval = sizeof(tws_string_t), read_line(ti, exec.e_orderref, &lval);
    }

	if(version >= 9) {
		lval = sizeof(tws_string_t), read_line(ti, exec.e_ev_rule, &lval);
		read_double(ti, &exec.e_ev_multiplier);
	}

    if(ti->connected)
        event_exec_details(ti->opaque, req_id, &contract, &exec);

    tws_destroy_contract(ti, &contract);
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
    int ival;
	tr_fa_msg_type_t fadata_type;

    read_int(ti, &ival); /*version*/
    read_int(ti, &ival), fadata_type = (tr_fa_msg_type_t)ival;

    xml = str = alloc_string(ti);
    read_line_of_arbitrary_length(ti, &xml, sizeof(tws_string_t)); /* xml */

    if(ti->connected) {
        event_receive_fa(ti->opaque, fadata_type, xml);
    }

    if (xml != str)
        free(xml);
    free_string(ti, str);
}

static void receive_historical_data(tws_instance_t *ti)
{
    double open, high, low, close, wap;
    int j;
    size_t lval;
    int ival, version, req_id, item_count, gaps, bar_count;
	long int volume;
    char *date = alloc_string(ti), *has_gaps = alloc_string(ti), *completion_from = alloc_string(ti), *completion_to = alloc_string(ti);

    read_int(ti, &ival), version = ival;
    read_int(ti, &ival), req_id = ival;

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
        read_long(ti, &volume);
        read_double(ti, &wap);
        lval = sizeof(tws_string_t); read_line(ti, has_gaps, &lval);
        gaps = !!strncasecmp(has_gaps, "false", 5);

        if(version >= 3)
            read_int(ti, &ival), bar_count = ival;
        else
            bar_count = -1;

        if(ti->connected)
            event_historical_data(ti->opaque, req_id, date, open, high, low, close, volume, bar_count, wap, gaps);

    }
    /* send end of dataset marker */
    if(ti->connected)
        event_historical_data_end(ti->opaque, req_id, completion_from, completion_to);

    free_string(ti, date);
    free_string(ti, has_gaps);
    free_string(ti, completion_from);
    free_string(ti, completion_to);
}

static void receive_scanner_parameters(tws_instance_t *ti)
{
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

static void receive_scanner_data(tws_instance_t *ti)
{
    tr_contract_details_t cdetails;
    char *distance = alloc_string(ti), *benchmark = alloc_string(ti), *projection = alloc_string(ti);
    size_t lval;
    int j;
    int ival, version, rank, ticker_id, num_elements;

    init_contract_details(ti, &cdetails);

    read_int(ti, &ival), version = ival;
    read_int(ti, &ival), ticker_id = ival;
    read_int(ti, &ival), num_elements = ival;

    if(ti->connected)
        event_scanner_data_start(ti->opaque, ticker_id, num_elements);

    for(j = 0; j < num_elements; j++) {
        char *legs_str = NULL;

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
        event_scanner_data_end(ti->opaque, ticker_id, num_elements);

    destroy_contract_details(ti, &cdetails);
    free_string(ti, distance);
    free_string(ti, benchmark);
    free_string(ti, projection);
}

static void receive_current_time(tws_instance_t *ti)
{
    long time;
    int ival;

    read_int(ti, &ival /*version unused */);
    read_long(ti, &time);

    if(ti->connected)
        event_current_time(ti->opaque, time);
}

static void receive_realtime_bars(tws_instance_t *ti)
{
    long time, volume;
    double open, high, low, close, wap;
    int ival, req_id, count;

    read_int(ti, &ival); /* version unused */
    read_int(ti, &req_id);
    read_long(ti, &time);
    read_double(ti, &open);
    read_double(ti, &high);
    read_double(ti, &low);
    read_double(ti, &close);
    read_long(ti, &volume);
    read_double(ti, &wap);
    read_int(ti, &count);

    if(ti->connected)
        event_realtime_bar(ti->opaque, req_id, time, open, high, low, close, volume, wap, count);
}

static void receive_fundamental_data(tws_instance_t *ti)
{
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

static void receive_contract_data_end(tws_instance_t *ti)
{
    int ival, req_id;

    read_int(ti, &ival); /* version ignored */
    read_int(ti, &ival), req_id = ival;

    if(ti->connected)
        event_contract_details_end(ti->opaque, req_id);
}

static void receive_open_order_end(tws_instance_t *ti)
{
    int ival;

    read_int(ti, &ival); /* version ignored */
    if(ti->connected)
        event_open_order_end(ti->opaque);
}

static void receive_acct_download_end(tws_instance_t *ti)
{
    char acct_name[200];
    size_t lval = sizeof acct_name;
    int ival;

    read_int(ti, &ival); /* version ignored */
    read_line(ti, acct_name, &lval);

    if(ti->connected)
        event_acct_download_end(ti->opaque, acct_name);
}

static void receive_execution_data_end(tws_instance_t *ti)
{
    int ival, req_id;

    read_int(ti, &ival); /* version ignored */
    read_int(ti, &ival); req_id = ival;

    if(ti->connected)
        event_exec_details_end(ti->opaque, req_id);
}

static void receive_delta_neutral_validation(tws_instance_t *ti)
{
    under_comp_t und;
    int ival, req_id;

    read_int(ti, &ival); /* version ignored */
    read_int(ti, &ival); req_id = ival;

    read_int(ti, &und.u_conid);
    read_double(ti, &und.u_delta);
    read_double(ti, &und.u_price);

    if(ti->connected)
        event_delta_neutral_validation(ti->opaque, req_id, &und);
}

static void receive_tick_snapshot_end(tws_instance_t *ti)
{
    int ival, req_id;

    read_int(ti, &ival); /* version ignored */
    read_int(ti, &ival); req_id = ival;

    if(ti->connected)
        event_tick_snapshot_end(ti->opaque, req_id);
}

static void receive_market_data_type(tws_instance_t *ti)
{
	int ival, req_id;
	market_data_type_t market_type;

    read_int(ti, &ival); /* version ignored */
    read_int(ti, &ival); req_id = ival;
    read_int(ti, &ival); market_type = (market_data_type_t)ival;

    if(ti->connected)
        event_market_data_type(ti->opaque, req_id, market_type);
}

static void receive_commission_report(tws_instance_t *ti)
{
	int ival;
	size_t lval;
	tr_commission_report_t report = {0};

	report.cr_exec_id = alloc_string(ti);
	report.cr_currency = alloc_string(ti);

    read_int(ti, &ival); /* version ignored */
	lval = sizeof(tws_string_t), read_line(ti, report.cr_exec_id, &lval);
	read_double(ti, &report.cr_commission);
	lval = sizeof(tws_string_t), read_line(ti, report.cr_currency, &lval);
	read_double(ti, &report.cr_realized_pnl);
	read_double(ti, &report.cr_yield);
	read_int(ti, &ival); report.cr_yield_redemption_date = ival;

    if(ti->connected)
        event_commission_report(ti->opaque, &report);
}


/* allows for reading events from within the same thread or an externally
 * spawned thread, returns 0 on success, -1 on error,
 */
int tws_event_process(tws_instance_t *ti)
{
    int ival;
    int valid = 1;
    tws_incoming_id_t msgcode;

	if (ti->rx_observe) {
		ti->rx_observe(ti, NULL, 0, 0);
	}

    read_int(ti, &ival);
    msgcode = (tws_incoming_id_t)ival;

    TWS_DEBUG_PRINTF((ti->opaque, "\nreceived id=%d, name=%s\n", (int)msgcode, tws_incoming_msg_name(msgcode)));

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
    case MARKET_DATA_TYPE: receive_market_data_type(ti); break;
	case COMMISSION_REPORT: receive_commission_report(ti); break;
    default: valid = 0; break;
    }

    return valid ? 0 : -1;
}

/* caller supplies start_thread method */
tws_instance_t *tws_create(void *opaque, tws_transmit_func_t *transmit, tws_receive_func_t *receive, tws_flush_func_t *flush, tws_open_func_t *open, tws_close_func_t *close, tws_transmit_element_func_t *tx_listener, tws_receive_element_func_t *rx_listener)
{
    tws_instance_t *ti = (tws_instance_t *) calloc(1, sizeof *ti);
    if (ti)
    {
        ti->opaque = opaque;
        ti->transmit = transmit;
        ti->receive = receive;
        ti->flush = flush;
        ti->open = open;
        ti->close = close;

		ti->tx_observe = tx_listener;
		ti->rx_observe = rx_listener;

        reset_io_buffers(ti);
    }

    return ti;
}

void tws_destroy(tws_instance_t *ti)
{
    tws_disconnect(ti);

    free(ti);
}

/* perform output buffering */
static int send_blob(tws_instance_t *ti, const char *src, size_t srclen)
{
    size_t len = sizeof(ti->tx_buf) - ti->tx_buf_next;
    int err = 0;

    if (ti->connected) {
		if (ti->tx_observe) {
			ti->tx_observe(ti, src, srclen, 0);
		}

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
	else {
		err = -1;
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
static int read_line(tws_instance_t *ti, char *line, size_t *len_ref)
{
    size_t j = 0;
	size_t len = *len_ref;
    int nread = -1, err = -1;

	*len_ref = 0;
    if (line == NULL) {
        TWS_DEBUG_PRINTF((ti->opaque, "read_line: line buffer is NULL\n"));
        goto out;
    }

    line[0] = '\0';
    for(j = 0; j < len; j++) {
        nread = read_char(ti);
        if(nread < 0) {
            TWS_DEBUG_PRINTF((ti->opaque, "read_line: going out 1, nread=%d\n", nread));
            goto out;
        }
        line[j] = (char) nread;
        if(line[j] == '\0')
            break;
    }

    if(j == len) {
        TWS_DEBUG_PRINTF((ti->opaque, "read_line: going out 2 j=%ld\n", j));
        goto out;
    }

    TWS_DEBUG_PRINTF((ti->opaque, "read_line: i read %s\n", line));
    *len_ref = j;
    err = 0;
out:
    if(err < 0) {
        if(nread > 0) {
            TWS_DEBUG_PRINTF((ti->opaque, "read_line: corruption happened (string longer than max)\n"));
        }
        // always close the connection in buffer overflow / error conditions; the next element fetch will be corrupt anyway and this way we prevent nasty surprises downrange.
        tws_disconnect(ti);
    }

	if (ti->rx_observe) {
		ti->rx_observe(ti, line, j, err);
	}

    return err;
}

/*
When fetching a parameter string value of arbitrary length, we return a pointer to
space allocated on the heap (we only use the string memory pool for shorter strings
as that one is size limited and (in its entirety!) too small for several messages.

We don't want to take a buffer overflow risk like that any more, so we fix this by
allowing arbitrary string length for this parameter type only.
*/
static int read_line_of_arbitrary_length(tws_instance_t *ti, char **val, size_t alloc_size)
{
    size_t j = 0;
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
            TWS_DEBUG_PRINTF((ti->opaque, "read_line_of_arbitrary_length: going out 0, heap alloc failure\n"));
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
                TWS_DEBUG_PRINTF((ti->opaque, "read_line_of_arbitrary_length: going out 1, heap alloc failure\n"));
                goto out;
            }
        }
        nread = read_char(ti);
        if(nread < 0) {
            TWS_DEBUG_PRINTF((ti->opaque, "read_line: going out 1, nread=%d\n", nread));
            goto out;
        }
        line[j] = (char) nread;
        if(line[j] == '\0')
            break;
    }

#undef MIN
#define MIN(a, b)		((a) < (b) ? (a) : (b))
    TWS_DEBUG_PRINTF((ti->opaque, "read_line: i read (len: %d) %.*s%s\n", (int)strlen(line), MIN((int)strlen(line), 500), line, (strlen(line) > 500 ? "(...)" : "")));
    *val = line;
    err = 0;
out:
    if(err < 0) {
        if(nread > 0) {
            TWS_DEBUG_PRINTF((ti->opaque, "read_line: corruption happened (string longer than max)\n"));
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

	if (ti->rx_observe) {
		ti->rx_observe(ti, line, j, err);
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

    /* return an impossibly large negative number on error to fail careless callers */
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
static int read_long(tws_instance_t *ti, long int *val)
{
    char line[3 * sizeof *val];
    size_t len = sizeof line;
    int err = read_line(ti, line, &len);

    /* return an impossibly large negative number on error to fail careless callers */
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
int tws_connect(tws_instance_t *ti, int client_id)
{
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

int tws_connected(tws_instance_t *ti)
{
    return ti->connected;
}

void  tws_disconnect(tws_instance_t *ti)
{
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
int tws_req_scanner_parameters(tws_instance_t *ti)
{
    if(ti->server_version < 24) return UPDATE_TWS;

	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_SCANNER_PARAMETERS);
	}

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
int tws_req_scanner_subscription(tws_instance_t *ti, int ticker_id, const tr_scanner_subscription_t *s)
{
    if(ti->server_version < 24) return UPDATE_TWS;

	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_SCANNER_SUBSCRIPTION);
	}

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
int tws_cancel_scanner_subscription(tws_instance_t *ti, int ticker_id)
{
    if(ti->server_version < 24) return UPDATE_TWS;

    if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, CANCEL_SCANNER_SUBSCRIPTION);
	}

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

static void send_combolegs(tws_instance_t *ti, const tr_contract_t *contract, const send_combolegs_mode mode)
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
Return 0 on error, !0 on successfully sending the tag list (a TagValue Vector in the original JAVA code).
*/
static int send_tag_list(tws_instance_t *ti, const tr_tag_value_t *list, int list_size)
{
    send_int(ti, list_size);
    if(list_size > 0) {
        int j;
        if (list == NULL) {
            TWS_DEBUG_PRINTF((ti->opaque, "send_tag_list: Algo Params array has not been properly set up: array is NULL\n"));
			return 0;
        }
        else {
            for(j = 0; j < list_size; j++) {
                if (list[j].t_tag == NULL) {
                    TWS_DEBUG_PRINTF((ti->opaque, "send_tag_list: Algo Params array has not been properly set up: tag is NULL\n"));
                    return 0;
                }
                send_str(ti, list[j].t_tag);
                send_str(ti, list[j].t_val);
            }
        }
    }
	return 1;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqMktData(int tickerId, Contract contract,
            String genericTickList, boolean snapshot) {

Starting with API version 9.0 (client version 30), version 6 of the 
REQ_MKT_DATA message is sent containing a new field that specifies the 
requested ticks as a list of comma-delimited integer Ids (generic tick types). 
Requests for these ticks will be answered if the tick type requested pertains 
to the contract at issue. 

Note that Generic Tick Tags cannot be specified if you elect to use the 
Snapshot market data subscription.

The generic market data tick types are:

Integer			Tick Type									Resulting 
ID Value													Tick Value
 
100		Option Volume (currently for stocks)				29, 30
 
101		Option Open Interest (currently for stocks)			27, 28
 
104		Historical Volatility (currently for stocks)		23
 
105     ?                                                   ?

106		Option Implied Volatility (currently for stocks)	24
 
107     ?                                                   ?

162		Index Future Premium								31
 
165		Miscellaneous Stats									15, 16, 17, 18, 19, 20, 21
 
221		Mark Price (used in TWS P&L computations)			37
 
225		Auction values (volume, price and imbalance)		34, 35, 36
 
232     ?                                                   ?

233		RTVolume											48
 
236		Shortable											46
 
256		Inventory											-
 
258		Fundamental Ratios									47
 
293     ?                                                   ?

294     ?                                                   ?

295     ?                                                   ?

318     ?                                                   ?

411		Real-time Historical Volatility						58

mdoff   mdoff blocks regular market data so that only       -
        specified generic ticks are received. In response 
		to a '233,mdoff' request, pure RTVolume data will 
		be returned without any additional records.
*/
int tws_req_mkt_data(tws_instance_t *ti, int ticker_id, const tr_contract_t *contract, const char generic_tick_list[], int snapshot)
{
    if(ti->server_version < MIN_SERVER_VER_SNAPSHOT_MKT_DATA && snapshot) {
        TWS_DEBUG_PRINTF((ti->opaque, "tws_req_mkt_data does not support snapshot market data requests\n"));
        return UPDATE_TWS;
    }

    if(ti->server_version < MIN_SERVER_VER_UNDER_COMP) {
        if(contract->c_undercomp) {
            TWS_DEBUG_PRINTF((ti->opaque, "tws_req_mkt_data does not support delta neutral orders\n"));
            return UPDATE_TWS;
        }
    }

    if (ti->server_version < MIN_SERVER_VER_REQ_MKT_DATA_CONID) {
        if (contract->c_conid > 0) {
            TWS_DEBUG_PRINTF((ti->opaque, "tws_req_mkt_data does not support conId parameter\n"));
            return UPDATE_TWS;
        }
    }

	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_MKT_DATA);
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
int tws_req_historical_data(tws_instance_t *ti, int ticker_id, const tr_contract_t *contract, const char end_date_time[], const char duration_str[], const char bar_size_setting[], const char what_to_show[], int use_rth, int format_date)
{
    if(ti->server_version < 16)
        return UPDATE_TWS;

	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_HISTORICAL_DATA);
	}

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
int tws_cancel_historical_data(tws_instance_t *ti, int ticker_id)
{
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
int tws_req_contract_details(tws_instance_t *ti, int req_id, const tr_contract_t *contract)
{
    /* This feature is only available for versions of TWS >=4 */
    if(ti->server_version < 4)
        return UPDATE_TWS;

    if(ti->server_version < MIN_SERVER_VER_SEC_ID_TYPE) {
        if(!IS_EMPTY(contract->c_secid_type) || !IS_EMPTY(contract->c_secid)) {
            TWS_DEBUG_PRINTF((ti->opaque, "tws_req_contract_details: it does not support secIdType and secId parameters."));
            return UPDATE_TWS;
        }
    }

	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_CONTRACT_DATA);
	}

    /* send req mkt data msg */
    send_int(ti, REQ_CONTRACT_DATA);
    send_int(ti, 6 /*VERSION*/);

    if(ti->server_version >= MIN_SERVER_VER_CONTRACT_DATA_CHAIN)
        send_int(ti, req_id);

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
int tws_req_mkt_depth(tws_instance_t *ti, int ticker_id, const tr_contract_t *contract, int num_rows)
{
    /* This feature is only available for versions of TWS >=6 */
    if(ti->server_version < 6)
        return UPDATE_TWS;

	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_MKT_DEPTH);
	}

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
int tws_cancel_mkt_data(tws_instance_t *ti, int ticker_id)
{
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
int tws_cancel_mkt_depth(tws_instance_t *ti, int ticker_id)
{
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
int tws_exercise_options(tws_instance_t *ti, int ticker_id, const tr_contract_t *contract, int exercise_action, int exercise_quantity, const char account[], int exc_override)
{
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
int tws_place_order(tws_instance_t *ti, int id, const tr_contract_t *contract, const tr_order_t *order)
{
    int vol26 = 0, version;

    if(ti->server_version < MIN_SERVER_VER_SCALE_ORDERS) {
        if(order->o_scale_init_level_size != INTEGER_MAX_VALUE ||
            DBL_NOTMAX(order->o_scale_price_increment)) {
                TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order: It does not support Scale orders\n"));
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
                        TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order does not support SSHORT flag for combo legs\n"));
                        return UPDATE_TWS;
                }
            }
        }
    }

    if(ti->server_version < MIN_SERVER_VER_WHAT_IF_ORDERS) {
        if(order->o_whatif) {
            TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order does not support what-if orders\n"));
            return UPDATE_TWS;
        }
    }

    if(ti->server_version < MIN_SERVER_VER_UNDER_COMP) {
        if(contract->c_undercomp) {
            TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order does not support delta-neutral orders\n"));
            return UPDATE_TWS;
        }
    }

    if(ti->server_version < MIN_SERVER_VER_SCALE_ORDERS2) {
        if(order->o_scale_subs_level_size != INTEGER_MAX_VALUE) {
            TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order does not support Subsequent Level Size for Scale orders\n"));
            return UPDATE_TWS;
        }
    }

    if(ti->server_version < MIN_SERVER_VER_ALGO_ORDERS) {
        if (!IS_EMPTY(order->o_algo_strategy)) {
            TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order: It does not support algo orders\n"));
            return UPDATE_TWS;
        }
    }

    if(ti->server_version < MIN_SERVER_VER_NOT_HELD) {
        if (order->o_not_held) {
            TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order: It does not support notHeld parameter\n"));
            return UPDATE_TWS;
        }
    }

    if (ti->server_version < MIN_SERVER_VER_SEC_ID_TYPE) {
        if(!IS_EMPTY(contract->c_secid_type) || !IS_EMPTY(contract->c_secid)) {
            TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order: It does not support secIdType and secId parameters\n"));
            return UPDATE_TWS;
        }
    }

    if (ti->server_version < MIN_SERVER_VER_PLACE_ORDER_CONID) {
        if (contract->c_conid > 0) {
            TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order: It does not support conId parameter\n"));
            return UPDATE_TWS;
        }
    }

    if (ti->server_version < MIN_SERVER_VER_SSHORTX) {
        if (order->o_exempt_code != -1) {
            TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order: It does not support exemptCode parameter\n"));
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
                    TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order: It does not support exemptCode parameter\n"));
                    return UPDATE_TWS;
                }
            }
        }
    }

	if (ti->server_version < MIN_SERVER_VER_HEDGE_ORDERS) {
		if (!IS_EMPTY(order->o_hedge_type)) {
            TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order: It does not support hedge orders\n"));
            return UPDATE_TWS;
		}
	}

    if (ti->server_version < MIN_SERVER_VER_HEDGE_ORDERS) {
        if (!IS_EMPTY(order->o_hedge_type)) {
            TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order: It does not support hedge orders.\n"));
            return UPDATE_TWS;
        }
    }
        
    if (ti->server_version < MIN_SERVER_VER_OPT_OUT_SMART_ROUTING) {
        if (order->o_opt_out_smart_routing) {
            TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order: It does not support optOutSmartRouting parameter.\n"));
            return UPDATE_TWS;
        }
    }
        
    if (ti->server_version < MIN_SERVER_VER_DELTA_NEUTRAL_CONID) {
        if (order->o_delta_neutral_con_id > 0 
        		|| !IS_EMPTY(order->o_delta_neutral_settling_firm)
        		|| !IS_EMPTY(order->o_delta_neutral_clearing_account)
        		|| !IS_EMPTY(order->o_delta_neutral_clearing_intent)
        		) {
            TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order: It does not support deltaNeutral parameters: ConId, SettlingFirm, ClearingAccount, ClearingIntent\n"));
            return UPDATE_TWS;
        }
    }

    if (ti->server_version < MIN_SERVER_VER_SCALE_ORDERS3) {
        if (order->o_scale_price_increment > 0 && DBL_NOTMAX(order->o_scale_price_increment)) {
        	if (DBL_NOTMAX(order->o_scale_price_adjust_value) ||
        		order->o_scale_price_adjust_interval != INTEGER_MAX_VALUE ||
        		DBL_NOTMAX(order->o_scale_profit_offset) ||
        		order->o_scale_auto_reset ||
        		order->o_scale_init_position != INTEGER_MAX_VALUE ||
        		order->o_scale_init_fill_qty != INTEGER_MAX_VALUE ||
        		order->o_scale_random_percent) {
		        TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order: It does not support Scale order parameters: PriceAdjustValue, PriceAdjustInterval, ProfitOffset, AutoReset, InitPosition, InitFillQty and RandomPercent\n"));
				return UPDATE_TWS;
			}
		}
    }
        
    if (ti->server_version < MIN_SERVER_VER_ORDER_COMBO_LEGS_PRICE && !strcasecmp(contract->c_sectype, "BAG")) {
        if (order->o_combo_legs_count > 0) {
			int j;

        	for (j = 0; j < order->o_combo_legs_count; j++) {
				tr_order_combo_leg_t *leg = &order->o_combo_legs[j];

        		if (DBL_NOTMAX(leg->cl_price)) {
			        TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order: It does not support per-leg prices for order combo legs.\n"));
					return UPDATE_TWS;
				}
			}
        }
    }
        
    if (ti->server_version < MIN_SERVER_VER_TRAILING_PERCENT) {
        if (DBL_NOTMAX(order->o_trailing_percent)) {
			TWS_DEBUG_PRINTF((ti->opaque, "tws_place_order: It does not support trailing percent parameter\n"));
			return UPDATE_TWS;
        }
    }
        
    send_int(ti, PLACE_ORDER);
    version = ti->server_version < MIN_SERVER_VER_NOT_HELD ? 27 : 38;
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
    if (ti->server_version < MIN_SERVER_VER_ORDER_COMBO_LEGS_PRICE) {
	    send_double(ti, DBL_NOTMAX(order->o_lmt_price) ? order->o_lmt_price : 0);
    }
    else {
	    send_double_max(ti, order->o_lmt_price);
    }
    if (ti->server_version < MIN_SERVER_VER_TRAILING_PERCENT) {
	    send_double(ti, DBL_NOTMAX(order->o_aux_price) ? order->o_aux_price : 0);
    }
    else {
	    send_double_max(ti, order->o_aux_price);
    }

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

    // Send order combo legs for BAG requests
    if(ti->server_version >= MIN_SERVER_VER_ORDER_COMBO_LEGS_PRICE && !strcasecmp(contract->c_sectype, "BAG")) {
		int j;

        send_int(ti, order->o_combo_legs_count);
        for (j = 0; j < order->o_combo_legs_count; j++) {
            tr_order_combo_leg_t *leg = &order->o_combo_legs[j];

			send_double_max(ti, leg->cl_price);
        }
    }

    if(ti->server_version >= MIN_SERVER_VER_SMART_COMBO_ROUTING_PARAMS && !strcasecmp(contract->c_sectype, "BAG")) {
		if (!send_tag_list(ti, order->o_smart_combo_routing_params, order->o_smart_combo_routing_params_count)) {
            // we may already have sent part of the constructed message so play it safe and discard the connection!
            tws_disconnect(ti);
		}
    }

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
                   
            if (ti->server_version >= MIN_SERVER_VER_DELTA_NEUTRAL_CONID && !IS_EMPTY(order->o_delta_neutral_order_type)) {
                send_int_max(ti, order->o_delta_neutral_con_id);
                send_str(ti, order->o_delta_neutral_settling_firm);
                send_str(ti, order->o_delta_neutral_clearing_account);
                send_str(ti, order->o_delta_neutral_clearing_intent);
            }
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

    if( ti->server_version >= MIN_SERVER_VER_TRAILING_PERCENT) {
        send_double_max(ti, order->o_trailing_percent);
    }
           
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

    if (ti->server_version >= MIN_SERVER_VER_SCALE_ORDERS3 && order->o_scale_price_increment > 0.0 && DBL_NOTMAX(order->o_scale_price_increment)) {
        send_double_max(ti, order->o_scale_price_adjust_value);
        send_int_max(ti, order->o_scale_price_adjust_interval);
        send_double_max(ti, order->o_scale_profit_offset);
        send_boolean(ti, order->o_scale_auto_reset);
        send_int_max(ti, order->o_scale_init_position);
        send_int_max(ti, order->o_scale_init_fill_qty);
        send_boolean(ti, order->o_scale_random_percent);
    }

	// HEDGE orders
	if (ti->server_version >= MIN_SERVER_VER_HEDGE_ORDERS) {
		send_str(ti, order->o_hedge_type);
		if (!IS_EMPTY(order->o_hedge_type)) {
			send_str(ti, order->o_hedge_param);
		}
	}

    if (ti->server_version >= MIN_SERVER_VER_OPT_OUT_SMART_ROUTING) {
        send_boolean(ti, order->o_opt_out_smart_routing);
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
            if (!send_tag_list(ti, order->o_algo_params, order->o_algo_params_count)) {
                // we may already have sent part of the constructed message so play it safe and discard the connection!
                tws_disconnect(ti);
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
int tws_req_account_updates(tws_instance_t *ti, int subscribe, const char acct_code[])
{
	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_ACCOUNT_DATA);
	}

    send_int(ti, REQ_ACCOUNT_DATA);
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
int tws_req_executions(tws_instance_t *ti, int req_id, const tr_exec_filter_t *filter)
{
	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_EXECUTIONS);
	}

    send_int(ti, REQ_EXECUTIONS);
    send_int(ti, 3 /*VERSION*/);
    if(ti->server_version >= MIN_SERVER_VER_EXECUTION_DATA_CHAIN)
        send_int(ti, req_id);

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
int tws_cancel_order(tws_instance_t *ti, int order_id)
{
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
int tws_req_open_orders(tws_instance_t *ti)
{
	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_OPEN_ORDERS);
	}

    send_int(ti, REQ_OPEN_ORDERS);
    send_int(ti, 1 /*VERSION*/);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_OORDER;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqIds( int numIds) {
*/
int tws_req_ids(tws_instance_t *ti, int numids)
{
	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_IDS);
	}

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
int tws_req_news_bulletins(tws_instance_t *ti, int allmsgs)
{
	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_NEWS_BULLETINS);
	}

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
int tws_cancel_news_bulletins(tws_instance_t *ti)
{
    send_int(ti, CANCEL_NEWS_BULLETINS);
    send_int(ti, 1 /*VERSION*/);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_CBULLETINS;
}

/*
similar to IB/TWS Java method:

    public synchronized void setServerLogLevel(int logLevel) {
*/
int tws_set_server_log_level(tws_instance_t *ti, int level)
{
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
int tws_req_auto_open_orders(tws_instance_t *ti, int auto_bind)
{
	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_AUTO_OPEN_ORDERS);
	}

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
int tws_req_all_open_orders(tws_instance_t *ti)
{
	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_ALL_OPEN_ORDERS);
	}

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
int tws_req_managed_accts(tws_instance_t *ti)
{
	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_MANAGED_ACCTS);
	}

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
int tws_request_fa(tws_instance_t *ti, tr_fa_msg_type_t fa_data_type)
{
    /* This feature is only available for versions of TWS >= 13 */
    if(ti->server_version < 13)
        return UPDATE_TWS;

	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_FA);
	}

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
int tws_replace_fa(tws_instance_t *ti, tr_fa_msg_type_t fa_data_type, const char xml[])
{
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
int tws_req_current_time(tws_instance_t *ti)
{
    if(ti->server_version < 33) {
        return UPDATE_TWS;
    }

	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_CURRENT_TIME);
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
int tws_req_fundamental_data(tws_instance_t *ti, int req_id, const tr_contract_t *contract, const char report_type[])
{
    if(ti->server_version < MIN_SERVER_VER_FUNDAMENTAL_DATA) {
        TWS_DEBUG_PRINTF((ti->opaque, "tws_req_fundamental_data does not support fundamental data requests"));
        return UPDATE_TWS;
    }

	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_FUNDAMENTAL_DATA);
	}

    send_int(ti, REQ_FUNDAMENTAL_DATA);
    send_int(ti, 1 /* version */);
    send_int(ti, req_id);
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
int tws_cancel_fundamental_data(tws_instance_t *ti, int req_id)
{
    if(ti->server_version < MIN_SERVER_VER_FUNDAMENTAL_DATA) {
        TWS_DEBUG_PRINTF((ti->opaque, "tws_cancel_fundamental data does not support fundamental data requests."));
        return UPDATE_TWS;
    }

    send_int(ti, CANCEL_FUNDAMENTAL_DATA);
    send_int(ti, 1 /*version*/);
    send_int(ti, req_id);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_CANFUNDDATA;
}

/*
similar to IB/TWS Java method:

    public synchronized void calculateImpliedVolatility(int reqId, Contract contract,
            double optionPrice, double underPrice) {
*/
int tws_calculate_implied_volatility(tws_instance_t *ti, int req_id, const tr_contract_t *contract, double option_price, double under_price)
{
    if (ti->server_version < MIN_SERVER_VER_REQ_CALC_IMPLIED_VOLAT) {
        TWS_DEBUG_PRINTF((ti->opaque, "tws_calculate_implied_volatility: It does not support calculate implied volatility requests\n"));
        return UPDATE_TWS;
    }

	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_CALC_IMPLIED_VOLAT);
	}

    // send calculate implied volatility msg
    send_int(ti, REQ_CALC_IMPLIED_VOLAT);
    send_int(ti, 1 /*version*/);
    send_int(ti, req_id);

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
int tws_cancel_calculate_implied_volatility(tws_instance_t *ti, int req_id)
{
    if (ti->server_version < MIN_SERVER_VER_CANCEL_CALC_IMPLIED_VOLAT) {
        TWS_DEBUG_PRINTF((ti->opaque, "tws_cancel_calculate_implied_volatility: It does not support calculate implied volatility cancellation\n"));
        return UPDATE_TWS;
    }

    // send cancel calculate implied volatility msg
    send_int(ti, CANCEL_CALC_IMPLIED_VOLAT);
    send_int(ti, 1 /*version*/);
    send_int(ti, req_id);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_CANCALCIMPLIEDVOLAT;
}

/*
similar to IB/TWS Java method:

    public synchronized void calculateOptionPrice(int reqId, Contract contract,
            double volatility, double underPrice) {
*/
int tws_calculate_option_price(tws_instance_t *ti, int req_id, const tr_contract_t *contract, double volatility, double under_price)
{
    if (ti->server_version < MIN_SERVER_VER_REQ_CALC_OPTION_PRICE) {
        TWS_DEBUG_PRINTF((ti->opaque, "tws_calculate_option_price: It does not support calculate option price requests\n"));
        return UPDATE_TWS;
    }

	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_CALC_OPTION_PRICE);
	}

    // send calculate option price msg
    send_int(ti, REQ_CALC_OPTION_PRICE);
    send_int(ti, 1 /*version*/);
    send_int(ti, req_id);

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
int tws_cancel_calculate_option_price(tws_instance_t *ti, int req_id)
{
    if (ti->server_version < MIN_SERVER_VER_CANCEL_CALC_OPTION_PRICE) {
        TWS_DEBUG_PRINTF((ti->opaque, "tws_cancel_calculate_option_price: It does not support calculate option price cancellation\n"));
        return UPDATE_TWS;
    }

    // send cancel calculate option price msg
    send_int(ti, CANCEL_CALC_OPTION_PRICE);
    send_int(ti, 1 /*version*/);
    send_int(ti, req_id);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_CANCALCOPTIONPRICE;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqGlobalCancel() {
*/
int tws_req_global_cancel(tws_instance_t *ti)
{
    if (ti->server_version < MIN_SERVER_VER_REQ_GLOBAL_CANCEL) {
        TWS_DEBUG_PRINTF((ti->opaque, "tws_req_global_cancel: It does not support globalCancel requests\n"));
        return UPDATE_TWS;
    }

	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_GLOBAL_CANCEL);
	}

    // send request global cancel msg
    send_int(ti, REQ_GLOBAL_CANCEL);
    send_int(ti, 1 /*version*/);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_REQGLOBALCANCEL;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqMarketDataType(int marketDataType) {
*/
int tws_req_market_data_type(tws_instance_t *ti, const market_data_type_t market_data_type)
{
    if (ti->server_version < MIN_SERVER_VER_REQ_MARKET_DATA_TYPE) {
        TWS_DEBUG_PRINTF((ti->opaque, "tws_req_market_data_type: It does not support marketDataType requests.\n"));
        return UPDATE_TWS;
    }
        
	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_MARKET_DATA_TYPE);
	}

    // send the reqMarketDataType message
    send_int(ti, REQ_MARKET_DATA_TYPE);
    send_int(ti, 1 /*version*/);
    send_int(ti, (int)market_data_type);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_REQMARKETDATATYPE;
}

/*
similar to IB/TWS Java method:

    public synchronized void reqRealTimeBars(int tickerId, Contract contract, int barSize, String whatToShow, boolean useRTH) {
*/
int tws_request_realtime_bars(tws_instance_t *ti, int ticker_id, const tr_contract_t *c, int bar_size, const char what_to_show[], int use_rth)
{
    if(ti->server_version < MIN_SERVER_VER_REAL_TIME_BARS)
        return UPDATE_TWS;

	if (ti->tx_observe) {
		ti->tx_observe(ti, NULL, 0, REQ_REAL_TIME_BARS);
	}

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
int tws_cancel_realtime_bars(tws_instance_t *ti, int ticker_id)
{
    if(ti->server_version < MIN_SERVER_VER_REAL_TIME_BARS)
        return UPDATE_TWS;

    send_int(ti, CANCEL_REAL_TIME_BARS);
    send_int(ti, 1 /*VERSION*/);
    send_int(ti, ticker_id);

    flush_message(ti);

    return ti->connected ? 0 : FAIL_SEND_CANRTBARS;
}

/* returns -1 on error, who knows 0 might be valid? */
int tws_server_version(tws_instance_t *ti)
{
    return ti->connected ? (int) ti->server_version : -1;
}

/* this routine is useless if used for synchronization
 * because of clock drift, use NTP for time sync
 */
const char *tws_connection_time(tws_instance_t *ti)
{
    return ti->connected ? ti->connect_time : 0;
}

const struct twsclient_errmsg *tws_strerror(int errcode)
{
    static const struct twsclient_errmsg unknown_err = {
        UNKNOWN_TWS_ERROR, "Unknown TWS failure code."
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

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

const char *tws_incoming_msg_name(tws_incoming_id_t x)
{
	int idx = (int)x;
	
	if (idx >= 0 && idx < ARRAY_SIZE(tws_incoming_msg_names))
	{
		return tws_incoming_msg_names[idx];
	}
	return "(unknown)";
}

const char *tws_outgoing_msg_name(tws_outgoing_id_t x)
{
	int idx = (int)x;
	
	if (idx >= 0 && idx < ARRAY_SIZE(tws_outgoing_msg_names))
	{
		return tws_outgoing_msg_names[idx];
	}
	return "(unknown)";
}

const char *fa_msg_type_name(tr_fa_msg_type_t x)
{
	int idx = (int)x;
	
	if (idx >= 0 && idx < ARRAY_SIZE(fa_msg_name))
	{
		return fa_msg_name[idx];
	}
	return "(unknown)";
}

const char *tick_type_name(tr_tick_type_t x)
{
	int idx = (int)x;

	if (idx >= 0 && idx < ARRAY_SIZE(tws_tick_type_names))
	{
		return tws_tick_type_names[idx];
	}
	return "(unknown)";
}

const char *market_data_type_name(market_data_type_t x)
{
	int idx = (int)x;
	
	if (idx >= 0 && idx < ARRAY_SIZE(tws_market_data_type_name))
	{
		return tws_market_data_type_name[idx];
	}
	return "(unknown)";
}

const char *tr_comboleg_type_name(tr_comboleg_type_t x)
{
	int idx = (int)x;
	static const char *names[] = {
		"SAME", "OPEN", "CLOSE", "(unknown)"
	};
	
	if (idx >= 0 && idx < ARRAY_SIZE(names))
	{
		return names[idx];
	}
	return "(unknown)";
}

const char *tr_origin_name(tr_origin_t x)
{
	int idx = (int)x;
	static const char *names[] = {
		"CUSTOMER", "FIRM"
	};

	if (idx >= 0 && idx < ARRAY_SIZE(names))
	{
		return names[idx];
	}
	return "(unknown)";
}

const char *tr_oca_type_name(tr_oca_type_t x)
{
	int idx = (int)x;
	static const char *names[] = {
		"UNDEFINED", "CANCEL_WITH_BLOCK", "REDUCE_WITH_BLOCK", "REDUCE_NON_BLOCK"
	};
	
	if (idx >= 0 && idx < ARRAY_SIZE(names))
	{
		return names[idx];
	}
	return "(unknown)";
}

const char *tr_auction_strategy_name(tr_auction_strategy_t x)
{
	int idx = (int)x;
	static const char *names[] = {
		"UNDEFINED", "MATCH", "IMPROVEMENT", "TRANSPARENT"
	};
	
	if (idx >= 0 && idx < ARRAY_SIZE(names))
	{
		return names[idx];
	}
	return "(unknown)";
}


double get_NAN(void)
{
	return *dNAN;
}
