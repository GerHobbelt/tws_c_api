/* API users must implement some or all of these functions */
#include "twsapi.h"
#include "twsapi-debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <time.h>


/*
helper functions:
*/

void tws_cb_print_under_comp(void *opaque, int indent_level, const under_comp_t *und)
{
    tws_cb_printf(opaque, indent_level, "UNDER_COMP: price=%g, delta=%g, conid=%d\n",
		und->u_price, und->u_delta, und->u_conid);
}

void tws_cb_print_tag_value_set(void *opaque, int indent_level, int count, const tr_tag_value_t *set)
{
	int i;

    tws_cb_printf(opaque, indent_level, "TAG/VALUE SET: count=%d\n", count);

	for (i = 0; i < count; i++)
	{
		const tr_tag_value_t *t = &set[i];

	    tws_cb_printf(opaque, indent_level + 1, "TAG/VALUE [%d]: tag=[%s], val=[%s]\n",
			i, t->t_tag, t->t_val);
	}
}

void tws_cb_print_order_combolegs(void *opaque, int indent_level, int count, const tr_order_combo_leg_t *legs)
{
	int i;

    tws_cb_printf(opaque, indent_level, "ORDER COMBOLEGS: count=%d\n", count);

	for (i = 0; i < count; i++)
	{
		const tr_order_combo_leg_t *leg = &legs[i];

	    tws_cb_printf(opaque, indent_level + 1, "ORDER COMBOLEG[%d]: price=%g\n",
			i, leg->cl_price);
	}
}

void tws_cb_print_combolegs(void *opaque, int indent_level, int count, const tr_comboleg_t *legs)
{
	int i;

    tws_cb_printf(opaque, indent_level, "COMBOLEGS: count=%d\n", count);

	for (i = 0; i < count; i++)
	{
		const tr_comboleg_t *leg = &legs[i];

	    tws_cb_printf(opaque, indent_level + 1, "COMBOLEG[%d]: action=[%s], exchange=[%s], designated_location=[%s], conid=%d, ratio=%d, open_close=%d (%s), short_sale_slot=%d, exempt_code=%d\n",
			i, leg->co_action, leg->co_exchange, leg->co_designated_location, leg->co_conid, leg->co_ratio, leg->co_open_close, tr_comboleg_type_name(leg->co_open_close), leg->co_short_sale_slot, leg->co_exempt_code);
	}
}

void tws_cb_print_contract(void *opaque, int indent_level, const tr_contract_t *contract)
{
    tws_cb_printf(opaque, indent_level, "CONTRACT: delta neutral:\n");
	if (contract->c_undercomp)
	{
		tws_cb_print_under_comp(opaque, indent_level + 1, contract->c_undercomp);
	}
    tws_cb_printf(opaque, indent_level + 1, "strike=%g, symbol=[%s], sectype=[%s], exchange=[%s], primary_exch=[%s], expiry=[%s], currency=[%s], right=[%s], local_symbol=[%s], multiplier=[%s], combolegs_descrip=[%s], secid_type=[%s], secid=[%s], conid=%d, include_expired=%d\n",
		contract->c_strike, contract->c_symbol, contract->c_sectype, contract->c_exchange, contract->c_primary_exch, contract->c_expiry, contract->c_currency, contract->c_right, contract->c_local_symbol, contract->c_multiplier,
		contract->c_combolegs_descrip, contract->c_secid_type, contract->c_secid, contract->c_conid, contract->c_include_expired);
	tws_cb_print_combolegs(opaque, 2, contract->c_num_combolegs, contract->c_comboleg);
}

void tws_cb_print_contract_details(void *opaque, const tr_contract_details_t *cd)
{
    tws_cb_printf(opaque, 1, "CONTRACT_DETAILS: contract summary:\n");
	tws_cb_print_contract(opaque, 2, &cd->d_summary);
    tws_cb_printf(opaque, 2, "min.tick: %g, coupon: %g, order types: [%s], valid exch: [%s], cusip: [%s], maturity: [%s], issue_date: [%s], ratings: [%s], bond_type: [%s], "
		"coupon_type: [%s], notes: [%s], long name: [%s], industry: [%s], category: [%s], subcategory: [%s], timezone: [%s], trading hours: [%s], liquid hours: [%s], price_magnifier: %d, "
        "under_conid: %d, ev_multiplier: %g, ev_rule: [%s]\n",
        cd->d_mintick, cd->d_coupon, cd->d_order_types, cd->d_valid_exchanges, cd->d_cusip, cd->d_maturity, cd->d_issue_date, cd->d_ratings, cd->d_bond_type,
        cd->d_coupon_type, cd->d_notes, cd->d_long_name, cd->d_industry, cd->d_category, cd->d_subcategory, cd->d_timezone_id, cd->d_trading_hours, cd->d_liquid_hours,
        cd->d_price_magnifier, cd->d_under_conid, cd->d_ev_multiplier, cd->d_ev_rule);
	tws_cb_print_tag_value_set(opaque, 2, cd->d_sec_id_list_count, cd->d_sec_id_list);
}

void tws_cb_print_order(void *opaque, const tr_order_t *order)
{
    tws_cb_printf(opaque, 1, "ORDER: discretionary_amt=%g, lmt_price=%g, aux_price=%g, percent_offset=%g, nbbo_price_cap=%g, starting_price=%g, stock_ref_price=%g, delta=%g, stock_range_lower=%g, stock_range_upper=%g, volatility=%g, delta_neutral_aux_price=%g, trail_stop_price=%g, trailing_percent=%g, basis_points=%g, scale_price_increment=%g, scale_price_adjust_value=%g, scale_profit_offset=%g\n",
		order->o_discretionary_amt, order->o_lmt_price, order->o_aux_price, order->o_percent_offset,
		order->o_nbbo_price_cap, order->o_starting_price, order->o_stock_ref_price, order->o_delta,
		order->o_stock_range_lower, order->o_stock_range_upper, order->o_volatility, order->o_delta_neutral_aux_price,
		order->o_trail_stop_price, order->o_trailing_percent, order->o_basis_points, order->o_scale_price_increment,
		order->o_scale_price_adjust_value, order->o_scale_profit_offset);

	tws_cb_printf(opaque, 2, "algo_strategy=[%s], good_after_time=[%s], good_till_date=[%s], fagroup=[%s], famethod=[%s], fapercentage=[%s], faprofile=[%s], action=[%s], order_type=[%s], tif=[%s], oca_group=[%s], account=[%s], open_close=[%s], orderref=[%s], designated_location=[%s], rule80a=[%s], settling_firm=[%s], delta_neutral_order_type=[%s], clearing_account=[%s], clearing_intent=[%s], hedge_type=[%s], hedge_param=[%s], delta_neutral_settling_firm=[%s], delta_neutral_clearing_account=[%s], delta_neutral_clearing_intent=[%s]\n",
		order->o_algo_strategy, order->o_good_after_time, order->o_good_till_date, order->o_fagroup,
		order->o_famethod, order->o_fapercentage, order->o_faprofile, order->o_action,
		order->o_order_type, order->o_tif, order->o_oca_group, order->o_account,
		order->o_open_close, order->o_orderref, order->o_designated_location, order->o_rule80a,
		order->o_settling_firm, order->o_delta_neutral_order_type, order->o_clearing_account, order->o_clearing_intent,
		order->o_hedge_type, order->o_hedge_param, order->o_delta_neutral_settling_firm, order->o_delta_neutral_clearing_account,
		order->o_delta_neutral_clearing_intent);

	tws_cb_printf(opaque, 2, "algo_params:\n");
	tws_cb_print_tag_value_set(opaque, 3, order->o_algo_params_count, order->o_algo_params);
	tws_cb_printf(opaque, 2, "Smart combo routing params:\n");
	tws_cb_print_tag_value_set(opaque, 3, order->o_smart_combo_routing_params_count, order->o_smart_combo_routing_params);

	tws_cb_print_order_combolegs(opaque, 2, order->o_combo_legs_count, order->o_combo_legs);

	tws_cb_printf(opaque, 2, "o_orderid=%d, total_quantity=%d, origin=%d (%s), clientid=%d, permid=%d, parentid=%d, display_size=%d, trigger_method=%d, min_qty=%d, volatility_type=%d, reference_price_type=%d, basis_points_type=%d, scale_subs_level_size=%d, scale_init_level_size=%d, scale_price_adjust_interval=%d, scale_init_position=%d, scale_init_fill_qty=%d, exempt_code=%d, delta_neutral_con_id=%d, oca_type=%d (%s), auction_strategy=%d (%s)"
		", short_sale_slot=%d, override_percentage_constraints=%d, firm_quote_only=%d, etrade_only=%d, all_or_none=%d, outside_rth=%d, hidden=%d, transmit=%d, block_order=%d, sweep_to_fill=%d, continuous_update=%d, whatif=%d, not_held=%d, opt_out_smart_routing=%d, scale_auto_reset=%d, scale_random_percent=%d\n",
		order->o_orderid,order->o_total_quantity,order->o_origin, tr_origin_name(order->o_origin),
		order->o_clientid,order->o_permid,order->o_parentid,order->o_display_size,
		order->o_trigger_method,order->o_min_qty,order->o_volatility_type,order->o_reference_price_type,
		order->o_basis_points_type,order->o_scale_subs_level_size,order->o_scale_init_level_size,order->o_scale_price_adjust_interval,
		order->o_scale_init_position,order->o_scale_init_fill_qty,order->o_exempt_code,order->o_delta_neutral_con_id,
		order->o_oca_type, tr_oca_type_name(order->o_oca_type),order->o_auction_strategy, tr_auction_strategy_name(order->o_auction_strategy),
		order->o_short_sale_slot,order->o_override_percentage_constraints,order->o_firm_quote_only,order->o_etrade_only,
		order->o_all_or_none,order->o_outside_rth,order->o_hidden,order->o_transmit,
		order->o_block_order,order->o_sweep_to_fill,order->o_continuous_update,order->o_whatif,
		order->o_not_held,order->o_opt_out_smart_routing,order->o_scale_auto_reset,order->o_scale_random_percent);
}

void tws_cb_print_order_status(void *opaque, const tr_order_status_t *ostatus)
{
    tws_cb_printf(opaque, 1, "ORDER_STATUS: ost_commission=%g, ost_min_commission=%g, ost_max_commission=%g, ost_status=[%s], ost_init_margin=[%s], ost_maint_margin=[%s], ost_equity_with_loan=[%s], ost_commission_currency=[%s], ost_warning_text=[%s]\n",
		ostatus->ost_commission, ostatus->ost_min_commission, ostatus->ost_max_commission, ostatus->ost_status,
		ostatus->ost_init_margin, ostatus->ost_maint_margin, ostatus->ost_equity_with_loan, ostatus->ost_commission_currency, ostatus->ost_warning_text);
}

void tws_cb_print_execution(void *opaque, const tr_execution_t *exec)
{
    tws_cb_printf(opaque, 1, "EXECUTION: e_price=%g, e_avg_price=%g, e_execid=[%s], e_time=[%s], e_acct_number=[%s], e_exchange=[%s], e_side=[%s], e_orderref=[%s], e_shares=%d, e_permid=%d, e_clientid=%d, e_liquidation=%d, e_orderid=%d, e_cum_qty=%d\n",
		exec->e_price, exec->e_avg_price, exec->e_execid, exec->e_time, exec->e_acct_number, exec->e_exchange, exec->e_side, exec->e_orderref, exec->e_shares, exec->e_permid, exec->e_clientid, exec->e_liquidation, exec->e_orderid, exec->e_cum_qty);
}

void tws_cb_print_commission_report(void *opaque, tr_commission_report_t *report)
{
	tws_cb_printf(opaque, 1, "COMMISSION_REPORT: cr_exec_id=[%s], cr_currency=[%s], cr_commission=%g, cr_realized_pnl=%g, cr_yield=%g, cr_yield_redemption_date=%d (%08X) (YYYYMMDD format)\n",
		report->cr_exec_id, report->cr_currency, report->cr_commission, report->cr_realized_pnl, report->cr_yield,
		report->cr_yield_redemption_date, report->cr_yield_redemption_date);
}





/*
event handlers:
*/

void event_tick_price(void *opaque, int ticker_id, tr_tick_type_t field, double price, int can_auto_execute)
{
    tws_cb_printf(opaque, 0, "tick_price: opaque=%p, ticker_id=%d, type=%d (%s), price=%g, can_auto=%d\n",
           opaque, ticker_id, (int)field, tick_type_name(field), price, can_auto_execute);
}

void event_tick_size(void *opaque, int ticker_id, tr_tick_type_t type, int size)
{
    tws_cb_printf(opaque, 0, "tick_size: opaque=%p, ticker_id=%d, type=%d (%s), size=%d\n",
           opaque, ticker_id, (int)type, tick_type_name(type), size);
}

void event_tick_option_computation(void *opaque, int ticker_id, tr_tick_type_t type, double implied_vol, double delta, double opt_price, double pv_dividend, double gamma, double vega, double theta, double und_price)
{
    tws_cb_printf(opaque, 0, "tick option computation: opaque=%p, ticker_id=%d, type=%d (%s), implied_vol=%g, delta=%g, opt_price=%g, pv_dividend=%g, gamma=%g, vega=%g, theta=%g, und_price=%g\n",
           opaque, ticker_id, (int)type, tick_type_name(type), implied_vol, delta, opt_price, pv_dividend, gamma, vega, theta, und_price);
}

void event_tick_generic(void *opaque, int ticker_id, tr_tick_type_t type, double value)
{
    tws_cb_printf(opaque, 0, "tick_generic: opaque=%p, ticker_id=%d, type=%d (%s), value=%g\n", opaque, ticker_id, type, tick_type_name(type), value);
}

void event_tick_string(void *opaque, int ticker_id, tr_tick_type_t type, const char value[])
{
    tws_cb_printf(opaque, 0, "tick_string: opaque=%p, ticker_id=%d, type=%d (%s), value=[%s]\n", opaque, ticker_id, type, tick_type_name(type), value);
}

void event_tick_efp(void *opaque, int ticker_id, tr_tick_type_t tick_type, double basis_points, const char formatted_basis_points[], double implied_futures_price, int hold_days, const char future_expiry[], double dividend_impact, double dividends_to_expiry)
{
    tws_cb_printf(opaque, 0, "tick_efp: opaque=%p, ticker_id=%d, type=%d (%s), basis_points=%g, formatted_basis_points=[%s], implied_futures_price=%g, hold_days=%d, future_expiry=[%s], dividend_impact=%g, dividends_to_expiry=%g\n",
		opaque, ticker_id, tick_type, tick_type_name(tick_type), basis_points, formatted_basis_points, implied_futures_price, hold_days, future_expiry, dividend_impact, dividends_to_expiry);
}

void event_order_status(void *opaque, int order_id, const char status[],
                        int filled, int remaining, double avg_fill_price, int perm_id,
                        int parent_id, double last_fill_price, int client_id, const char why_held[])
{
    tws_cb_printf(opaque, 0, "order_status: opaque=%p, order_id=%d, status=[%s], filled=%d, remaining %d, avg_fill_price=%g, last_fill_price=%g, perm_id=%d, parent_id=%d, client_id=%d, why_held=[%s]\n",
		opaque, order_id, status, filled, remaining, avg_fill_price, last_fill_price, perm_id, parent_id, client_id, why_held);
}

void event_open_order(void *opaque, int order_id, const tr_contract_t *contract, const tr_order_t *order, const tr_order_status_t *ostatus)
{
    /* commission values might be DBL_MAX */
    if(fabs(ostatus->ost_commission - DBL_MAX) < DBL_EPSILON)
        tws_cb_printf(opaque, 0, "open_order: opaque=%p, commission not reported\n", opaque);
    else
		tws_cb_printf(opaque, 0, "open_order: opaque=%p, commission for order_id=%d was %.4g\n", opaque, order_id, ostatus->ost_commission);

    tws_cb_printf(opaque, 0, "open_order: opaque=%p, order_id=%d, sym=[%s], local_symbol=[%s]\n", opaque, order_id, contract->c_symbol, contract->c_local_symbol);
	tws_cb_print_contract(opaque, 1, contract);
	tws_cb_print_order(opaque, order);
	tws_cb_print_order_status(opaque, ostatus);
}

void event_update_account_value(void *opaque, const char key[], const char val[],
                                const char currency[], const char account_name[])
{
    tws_cb_printf(opaque, 0, "update_account_value: %p, key=[%s] val=[%s], currency=[%s], name=[%s]\n",
           opaque, key, val, currency, account_name);
}

void event_update_portfolio(void *opaque, const tr_contract_t *contract, int position,
                            double mkt_price, double mkt_value, double average_cost,
                            double unrealized_pnl, double realized_pnl, const char account_name[])
{
    tws_cb_printf(opaque, 0, "update_portfolio: %p, sym=%s, position=%d, mkt_price=%.4g, mkt_value=%.4g, avg_cost=%.4g, unrealized_pnl=%.4g, realized_pnl=%.4g account_name=%s\n",
           opaque, contract->c_symbol, position, mkt_price, mkt_value, average_cost, unrealized_pnl, realized_pnl, account_name);
	tws_cb_print_contract(opaque, 1, contract);
}

void event_update_account_time(void *opaque, const char time_stamp[])
{
    tws_cb_printf(opaque, 0, "update_account_time: opaque=%p, time_stamp=[%s]\n", opaque, time_stamp);
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
    tws_cb_printf(opaque, 0, "next_valid_id for order placement %d (opaque=%p)\n", order_id, opaque);
}

void event_contract_details(void *opaque, int req_id, const tr_contract_details_t *cd)
{
    tws_cb_printf(opaque, 0, "contract_details: opaque=%p, req_id=%d, ...\n", opaque, req_id);
	tws_cb_print_contract_details(opaque, cd);
}

void event_bond_contract_details(void *opaque, int req_id, const tr_contract_details_t *cd)
{
    tws_cb_printf(opaque, 0, "bond_contract_details: opaque=%p, req_id=%d, ...\n", opaque, req_id);
	tws_cb_print_contract_details(opaque, cd);
}

void event_exec_details(void *opaque, int order_id, const tr_contract_t *contract, const tr_execution_t *execution)
{
    tws_cb_printf(opaque, 0, "exec_details: opaque=%p, order_id=%d, ...\n", opaque, order_id);
	tws_cb_print_contract(opaque, 1, contract);
	tws_cb_print_execution(opaque, execution);
}

void event_error(void *opaque, int ticker_id, int error_code, const char error_string[])
{
    tws_cb_printf(opaque, 0, "error: opaque=%p, ticker_id=%d, error_code=%d, msg='%s'\n", opaque, ticker_id, error_code, error_string);
}

void event_update_mkt_depth(void *opaque, int ticker_id, int position, int operation, int side, double price, int size)
{
    tws_cb_printf(opaque, 0, "update_mkt_depth: opaque=%p, ticker_id=%d, posaition=%d, operation=%d, size=%d, price=%g, size=%d\n",
		opaque, ticker_id, position, operation, side, price, size);
}

void event_update_mkt_depth_l2(void *opaque, int ticker_id, int position, const char *market_maker, int operation, int side, double price, int size)
{
    tws_cb_printf(opaque, 0, "update_mkt_depth_l2: opaque=%p, ticker_id=%d, position=%d, market_maker=[%s], operation=%d, side=%d, price=%g, size=%d\n",
		opaque, ticker_id, position, market_maker, operation, side, price, size);
}

void event_update_news_bulletin(void *opaque, int msgid, int msg_type, const char news_msg[], const char origin_exch[])
{
    tws_cb_printf(opaque, 0, "update_news_bulletin: opaque=%p, msg_id=%d, msg_type=%d, msg=[%s], origin_exchange=[%d]\n",
		opaque, msgid, msg_type, news_msg, origin_exch);
}

void event_managed_accounts(void *opaque, const char accounts_list[])
{
    tws_cb_printf(opaque, 0, "managed_accounts: opaque=%p, accounts_list=[%s]\n",
		opaque, accounts_list);
}

void event_receive_fa(void *opaque, tr_fa_msg_type_t fa_data_type, const char cxml[])
{
    tws_cb_printf(opaque, 0, "receive_fa: opaque=%p, fa_data_type=%d (%s), xml='%s'\n", opaque, (int)fa_data_type, fa_msg_type_name(fa_data_type), cxml);
}

void event_historical_data(void *opaque, int reqid, const char date[], double open, double high, double low, double close, long int volume, int bar_count, double wap, int has_gaps)
{
    tws_cb_printf(opaque, 0, "historical: opaque=%p, reqid=%d, date=%s, ohlc=%.4g/%.4g/%.4g/%.4g, volume=%ld, bar_count=%d, wap=%.4g, has_gaps=%d\n",
		opaque, reqid, date, open, high, low, close, volume, bar_count, wap, has_gaps);
}

void event_historical_data_end(void *opaque, int reqid, const char completion_from[], const char completion_to[])
{
    tws_cb_printf(opaque, 0, "historical list end: opaque=%p, reqid=%d, from date=[%s], to date=[%s]\n", opaque, reqid, completion_from, completion_to);
}

void event_scanner_parameters(void *opaque, const char xml[])
{
    tws_cb_printf(opaque, 0, "scanner_parameters: opaque=%p, xml(len=%d)=[%s]\n", opaque, (int)strlen(xml), xml);
}

void event_scanner_data(void *opaque, int ticker_id, int rank, tr_contract_details_t *cd, const char distance[], const char benchmark[], const char projection[], const char legs_str[])
{
    tws_cb_printf(opaque, 0, "scanner_data: opaque=%p, ticker_id=%d, rank=%d, distance=[%s], benchmark=[%s], projection=[%s], legs_str=[%s]\n",
           opaque, ticker_id, rank, distance, benchmark, projection, legs_str);
	tws_cb_print_contract_details(opaque, cd);

    /* it seems that NYSE traded stocks have different market_name and trading_class from NASDAQ */
}

void event_scanner_data_start(void *opaque, int ticker_id, int num_elements)
{
    tws_cb_printf(opaque, 0, "scanner_data_start: opaque=%p, ticker_id=%d, num_elements=%d\n", opaque, ticker_id, num_elements);
}

void event_scanner_data_end(void *opaque, int ticker_id, int num_elements)
{
    tws_cb_printf(opaque, 0, "scanner_data_end: opaque=%p, ticker_id=%d, num_elements=%d\n", opaque, ticker_id, num_elements);
}

void event_current_time(void *opaque, long time)
{
    char tbuf[40];
    time_t timestamp = (time_t)time;

    strftime(tbuf, sizeof(tbuf), "[%Y%m%dT%H%M%S] ", gmtime(&timestamp));

    tws_cb_printf(opaque, 0, "current_time: opaque=%p, time=%ld ~ '%s'\n", opaque, time, tbuf);
}

void event_realtime_bar(void *opaque, int reqid, long time, double open, double high, double low, double close, long int volume, double wap, int count)
{
    tws_cb_printf(opaque, 0, "realtime_bar: opaque=%p, reqid=%d, time=%ld, ohlc=%.4g/%.4g/%.4g/%.4g, vol=%ld, wap=%.4g, count=%d\n", opaque, reqid, time, open, high, low, close, volume, wap, count);
}

void event_fundamental_data(void *opaque, int reqid, const char data[])
{
    tws_cb_printf(opaque, 0, "fundamental_data: opaque=%p, reqid=%d, data=[%s]\n", opaque, reqid, data);
}

void event_contract_details_end(void *opaque, int reqid)
{
    tws_cb_printf(opaque, 0, "contract_details_end: opaque=%p, reqid=%d\n", opaque, reqid);
}

void event_open_order_end(void *opaque)
{
    tws_cb_printf(opaque, 0, "open_order_end: opaque=%p\n", opaque);
}

void event_delta_neutral_validation(void *opaque, int reqid, const under_comp_t *und)
{
    tws_cb_printf(opaque, 0, "delta_neutral_validation: opaque=%p, reqid=%d, ...\n", opaque, reqid);
    tws_cb_print_under_comp(opaque, 1, und);
}

void event_acct_download_end(void *opaque, const char acct_name[])
{
    tws_cb_printf(opaque, 0, "acct_download_end: opaque=%p, account name=[%s]\n", opaque, acct_name);
}

void event_exec_details_end(void *opaque, int reqid)
{
    tws_cb_printf(opaque, 0, "exec_details_end: opaque=%p, reqid=%d\n", opaque, reqid);
}

void event_tick_snapshot_end(void *opaque, int reqid)
{
    tws_cb_printf(opaque, 0, "tick_snapshot_end: opaque=%p, reqid=%d\n", opaque, reqid);
}

void event_market_data_type(void *opaque, int reqid, market_data_type_t data_type)
{
	tws_cb_printf(opaque, 0, "market_data_type: opaque=%p, reqid=%d, data_type=%d (%s)\n", opaque, reqid, (int)data_type, market_data_type_name(data_type));
}

void event_commission_report(void *opaque, tr_commission_report_t *report)
{
	tws_cb_printf(opaque, 0, "commission_report: opaque=%p, ...\n", opaque);
	tws_cb_print_commission_report(opaque, report);
}

