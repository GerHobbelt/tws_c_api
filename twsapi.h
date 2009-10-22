#ifndef TWSAPI_H_
#define TWSAPI_H_

#ifdef __cplusplus
extern "C" {
#endif

/* public C API */
#define TWSCLIENT_VERSION 46

    typedef struct under_comp {
        double u_price;
        double u_delta;
        int    u_conid;
    } under_comp_t;

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

    typedef struct tr_contract_details {
        double                     d_mintick;
        double                     d_coupon;
        contract_details_summary_t d_summary;
        char *                     d_market_name;
        char *                     d_trading_class;
        char *                     d_order_types;
        char *                     d_valid_exchanges;
        char *                     d_cusip;
        char *                     d_maturity;
        char *                     d_issue_date;
        char *                     d_ratings;
        char *                     d_bond_type;
        char *                     d_coupon_type;
        char *                     d_desc_append;
        char *                     d_next_option_date;
        char *                     d_next_option_type;
        char *                     d_notes;
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
        unsigned                   d_convertible: 1;
        unsigned                   d_callable: 1;
        unsigned                   d_putable: 1;
        unsigned                   d_next_option_partial:1;
    } tr_contract_details_t;

    typedef struct tr_comboleg {
        char *co_action; /* BUY, SELL */
        char *co_exchange;
        char *co_designated_location; /* set to "" if unused, as usual */
        int   co_conid;
        int   co_ratio;
        int   co_open_close;
        int   co_short_sale_slot; /* 1 = clearing broker, 2 = third party */
    } tr_comboleg_t;

    typedef struct tr_contract {
        under_comp_t  *c_undercomp;
        double         c_strike;
        char *         c_symbol;
        char *         c_sectype;
        char *         c_exchange;
        char *         c_primary_exch;
        char *         c_expiry;
        char *         c_currency;
        char *         c_right;
        char *         c_local_symbol;
        char *         c_multiplier;
        char *         c_combolegs_descrip;
        char *         c_secid_type;
        char *         c_secid;
        tr_comboleg_t *c_comboleg;
        long           c_num_combolegs;
        int            c_conid;
        unsigned       c_include_expired:1;
    } tr_contract_t;

    struct tag_value {
        char *t_tag;
        char *t_val;
    };

    typedef struct tr_order {   
        double   o_discretionary_amt;
        double   o_lmt_price;
        double   o_aux_price;
        double   o_percent_offset;
        double   o_nbbo_price_cap;
        double   o_starting_price;
        double   o_stock_ref_price;
        double   o_delta;
        double   o_stock_range_lower;
        double   o_stock_range_upper;
        double   o_volatility;
        double   o_delta_neutral_aux_price;
        double   o_trail_stop_price;
        double   o_basis_points;
        double   o_scale_price_increment;
        char *   o_algo_strategy;
        char *   o_good_after_time;
        char *   o_good_till_date;
        char *   o_fagroup;
        char *   o_famethod;
        char *   o_fapercentage;
        char *   o_faprofile;
        char *   o_action;
        char *   o_order_type;
        char *   o_tif;
        char *   o_oca_group;
        char *   o_account;
        char *   o_open_close;
        char *   o_orderref;
        char *   o_designated_location;
        char *   o_rule80a;
        char *   o_settling_firm;
        char *   o_delta_neutral_order_type;
        char *   o_clearing_account;
        char *   o_clearing_intent;
        struct tag_value *o_tvector; /* array of length o_tval_count, API user responsible for alloc/free */
        int      o_tval_count;       /* how many tag values are in o_tvector, 0 if unused */
        int      o_orderid;
        int      o_total_quantity;
        int      o_origin;
#define CUSTOMER 0
#define FIRM 1
        int      o_clientid;
        int      o_permid;
        int      o_parentid;
        int      o_display_size;
        int      o_trigger_method;
        int      o_min_qty;
        int      o_volatility_type;
        int      o_reference_price_type;
        int      o_basis_points_type;
        int      o_scale_subs_level_size;
        int      o_scale_init_level_size;
        unsigned o_oca_type:3;
#define CANCEL_WITH_BLOCK 1
#define REDUCE_WITH_BLOCK 2
#define REDUCE_NON_BLOCK 3
        unsigned o_auction_strategy:3;
#define AUCTION_MATCH 1
#define AUCTION_IMPROVEMENT 2
#define AUCTION_TRANSPARENT 3
        unsigned o_short_sale_slot:2; /*1 or 2 */
        unsigned o_override_percentage_constraints:1;
        unsigned o_firm_quote_only:1;
        unsigned o_etrade_only:1;
        unsigned o_all_or_none:1;
        unsigned o_outside_rth: 1; /* ignore_rth disappeared */
        unsigned o_hidden: 1;
        unsigned o_transmit: 1;
        unsigned o_block_order: 1;
        unsigned o_sweep_to_fill: 1;
        unsigned o_continuous_update: 1;
        unsigned o_whatif: 1;
        unsigned o_not_held: 1;
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

/* what the heck are these? */
#define OPT_UNKNOWN  "?"
#define OPT_BROKER_DEALER  "b"
#define OPT_CUSTOMER  "c"
#define OPT_FIRM  "f"
#define OPT_ISEMM  "m"
#define OPT_FARMM  "n"
#define OPT_SPECIALIST  "y"

#define GROUPS 1
#define PROFILES 2
#define ALIASES 3
    typedef void (*tws_func_t)(void *arg);

    /* must return 0 on success, -1 on failure */
    typedef int (*start_thread_t)(tws_func_t func, void *arg);
    /* it's up to user whether to use threads or not */
#define TWS_NO_THREAD  ((start_thread_t) 0)

    typedef void (*external_func_t)(int);
    /* "func" calls this one once at startup and again at termination,
     * param 0 indicates startup, param 1 indicates termination
     */

    /* user is responsible for implementing this function, see example.c */
    typedef int (*resolve_name_t)(const void *name /*IN*/, void *addr /*OUT*/, long *addr_len /*IN,OUT*/);
    /* returns -1 on failure, 0 otherwise, addr_len 4 means ipv4, 16 means ipv6 */

    /* creates new tws client instance, starts reader thread and 
     * and records opaque user defined pointer to be supplied in all callbacks
     */
    void  *tws_create(start_thread_t start_thread, void *opaque, external_func_t myfunc);
    void   tws_destroy(void *tws_instance);
    int    tws_connected(void *tws_instance); /* true=1 or false=0 */
    int    tws_event_process(void *tws_instance); /* dispatches event to a callback.c func */

    int    tws_connect(void *tws, const char host[], unsigned short port, int clientid, resolve_name_t resolv_func);
    void   tws_disconnect(void *tws);
    int    tws_req_scanner_parameters(void *tws);
    int    tws_req_scanner_subscription(void *tws, int ticker_id, tr_scanner_subscription_t *subscription);
    int    tws_cancel_scanner_subscription(void *tws, int ticker_id);
    int    tws_req_mkt_data(void *tws, int ticker_id, tr_contract_t *contract, const char generic_tick_list[], long snapshot);
    int    tws_req_historical_data(void *tws, int ticker_id, tr_contract_t *contract, const char end_date_time[], const char duration_str[], const char bar_size_setting[], const char what_to_show[], int use_rth, int format_date);
    int    tws_cancel_historical_data(void *tws, int ticker_id);
    int    tws_cancel_mkt_data(void *tws, int ticker_id);
    int    tws_exercise_options(void *tws, int ticker_id, tr_contract_t *contract, int exercise_action, int exercise_quantity, const char account[], int override);
    int    tws_place_order(void *tws, long order_id, tr_contract_t *contract, tr_order_t *order);
    int    tws_cancel_order(void *tws, long order_id);
    int    tws_req_open_orders(void *tws);
    int    tws_req_account_updates(void *tws, int subscribe, const char acct_code[]);
    int    tws_req_executions(void *tws, int reqid, tr_exec_filter_t *filter);
    int    tws_req_ids(void *tws, int num_ids);
    int    tws_req_contract_details(void *tws, int reqid, tr_contract_t *contract);    
    int    tws_req_mkt_depth(void *tws, int ticker_id, tr_contract_t *contract, int num_rows);
    int    tws_cancel_mkt_depth(void *tws, int ticker_id);
    int    tws_req_news_bulletins(void *tws, int all_msgs);
    int    tws_cancel_news_bulletins(void *tws);
    int    tws_set_server_log_level(void *tws, int level);
    int    tws_req_auto_open_orders(void *tws, int auto_bind);
    int    tws_req_all_open_orders(void *tws);
    int    tws_req_managed_accts(void *tws);
    int    tws_request_fa(void *tws, long fa_data_type);
    int    tws_replace_fa(void *tws, long fa_data_type, const char cxml[]);
    int    tws_req_current_time(void *tws);
    int    tws_request_realtime_bars(void *tws, int ticker_id, tr_contract_t *c, int bar_size, const char what_to_show[], int use_rth);
    int    tws_cancel_realtime_bars(void *tws, int ticker_id);
    /**** 2 auxilliary routines */
    int    tws_server_version(void *tws);
    const char *tws_connection_time(void *tws);
/************************************ callbacks *************************************/
/* API users must implement some or all of these C functions */
    void event_tick_price(void *opaque, int ticker_id, long field, double price,
                          int can_auto_execute);
    void event_tick_size(void *opaque, int ticker_id, long field, int size);
    void event_tick_option_computation(void *opaque, int ticker_id, int type, double implied_vol, double delta, double model_price, double pr_dividend);
    void event_tick_generic(void *opaque, int ticker_id, int type, double value);
    void event_tick_string(void *opaque, int ticker_id, int type, const char value[]);
    void event_tick_efp(void *opaque, int ticker_id, int tick_type, double basis_points, const char formatted_basis_points[], double implied_futures_price, int hold_days, const char future_expiry[], double dividend_impact, double dividends_to_expiry);
    void event_order_status(void *opaque, long order_id, const char status[],
                            int filled, int remaining, double avg_fill_price, int perm_id,
                            int parent_id, double last_fill_price, int client_id, const char why_held[]);
    void event_open_order(void *opaque, long order_id, const tr_contract_t *contract, const tr_order_t *order, const tr_order_status_t *ost);
    void event_connection_closed(void *opaque);
    void event_update_account_value(void *opaque, const char key[], const char val[],
                                    const char currency[], const char account_name[]);
    void event_update_portfolio(void *opaque, const tr_contract_t *contract, int position,
                                double mkt_price, double mkt_value, double average_cost,
                                double unrealized_pnl, double realized_pnl, const char account_name[]);
    void event_update_account_time(void *opaque, const char time_stamp[]);
    void event_next_valid_id(void *opaque, long order_id);
    void event_contract_details(void *opaque, int req_id, const tr_contract_details_t *contract_details);
    void event_bond_contract_details(void *opaque, int req_id, const tr_contract_details_t *contract_details);
    void event_exec_details(void *opaque, long order_id, const tr_contract_t *contract,
                            const tr_execution_t *execution);
    void event_error(void *opaque, int id, int error_code, const char error_string[]);
    void event_update_mkt_depth(void *opaque, int ticker_id, int position, int operation, int side,
                                double price, int size);
    void event_update_mkt_depth_l2(void *opaque, int ticker_id, int position,
                                   char *market_maker, int operation, int side, double price, int size);
    void event_update_news_bulletin(void *opaque, int msgid, int msg_type, const char news_msg[],
                                    const char origin_exch[]);
    void event_managed_accounts(void *opaque, const char accounts_list[]);
    void event_receive_fa(void *opaque, long fa_data_type, const char cxml[]);
    void event_historical_data(void *opaque, int reqid, const char date[], double open, double high, double low, double close, int volume, int bar_count, double wap, int has_gaps);
    void event_scanner_parameters(void *opaque, const char xml[]);
    void event_scanner_data(void *opaque, int ticker_id, int rank, tr_contract_details_t *cd, const char distance[], const char benchmark[], const char projection[], const char legs_str[]);
    void event_scanner_data_end(void *opaque, int ticker_id);
    void event_current_time(void *opaque, long time);
    void event_realtime_bar(void *opaque, int reqid, long time, double open, double high, double low, double close, long volume, double wap, int count);
    void event_fundamental_data(void *opaque, int reqid, const char data[]);
    void event_contract_details_end(void *opaque, int reqid);
    void event_open_order_end(void *opaque);
    void event_delta_neutral_validation(void *opaque, int reqid, under_comp_t *und);
    void event_acct_download_end(void *opaque, char acct_name[]);
    void event_exec_details_end(void *opaque, int reqid);
    void event_tick_snapshot_end(void *opaque, int reqid);

/*outgoing message IDs */
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
        CANCEL_FUNDAMENTAL_DATA = 53
    };

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
        TICK_SNAPSHOT_END = 57
    };


/* ERROR codes */
    enum twsclient_error_codes {
        NO_VALID_ID = -1,
        NO_ERROR, /* thanks to Gino for pointing out that 0 is special */
        ALREADY_CONNECTED,
        CONNECT_FAIL,
        UPDATE_TWS,
        NOT_CONNECTED,
        UNKNOWN_ID,
        FAIL_SEND_REQMKT,
        FAIL_SEND_CANMKT,
        FAIL_SEND_ORDER,
        FAIL_SEND_ACCT,
        FAIL_SEND_EXEC,
        FAIL_SEND_CORDER,
        FAIL_SEND_OORDER,
        UNKNOWN_CONTRACT,  
        FAIL_SEND_REQCONTRACT,
        FAIL_SEND_REQMKTDEPTH,
        FAIL_SEND_CANMKTDEPTH,
        FAIL_SEND_SERVER_LOG_LEVEL,
        FAIL_SEND_FA_REQUEST,
        FAIL_SEND_FA_REPLACE,
        FAIL_SEND_REQSCANNERPARAMETERS,
        FAIL_SEND_CANSCANNER,
        FAIL_SEND_REQSCANNER,
        FAIL_SEND_REQRTBARS,
        FAIL_SEND_CANRTBARS,
        FAIL_SEND_REQCURRTIME,
        FAIL_SEND_REQFUNDDATA,
        FAIL_SEND_CANFUNDDATA
    };

    struct twsclient_errmsg {
        long err_code;
        char *err_msg;
    };

#ifdef TWSAPI_GLOBALS
    struct twsclient_errmsg twsclient_err_indication[] = {
        /* these correspond to enum twsclient_error_codes, save for code -1 
         * usage: if(err_code >= 0) puts(twsclient_err_indication[err_code]);
         */
        { 0, "No error" },
        { 501, "Already connected." },
        { 502, "Couldn't connect to TWS.  Confirm that \"Enable ActiveX and Socket Clients\" is enabled on the TWS \"Configure->API\" menu." },
        { 503, "The TWS is out of date and must be upgraded."} , 
        { 504,  "Not connected" },
        { 505, "Fatal Error: Unknown message id."},
        { 510, "Request Market Data Sending Error - "},
        { 511, "Cancel Market Data Sending Error - "},
        { 512, "Order Sending Error - "},
        { 513, "Account Update Request Sending Error -"},
        { 514, "Request For Executions Sending Error -"},
        { 515, "Cancel Order Sending Error -"},
        { 516, "Request Open Order Sending Error -"},
        { 517, "Unknown contract. Verify the contract details supplied."},
        { 518, "Request Contract Data Sending Error - "},
        { 519, "Request Market Depth Sending Error - "},
        { 520, "Cancel Market Depth Sending Error - "},
        { 521, "Set Server Log Level Sending Error - "},
        { 522, "FA Information Request Sending Error - "},
        { 523, "FA Information Replace Sending Error - "},
        { 524,  "Request Scanner Subscription Sending Error - "},
        { 525, "Cancel Scanner Subscription Sending Error - "},
        { 526, "Request Scanner Parameter Sending Error - "},
        { 527, "Request Historical Data Sending Error - "},
        { 528, "Request Historical Data Sending Error - "},
        { 529, "Request Real-time Bar Data Sending Error - "},
        { 530, "Cancel Real-time Bar Data Sending Error - "},
        { 531, "Request Current Time Sending Error - "},
        { 532, "Request Fundamental Data Sending Error - "},
        { 533, "Cancel Fundamental Data Sending Error - "}
    };

    char *tws_incoming_msg_names[] = {
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
        "current_time", "realtime_bars"
    };

    char *fa_msg_name[] = { "GROUPS", "PROFILES", "ALIASES" };
    static const unsigned int d_nan[2] = {~0U, ~(1U<<31)};
    double *dNAN = (double *)(void *) d_nan;
#else
    extern struct twsclient_errmsg twsclient_err_indication[];
    extern char *tws_incoming_msg_names[];
    extern char *fa_msg_name[];
    extern double *dNAN;
#endif /* TWSAPI_GLOBALS */

#define fa_msg_type_name(x) (((x)>= GROUPS && (x) <= ALIASES) ? fa_msg_name[x] : 0)
#define MODEL_OPTION 13

#ifdef __cplusplus
}
#endif

#endif /* TWSAPI_H_ */
