/* API users must implement some or all of these functions */
#include "twsapi.h"

void event_tick_price(void *opaque_user_defined, long ticker_id, long field, double price,
                      int can_auto_execute)
{
    /*implement me*/
    printf("tick_price: opaque=%x, ticker_id=%ld, type=%ld, price=%.2f, can_auto=%d\n",
           opaque_user_defined, ticker_id, field, price, can_auto_execute);
}

void event_tick_size(void *opaque_user_defined, long ticker_id, long field, int size)
{
    /*implement me*/
    printf("tick_size: opaque=%x, ticker_id=%ld, type=%ld, size=%d\n",
           opaque_user_defined, ticker_id, field, size);

}

void event_order_status(void *opaque_user_defined, long order_id, const char status[],
                        int filled, int remaining, double avg_fill_price, int perm_id,
                        int parent_id, double last_fill_price, int client_id)
{

}

void event_open_order(void *opaque_user_defined, long order_id, const tr_contract_t *contract,
                      const tr_order_t *order)
{
    /*implement me*/
    printf("open_order: order_id=%d\n", order_id);
}

void event_win_error(void *opaque_user_defined, const char str[], int last_error)
{

}

void event_connection_closed(void *opaque_user_defined)
{
}

void event_update_account_value(void *opaque_user_defined, const char key[], const char val[],
                                const char currency[], const char account_name[])
{
}

void event_update_portfolio(void *opaque_user_defined, const tr_contract_t *contract, int position,
                            double mkt_price, double mkt_value, double average_cost,
                            double unrealized_pnl, double realized_pnl, const char account_name[])
{

}

void event_update_account_time(void *opaque_user_defined, const char time_stamp[])
{

}

void event_next_valid_id(void *opaque_user_defined, long order_id)
{

}

void event_contract_details(void *opaque_user_defined, const tr_contract_details_t *contract_details)
{

}

void event_exec_details(void *opaque_user_defined, long order_id, const tr_contract_t *contract,
                        const tr_execution_t *execution)
{

}

void event_error(void *opaque_user_defined, int id, int error_code, const char error_string[])
{
}

void event_update_mkt_depth(void *opaque_user_defined, long ticker_id, int position, int operation, int side, double price, int size)
{

}

void event_update_mkt_depth_l2(void *opaque_user_defined, long ticker_id, int position,
                               char *market_maker, int operation, int side, double price, int size)
{

}

void event_update_news_bulletin(void *opaque_user_defined, int msgid, int msg_type, const char news_msg[], const char origin_exch[])
{

}

void event_managed_accounts(void *opaque_user_defined, const char accounts_list[])
{

}

void event_receive_fa(void *opaque_user_defined, long fa_data_type, const char cxml[])
{

}
