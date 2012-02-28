#ifndef TWSAPI_H_
#define TWSAPI_H_

#ifdef __cplusplus
extern "C" {
#endif

/* public C API */
#define TWSCLIENT_VERSION 53

typedef struct under_comp {
    double u_price;
    double u_delta;
    int    u_conid;
} under_comp_t;

#if 0
typedef struct contract_details_summary {
    double s_strike;
    char  *s_symbol;
    char  *s_sectype;
    char  *s_expiry;
    char  *s_right;
    char  *s_exchange;
    char  *s_primary_exch;
    char  *s_currency;
    char  *s_local_symbol;
    char  *s_multiplier;
    int    s_conid;
} contract_details_summary_t;
#endif

typedef enum {
	MDT_UNKNOWN = 0,
    REALTIME	= 1,
    FROZEN		= 2,
} market_data_type_t;

typedef enum {
    COMBOLEG_SAME    = 0,                               /* open/close leg value is same as combo */
    COMBOLEG_OPEN    = 1,
    COMBOLEG_CLOSE   = 2,
    COMBOLEG_UNKNOWN = 3,
} tr_comboleg_type_t;

typedef struct tr_comboleg {
    char *co_action;                                    /* BUY/SELL/SSHORT/SSHORTX */
    char *co_exchange;
    char *co_designated_location;                       /* set to "" if unused, as usual */
    int   co_conid;
    int   co_ratio;
    tr_comboleg_type_t co_open_close;
    int   co_short_sale_slot;                           /* 1 = clearing broker, 2 = third party */
    int   co_exempt_code;                               /* set to -1 if you do not use it */
} tr_comboleg_t;

typedef struct tr_contract {
    under_comp_t  *c_undercomp;                         /* delta neutral */
    double         c_strike;                            /* strike price for options */
    char *         c_symbol;                            /* underlying symbol */
    char *         c_sectype;                           /* security type ("BAG" -> transmits combo legs, "" -> does not transmit combo legs) */
    char *         c_exchange;
    char *         c_primary_exch;                      /* for SMART orders, specify an actual exchange where the contract trades, e.g. ISLAND.  Pick a non-aggregate (ie not the SMART exchange) exchange that the contract trades on.  DO NOT SET TO SMART. */
    char *         c_expiry;                            /* expiration for futures and options */
    char *         c_currency;                          /* currency, e.g. USD */
    char *         c_right;                             /* put or call (P or C) */
    char *         c_local_symbol;                      /* local symbol for futures or options, e.g. ESZN for ES DEC09 contract  */
    char *         c_multiplier;
    char *         c_combolegs_descrip;                 /* received in open order version 14 and up for all combos */
    char *         c_secid_type;                        /* CUSIP;SEDOL;ISIN;RIC */
    char *         c_secid;
    tr_comboleg_t *c_comboleg;                          /* COMBOS */
    int            c_num_combolegs;
    int            c_conid;                             /* contract id, returned from TWS */
    unsigned       c_include_expired: 1;                /* for contract requests, specifies that even expired contracts should be returned.  Can not be set to true for orders. */
} tr_contract_t;

typedef struct tr_contract_details {
    double                     d_mintick;
    double                     d_coupon;                /* for bonds */
    tr_contract_t              d_summary;
    char *                     d_market_name;
    char *                     d_trading_class;
    char *                     d_order_types;
    char *                     d_valid_exchanges;
    char *                     d_cusip;                 /* for bonds */
    char *                     d_maturity;              /* for bonds */
    char *                     d_issue_date;            /* for bonds */
    char *                     d_ratings;               /* for bonds */
    char *                     d_bond_type;             /* for bonds */
    char *                     d_coupon_type;           /* for bonds */
    char *                     d_desc_append;           /* for bonds */
    char *                     d_next_option_date;      /* for bonds */
    char *                     d_next_option_type;      /* for bonds */
    char *                     d_notes;                 /* for bonds */
    char *                     d_long_name;
    char *                     d_contract_month;
    char *                     d_industry;
    char *                     d_category;
    char *                     d_subcategory;
    char *                     d_timezone_id;
    char *                     d_trading_hours;
    char *                     d_liquid_hours;
    int                        d_price_magnifier;
    int                        d_under_conid;
    unsigned                   d_convertible: 1;        /* for bonds */
    unsigned                   d_callable: 1;           /* for bonds */
    unsigned                   d_putable: 1;            /* for bonds */
    unsigned                   d_next_option_partial:1; /* for bonds */
} tr_contract_details_t;

typedef struct tr_tag_value {
    char *t_tag;
    char *t_val;
} tr_tag_value_t;

typedef enum tr_origin
{
    CUSTOMER = 0,
    FIRM = 1
} tr_origin_t;

typedef enum tr_oca_type
{
    OCA_UNDEFINED = 0,
    CANCEL_WITH_BLOCK = 1,
    REDUCE_WITH_BLOCK = 2,
    REDUCE_NON_BLOCK = 3
} tr_oca_type_t;

typedef enum tr_auction_strategy
{
    AUCTION_UNDEFINED = 0,
    AUCTION_MATCH = 1,
    AUCTION_IMPROVEMENT = 2,
    AUCTION_TRANSPARENT = 3
} tr_auction_strategy_t;

typedef struct tr_order {
    double   o_discretionary_amt;                       /* SMART routing only: amount you are willing to pay above your specified limit price */
    double   o_lmt_price;                               /* Basic Order Field: limit price  */
    double   o_aux_price;                               /* Basic Order Field: stop price, trailing amount, or offset amount  */
    double   o_percent_offset;                          /* Advanced order field: the offset amount for Relative (REL) orders, specified as a percent; specify either this OR the offset amount in m_auxPrice */
    double   o_nbbo_price_cap;                          /* SMART routing only: see 'm_firmQuoteOnly' comment */
    double   o_starting_price;                          /* For BOX option-pegged-to-stock orders only */
    double   o_stock_ref_price;                         /* For BOX option-pegged-to-stock orders only */
    double   o_delta;                                   /* For BOX option-pegged-to-stock orders only */
    double   o_stock_range_lower;                       /* For Pegged-to-stock or Volatility orders only: if stock price moves below this price, order will be canceled */
    double   o_stock_range_upper;                       /* For Pegged-to-stock or Volatility orders only: if stock price moves above this price, order will be canceled */
    double   o_volatility;                              /* For Volatility orders only: volatility (percent) */
    double   o_delta_neutral_aux_price;
    double   o_trail_stop_price;                        /* Advanced order field: initial stop price for trailing stop (TRAIL) orders */
    double   o_basis_points;
    double   o_scale_price_increment;
    char *   o_algo_strategy;
    char *   o_good_after_time;                         /* Advanced order field: format: YYYYMMDD HH:MM:SS {time zone}  e.g. 20060505 08:00:00 EST */
    char *   o_good_till_date;                          /* Advanced order field: format: YYYYMMDD HH:MM:SS {time zone}  e.g. 20060505 08:00:00 EST */
    char *   o_fagroup;                                 /* For Financial advisors (FA) only: Advisor group, e.g. "All" */
    char *   o_famethod;                                /* For Financial advisors (FA) only: Advisor method: PctChange, AvailableEquity, NetLiq, or EqualQuantity */
    char *   o_fapercentage;                            /* For Financial advisors (FA) only: Advisor percentage, used when the method is set to PctChange */
    char *   o_faprofile;                               /* For Financial advisors (FA) only: Advisor profile */
    char *   o_action;                                  /* Basic Order Field: specify BUY or SELL; non-cleared customers can specify SSHORT */
    char *   o_order_type;                              /* Basic Order Field: order type, e.g. LMT, MKT, STOP, TRAIL, REL */
    char *   o_tif;                                     /* Advanced order field: Time in force, e.g. DAY, GTC */
    char *   o_oca_group;                               /* Advanced order field: OCA group name (OCA = "one cancels all") */
    char *   o_account;                                 /* Clearing info: IB account; can be left blank for users with only a single account   */
    char *   o_open_close;                              /* For non-cleared (i.e. institutional) customers only: open/close flag: O=Open, C=Close */
    char *   o_orderref;                                /* Advanced order field: order reference, enter any free-form text */
    char *   o_designated_location;                     /* For non-cleared (i.e. institutional) customers only: specifies where the shares are held; set only when m_shortSaleSlot=2 */
    char *   o_rule80a;                                 /* Advanced order field: Individual = 'I', Agency = 'A', AgentOtherMember = 'W', IndividualPTIA = 'J', AgencyPTIA = 'U', AgentOtherMemberPTIA = 'M', IndividualPT = 'K', AgencyPT = 'Y', AgentOtherMemberPT = 'N' */
    char *   o_settling_firm;
    char *   o_delta_neutral_order_type;
    char *   o_clearing_account;                        /* Clearing info: True beneficiary of the order */
    char *   o_clearing_intent;                         /* Clearing info: "" (Default), "IB", "Away", "PTA" (PostTrade) */
	char *   o_hedge_type;								/* Hedge Orders: 'D' - delta, 'B' - beta, 'F' - FX, 'P' - pair */
	char *   o_hedge_param;								/* Hedge Orders: 'beta=X' value for beta hedge, 'ratio=Y' for pair hedge */
    char *   o_delta_neutral_settling_firm;				/* For Volatility orders only: */
    char *   o_delta_neutral_clearing_account;			/* For Volatility orders only: */
    char *   o_delta_neutral_clearing_intent;			/* For Volatility orders only: */

    tr_tag_value_t *o_algo_params;                      /* 'm_algoParams': array of length o_algo_params_count, API user responsible for alloc/free */
	tr_tag_value_t *o_smart_combo_routing_params;		/* Smart combo routing params: 'm_smartComboRoutingParams': array of length o_smart_combo_routing_params, API user responsible for alloc/free */

    int      o_algo_params_count;                       /* how many tag values are in o_algo_params, 0 if unused */
	int      o_smart_combo_routing_params_count;        /* how many tag values are in o_smart_combo_routing_params, 0 if unused */
    int      o_orderid;                                 /* Basic Order Field: order id generated by API client */
    int      o_total_quantity;                          /* Basic Order Field: order size */
    tr_origin_t o_origin;                               /* For non-cleared (i.e. institutional) customers only: origin: 0=Customer, 1=Firm */
    int      o_clientid;                                /* Basic Order Field: client id of the API client that submitted the order */
    int      o_permid;                                  /* Basic Order Field: TWS order ID (not specified by API) */
    int      o_parentid;                                /* Advanced order field: order id of parent order, to associate attached stop, trailing stop, or bracket orders with parent order (Auto STP, TRAIL)  */
    int      o_display_size;                            /* Advanced order field: the portion of the order that will be visible to the world */
    int      o_trigger_method;                          /* Advanced order field: for stop orders:  0=Default, 1=Double_Bid_Ask, 2=Last, 3=Double_Last, 4=Bid_Ask, 7=Last_or_Bid_Ask, 8=Mid-point */
    int      o_min_qty;                                 /* Advanced order field: no partial fills less than the size specified here */
    int      o_volatility_type;                         /* For Volatility orders only: volatility type: 1=daily, 2=annual */
    int      o_reference_price_type;                    /* For Volatility orders only: what to use as the current stock price: 1=bid/ask average, 2 = bid or ask */
    int      o_basis_points_type;
    int      o_scale_subs_level_size;
    int      o_scale_init_level_size;
    int      o_exempt_code;                             /* set to -1 if you do not use it */
    int      o_delta_neutral_con_id;					/* For Volatility orders only: */
    tr_oca_type_t o_oca_type;                           /* Advanced order field: OCA group type  1 = CANCEL_WITH_BLOCK, 2 = REDUCE_WITH_BLOCK, 3 = REDUCE_NON_BLOCK */
    tr_auction_strategy_t o_auction_strategy;           /* For BOX option-pegged-to-stock and Volatility orders only: 1=AUCTION_MATCH, 2=AUCTION_IMPROVEMENT, 3=AUCTION_TRANSPARENT */
    unsigned o_short_sale_slot: 2;                      /* For non-cleared (i.e. institutional) customers only: specify only if m_action is "SSHORT": 1 if you hold the shares, 2 if they will be delivered from elsewhere */
    unsigned o_override_percentage_constraints: 1;      /* Advanced order field: set true to override normal percentage constraint checks */
    unsigned o_firm_quote_only: 1;                      /* SMART routing only: if true, specifies that order should be routed to exchanges showing a "firm" quote, but not if the exchange is off the NBBO by more than the 'm_nbboPriceCap' amount */
    unsigned o_etrade_only: 1;
    unsigned o_all_or_none: 1;                          /* Advanced order field: if set to true, there can be no partial fills for the order */
    unsigned o_outside_rth: 1;                          /* Advanced order field: if true, order could fill or trigger anytime; if false, order will fill or trigger only during regular trading hours */
    unsigned o_hidden: 1;                               /* Advanced order field: if true, order will be hidden, and will not be reflected in the market data or deep book */
    unsigned o_transmit: 1;                             /* Advanced order field: if false, order will be created in TWS but not transmitted */
    unsigned o_block_order: 1;                          /* Advanced order field: block order, for ISE option orders only */
    unsigned o_sweep_to_fill: 1;                        /* Advanced order field: for SMART orders, specifies that orders should be split and sent to multiple exchanges at the same time */
    unsigned o_continuous_update: 1;                    /* For Volatility orders only: if true, price will be continuously recalculated after order submission */
    unsigned o_whatif: 1;                               /* if true, the order will not be submitted, but margin info will be returned */
    unsigned o_not_held: 1;
	unsigned o_opt_out_smart_routing: 1;				/* SMART routing only: */
} tr_order_t;

typedef struct tr_order_status {
    double ost_commission;
    double ost_min_commission;
    double ost_max_commission;
    char  *ost_status;
    char  *ost_init_margin;
    char  *ost_maint_margin;
    char  *ost_equity_with_loan;
    char  *ost_commission_currency;
    char  *ost_warning_text;
} tr_order_status_t;

typedef struct tr_execution {
    double e_price;
    double e_avg_price;
    char  *e_execid;
    char  *e_time;
    char  *e_acct_number;
    char  *e_exchange;
    char  *e_side;
	char  *e_orderref;
    int    e_shares;
    int    e_permid;
    int    e_clientid;
    int    e_liquidation;
    int    e_orderid;
    int    e_cum_qty;
} tr_execution_t;

typedef struct tr_exec_filter {
    char *f_acctcode;
    char *f_time;
    char *f_symbol;
    char *f_sectype;
    char *f_exchange;
    char *f_side;
    int   f_clientid;
} tr_exec_filter_t;

typedef struct tr_scanner_subscription {
    double scan_above_price;
    double scan_below_price;
    double scan_coupon_rate_above;
    double scan_coupon_rate_below;
    double scan_market_cap_above;
    double scan_market_cap_below;
    char  *scan_exclude_convertible;
    char  *scan_instrument;
    char  *scan_location_code;
    char  *scan_maturity_date_above;
    char  *scan_maturity_date_below;
    char  *scan_moody_rating_above;
    char  *scan_moody_rating_below;
    char  *scan_code;
    char  *scan_sp_rating_above;
    char  *scan_sp_rating_below;
    char  *scan_scanner_setting_pairs;
    char  *scan_stock_type_filter;
    int    scan_above_volume;
    int    scan_number_of_rows;
    int    scan_average_option_volume_above;
} tr_scanner_subscription_t;

typedef enum tr_tick_type {
    TICK_UNDEFINED = -1,
    BID_SIZE   = 0,
    BID        = 1,
    ASK        = 2,
    ASK_SIZE   = 3,
    LAST       = 4,
    LAST_SIZE  = 5,
    HIGH       = 6,
    LOW        = 7,
    VOLUME     = 8,
    CLOSE      = 9,
    BID_OPTION = 10,
    ASK_OPTION = 11,
    LAST_OPTION = 12,
    MODEL_OPTION = 13,
    OPEN         = 14,
    LOW_13_WEEK  = 15,
    HIGH_13_WEEK = 16,
    LOW_26_WEEK  = 17,
    HIGH_26_WEEK = 18,
    LOW_52_WEEK  = 19,
    HIGH_52_WEEK = 20,
    AVG_VOLUME   = 21,
    OPEN_INTEREST = 22,
    OPTION_HISTORICAL_VOL = 23, /* volatility */
    OPTION_IMPLIED_VOL = 24,    /* volatility */
    OPTION_BID_EXCH = 25,
    OPTION_ASK_EXCH = 26,
    OPTION_CALL_OPEN_INTEREST = 27,
    OPTION_PUT_OPEN_INTEREST = 28,
    OPTION_CALL_VOLUME = 29,
    OPTION_PUT_VOLUME = 30,
    INDEX_FUTURE_PREMIUM = 31,
    BID_EXCH = 32,
    ASK_EXCH = 33,
    AUCTION_VOLUME = 34,
    AUCTION_PRICE = 35,
    AUCTION_IMBALANCE = 36,
    MARK_PRICE = 37,
    BID_EFP_COMPUTATION  = 38,
    ASK_EFP_COMPUTATION  = 39,
    LAST_EFP_COMPUTATION = 40,
    OPEN_EFP_COMPUTATION = 41,
    HIGH_EFP_COMPUTATION = 42,
    LOW_EFP_COMPUTATION = 43,
    CLOSE_EFP_COMPUTATION = 44,
    LAST_TIMESTAMP = 45,
    SHORTABLE = 46,
    FUNDAMENTAL_RATIOS = 47,
    RT_VOLUME = 48,
    HALTED = 49,
    BID_YIELD = 50,
    ASK_YIELD = 51,
    LAST_YIELD = 52,
    CUST_OPTION_COMPUTATION = 53,
    TRADE_COUNT = 54,
    TRADE_RATE = 55,
    VOLUME_RATE = 56,
    LAST_RTH_TRADE = 57,
    RT_HISTORICAL_VOL = 58    /* realtime historical volatility -- according to the IB API docs at http://www.interactivebrokers.com/php/apiUsersGuide/apiguide/tables/generic_tick_types.htm */
} tr_tick_type_t;


/* what the heck are these? */
#define OPT_UNKNOWN           "?"
#define OPT_BROKER_DEALER     "b"
#define OPT_CUSTOMER          "c"
#define OPT_FIRM              "f"
#define OPT_ISEMM             "m"
#define OPT_FARMM             "n"
#define OPT_SPECIALIST        "y"

#define GROUPS         1
#define PROFILES       2
#define ALIASES        3


// internal use structure, treat as a reference/handle:
struct tws_instance;
typedef struct tws_instance tws_instance_t;


typedef void (*tws_func_t)(void *arg);

typedef void (*external_func_t)(int state, void *arg);
/* "func" calls this one once at startup and again at termination,
 * state == 0 indicates startup, state == 1 indicates termination, state == 2 indicates thread termination pending
 */

/*
 * user must specify the send / recv and close callbacks to transmit, receive and close the network connection to TWS
 */
typedef int tws_transmit_func_t(void *arg, const void *buf, unsigned int buflen);
typedef int tws_receive_func_t(void *arg, void *buf, unsigned int max_bufsize);
/* 'flush()' marks the end of the outgoing message: it should be transmitted ASAP */
typedef int tws_flush_func_t(void *arg);
/* open callback is invoked when tws_connect is invoked and no connection has been established yet (tws_connected() == false); return 0 on success; a twsclient_error_codes error code on failure. */
typedef int tws_open_func_t(void *arg);
/* close callback is invoked on error or when tws_disconnect is invoked */
typedef int tws_close_func_t(void *arg);

/* creates new tws client instance and
 * and records opaque user defined pointer to be supplied in all callbacks
 */
tws_instance_t *tws_create(void *opaque, tws_transmit_func_t *transmit, tws_receive_func_t *receive, tws_flush_func_t *flush, tws_open_func_t *open, tws_close_func_t *close);
/* tws_destroy() implicitly calls tws_disconnect() but for reasons of symmetry it is advised to explicitly invoke tws_disconnect() (<-> tws_connect()) before invoking tws_destroy() (<->tws_create()) */
void   tws_destroy(tws_instance_t *tws_instance);
int    tws_connected(tws_instance_t *tws_instance); /* true=1 or false=0 */
int    tws_event_process(tws_instance_t *tws_instance); /* dispatches event to a callback.c func */

/* init TWS structures to default values */
void   tws_init_tr_comboleg(tws_instance_t *tws, tr_comboleg_t *comboleg_ref);
void   tws_destroy_tr_comboleg(tws_instance_t *tws, tr_comboleg_t *comboleg_ref);

/* transmit connect message and wait for response */
int    tws_connect(tws_instance_t *tws, int client_id);
void   tws_disconnect(tws_instance_t *tws);

/* sends message REQ_SCANNER_PARAMETERS to IB/TWS */
int    tws_req_scanner_parameters(tws_instance_t *tws);
/* sends message REQ_SCANNER_SUBSCRIPTION to IB/TWS */
int    tws_req_scanner_subscription(tws_instance_t *tws, int ticker_id, tr_scanner_subscription_t *subscription);
/* sends message CANCEL_SCANNER_SUBSCRIPTION to IB/TWS */
int    tws_cancel_scanner_subscription(tws_instance_t *tws, int ticker_id);
/* sends message REQ_MKT_DATA to IB/TWS */
int    tws_req_mkt_data(tws_instance_t *tws, int ticker_id, tr_contract_t *contract, const char generic_tick_list[], int snapshot);
/* sends message REQ_HISTORICAL_DATA to IB/TWS */
int    tws_req_historical_data(tws_instance_t *tws, int ticker_id, tr_contract_t *contract, const char end_date_time[], const char duration_str[], const char bar_size_setting[], const char what_to_show[], int use_rth, int format_date);
/* sends message CANCEL_HISTORICAL_DATA to IB/TWS */
int    tws_cancel_historical_data(tws_instance_t *tws, int ticker_id);
/* sends message CANCEL_MKT_DATA to IB/TWS */
int    tws_cancel_mkt_data(tws_instance_t *tws, int ticker_id);
/* sends message EXERCISE_OPTIONS to IB/TWS */
int    tws_exercise_options(tws_instance_t *tws, int ticker_id, tr_contract_t *contract, int exercise_action, int exercise_quantity, const char account[], int exc_override);
/* sends message PLACE_ORDER to IB/TWS */
int    tws_place_order(tws_instance_t *tws, int order_id, tr_contract_t *contract, tr_order_t *order);
/* sends message CANCEL_ORDER to IB/TWS */
int    tws_cancel_order(tws_instance_t *tws, int order_id);
/* sends message REQ_OPEN_ORDERS to IB/TWS */
int    tws_req_open_orders(tws_instance_t *tws);
/* sends message REQ_ACCOUNT_DATA to IB/TWS */
int    tws_req_account_updates(tws_instance_t *tws, int subscribe, const char acct_code[]);
/* sends message REQ_EXECUTIONS to IB/TWS */
int    tws_req_executions(tws_instance_t *tws, int reqid, tr_exec_filter_t *filter);
/* sends message REQ_IDS to IB/TWS */
int    tws_req_ids(tws_instance_t *tws, int num_ids);
/* sends message REQ_CONTRACT_DATA to IB/TWS */
int    tws_req_contract_details(tws_instance_t *tws, int reqid, tr_contract_t *contract);
/* sends message REQ_MKT_DEPTH to IB/TWS */
int    tws_req_mkt_depth(tws_instance_t *tws, int ticker_id, tr_contract_t *contract, int num_rows);
/* sends message CANCEL_MKT_DEPTH to IB/TWS */
int    tws_cancel_mkt_depth(tws_instance_t *tws, int ticker_id);
/* sends message REQ_NEWS_BULLETINS to IB/TWS */
int    tws_req_news_bulletins(tws_instance_t *tws, int all_msgs);
/* sends message CANCEL_NEWS_BULLETINS to IB/TWS */
int    tws_cancel_news_bulletins(tws_instance_t *tws);
/* sends message SET_SERVER_LOGLEVEL to IB/TWS */
int    tws_set_server_log_level(tws_instance_t *tws, int level);
/* sends message REQ_AUTO_OPEN_ORDERS to IB/TWS */
int    tws_req_auto_open_orders(tws_instance_t *tws, int auto_bind);
/* sends message REQ_ALL_OPEN_ORDERS to IB/TWS */
int    tws_req_all_open_orders(tws_instance_t *tws);
/* sends message REQ_MANAGED_ACCTS to IB/TWS */
int    tws_req_managed_accts(tws_instance_t *tws);
/* sends message REQ_FA to IB/TWS */
int    tws_request_fa(tws_instance_t *tws, int fa_data_type);
/* sends message REPLACE_FA to IB/TWS */
int    tws_replace_fa(tws_instance_t *tws, int fa_data_type, const char cxml[]);
/* sends message REQ_CURRENT_TIME to IB/TWS */
int    tws_req_current_time(tws_instance_t *tws);
/* sends message REQ_FUNDAMENTAL_DATA to IB/TWS */
int    tws_req_fundamental_data(tws_instance_t *tws, int reqid, tr_contract_t *contract, const char report_type[]);
/* sends message CANCEL_FUNDAMENTAL_DATA to IB/TWS */
int    tws_cancel_fundamental_data(tws_instance_t *tws, int reqid);
/* sends message REQ_CALC_IMPLIED_VOLAT to IB/TWS */
int    tws_calculate_implied_volatility(tws_instance_t *tws, int reqid, tr_contract_t *contract, double option_price, double under_price);
/* sends message CANCEL_CALC_IMPLIED_VOLAT to IB/TWS */
int    tws_cancel_calculate_implied_volatility(tws_instance_t *tws, int reqid);
/* sends message REQ_CALC_OPTION_PRICE to IB/TWS */
int    tws_calculate_option_price(tws_instance_t *tws, int reqid, tr_contract_t *contract, double volatility, double under_price);
/* sends message CANCEL_CALC_OPTION_PRICE to IB/TWS */
int    tws_cancel_calculate_option_price(tws_instance_t *tws, int reqid);
/* sends message REQ_GLOBAL_CANCEL to IB/TWS */
int    tws_req_global_cancel(tws_instance_t *tws);
/* sends message REQ_MARKET_DATA_TYPE to IB/TWS */
int    tws_req_market_data_type(tws_instance_t *tws, market_data_type_t market_data_type);
/* sends message REQ_REAL_TIME_BARS to IB/TWS */
int    tws_request_realtime_bars(tws_instance_t *tws, int ticker_id, tr_contract_t *c, int bar_size, const char what_to_show[], int use_rth);
/* sends message CANCEL_REAL_TIME_BARS to IB/TWS */
int    tws_cancel_realtime_bars(tws_instance_t *tws, int ticker_id);

/**** 2 auxilliary routines */
int    tws_server_version(tws_instance_t *tws);
const char *tws_connection_time(tws_instance_t *tws);

/************************************ callbacks *************************************/
/* API users must implement some or all of these C functions; the comment before each function describes which incoming message(s) fire the given event: */

/* fired by: TICK_PRICE */
void event_tick_price(void *opaque, int ticker_id, tr_tick_type_t field, double price, int can_auto_execute);
/* fired by: TICK_PRICE (for modern versions, then immediately preceeded by an invocation of event_tick_price()), TICK_SIZE */
void event_tick_size(void *opaque, int ticker_id, tr_tick_type_t field, int size);
/* fired by: TICK_OPTION_COMPUTATION */
void event_tick_option_computation(void *opaque, int ticker_id, tr_tick_type_t type, double implied_vol, double delta, double opt_price, double pv_dividend, double gamma, double vega, double theta, double und_price);
/* fired by: TICK_GENERIC */
void event_tick_generic(void *opaque, int ticker_id, tr_tick_type_t type, double value);
/* fired by: TICK_STRING */
void event_tick_string(void *opaque, int ticker_id, tr_tick_type_t type, const char value[]);
/* fired by: TICK_EFP */
void event_tick_efp(void *opaque, int ticker_id, tr_tick_type_t tick_type, double basis_points, const char formatted_basis_points[], double implied_futures_price, int hold_days, const char future_expiry[], double dividend_impact, double dividends_to_expiry);
/* fired by: ORDER_STATUS */
void event_order_status(void *opaque, int order_id, const char status[], int filled, int remaining, double avg_fill_price, int perm_id, int parent_id, double last_fill_price, int client_id, const char why_held[]);
/* fired by: OPEN_ORDER */
void event_open_order(void *opaque, int order_id, const tr_contract_t *contract, const tr_order_t *order, const tr_order_status_t *ost);
/* fired by: OPEN_ORDER_END */
void event_open_order_end(void *opaque);
/* fired by: ACCT_VALUE */
void event_update_account_value(void *opaque, const char key[], const char val[], const char currency[], const char account_name[]);
/* fired by: PORTFOLIO_VALUE */
void event_update_portfolio(void *opaque, const tr_contract_t *contract, int position, double mkt_price, double mkt_value, double average_cost, double unrealized_pnl, double realized_pnl, const char account_name[]);
/* fired by: ACCT_UPDATE_TIME */
void event_update_account_time(void *opaque, const char time_stamp[]);
/* fired by: NEXT_VALID_ID */
void event_next_valid_id(void *opaque, int order_id);
/* fired by: CONTRACT_DATA */
void event_contract_details(void *opaque, int req_id, const tr_contract_details_t *contract_details);
/* fired by: CONTRACT_DATA_END */
void event_contract_details_end(void *opaque, int reqid);
/* fired by: BOND_CONTRACT_DATA */
void event_bond_contract_details(void *opaque, int req_id, const tr_contract_details_t *contract_details);
/* fired by: EXECUTION_DATA */
void event_exec_details(void *opaque, int order_id, const tr_contract_t *contract, const tr_execution_t *execution);
/* fired by: EXECUTION_DATA_END */
void event_exec_details_end(void *opaque, int reqid);
/* fired by: ERR_MSG */
void event_error(void *opaque, int id, int error_code, const char error_string[]);
/* fired by: MARKET_DEPTH */
void event_update_mkt_depth(void *opaque, int ticker_id, int position, int operation, int side, double price, int size);
/* fired by: MARKET_DEPTH_L2 */
void event_update_mkt_depth_l2(void *opaque, int ticker_id, int position, char *market_maker, int operation, int side, double price, int size);
/* fired by: NEWS_BULLETINS */
void event_update_news_bulletin(void *opaque, int msgid, int msg_type, const char news_msg[], const char origin_exch[]);
/* fired by: MANAGED_ACCTS */
void event_managed_accounts(void *opaque, const char accounts_list[]);
/* fired by: RECEIVE_FA */
void event_receive_fa(void *opaque, int fa_data_type, const char cxml[]);
/* fired by: HISTORICAL_DATA (possibly multiple times per incoming message) */
void event_historical_data(void *opaque, int reqid, const char date[], double open, double high, double low, double close, int volume, int bar_count, double wap, int has_gaps);
/* fired by: HISTORICAL_DATA  (once, after one or more invocations of event_historical_data()) */
void event_historical_data_end(void *opaque, int reqid, const char completion_from[], const char completion_to[]);
/* fired by: SCANNER_PARAMETERS */
void event_scanner_parameters(void *opaque, const char xml[]);
/* fired by: SCANNER_DATA (possibly multiple times per incoming message) */
void event_scanner_data(void *opaque, int ticker_id, int rank, tr_contract_details_t *cd, const char distance[], const char benchmark[], const char projection[], const char legs_str[]);
/* fired by: SCANNER_DATA (once, after one or more invocations of event_scanner_data()) */
void event_scanner_data_end(void *opaque, int ticker_id);
/* fired by: CURRENT_TIME */
void event_current_time(void *opaque, long time);
/* fired by: REAL_TIME_BARS */
void event_realtime_bar(void *opaque, int reqid, long time, double open, double high, double low, double close, long volume, double wap, int count);
/* fired by: FUNDAMENTAL_DATA */
void event_fundamental_data(void *opaque, int reqid, const char data[]);
/* fired by: DELTA_NEUTRAL_VALIDATION */
void event_delta_neutral_validation(void *opaque, int reqid, under_comp_t *und);
/* fired by: ACCT_DOWNLOAD_END */
void event_acct_download_end(void *opaque, char acct_name[]);
/* fired by: TICK_SNAPSHOT_END */
void event_tick_snapshot_end(void *opaque, int reqid);
/* fired by: MARKET_DATA_TYPE */
void event_market_data_type(void *opaque, int reqid, market_data_type_t data_type);

/* outgoing message IDs */
enum tws_outgoing_ids {
    REQ_MKT_DATA = 1,
    CANCEL_MKT_DATA = 2,
    PLACE_ORDER = 3,
    CANCEL_ORDER = 4,
    REQ_OPEN_ORDERS = 5,
    REQ_ACCOUNT_DATA = 6,
    REQ_EXECUTIONS = 7,
    REQ_IDS = 8,
    REQ_CONTRACT_DATA = 9,
    REQ_MKT_DEPTH = 10,
    CANCEL_MKT_DEPTH = 11,
    REQ_NEWS_BULLETINS = 12,
    CANCEL_NEWS_BULLETINS = 13,
    SET_SERVER_LOGLEVEL = 14,
    REQ_AUTO_OPEN_ORDERS = 15,
    REQ_ALL_OPEN_ORDERS = 16,
    REQ_MANAGED_ACCTS = 17,
    REQ_FA = 18,
    REPLACE_FA = 19,
    REQ_HISTORICAL_DATA = 20,
    EXERCISE_OPTIONS = 21,
    REQ_SCANNER_SUBSCRIPTION = 22,
    CANCEL_SCANNER_SUBSCRIPTION = 23,
    REQ_SCANNER_PARAMETERS = 24,
    CANCEL_HISTORICAL_DATA = 25,
    REQ_CURRENT_TIME = 49,
    REQ_REAL_TIME_BARS = 50,
    CANCEL_REAL_TIME_BARS = 51,
    REQ_FUNDAMENTAL_DATA = 52,
    CANCEL_FUNDAMENTAL_DATA = 53,
    REQ_CALC_IMPLIED_VOLAT = 54,
    REQ_CALC_OPTION_PRICE = 55,
    CANCEL_CALC_IMPLIED_VOLAT = 56,
    CANCEL_CALC_OPTION_PRICE = 57,
    REQ_GLOBAL_CANCEL = 58,
	REQ_MARKET_DATA_TYPE = 59,
};

#define  MIN_SERVER_VER_REAL_TIME_BARS 34
#define  MIN_SERVER_VER_SCALE_ORDERS  35
#define  MIN_SERVER_VER_SNAPSHOT_MKT_DATA 35
#define  MIN_SERVER_VER_SSHORT_COMBO_LEGS 35
#define  MIN_SERVER_VER_WHAT_IF_ORDERS 36
#define  MIN_SERVER_VER_CONTRACT_CONID 37
#define  MIN_SERVER_VER_PTA_ORDERS 39
#define  MIN_SERVER_VER_FUNDAMENTAL_DATA  40
#define  MIN_SERVER_VER_UNDER_COMP  40
#define  MIN_SERVER_VER_CONTRACT_DATA_CHAIN  40
#define  MIN_SERVER_VER_SCALE_ORDERS2  40
#define  MIN_SERVER_VER_ALGO_ORDERS 41
#define  MIN_SERVER_VER_EXECUTION_DATA_CHAIN 42
#define  MIN_SERVER_VER_NOT_HELD 44
#define  MIN_SERVER_VER_SEC_ID_TYPE 45
#define  MIN_SERVER_VER_PLACE_ORDER_CONID 46
#define  MIN_SERVER_VER_REQ_MKT_DATA_CONID 47
#define  MIN_SERVER_VER_REQ_CALC_IMPLIED_VOLAT 49
#define  MIN_SERVER_VER_REQ_CALC_OPTION_PRICE 50
#define  MIN_SERVER_VER_CANCEL_CALC_IMPLIED_VOLAT 50
#define  MIN_SERVER_VER_CANCEL_CALC_OPTION_PRICE 50
#define  MIN_SERVER_VER_SSHORTX_OLD 51
#define  MIN_SERVER_VER_SSHORTX 52
#define  MIN_SERVER_VER_REQ_GLOBAL_CANCEL 53
#define  MIN_SERVER_VER_HEDGE_ORDERS 54
#define  MIN_SERVER_VER_REQ_MARKET_DATA_TYPE 55
#define  MIN_SERVER_VER_OPT_OUT_SMART_ROUTING 56
#define  MIN_SERVER_VER_SMART_COMBO_ROUTING_PARAMS 57
#define  MIN_SERVER_VER_DELTA_NEUTRAL_CONID 58


enum tws_incoming_ids {
    TICK_PRICE = 1,
    TICK_SIZE = 2,
    ORDER_STATUS = 3,
    ERR_MSG = 4,
    OPEN_ORDER = 5,
    ACCT_VALUE = 6,
    PORTFOLIO_VALUE = 7,
    ACCT_UPDATE_TIME = 8,
    NEXT_VALID_ID = 9,
    CONTRACT_DATA = 10,
    EXECUTION_DATA = 11,
    MARKET_DEPTH = 12,
    MARKET_DEPTH_L2 = 13,
    NEWS_BULLETINS = 14,
    MANAGED_ACCTS = 15,
    RECEIVE_FA = 16,
    HISTORICAL_DATA = 17,
    BOND_CONTRACT_DATA = 18,
    SCANNER_PARAMETERS = 19,
    SCANNER_DATA = 20,
    TICK_OPTION_COMPUTATION = 21,
    TICK_GENERIC = 45,
    TICK_STRING = 46,
    TICK_EFP = 47,
    CURRENT_TIME = 49,
    REAL_TIME_BARS = 50,
    FUNDAMENTAL_DATA = 51,
    CONTRACT_DATA_END = 52,
    OPEN_ORDER_END = 53,
    ACCT_DOWNLOAD_END = 54,
    EXECUTION_DATA_END = 55,
    DELTA_NEUTRAL_VALIDATION = 56,
    TICK_SNAPSHOT_END = 57,
	MARKET_DATA_TYPE = 58,
};


/* ERROR codes */
typedef enum twsclient_error_codes {
    NO_VALID_ID = -1,
    NO_TWS_ERROR = 0, /* thanks to Gino for pointing out that 0 is special */
    UNKNOWN_TWS_ERROR = 500,
    ALREADY_CONNECTED = 501,
    CONNECT_FAIL = 502,
    UPDATE_TWS = 503,
    NOT_CONNECTED = 504,
    UNKNOWN_ID = 505,
    ZERO_BYTE_READ = 506,
    NULL_STRING_READ = 507,
    NO_BYTES_READ = 508,
    SOCKET_EXCEPTION = 509,

    FAIL_SEND_REQMKT = 510,
    FAIL_SEND_CANMKT = 511,
    FAIL_SEND_ORDER = 512,
    FAIL_SEND_ACCT = 513,
    FAIL_SEND_EXEC = 514,
    FAIL_SEND_CORDER = 515,
    FAIL_SEND_OORDER = 516,
    UNKNOWN_CONTRACT = 517,
    FAIL_SEND_REQCONTRACT = 518,
    FAIL_SEND_REQMKTDEPTH = 519,
    FAIL_SEND_CANMKTDEPTH = 520,
    FAIL_SEND_SERVER_LOG_LEVEL = 521,
    FAIL_SEND_FA_REQUEST = 522,
    FAIL_SEND_FA_REPLACE = 523,
    FAIL_SEND_REQSCANNER = 524,
    FAIL_SEND_CANSCANNER = 525,
    FAIL_SEND_REQSCANNERPARAMETERS = 526,
    FAIL_SEND_REQHISTDATA = 527,
    FAIL_SEND_CANHISTDATA = 528,
    FAIL_SEND_REQRTBARS = 529,
    FAIL_SEND_CANRTBARS = 530,
    FAIL_SEND_REQCURRTIME = 531,
    FAIL_SEND_REQFUNDDATA = 532,
    FAIL_SEND_CANFUNDDATA = 533,
    FAIL_SEND_REQCALCIMPLIEDVOLAT = 534,
    FAIL_SEND_REQCALCOPTIONPRICE = 535,
    FAIL_SEND_CANCALCIMPLIEDVOLAT = 536,
    FAIL_SEND_CANCALCOPTIONPRICE = 537,
    FAIL_SEND_REQGLOBALCANCEL = 538,
	FAIL_SEND_REQMARKETDATATYPE = 539,

    FAIL_SEND_BULLETINS = 596,
    FAIL_SEND_CBULLETINS = 597,
    FAIL_SEND_REQIDS = 598,
    FAIL_SEND_EXCERCISE_OPTIONS = 599,
} twsclient_error_code_t;

struct twsclient_errmsg {
    twsclient_error_code_t err_code;
    const char *err_msg;
};

#ifdef TWSAPI_GLOBALS
struct twsclient_errmsg twsclient_err_indication[] = {
    /* these correspond to enum twsclient_error_codes, save for code -1
     * usage: if(err_code >= 0) puts(twsclient_err_indication[err_code]);
     */
    { NO_TWS_ERROR, "No error" },
    { NO_VALID_ID, "No valid client ID" },
    { ALREADY_CONNECTED, "Already connected." },
    { CONNECT_FAIL, "Couldn't connect to TWS.  Confirm that \"Enable ActiveX and Socket Clients\" is enabled on the TWS \"Configure->API\" menu." },
    { UPDATE_TWS, "Your version of TWS is out of date and must be upgraded." },
    { NOT_CONNECTED, "Not connected" },
    { UNKNOWN_ID, "Fatal Error: Unknown message id."},
    { ZERO_BYTE_READ, "Unexplained zero bytes read." },
    { NULL_STRING_READ, "Null string read when expecting integer" },
    { NO_BYTES_READ, "Error: no bytes read or no null terminator found" },
    { SOCKET_EXCEPTION, "Exception caught while reading socket - " },

    { FAIL_SEND_REQMKT, "Request Market Data Sending Error - "},
    { FAIL_SEND_CANMKT, "Cancel Market Data Sending Error - "},
    { FAIL_SEND_ORDER, "Order Sending Error - "},
    { FAIL_SEND_ACCT, "Account Update Request Sending Error -"},
    { FAIL_SEND_EXEC, "Request For Executions Sending Error -"},
    { FAIL_SEND_CORDER, "Cancel Order Sending Error -"},
    { FAIL_SEND_OORDER, "Request Open Order Sending Error -"},
    { UNKNOWN_CONTRACT, "Unknown contract. Verify the contract details supplied."},
    { FAIL_SEND_REQCONTRACT, "Request Contract Data Sending Error - "},
    { FAIL_SEND_REQMKTDEPTH, "Request Market Depth Sending Error - "},
    { FAIL_SEND_CANMKTDEPTH, "Cancel Market Depth Sending Error - "},
    { FAIL_SEND_SERVER_LOG_LEVEL, "Set Server Log Level Sending Error - "},
    { FAIL_SEND_FA_REQUEST, "FA Information Request Sending Error - "},
    { FAIL_SEND_FA_REPLACE, "FA Information Replace Sending Error - "},
    { FAIL_SEND_REQSCANNER, "Request Scanner Subscription Sending Error - "},
    { FAIL_SEND_CANSCANNER, "Cancel Scanner Subscription Sending Error - "},
    { FAIL_SEND_REQSCANNERPARAMETERS, "Request Scanner Parameter Sending Error - "},
    { FAIL_SEND_REQHISTDATA, "Request Historical Data Sending Error - "},
    { FAIL_SEND_CANHISTDATA, "Cancel Historical Data Sending Error - "},
    { FAIL_SEND_REQRTBARS, "Request Real-time Bar Data Sending Error - "},
    { FAIL_SEND_CANRTBARS, "Cancel Real-time Bar Data Sending Error - "},
    { FAIL_SEND_REQCURRTIME, "Request Current Time Sending Error - "},
    { FAIL_SEND_REQFUNDDATA, "Request Fundamental Data Sending Error - "},
    { FAIL_SEND_CANFUNDDATA, "Cancel Fundamental Data Sending Error - "},
    /* since TWS API 9.64: */
    { FAIL_SEND_REQCALCIMPLIEDVOLAT, "Request Calculate Implied Volatility Sending Error - "},
    { FAIL_SEND_REQCALCOPTIONPRICE, "Request Calculate Option Price Sending Error - "},
    { FAIL_SEND_CANCALCIMPLIEDVOLAT, "Cancel Calculate Implied Volatility Sending Error - "},
    { FAIL_SEND_CANCALCOPTIONPRICE, "Cancel Calculate Option Price Sending Error - "},
    /* since TWS API 9.65: */
    { FAIL_SEND_REQGLOBALCANCEL, "Request Global Cancel Sending Error - "},
	/* since TWS API 9.66: */
	{ FAIL_SEND_REQMARKETDATATYPE, "Request Market Data Type Sending Error - "},

    /* related to bugfixes compared to the TWS Java implementation: */
    { FAIL_SEND_BULLETINS, "Request News Bulletins Sending Error - "},
    { FAIL_SEND_CBULLETINS, "Cancel News Bulletins Sending Error - "},
    { FAIL_SEND_REQIDS, "Request IDs Sending Error - "},
    { FAIL_SEND_EXCERCISE_OPTIONS, "Excercise Options Sending Error - "},

    /* -------------------------------------------------------------- */
    /* The following items are taken from the TWS API on-line documentation: the error code strings for errors reported by the TWS client in the non-5xx range: */

    /* Error Codes */
    { 100, "Max rate of messages per second has been exceeded." },
    { 101, "Max number of tickers has been reached." },
    { 102, "Duplicate ticker ID." },
    { 103, "Duplicate order ID." },
    { 104, "Can't modify a filled order." },
    { 105, "Order being modified does not match original order." },
    { 106, "Can't transmit order ID:" },
    { 107, "Cannot transmit incomplete order." },
    { 109, "Price is out of the range defined by the Percentage setting at order defaults frame. The order will not be transmitted." },
    { 110, "The price does not conform to the minimum price variation for this contract." },
    { 111, "The TIF (Tif type) and the order type are incompatible." },
    { 113, "The Tif option should be set to DAY for MOC and LOC orders." },
    { 114, "Relative orders are valid for stocks only." },
    { 115, "Relative orders for US stocks can only be submitted to SMART, SMART_ECN, INSTINET, or PRIMEX." },
    { 116, "The order cannot be transmitted to a dead exchange." },
    { 117, "The block order size must be at least 50." },
    { 118, "VWAP orders must be routed through the VWAP exchange." },
    { 119, "Only VWAP orders may be placed on the VWAP exchange." },
    { 120, "It is too late to place a VWAP order for today." },
    { 121, "Invalid BD flag for the order. Check \"Destination\" and \"BD\" flag." },
    { 122, "No request tag has been found for order:" },
    { 123, "No record is available for conid:" },
    { 124, "No market rule is available for conid:" },
    { 125, "Buy price must be the same as the best asking price." },
    { 126, "Sell price must be the same as the best bidding price." },
    { 129, "VWAP orders must be submitted at least three minutes before the start time." },
    { 131, "The sweep-to-fill flag and display size are only valid for US stocks routed through SMART, and will be ignored." },
    { 132, "This order cannot be transmitted without a clearing account." },
    { 133, "Submit new order failed." },
    { 134, "Modify order failed." },
    { 135, "Can't find order with ID =" },
    { 136, "This order cannot be cancelled." },
    { 137, "VWAP orders can only be cancelled up to three minutes before the start time." },
    { 138, "Could not parse ticker request:" },
    { 139, "Parsing error:" },
    { 140, "The size value should be an integer:" },
    { 141, "The price value should be a double:" },
    { 142, "Institutional customer account does not have account info" },
    { 143, "Requested ID is not an integer number." },
    { 144, "Order size does not match total share allocation.  To adjust the share allocation, right-click on the order and select “Modify > Share Allocation.”" },
    { 145, "Error in validating entry fields -" },
    { 146, "Invalid trigger method." },
    { 147, "The conditional contract info is incomplete." },
    { 148, "A conditional order can only be submitted when the order type is set to limit or market." },
    { 151, "This order cannot be transmitted without a user name." },
    { 152, "The \"hidden\" order attribute may not be specified for this order." },
    { 153, "EFPs can only be limit orders." },
    { 154, "Orders cannot be transmitted for a halted security." },
    { 155, "A sizeOp order must have a username and account." },
    { 156, "A SizeOp order must go to IBSX" },
    { 157, "An order can be EITHER Iceberg or Discretionary. Please remove either the Discretionary amount or the Display size." },
    { 158, "You must specify an offset amount or a percent offset value." },
    { 159, "The percent offset value must be between 0% and 100%." },
    { 160, "The size value cannot be zero." },
    { 161, "Cancel attempted when order is not in a cancellable state. Order permId =" },
    { 162, "Historical market data Service error message." },
    { 163, "The price specified would violate the percentage constraint specified in the default order settings." },
    { 164, "There is no market data to check price percent violations." },
    { 165, "Historical market Data Service query message." },
    { 166, "HMDS Expired Contract Violation." },
    { 167, "VWAP order time must be in the future." },
    { 168, "Discretionary amount does not conform to the minimum price variation for this contract." },

    { 200, "No security definition has been found for the request." },
    { 201, "Order rejected - Reason:" },
    { 202, "Order cancelled - Reason:" },
    { 203, "The security <security> is not available or allowed for this account." },

    { 300, "Can't find EId with ticker Id:" },
    { 301, "Invalid ticker action:" },
    { 302, "Error parsing stop ticker string:" },
    { 303, "Invalid action:" },
    { 304, "Invalid account value action:" },
    { 305, "Request parsing error, the request has been ignored." },
    { 306, "Error processing DDE request:" },
    { 307, "Invalid request topic:" },
    { 308, "Unable to create the 'API' page in TWS as the maximum number of pages already exists." },
    { 309, "Max number (3) of market depth requests has been reached.   Note: TWS currently limits users to a maximum of 3 distinct market depth requests. This same restriction applies to API clients, however API clients may make multiple market depth requests for the same security." },
    { 310, "Can't find the subscribed market depth with tickerId:" },
    { 311, "The origin is invalid." },
    { 312, "The combo details are invalid." },
    { 313, "The combo details for leg '<leg number>' are invalid." },
    { 314, "Security type 'BAG' requires combo leg details." },
    { 315, "Stock combo legs are restricted to SMART order routing." },
    { 316, "Market depth data has been HALTED. Please re-subscribe." },
    { 317, "Market depth data has been RESET. Please empty deep book contents before applying any new entries." },
    { 319, "Invalid log level <log level>" },
    { 320, "Server error when reading an API client request." },
    { 321, "Server error when validating an API client request." },
    { 322, "Server error when processing an API client request." },
    { 323, "Server error: cause - %s" },
    { 324, "Server error when reading a DDE client request (missing information)." },
    { 325, "Discretionary orders are not supported for this combination of exchange and order type." },
    { 326, "Unable to connect as the client id is already in use. Retry with a unique client id." },
    { 327, "Only API connections with clientId set to 0 can set the auto bind TWS orders property." },
    { 328, "Trailing stop orders can be attached to limit or stop-limit orders only." },
    { 329, "Order modify failed. Cannot change to the new order type." },
    { 330, "Only FA or STL customers can request managed accounts list." },
    { 331, "Internal error. FA or STL does not have any managed accounts." },
    { 332, "The account codes for the order profile are invalid." },
    { 333, "Invalid share allocation syntax." },
    { 334, "Invalid Good Till Date order" },
    { 335, "Invalid delta: The delta must be between 0 and 100." },
    { 336, "The time or time zone is invalid.   The correct format is 'hh:mm:ss xxx' where 'xxx' is an optionally specified time-zone. E.g.: 15:59:00 EST.   Note that there is a space between the time and the time zone.   If no time zone is specified, local time is assumed." },
    { 337, "The date, time, or time-zone entered is invalid. The correct format is yyyymmdd hh:mm:ss xxx where yyyymmdd and xxx are optional. E.g.: 20031126 15:59:00 EST.   Note that there is a space between the date and time, and between the time and time-zone.   If no date is specified, current date is assumed.   If no time-zone is specified, local time-zone is assumed." },
    { 338, "Good After Time orders are currently disabled on this exchange." },
    { 339, "Futures spread are no longer supported. Please use combos instead." },
    { 340, "Invalid improvement amount for box auction strategy." },
    { 341, "Invalid delta. Valid values are from 1 to 100.   You can set the delta from the \"Pegged to Stock\" section of the Order Ticket Panel, or by selecting Page/Layout from the main menu and adding the Delta column." },
    { 342, "Pegged order is not supported on this exchange." },
    { 343, "The date, time, or time-zone entered is invalid. The correct format is yyyymmdd hh:mm:ss xxx where yyyymmdd and xxx are optional. E.g.: 20031126 15:59:00 EST.   Note that there is a space between the date and time, and between the time and time-zone.   If no date is specified, current date is assumed.   If no time-zone is specified, local time-zone is assumed." },
    { 344, "The account logged into is not a financial advisor account." },
    { 345, "Generic combo is not supported for FA advisor account." },
    { 346, "Not an institutional account or an away clearing account." },
    { 347, "Short sale slot value must be 1 (broker holds shares) or 2 (delivered from elsewhere)." },
    { 348, "Order not a short sale -- type must be SSHORT to specify short sale slot." },
    { 349, "Generic combo does not support \"Good After\" attribute." },
    { 350, "Minimum quantity is not supported for best combo order." },
    { 351, "The \"Regular Trading Hours only\" flag is not valid for this order." },
    { 352, "Short sale slot value of 2 (delivered from elsewhere) requires location." },
    { 353, "Short sale slot value of 1 requires no location be specified." },
    { 354, "Not subscribed to requested market data." },
    { 355, "Order size does not conform to market rule." },
    { 356, "Smart-combo order does not support OCA group." },
    { 357, "Your client version is out of date." },
    { 358, "Smart combo child order not supported." },
    { 359, "Combo order only supports reduce on fill without block(OCA)." },
    { 360, "No whatif check support for smart combo order." },
    { 361, "Invalid trigger price." },
    { 362, "Invalid adjusted stop price." },
    { 363, "Invalid adjusted stop limit price." },
    { 364, "Invalid adjusted trailing amount." },
    { 365, "No scanner subscription found for ticker id:" },
    { 366, "No historical data query found for ticker id:" },
    { 367, "Volatility type if set must be 1 or 2 for VOL orders. Do not set it for other order types." },
    { 368, "Reference Price Type must be 1 or 2 for dynamic volatility management. Do not set it for non-VOL orders." },
    { 369, "Volatility orders are only valid for US options." },
    { 370, "Dynamic Volatility orders must be SMART routed, or trade on a Price Improvement Exchange." },
    { 371, "VOL order requires positive floating point value for volatility. Do not set it for other order types." },
    { 372, "Cannot set dynamic VOL attribute on non-VOL order." },
    { 373, "Can only set stock range attribute on VOL or RELATIVE TO STOCK order." },
    { 374, "If both are set, the lower stock range attribute must be less than the upper stock range attribute." },
    { 375, "Stock range attributes cannot be negative." },
    { 376, "The order is not eligible for continuous update. The option must trade on a cheap-to-reroute exchange." },
    { 377, "Must specify valid delta hedge order aux. price." },
    { 378, "Delta hedge order type requires delta hedge aux. price to be specified." },
    { 379, "Delta hedge order type requires that no delta hedge aux. price be specified." },
    { 380, "This order type is not allowed for delta hedge orders." },
    { 381, "Your DDE.dll needs to be upgraded." },
    { 382, "The price specified violates the number of ticks constraint specified in the default order settings." },
    { 383, "The size specified violates the size constraint specified in the default order settings." },
    { 384, "Invalid DDE array request." },
    { 385, "Duplicate ticker ID for API scanner subscription." },
    { 386, "Duplicate ticker ID for API historical data query." },
    { 387, "Unsupported order type for this exchange and security type." },
    { 388, "Order size is smaller than the minimum requirement." },
    { 389, "Supplied routed order ID is not unique." },
    { 390, "Supplied routed order ID is invalid." },
    { 391, "The time or time-zone entered is invalid. The correct format is hh:mm:ss xxx where xxx is an optionally specified time-zone. E.g.: 15:59:00 EST.   Note that there is a space between the time and the time zone.   If no time zone is specified, local time is assumed.   " },
    { 392, "Invalid order: contract expired." },
    { 393, "Short sale slot may be specified for delta hedge orders only." },
    { 394, "Invalid Process Time: must be integer number of milliseconds between 100 and 2000.  Found:" },
    { 395, "Due to system problems, orders with OCA groups are currently not being accepted." },
    { 396, "Due to system problems, application is currently accepting only Market and Limit orders for this contract." },
    { 397, "Due to system problems, application is currently accepting only Market and Limit orders for this contract." },
    { 398, "< > cannot be used as a condition trigger." },
    { 399, "Order message error" },

    { 400, "Algo order error." },
    { 401, "Length restriction." },
    { 402, "Conditions are not allowed for this contract." },
    { 403, "Invalid stop price." },
    { 404, "Shares for this order are not immediately available for short sale. The order will be held while we attempt to locate the shares." },
    { 405, "The child order quantity should be equivalent to the parent order size." },
    { 406, "The currency < > is not allowed." },
    { 407, "The symbol should contain valid non-unicode characters only." },
    { 408, "Invalid scale order increment." },
    { 409, "Invalid scale order. You must specify order component size." },
    { 410, "Invalid subsequent component size for scale order." },
    { 411, "The \"Outside Regular Trading Hours\" flag is not valid for this order." },
    { 412, "The contract is not available for trading." },
    { 413, "What-if order should have the transmit flag set to true." },
    { 414, "Snapshot market data subscription is not applicable to generic ticks." },
    { 415, "Wait until previous RFQ finishes and try again." },
    { 416, "RFQ is not applicable for the contract. Order ID:" },
    { 417, "Invalid initial component size for scale order." },
    { 418, "Invalid scale order profit offset." },
    { 419, "Missing initial component size for scale order." },
    { 420, "Invalid real-time query." },
    { 421, "Invalid route." },
    { 422, "The account and clearing attributes on this order may not be changed." },
    { 423, "Cross order RFQ has been expired. THI committed size is no longer available. Please open order dialog and verify liquidity allocation." },
    { 424, "FA Order requires allocation to be specified." },
    { 425, "FA Order requires per-account manual allocations because there is no common clearing instruction. Please use order dialog Adviser tab to enter the allocation." },
    { 426, "None of the accounts have enough shares." },
    { 427, "Mutual Fund order requires monetary value to be specified." },
    { 428, "Mutual Fund Sell order requires shares to be specified." },
    { 429, "Delta neutral orders are only supported for combos (BAG security type)." },
    { 430, "We are sorry, but fundamentals data for the security specified is not available." },
    { 431, "What to show field is missing or incorrect." },
    { 432, "Commission must not be negative." },
    { 433, "Invalid \"Restore size after taking profit\" for multiple account allocation scale order." },
    { 434, "The order size cannot be zero." },
    { 435, "You must specify an account." },
    { 436, "You must specify an allocation (either a single account, group, or profile)." },
    { 437, "Order can have only one flag Outside RTH or Allow PreOpen." },
    { 438, "The application is now locked." },
    { 439, "Order processing failed. Algorithm definition not found." },
    { 440, "Order modify failed. Algorithm cannot be modified." },
    { 441, "Algo attributes validation failed:" },
    { 442, "Specified algorithm is not allowed for this order." },
    { 443, "Order processing failed. Unknown algo attribute." },
    { 444, "Volatility Combo order is not yet acknowledged. Cannot submit changes at this time." },
    { 445, "The RFQ for this order is no longer valid." },
    { 446, "Missing scale order profit offset." },
    { 447, "Missing scale price adjustment amount or interval." },
    { 448, "Invalid scale price adjustment interval." },
    { 449, "Unexpected scale price adjustment amount or interval." },
    { 450, "Dividend schedule query failed." },                                /* was listed as '40' on the IB/TWS web site */

    /* System Message Codes */
    { 1100, "Connectivity between IB and TWS has been lost." },
    { 1101, "Connectivity between IB and TWS has been restored- data lost.*" },
    { 1102, "Connectivity between IB and TWS has been restored- data maintained." },
    { 1300, "TWS socket port has been reset and this connection is being dropped.   Please reconnect on the new port - <port_num>   *Market and account data subscription requests must be resubmitted." },

    /* Warning Message Codes */
    { 2100, "New account data requested from TWS.  API client has been unsubscribed from account data." },
    { 2101, "Unable to subscribe to account as the following clients are subscribed to a different account." },
    { 2102, "Unable to modify this order as it is still being processed." },
    { 2103, "A market data farm is disconnected." },
    { 2104, "A market data farm is connected." },
    { 2105, "A historical data farm is disconnected." },
    { 2106, "A historical data farm is connected." },
    { 2107, "A historical data farm connection has become inactive but should be available upon demand." },
    { 2108, "A market data farm connection has become inactive but should be available upon demand." },
    { 2109, "Order Event Warning: Attribute “Outside Regular Trading Hours” is ignored based on the order type and destination. PlaceOrder is now processed." },
    { 2110, "Connectivity between TWS and server is broken. It will be restored automatically." },
};

const char *tws_incoming_msg_names[] = {
    "tick_price", "tick_size", "order_status", "err_msg", "open_order",
    "acct_value", "portfolio_value", "acct_update_time", "next_valid_id",
    "contract_data", "execution_data", "market_depth", "market_depth_l2",
    "news_bulletins", "managed_accts", "receive_fa", "historical_data",
    "bond_contract_data", "scanner_parameters", "scanner_data", "tick_option_computation",
    /* place holders follow */
    "msg22", "msg23", "msg24", "msg25", "msg26", "msg27", "msg28", "msg29", "msg30",
    "msg31", "msg32", "msg33", "msg34", "msg35", "msg36", "msg37", "msg38", "msg39",
    "msg40", "msg41", "msg42", "msg43", "msg44",
    /* end of place holders */
    "tick_generic", "tick_string", "tick_efp", "msg48",
    "current_time", "realtime_bars",
    "fundamental_data",
    "contract_data_end", "open_order_end", "acct_download_end", "execution_data_end",
    "delta_neutral_validation", "tick_snapshot_end", "market_data_type"
};

/* map tr_tick_type_t to 'descriptive' string */
const char *tws_tick_type_names[] = {
    "bidSize",
    "bidPrice",
    "askPrice",
    "askSize",
    "lastPrice",
    "lastSize",
    "high",
    "low",
    "volume",
    "close",
    "bidOptComp",
    "askOptComp",
    "lastOptComp",
    "modelOptComp",
    "open",
    "13WeekLow",
    "13WeekHigh",
    "26WeekLow",
    "26WeekHigh",
    "52WeekLow",
    "52WeekHigh",
    "AvgVolume",
    "OpenInterest",
    "OptionHistoricalVolatility",
    "OptionImpliedVolatility",
    "OptionBidExchStr",
    "OptionAskExchStr",
    "OptionCallOpenInterest",
    "OptionPutOpenInterest",
    "OptionCallVolume",
    "OptionPutVolume",
    "IndexFuturePremium",
    "bidExch",
    "askExch",
    "auctionVolume",
    "auctionPrice",
    "auctionImbalance",
    "markPrice",
    "bidEFP",
    "askEFP",
    "lastEFP",
    "openEFP",
    "highEFP",
    "lowEFP",
    "closeEFP",
    "lastTimestamp",
    "shortable",
    "fundamentals",
    "RTVolume",
    "halted",
    "bidYield",
    "askYield",
    "lastYield",
    "custOptComp",
    "trades",
    "trades/min",
    "volume/min",
    "lastRTHTrade",
    "RTHistoricalVolatility"
};

const char *fa_msg_name[] = { "GROUPS", "PROFILES", "ALIASES" };
const char *tws_market_data_type_name[] = { "Real Time", "Frozen" };
static const unsigned int d_nan[2] = {~0U, ~(1U<<31)};
const double *dNAN = (const double *)(const void *) d_nan;
#else
extern struct twsclient_errmsg twsclient_err_indication[];
extern const char *tws_incoming_msg_names[];
extern const char *tws_tick_type_names[];
extern const char *fa_msg_name[];
extern const char *tws_market_data_type_name[];
extern const double *dNAN;
#endif /* TWSAPI_GLOBALS */

const struct twsclient_errmsg *tws_strerror(int errcode);


#define fa_msg_type_name(x) (((x) >= GROUPS && (x) <= ALIASES) ? fa_msg_name[(x) - 1] : 0)
#define tick_type_name(x) (((x) >= BID_SIZE && (x) < sizeof(tws_tick_type_names) / sizeof(tws_tick_type_names[0])) ? tws_tick_type_names[x] : 0)
#define market_data_type_name(x) (((x) >= MDT_UNKNOWN && (x) <= FROZEN) ? tws_market_data_type_name[x] : 0)

#define MODEL_OPTION 13

#ifdef __cplusplus
}
#endif

#endif /* TWSAPI_H_ */
