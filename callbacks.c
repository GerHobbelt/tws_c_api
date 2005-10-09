/* API users must implement some or all of these functions */
#include "twsapi.h"

#include <stdio.h>

void event_tick_price(void *opaque, long ticker_id, long field, double price,
                      int can_auto_execute)
{
    /*implement me*/
    printf("tick_price: opaque=%p, ticker_id=%ld, type=%ld, price=%.2f, can_auto=%d\n",
           opaque, ticker_id, field, price, can_auto_execute);
}

void event_tick_size(void *opaque, long ticker_id, long field, int size)
{
    /*implement me*/
    printf("tick_size: opaque=%p, ticker_id=%ld, type=%ld, size=%d\n",
           opaque, ticker_id, field, size);
}

void event_order_status(void *opaque, long order_id, const char status[],
                        int filled, int remaining, double avg_fill_price, int perm_id,
                        int parent_id, double last_fill_price, int client_id)
{

}

void event_open_order(void *opaque, long order_id, const tr_contract_t *contract,
                      const tr_order_t *order)
{
    /*implement me*/
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

void event_update_mkt_depth(void *opaque, long ticker_id, int position, int operation, int side, double price, int size)
{

}

void event_update_mkt_depth_l2(void *opaque, long ticker_id, int position,
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

void event_historical_data(void *opaque, int reqid, const char date[], double open, double high, double low, double close, int volume, double wap, int has_gaps)
{
    printf("historical: opaque=%p, reqid=%d, date=%s, %.3lf, %.3lf, %.3lf, %.3f, %d, wap=%.3lf, has_gaps=%d\n", opaque, reqid, date, open, high, low, close, volume, wap, has_gaps);
}
