/* API users must implement some or all of these functions */
#include "twsapi.h"

#include <stdio.h>

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

void event_order_status(void *opaque, long order_id, const char status[],
                        int filled, int remaining, double avg_fill_price, int perm_id,
                        int parent_id, double last_fill_price, int client_id)
{

}

void event_open_order(void *opaque, long order_id, const tr_contract_t *contract,
                      const tr_order_t *order)
{
     printf("open_order: order_id=%ld\n", order_id);
}

void event_win_error(void *opaque, const char str[], int last_error)
{
    printf("win_error: %p, str=%s, last_error=%d\n", opaque, str, last_error);
}

void event_connection_closed(void *opaque)
{
}

void event_update_account_value(void *opaque, const char key[], const char val[],
                                const char currency[], const char account_name[])
{
}

void event_update_portfolio(void *opaque, const tr_contract_t *contract, int position,
                            double mkt_price, double mkt_value, double average_cost,
                            double unrealized_pnl, double realized_pnl, const char account_name[])
{

}

void event_update_account_time(void *opaque, const char time_stamp[])
{

}

void event_next_valid_id(void *opaque, long order_id)
{

}

void event_contract_details(void *opaque, const tr_contract_details_t *contract_details)
{

}

void event_bond_contract_details(void *opaque, const tr_contract_details_t *contract_details)
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

void event_scanner_data(void *opaque, int ticker_id, int rank, tr_contract_details_t *cd, const char distance[], const char benchmark[], const char projection[])
{
    printf("scanner_data: opaque=%p, ticker_id=%d, rank=%d, distance=%s, benchmark=%s, projection=%s\n",
	   opaque, ticker_id, rank, distance, benchmark, projection);
    printf("scanner_data details: sym=%s, sectype=%s, expiry=%s, strike=%.3lf, right=%s, exch=%s, currency=%s, local_sym=%s, market_name=%s, trading_class=%s\n", cd->d_summary.s_symbol, cd->d_summary.s_sectype, cd->d_summary.s_expiry, cd->d_summary.s_strike, cd->d_summary.s_right, cd->d_summary.s_exchange, cd->d_summary.s_currency, cd->d_summary.s_local_symbol, cd->d_market_name, cd->d_trading_class);

    /* it seems that NYSE traded stocks have different market_name and trading_class from NASDAQ */
}
