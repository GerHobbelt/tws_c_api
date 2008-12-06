/* API users must implement some or all of these functions */
#include "twsapi.h"

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>

void event_tick_price(void *opaque, int ticker_id, long field, double price,
                      int can_auto_execute)
{
    printf("tick_price: opaque=%p, ticker_id=%d, type=%ld, price=%.2lf, can_auto=%d\n",
           opaque, ticker_id, field, price, can_auto_execute);
}

void event_tick_size(void *opaque, int ticker_id, long type, int size)
{
    printf("tick_size: opaque=%p, ticker_id=%d, type=%ld, size=%d\n",
           opaque, ticker_id, type, size);
}

void event_tick_option_computation(void *opaque, int ticker_id, int type, double implied_vol, double delta, double model_price, double pr_dividend)
{
}

void event_tick_generic(void *opaque, int ticker_id, int type, double value)
{
}

void event_tick_string(void *opaque, int ticker_id, int type, const char value[])
{
}

void event_tick_efp(void *opaque, int ticker_id, int tick_type, double basis_points, const char formatted_basis_points[], double implied_futures_price, int hold_days, const char future_expiry[], double dividend_impact, double dividends_to_expiry)
{
}

void event_order_status(void *opaque, long order_id, const char status[],
                        int filled, int remaining, double avg_fill_price, int perm_id,
                        int parent_id, double last_fill_price, int client_id, const char why_held[])
{
    printf("order_status: opaque=%p, order_id=%ld, filled=%d remaining %d, avg_fill_price=%lf, last_fill_price=%lf, why_held=%s\n", opaque, order_id, filled, remaining, avg_fill_price, last_fill_price, why_held);
}

void event_open_order(void *opaque, long order_id, const tr_contract_t *contract, const tr_order_t *order, const tr_order_status_t *ostatus)
{
    /* commission values might be DBL_MAX */
    if(fabs(ostatus->ost_commission - DBL_MAX) < DBL_EPSILON)
        printf("open_order: commission not reported\n");
    else
        printf("open_order: commission for %ld was %.4lf\n", order_id, ostatus->ost_commission);

    printf("open_order: opaque=%p, order_id=%ld, sym=%s\n", opaque, order_id, contract->c_symbol);
}

void event_connection_closed(void *opaque)
{
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
    printf("update_portfolio: %p, sym=%s, position=%d, mkt_price=%.4lf, mkt_value=%.4lf, avg_cost=%.4lf, unrealized_pnl=%.4lf, realized pnl=%.4lf name=%s\n",
           opaque, contract->c_symbol, position, mkt_price, mkt_value, average_cost, unrealized_pnl, realized_pnl, account_name);
}

void event_update_account_time(void *opaque, const char time_stamp[])
{

}

void event_next_valid_id(void *opaque, long order_id)
{
    /* invoked once at connection establishment
     * the scope of this variable is not program wide, instance wide,
     * or even system wide. It has the same scope as the TWS account itself.
     * It appears to be persistent across TWS restarts.
     * Well behaved human and automatic TWS clients shall increment
     * this order_id atomically and cooperatively
     */
    printf("next_valid_id for order placement %ld\n", order_id);
}

void event_contract_details(void *opaque, int req_id, const tr_contract_details_t *contract_details)
{

}

void event_bond_contract_details(void *opaque, int req_id, const tr_contract_details_t *contract_details)
{

}

void event_exec_details(void *opaque, long order_id, const tr_contract_t *contract,
                        const tr_execution_t *execution)
{

}

void event_error(void *opaque, int id, int error_code, const char error_string[])
{
    printf("error: opaque=%p, error_code=%d, msg=%s\n", opaque, error_code, error_string);
}

void event_update_mkt_depth(void *opaque, int ticker_id, int position, int operation, int side, double price, int size)
{

}

void event_update_mkt_depth_l2(void *opaque, int ticker_id, int position,
                               char *market_maker, int operation, int side, double price, int size)
{

}

void event_update_news_bulletin(void *opaque, int msgid, int msg_type, const char news_msg[], const char origin_exch[])
{

}

void event_managed_accounts(void *opaque, const char accounts_list[])
{

}

void event_receive_fa(void *opaque, long fa_data_type, const char cxml[])
{

}

void event_historical_data(void *opaque, int reqid, const char date[], double open, double high, double low, double close, int volume, int bar_count, double wap, int has_gaps)
{
    printf("historical: opaque=%p, reqid=%d, date=%s, %.3lf, %.3lf, %.3lf, %.3lf, %d, wap=%.3lf, has_gaps=%d\n", opaque, reqid, date, open, high, low, close, volume, wap, has_gaps);
}

void event_scanner_parameters(void *opaque, const char xml[])
{
    printf("scanner_parameters: opaque=%p, xml=%s\n", opaque, xml);
}

void event_scanner_data(void *opaque, int ticker_id, int rank, tr_contract_details_t *cd, const char distance[], const char benchmark[], const char projection[], const char legs_str[])
{
    printf("scanner_data: opaque=%p, ticker_id=%d, rank=%d, distance=%s, benchmark=%s, projection=%s\n",
           opaque, ticker_id, rank, distance, benchmark, projection);
    printf("scanner_data details: sym=%s, sectype=%s, expiry=%s, strike=%.3lf, right=%s, exch=%s, currency=%s, local_sym=%s, market_name=%s, trading_class=%s\n", cd->d_summary.s_symbol, cd->d_summary.s_sectype, cd->d_summary.s_expiry, cd->d_summary.s_strike, cd->d_summary.s_right, cd->d_summary.s_exchange, cd->d_summary.s_currency, cd->d_summary.s_local_symbol, cd->d_market_name, cd->d_trading_class);

    /* it seems that NYSE traded stocks have different market_name and trading_class from NASDAQ */
}

void event_scanner_data_end(void *opaque, int ticker_id)
{
}

void event_current_time(void *opaque, long time)
{
}

void event_realtime_bar(void *opaque, int reqid, long time, double open, double high, double low, double close, long volume, double wap, int count)
{
    printf("realtime_bar: %p reqid=%d time=%ld, ohlc=%.4lf/%.4lf/%.4lf/%.4lf, vol=%ld, wap=%.4lf, count=%d\n", opaque, reqid, time, open, high, low, close, volume, wap, count);
}

void event_fundamental_data(void *opaque, int reqid, const char data[])
{

}

void event_contract_details_end(void *opaque, int reqid)
{

}
