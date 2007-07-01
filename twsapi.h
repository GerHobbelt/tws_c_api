#ifndef TWSAPI_H_
#define TWSAPI_H_

#ifdef __cplusplus
extern "C" {
#endif

/* public C API */
#define TWSCLIENT_VERSION 32

    typedef struct tr_contract_details {
        double d_mintick;
        double d_coupon;
        struct contract_details_summary {
            double s_strike;
            char *s_symbol;
            char *s_sectype;
            char *s_expiry;
            char *s_right;
            char *s_exchange;
            char *s_currency;
            char *s_local_symbol;
            char *s_cusip;
            char *s_maturity;
            char *s_issue_date;
            char *s_ratings;
            char *s_bond_type;
            char *s_coupon_type;
            char *s_desc_append;
            char *s_next_option_date;
            char *s_next_option_type;
            char *s_notes;
            unsigned s_convertible: 1;
            unsigned s_callable: 1;
            unsigned s_putable: 1;
            unsigned s_next_option_partial:1;
        } d_summary;
        char   *d_market_name;
        char   *d_trading_class;
        char   *d_multiplier;
        char   *d_order_types;
        char   *d_valid_exchanges;
        int    d_conid;
        int    d_price_magnifier;
    } tr_contract_details_t;

    typedef struct tr_comboleg {
        char *co_action; /* BUY, SELL */
        char *co_exchange;
        int  co_conid;
        int  co_ratio;
        int  co_open_close;
    } tr_comboleg_t;

    typedef struct tr_contract {
        double c_strike;
        char *c_symbol;
        char *c_sectype;
        char *c_exchange;
        char *c_primary_exch;
        char *c_expiry;
        char *c_currency;
        char *c_right;
        char *c_local_symbol;
        char *c_multiplier;
        char *c_combolegs_descrip;
        tr_comboleg_t *c_comboleg;
        long c_num_combolegs;
	unsigned c_include_expired:1;
    } tr_contract_t;

    typedef struct tr_order {
        double o_discretionary_amt;
        double o_lmt_price;
        double o_aux_price;
        double o_percent_offset; /* make sure to default init to DBL_MAX */
        double o_nbbo_price_cap; /* make sure to default init to DBL_MAX */
        double o_starting_price; /* make sure to default init to DBL_MAX */
        double o_stock_ref_price; /* make sure to default init to DBL_MAX */
        double o_delta; /* make sure to default init to DBL_MAX */
        double o_stock_range_lower, o_stock_range_upper; /* make sure to default init to DBL_MAX */
	double o_volatility;
	double o_delta_neutral_aux_price;
	double o_trail_stop_price;
        double o_basis_points;
        char *o_good_after_time;
        char *o_good_till_date;
        char *o_shares_allocation;
        char *o_fagroup;
        char *o_famethod;
        char *o_primary_exchange; /* default init to "" */
        char *o_fapercentage;
        char *o_faprofile;
        char *o_action;
        char *o_order_type;
        char *o_tif;
        char *o_oca_group;
        char *o_account;
        char *o_open_close; /* "O", "C" default init to "O" */
        char *o_orderref;
        char *o_designated_location; /* when slot == 2 only; default init to "" */
        char *o_rule80a;
        char *o_settling_firm;
	char *o_delta_neutral_order_type;
        int  o_orderid;
        int  o_total_quantity;
        int  o_origin; /* default init to CUSTOMER */
#define CUSTOMER 0
#define FIRM 1
        int  o_clientid;
        int  o_permid;
        int  o_parentid;
        int  o_display_size;
        int  o_trigger_method;
        int  o_min_qty; /* make sure to default init to ~(1 << 31) */
	int  o_volatility_type;
	int  o_reference_price_type;
        int  o_basis_points_type;
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
        unsigned o_rth_only:1;
        unsigned o_all_or_none:1;
        unsigned o_ignore_rth: 1;
        unsigned o_hidden: 1;
        unsigned o_transmit: 1; /*default init to true */
        unsigned o_block_order: 1;
        unsigned o_sweep_to_fill: 1;
	unsigned o_continuous_update: 1;
    } tr_order_t;

    typedef struct tr_execution {
        double e_price;
        char *e_execid;
        char *e_time;
        char *e_acct_number;
        char *e_exchange;
        char *e_side;
        int  e_shares;
        int e_permid;
        int e_clientid;
        int e_liquidation;
        int e_orderid;
    } tr_execution_t;

    typedef struct tr_exec_filter {
        char *f_acctcode;
        char *f_time;
        char *f_symbol;
        char *f_sectype;
        char *f_exchange;
        char *f_side;
        int f_clientid;
    } tr_exec_filter_t;

    typedef struct tr_scanner_subscription {
	double scan_above_price;
	double scan_below_price;
	double scan_coupon_rate_above;
	double scan_coupon_rate_below;
	double scan_market_cap_above;
	double scan_market_cap_below;
	char *scan_exclude_convertible;	
	char *scan_instrument;
	char *scan_location_code;
	char *scan_maturity_date_above;
	char *scan_maturity_date_below;
	char *scan_moody_rating_above;
	char *scan_moody_rating_below;
	char *scan_code;
	char *scan_sp_rating_above;
	char *scan_sp_rating_below;
	char *scan_scanner_setting_pairs;
	char *scan_stock_type_filter;
	int scan_above_volume;
	int scan_number_of_rows;
	int scan_average_option_volume_above;
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
    typedef void (*external_func_t)(int);
    /* "func" calls this one once at startup and again at termination,
     * param 0 indicates startup, param 1 indicates termination
     */

/* creates new tws client instance, starts reader thread and 
 * and records opaque user defined pointer to be supplied in all callbacks
 */
    void  *tws_create(start_thread_t start_thread, void *opaque, external_func_t myfunc);
    void  tws_destroy(void *tws_instance);

    int   tws_connect(void *tws, const char host[], unsigned short port, int clientid);
    void  tws_disconnect(void *tws);
    int   tws_req_scanner_parameters(void *tws);
    int   tws_req_scanner_subscription(void *tws, int ticker_id, tr_scanner_subscription_t *subscription);
    int   tws_cancel_scanner_subscription(void *tws, int ticker_id);
    int   tws_req_mkt_data(void *tws, int ticker_id, tr_contract_t *contract, const char generic_tick_list[]);
    int   tws_req_historical_data(void *tws, int ticker_id, tr_contract_t *contract, const char end_date_time[], const char duration_str[], const char bar_size_setting[], const char what_to_show[], int use_rth, int format_date);
    int   tws_cancel_historical_data(void *tws, int ticker_id);
    int   tws_cancel_mkt_data(void *tws, int ticker_id);
    int   tws_exercise_options(void *tws, int ticker_id, tr_contract_t *contract, int exercise_action, int exercise_quantity, const char account[], int override);
    int   tws_place_order(void *tws, long order_id, tr_contract_t *contract, tr_order_t *order);
    int   tws_cancel_order(void *tws, long order_id);
    int   tws_req_open_orders(void *tws);
    int   tws_req_account_updates(void *tws, int subscribe, const char acct_code[]);
    int   tws_req_executions(void *tws, tr_exec_filter_t *filter);
    int   tws_req_ids(void *tws, int num_ids);
    int   tws_check_messages(void *tws);
    int   tws_req_contract_details(void *tws, tr_contract_t *contract);    
    int   tws_req_mkt_depth(void *tws, int ticker_id, tr_contract_t *contract, int num_rows);
    int   tws_cancel_mkt_depth(void *tws, int ticker_id);
    int   tws_req_news_bulletins(void *tws, int all_msgs);
    int   tws_cancel_news_bulletins(void *tws);
    int   tws_set_server_log_level(void *tws, int level);
    int   tws_req_auto_open_orders(void *tws, int auto_bind);
    int   tws_req_all_open_orders(void *tws);
    int   tws_req_managed_accts(void *tws);
    int   tws_request_fa(void *tws, long fa_data_type);
    int   tws_replace_fa(void *tws, long fa_data_type, const char cxml[]);
    int tws_req_current_time(void *tws);
    int   tws_request_realtime_bars(void *tws, int ticker_id, tr_contract_t *c, int bar_size, const char what_to_show[], int use_rth);
    int tws_cancel_realtime_bars(void *tws, int ticker_id);
    /**** 2 auxilliary routines */
    int tws_server_version(void *tws);
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
    void event_open_order(void *opaque, long order_id, const tr_contract_t *contract,
                          const tr_order_t *order);
    void event_win_error(void *opaque, const char str[], int last_error);
    void event_connection_closed(void *opaque);
    void event_update_account_value(void *opaque, const char key[], const char val[],
                                    const char currency[], const char account_name[]);
    void event_update_portfolio(void *opaque, const tr_contract_t *contract, int position,
                                double mkt_price, double mkt_value, double average_cost,
                                double unrealized_pnl, double realized_pnl, const char account_name[]);
    void event_update_account_time(void *opaque, const char time_stamp[]);
    void event_next_valid_id(void *opaque, long order_id);
    void event_contract_details(void *opaque, const tr_contract_details_t *contract_details);
    void event_bond_contract_details(void *opaque, const tr_contract_details_t *contract_details);
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
    void event_realtime_bar(void *opaque, int reqid, long time, double open, double high, double low, double close, long volume, double wap, int count);
    void event_current_time(void *opaque, long time);

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
        CANCEL_REAL_TIME_BARS = 51
    };

    enum tws_incoming_ids {
        TICK_PRICE = 1,
        TICK_SIZE =2,
        ORDER_STATUS =3,
        ERR_MSG =4,
        OPEN_ORDER =5,
        ACCT_VALUE =6,
        PORTFOLIO_VALUE =7,
        ACCT_UPDATE_TIME =8,
        NEXT_VALID_ID =9,
        CONTRACT_DATA =10,
        EXECUTION_DATA =11,
        MARKET_DEPTH =12,
        MARKET_DEPTH_L2 =13,
        NEWS_BULLETINS =14,
        MANAGED_ACCTS =15,
        RECEIVE_FA =16,
        HISTORICAL_DATA =17,
        BOND_CONTRACT_DATA =18,
	SCANNER_PARAMETERS =19,
	SCANNER_DATA =20,
	TICK_OPTION_COMPUTATION =21,
	TICK_GENERIC =45,
	TICK_STRING =46,
        TICK_EFP =47,
        CURRENT_TIME =49,
        REAL_TIME_BARS = 50        
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
        FAIL_SEND_REQCURRTIME
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
        { 531, "Request Current Time Sending Error - "}
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
