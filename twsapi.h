#ifndef TWSAPI_H_
#define TWSAPI_H_

#ifdef __cplusplus
extern "C" {
#endif

/* public C API */
#define TWSCLIENT_VERSION 17

    typedef struct tr_contract_details {
	double d_mintick;
	struct contract_details_summary {
	    double s_strike;
	    char *s_symbol;
	    char *s_sectype;
	    char *s_expiry;
	    char *s_right;
	    char *s_exchange;
	    char *s_currency;
	    char *s_local_symbol;
	} d_summary;
	char   *d_market_name;
	char   *d_trading_class;
	char   *d_multiplier;
	char   *d_order_types;
	char   *d_valid_exchanges;
	int    d_conid;
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
	tr_comboleg_t *c_comboleg;
	long c_num_combolegs;
    } tr_contract_t;

    typedef struct tr_order {
	double o_discretionary_amt;
	double o_lmt_price;
	double o_aux_price;
	char *o_good_after_time;
	char *o_good_till_date;
	char *o_shares_allocation;
	char *o_fagroup;
	char *o_famethod;
	char *o_fapercentage;
	char *o_faprofile;
	char *o_action;
	char *o_ordertype;
	char *o_tif;
	char *o_oca_group;
	char *o_account;
	char *o_open_close;
	char *o_orderref;
	int  o_orderid;
	int  o_total_quantity;
	int  o_origin;
	int  o_clientid;
	int  o_permid;
	int  o_parentid;
	int  o_display_size;
	int  o_trigger_method;
	unsigned int  o_ignore_rth:1;
	unsigned int  o_hidden: 1;
	unsigned int  o_transmit: 1;
	unsigned int  o_block_order: 1;
	unsigned int  o_sweep_to_fill: 1;
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

    typedef void (*tws_func_t)(void *arg);
/* must return 0 on success, -1 on failure */
    typedef int (*start_thread_t)(tws_func_t func, void *arg);

/* creates new tws client instance, starts reader thread and 
 * and records opaque user defined pointer to be supplied in all callbacks
 */
    void  *tws_create(start_thread_t start_thread, void *opaque_user_defined);
    void  tws_destroy(void *tws_instance);

    int   tws_connect(void *tws, const char host[], unsigned short port, int clientid);
    void  tws_disconnect(void *tws);
    int   tws_req_mkt_data(void *tws, long ticker_id, tr_contract_t *contract);
    int   tws_cancel_mkt_data(void *tws, long ticker_id);
    int   tws_place_order(void *tws, long order_id, tr_contract_t *contract, tr_order_t *order);
    int   tws_cancel_order(void *tws, long order_id);
    int   tws_req_open_orders(void *tws);
    int   tws_req_account_updates(void *tws, int subscribe, const char acct_code[]);
    int   tws_req_executions(void *tws, tr_exec_filter_t *filter);
    int   tws_req_ids(void *tws, int num_ids);
    int   tws_check_messages(void *tws);
    int   tws_req_contract_details(void *tws, tr_contract_t *contract);    
    int   tws_req_mkt_depth(void *tws, long ticker_id, tr_contract_t *contract);
    int   tws_cancel_mkt_depth(void *tws, long ticker_id);
    int   tws_req_news_bulletins(void *tws, int all_msgs);
    int   tws_cancel_news_bulletins(void *tws);
    int   tws_set_server_log_level(void *tws, int level);
    int   tws_req_auto_open_orders(void *tws, int auto_bind);
    int   tws_req_all_open_orders(void *tws);
    int   tws_req_managed_accts(void *tws);
    int   tws_request_fa(void *tws, long fa_data_type);
    int   tws_replace_fa(void *tws, long fa_data_type, const char cxml[]);

/************************************ callbacks *************************************/
/* API user must implement these C functions */
    void event_tick_price(void *opaque_user_defined, long ticker_id, long field, double price,
			  int can_auto_execute);
    void event_tick_size(void *opaque_user_defined, long ticker_id, long field, int size);
    void event_order_status(void *opaque_user_defined, long order_id, const char status[],
			    int filled, int remaining, double avg_fill_price, int perm_id,
			    int parent_id, double last_fill_price, int client_id);
    void event_open_order(void *opaque_user_defined, long order_id, const tr_contract_t *contract,
			  const tr_order_t *order);
    void event_win_error(void *opaque_user_defined, const char str[], int last_error);
    void event_connection_closed(void *opaque_user_defined);
    void event_update_account_value(void *opaque_user_defined, const char key[], const char val[],
				    const char currency[], const char account_name[]);
    void event_update_portfolio(void *opaque_user_defined, const tr_contract_t *contract, int position,
				double mkt_price, double mkt_value, double average_cost,
				double unrealized_pnl, double realized_pnl, const char account_name[]);
    void event_update_account_time(void *opaque_user_defined, const char time_stamp[]);
    void event_next_valid_id(void *opaque_user_defined, long order_id);
    void event_contract_details(void *opaque_user_defined, const tr_contract_details_t *contract_details);
    void event_exec_details(void *opaque_user_defined, long order_id, const tr_contract_t *contract,
			    const tr_execution_t *execution);
    void event_error(void *opaque_user_defined, int id, int error_code, const char error_string[]);
    void event_update_mkt_depth(void *opaque_user_defined, long ticker_id, int position, int operation, int side,
				double price, int size);
    void event_update_mkt_depth_l2(void *opaque_user_defined, long ticker_id, int position,
				   char *market_maker, int operation, int side, double price, int size);
    void event_update_news_bulletin(void *opaque_user_defined, int msgid, int msg_type, const char news_msg[],
				    const char origin_exch[]);
    void event_managed_accounts(void *opaque_user_defined, const char accounts_list[]);
    void event_receive_fa(void *opaque_user_defined, long fa_data_type, const char cxml[]);

/*outgoing message IDs */
    enum tws_outgoing_ids {
	REQ_MKT_DATA = 1,
	CANCEL_MKT_DATA,
	PLACE_ORDER,
	CANCEL_ORDER,
	REQ_OPEN_ORDERS,
	REQ_ACCOUNT_DATA,
	REQ_EXECUTIONS,
	REQ_IDS,
	REQ_CONTRACT_DATA,
	REQ_MKT_DEPTH,
	CANCEL_MKT_DEPTH,
	REQ_NEWS_BULLETINS,
	CANCEL_NEWS_BULLETINS,
	SET_SERVER_LOGLEVEL,
	REQ_AUTO_OPEN_ORDERS,
	REQ_ALL_OPEN_ORDERS,
	REQ_MANAGED_ACCTS,
	REQ_FA,
	REPLACE_FA
    };

    enum tws_incoming_ids {
	TICK_PRICE = 1,
	TICK_SIZE,
	ORDER_STATUS,
	ERR_MSG,
	OPEN_ORDER,
	ACCT_VALUE,
	PORTFOLIO_VALUE,
	ACCT_UPDATE_TIME,
	NEXT_VALID_ID,
	CONTRACT_DATA,
	EXECUTION_DATA,
	MARKET_DEPTH,
	MARKET_DEPTH_L2,
	NEWS_BULLETINS,
	MANAGED_ACCTS,
	RECEIVE_FA,
	no_more_messages
    };


/* ERROR codes */
    enum twsclient_error_codes {
	NO_VALID_ID = -1,
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
	FAIL_SEND_FA_REPLACE
    };

    struct twsclient_errmsg {
	long err_code;
	char *err_msg;
    };

#ifdef TWSAPI_GLOBALS
    struct twsclient_errmsg twsclient_err_indication[] = {
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
    };

    char *tws_incoming_msg_names[] = {
	"tick_price", "tick_size", "order_status", "err_msg", "open_order",
	"acct_value", "portfolio_value", "acct_update_time", "next_valid_id",
	"contract_data", "execution_data", "market_depth", "market_depth_l2",
	"news_bulletins", "managed_accts", "receive_fa", 0
    };

    static const unsigned int d_nan[2] = {~0U, ~(1U<<31)};
    double *dNAN = (double *) d_nan;
#else
    extern struct twsclient_errmsg twsclient_err_indication[];
    extern char *tws_incoming_msg_names[];
    extern double *dNAN;
#endif /* TWSAPI_GLOBALS */

#ifdef WINDOWS
#define strcasecmp(x,y) _stricmp(x,y)
#endif

#ifdef __cplusplus
}
#endif

#endif /* TWSAPI_H_ */
