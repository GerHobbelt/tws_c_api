/* API users must implement some or all of these functions */
#include "twsapi.h"

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>

void event_tick_price(void *opaque, int ticker_id, tr_tick_type_t field, double price, int can_auto_execute)
{
    printf("tick_price: opaque=%p, ticker_id=%d, type=%d, price=%.2lf, can_auto=%d\n",
           opaque, ticker_id, (int)field, price, can_auto_execute);
}

void event_tick_size(void *opaque, int ticker_id, tr_tick_type_t type, int size)
{
    printf("tick_size: opaque=%p, ticker_id=%d, type=%d, size=%d\n",
           opaque, ticker_id, (int)type, size);
}

void event_tick_option_computation(void *opaque, int ticker_id, tr_tick_type_t type, double implied_vol, double delta, double opt_price, double pv_dividend, double gamma, double vega, double theta, double und_price)
{
    printf("tick option computation: opaque=%p, ticker_id=%d, type=%d, implied_vol=%f, delta=%f, opt_price=%f, pv_dividend=%f, gamma=%f, vega=%f, theta=%f, und_price=%f\n",
           opaque, ticker_id, (int)type, implied_vol, delta, opt_price, pv_dividend, gamma, vega, theta, und_price);
}

void event_tick_generic(void *opaque, int ticker_id, int type, double value)
{
    printf("tick_generic: opaque=%p, ticker_id=%d, type=%d, value=%f\n", opaque, ticker_id, type, value);
}

void event_tick_string(void *opaque, int ticker_id, int type, const char value[])
{
    printf("tick_string: opaque=%p, ticker_id=%d, type=%d, value=[%s]\n", opaque, ticker_id, type, value);
}

void event_tick_efp(void *opaque, int ticker_id, int tick_type, double basis_points, const char formatted_basis_points[], double implied_futures_price, int hold_days, const char future_expiry[], double dividend_impact, double dividends_to_expiry)
{
    printf("tick_efp: opaque=%p, ticker_id=%d, type=%d, basis_points=%f, formatted_basis_points=[%s], implied_futures_price=%f, hold_days=%d, future_expiry=[%s], dividend_impact=%f, dividends_to_expiry=%f\n", 
		opaque, ticker_id, tick_type, basis_points, formatted_basis_points, implied_futures_price, hold_days, future_expiry, dividend_impact, dividends_to_expiry);
}

void event_order_status(void *opaque, int order_id, const char status[],
                        int filled, int remaining, double avg_fill_price, int perm_id,
                        int parent_id, double last_fill_price, int client_id, const char why_held[])
{
    printf("order_status: opaque=%p, order_id=%d, filled=%d remaining %d, avg_fill_price=%f, last_fill_price=%f, why_held=[%s]\n", 
		opaque, order_id, filled, remaining, avg_fill_price, last_fill_price, why_held);
}

void event_open_order(void *opaque, int order_id, const tr_contract_t *contract, const tr_order_t *order, const tr_order_status_t *ostatus)
{
    /* commission values might be DBL_MAX */
    if(fabs(ostatus->ost_commission - DBL_MAX) < DBL_EPSILON)
        printf("open_order: commission not reported\n");
    else
        printf("open_order: commission for %d was %.4lf\n", order_id, ostatus->ost_commission);

    printf("open_order: opaque=%p, order_id=%d, sym=%s\n", opaque, order_id, contract->c_symbol);
}

void event_update_account_value(void *opaque, const char key[], const char val[],
                                const char currency[], const char account_name[])
{
    printf("update_account_value: %p, key=%s val=%s, currency=%s, name=%s\n",
           opaque, key, val, currency, account_name);
}

void event_update_portfolio(void *opaque, const tr_contract_t *contract, int position,
                            double mkt_price, double mkt_value, double average_cost,
                            double unrealized_pnl, double realized_pnl, const char account_name[])
{
    printf("update_portfolio: %p, sym=%s, position=%d, mkt_price=%.4f, mkt_value=%.4f, avg_cost=%.4f, unrealized_pnl=%.4f, realized_pnl=%.4f name=%s\n",
           opaque, contract->c_symbol, position, mkt_price, mkt_value, average_cost, unrealized_pnl, realized_pnl, account_name);
}

void event_update_account_time(void *opaque, const char time_stamp[])
{
    printf("update_account_time: opaque=%p, time_stamp=[%s]\n", opaque, time_stamp);
}

void event_next_valid_id(void *opaque, int order_id)
{
    /* invoked once at connection establishment
     * the scope of this variable is not program wide, instance wide,
     * or even system wide. It has the same scope as the TWS account itself.
     * It appears to be persistent across TWS restarts.
     * Well behaved human and automatic TWS clients shall increment
     * this order_id atomically and cooperatively
     */
    printf("next_valid_id for order placement %d (opaque=%p)\n", order_id, opaque);
}

void event_contract_details(void *opaque, int req_id, const tr_contract_details_t *cd)
{
    printf("contract_details: opaque=%p, ...\n", opaque);
    printf("contract details: sym=%s, sectype=%s, expiry=%s, strike=%.3lf, right=%s, exch=%s, primary exch=%s, currency=%s, multiplier=%s, local_sym=%s, market_name=%s, trading_class=%s, conid=%d\n",
        cd->d_summary.c_symbol, cd->d_summary.c_sectype, cd->d_summary.c_expiry, cd->d_summary.c_strike, cd->d_summary.c_right, cd->d_summary.c_exchange, cd->d_summary.c_primary_exch,
        cd->d_summary.c_currency, cd->d_summary.c_multiplier, cd->d_summary.c_local_symbol, cd->d_market_name, cd->d_trading_class, cd->d_summary.c_conid);
    printf("contract details: min.tick: %f, coupon: %f, order types: %s, valid exch: %s, cusip: %s, maturity: %s, issue_date: %s, ratings: %s, bond_type: %s, "
        "coupon_type: %s, notes: %s, long name: %s, industry: %s, category: %s, subcategory: %s, timezone: %s, trading hours: %s, liquid hours: %s, price_magnifier: %d, "
        "under_conid: %d\n",
        cd->d_mintick, cd->d_coupon, cd->d_order_types, cd->d_valid_exchanges, cd->d_cusip, cd->d_maturity, cd->d_issue_date, cd->d_ratings, cd->d_bond_type,
        cd->d_coupon_type, cd->d_notes, cd->d_long_name, cd->d_industry, cd->d_category, cd->d_subcategory, cd->d_timezone_id, cd->d_trading_hours, cd->d_liquid_hours,
        cd->d_price_magnifier, cd->d_under_conid);
}

void event_bond_contract_details(void *opaque, int req_id, const tr_contract_details_t *cd)
{
    printf("bond_contract_details: opaque=%p, ...\n", opaque);
    printf("bond contract details: sym=%s, sectype=%s, expiry=%s, strike=%.3lf, right=%s, exch=%s, primary exch=%s, currency=%s, multiplier=%s, local_sym=%s, market_name=%s, trading_class=%s, conid=%d\n",
        cd->d_summary.c_symbol, cd->d_summary.c_sectype, cd->d_summary.c_expiry, cd->d_summary.c_strike, cd->d_summary.c_right, cd->d_summary.c_exchange, cd->d_summary.c_primary_exch,
        cd->d_summary.c_currency, cd->d_summary.c_multiplier, cd->d_summary.c_local_symbol, cd->d_market_name, cd->d_trading_class, cd->d_summary.c_conid);
    printf("bond contract details: min.tick: %f, coupon: %f, order types: %s, valid exch: %s, cusip: %s, maturity: %s, issue_date: %s, ratings: %s, bond_type: %s, "
        "coupon_type: %s, notes: %s, long name: %s, industry: %s, category: %s, subcategory: %s, timezone: %s, trading hours: %s, liquid hours: %s, price_magnifier: %d, "
        "under_conid: %d\n",
        cd->d_mintick, cd->d_coupon, cd->d_order_types, cd->d_valid_exchanges, cd->d_cusip, cd->d_maturity, cd->d_issue_date, cd->d_ratings, cd->d_bond_type,
        cd->d_coupon_type, cd->d_notes, cd->d_long_name, cd->d_industry, cd->d_category, cd->d_subcategory, cd->d_timezone_id, cd->d_trading_hours, cd->d_liquid_hours,
        cd->d_price_magnifier, cd->d_under_conid);
}

void event_exec_details(void *opaque, int order_id, const tr_contract_t *contract, const tr_execution_t *execution)
{
    printf("exec_details: opaque=%p, ...\n", opaque);
}

void event_error(void *opaque, int ticker_id, int error_code, const char error_string[])
{
    struct my_tws_io_info *info = (struct my_tws_io_info *)opaque;

    printf("error: opaque=%p, id=%d, error_code=%d, msg=%s\n", opaque, ticker_id, error_code, error_string);
}

void event_update_mkt_depth(void *opaque, int ticker_id, int position, int operation, int side, double price, int size)
{
    printf("update_mkt_depth: opaque=%p, ticker_id=%d, ...\n", opaque, ticker_id);
}

void event_update_mkt_depth_l2(void *opaque, int ticker_id, int position, char *market_maker, int operation, int side, double price, int size)
{
    printf("update_mkt_depth_l2: opaque=%p, ticker_id=%d, ...\n", opaque, ticker_id);
}

void event_update_news_bulletin(void *opaque, int msgid, int msg_type, const char news_msg[], const char origin_exch[])
{
    printf("update_news_bulletin: opaque=%p, ...\n", opaque);
}

void event_managed_accounts(void *opaque, const char accounts_list[])
{
    printf("managed_accounts: opaque=%p, ...\n", opaque);
}

void event_receive_fa(void *opaque, int fa_data_type, const char cxml[])
{
    printf("receive_fa: opaque=%p, fa_data_type=%d, xml='%s'\n", opaque, fa_data_type, cxml);
}

void event_historical_data(void *opaque, int reqid, const char date[], double open, double high, double low, double close, int volume, int bar_count, double wap, int has_gaps)
{
    printf("historical: opaque=%p, reqid=%d, date=%s, %.3f, %.3f, %.3f, %.3f, %d, wap=%.3f, has_gaps=%d\n", opaque, reqid, date, open, high, low, close, volume, wap, has_gaps);
}

void event_historical_data_end(void *opaque, int reqid, const char completion_from[], const char completion_to[])
{
    printf("historical list end: opaque=%p, reqid=%d, from date=%s, to date=%s\n", opaque, reqid, completion_from, completion_to);
}

void event_scanner_parameters(void *opaque, const char xml[])
{
    printf("scanner_parameters: opaque=%p, xml(len=%d)=%s\n", opaque, (int)strlen(xml), xml);
}

void event_scanner_data(void *opaque, int ticker_id, int rank, tr_contract_details_t *cd, const char distance[], const char benchmark[], const char projection[], const char legs_str[])
{
    printf("scanner_data: opaque=%p, ticker_id=%d, rank=%d, distance=%s, benchmark=%s, projection=%s, legs_str=%s\n",
           opaque, ticker_id, rank, distance, benchmark, projection, legs_str);
    printf("scanner_data details: sym=%s, sectype=%s, expiry=%s, strike=%.3lf, right=%s, exch=%s, primary exch=%s, currency=%s, multiplier=%s, local_sym=%s, market_name=%s, trading_class=%s, conid=%d\n",
        cd->d_summary.c_symbol, cd->d_summary.c_sectype, cd->d_summary.c_expiry, cd->d_summary.c_strike, cd->d_summary.c_right, cd->d_summary.c_exchange, cd->d_summary.c_primary_exch,
        cd->d_summary.c_currency, cd->d_summary.c_multiplier, cd->d_summary.c_local_symbol, cd->d_market_name, cd->d_trading_class, cd->d_summary.c_conid);
    printf("scanner_data details: min.tick: %f, coupon: %f, order types: %s, valid exch: %s, cusip: %s, maturity: %s, issue_date: %s, ratings: %s, bond_type: %s, "
        "coupon_type: %s, notes: %s, long name: %s, industry: %s, category: %s, subcategory: %s, timezone: %s, trading hours: %s, liquid hours: %s, price_magnifier: %d, "
        "under_conid: %d\n",
        cd->d_mintick, cd->d_coupon, cd->d_order_types, cd->d_valid_exchanges, cd->d_cusip, cd->d_maturity, cd->d_issue_date, cd->d_ratings, cd->d_bond_type,
        cd->d_coupon_type, cd->d_notes, cd->d_long_name, cd->d_industry, cd->d_category, cd->d_subcategory, cd->d_timezone_id, cd->d_trading_hours, cd->d_liquid_hours,
        cd->d_price_magnifier, cd->d_under_conid);

    /* it seems that NYSE traded stocks have different market_name and trading_class from NASDAQ */
}

void event_scanner_data_end(void *opaque, int ticker_id)
{
    printf("scanner_data_end: opaque=%p, ticker_id=%d\n", opaque, ticker_id);
}

void event_current_time(void *opaque, long time)
{
    char tbuf[40];
    time_t timestamp = (time_t)time;

    strftime(tbuf, sizeof(tbuf), "[%Y%m%dT%H%M%S] ", gmtime(&timestamp));

    printf("current_time: opaque=%p, time=%ld ~ '%s'\n", opaque, time, tbuf);
}

void event_realtime_bar(void *opaque, int reqid, long time, double open, double high, double low, double close, long volume, double wap, int count)
{
    printf("realtime_bar: %p reqid=%d time=%ld, ohlc=%.4lf/%.4lf/%.4lf/%.4lf, vol=%ld, wap=%.4lf, count=%d\n", opaque, reqid, time, open, high, low, close, volume, wap, count);
}

void event_fundamental_data(void *opaque, int reqid, const char data[])
{
    printf("fundamental_data: opaque=%p, reqid=%d, ...\n", opaque, reqid);
}

void event_contract_details_end(void *opaque, int reqid)
{
    printf("contract_details_end: opaque=%p, ...\n", opaque);
}

void event_open_order_end(void *opaque)
{
    printf("open_order_end: opaque=%p\n", opaque);
}

void event_delta_neutral_validation(void *opaque, int reqid, under_comp_t *und)
{
    printf("delta_neutral_validation: opaque=%p, reqid=%d, ...\n", opaque, reqid);
}

void event_acct_download_end(void *opaque, char acct_name[])
{
    printf("acct_download_end: opaque=%p, account name='%s'\n", opaque, acct_name);
}

void event_exec_details_end(void *opaque, int reqid)
{
    printf("exec_details_end: opaque=%p, reqid=%d\n", opaque, reqid);
}

void event_tick_snapshot_end(void *opaque, int reqid)
{
    printf("tick_snapshot_end: opaque=%p, reqid=%d\n", opaque, reqid);
}

void event_market_data_type(void *opaque, int reqid, market_data_type_t data_type)
{
	printf("market_data_type: opaque=%p, reqid=%d, data_type=%d\n", opaque, reqid, (int)data_type);
}

void event_commission_report(void *opaque, tr_commission_report_t *report)
{
	printf("commission_report: opaque=%p, ...\n", opaque);
	printf("          cr_exec_id=[%s], cr_currency=[%s], cr_commission=%f, cr_realized_pnl=%f, cr_yield=%f, cr_yield_redemption_date=%d (%08X) (YYYYMMDD format)\n",
		report->cr_exec_id, report->cr_currency, report->cr_commission, report->cr_realized_pnl, report->cr_yield, 
		report->cr_yield_redemption_date, report->cr_yield_redemption_date);
}

