#ifndef TWSAPI_H_
#define TWSAPI_H_

/*
0: nothing
1: enums only
2: 1 + defines
3: 2 + structures
4: everything (except possibly the globals)
*/
#if !defined(TWS_DEFINITIONS)
#define TWS_DEFINITIONS					4
#endif

#if TWS_DEFINITIONS > 0

/*
	As listed in http://files.meetup.com/1551526/Interactive_Brokers_API.pdf

	Data Streams - Limitations:

	● 50 Messages / sec
	● 100 tickers simultaneously
	● 3 Level II Feeds simultaneously
	● Sampled Tick Stream
	● Differential Tick Stream
	● Aggregate higher time frames yourself
	● No technical indicators
	● Everyone has to reinvent the wheel


	Gotchas:

	1. GUI Check-boxes from TWS pollute API behavior
	2. Throttling WRT historical data (60 reqs/10 min)
	3. Unhanded Exceptions / Errors (not uniform)
	4. Daily Server "Reboot"
	5. Daily TWS “Reboot”
	6. Security Device / Dongle
	7. Paper Trade vs Live Trade machine restriction
	8. Data Issues


	Gotchas - Paper v.s. Live:
	● Restrictions
		○ both must be on same physical computer to run	simultaneously
		○ combined they are under the same data throttles
		○ another reason to create a data broker
	● Paper uses only Top of the Book to simulate orders



	Gotchas - Data Issues:

	Strategy for dealing with bad data:
	● Spikes?
	● Bid / Ask Crossed?
	● What does DanBot do?
	● Clock Drift
	● Flash Crash
*/


#ifdef __cplusplus
namespace tws {
#endif

typedef enum {
	FAMT_UNKNOWN  = 0,
	GROUPS        = 1,
	PROFILES      = 2,
	ALIASES       = 3,
} tr_fa_msg_type_t;

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
    RT_HISTORICAL_VOL = 58    /* real-time historical volatility -- according to the IB API docs at http://www.interactivebrokers.com/php/apiUsersGuide/apiguide/tables/generic_tick_types.htm */
} tr_tick_type_t;


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
	COMMISSION_REPORT = 59,
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

	/* Error messages */
	FAIL_MAX_RATE_OF_MESSAGES_EXCEEDED = 100,
	FAIL_MAX_NUMBER_OF_TICKERS_REACHED = 101,
	FAIL_DUPLICATE_TICKER_ID = 102,
	FAIL_DUPLICATE_ORDER_ID = 103,
	FAIL_MODIFY_FILLED_ORDER = 104,
	FAIL_MODIFIED_ORDER_DOES_NOT_MATCH_ORIGINAL = 105,
	FAIL_TRANSMIT_ORDER_ID = 106,
	FAIL_TRANSMIT_INCOMPLETE_ORDER = 107,
	FAIL_PRICE_OUT_OF_RANGE = 109,
	FAIL_PRICE_DOES_NOT_CONFORM_TO_VARIATION = 110,
	FAIL_TIF_AND_ORDER_TYPE_ARE_INCOMPATIBLE = 111,
	FAIL_ILLEGAL_TIF_OPTION = 113,
	FAIL_RELATIVE_ORDERS_INVALID = 114,
	FAIL_SUBMIT_RELATIVE_ORDERS = 115,
	FAIL_DEAD_EXCHANGE = 116,
	FAIL_BLOCK_ORDER_SIZE = 117,
	FAIL_VWAP_ORDERS_ROUTING = 118,
	FAIL_NON_VWAP_ORDERS_ON_VWAP_EXCHANGE = 119,
	FAIL_VWAP_ORDER_TOO_LATE = 120,
	FAIL_INVALID_BD_FLAG = 121,
	FAIL_NO_REQUEST_TAG = 122,
	FAIL_NO_RECORD_AVAILABLE = 123,
	FAIL_NO_MARKET_RULE_AVAILABLE = 124,
	FAIL_BUY_PRICE_INCORRECT = 125,
	FAIL_SELL_PRICE_INCORRECT = 126,
	FAIL_VWAP_ORDER_PLACED_AFTER_START_TIME = 129,
	FAIL_SWEEP_TO_FILL_INCORRECT = 131,
	FAIL_CLEARING_ACCOUNT_MISSING = 132,
	FAIL_SUBMIT_NEW_ORDER = 133,
	FAIL_MODIFY_ORDER = 134,
	FAIL_FIND_ORDER = 135,
	FAIL_ORDER_CANNOT_CANCEL = 136,
	FAIL_CANNOT_CANCEL_VWAP_ORDER = 137,
	FAIL_COULD_NOT_PARSE_TICKER_REQUEST = 138,
	FAIL_PARSING_ERROR = 139,
	FAIL_SIZE_VALUE_NOT_INTEGER = 140,
	FAIL_PRICE_VALUE_NOT_FLTPOINT = 141,
	FAIL_MISSING_INSTITUTIONAL_ACCOUNT_INFO = 142,
	FAIL_REQUESTED_ID_NOT_INTEGER = 143,
	FAIL_ORDER_SIZE_DOES_NOT_MATCH_ALLOCATION = 144,
	FAIL_INVALID_ENTRY_FIELDS = 145,
	FAIL_INVALID_TRIGGER_METHOD = 146,
	FAIL_CONDITIONAL_CONTRACT_INFO_INCOMPLETE = 147,
	FAIL_CANNOT_SUBMIT_CONDITIONAL_ORDER = 148,
	FAIL_ORDER_TRANSMITTED_WITHOUT_USER_NAME = 151,
	FAIL_INVALID_HIDDEN_ORDER_ATTRIBUTE = 152,
	FAIL_EFPS_CAN_ONLY_LIMIT_ORDERS = 153,
	FAIL_ORDERS_TRANSMITTED_FOR_HALTED_SECURITY = 154,
	FAIL_SIZEOP_ORDER_MUST_HAVE_USERNAME_AND_ACCOUNT = 155,
	FAIL_SIZEOP_ORDER_MUST_GO_TO_IBSX = 156,
	FAIL_ICEBERG_OR_DISCRETIONARY = 157,
	FAIL_MUST_SPECIFY_OFFSET = 158,
	FAIL_PERCENT_OFFSET_VALUE_OUT_OF_RANGE = 159,
	FAIL_SIZE_VALUE_ZERO = 160,
	FAIL_ORDER_NOT_IN_CANCELLABLE_STATE = 161,
	FAIL_HISTORICAL_MARKET_DATA_SERVICE = 162,
	FAIL_VIOLATE_PERCENTAGE_IN_ORDER_SETTINGS = 163,
	FAIL_NO_MARKET_DATA_TO_CHECK_VIOLATIONS = 164,
	INFO_HISTORICAL_MARKET_DATA_SERVICE_QUERY = 165,
	FAIL_HMDS_EXPIRED_CONTRACT_VIOLATION = 166,
	FAIL_VWAP_ORDER_NOT_IN_FUTURE = 167,
	FAIL_DISCRETIONARY_AMOUNT_MISMATCH = 168,
                                                                                   
	FAIL_NO_SECURITY_DEFINITION = 200,
	FAIL_ORDER_REJECTED = 201,
	FAIL_ORDER_CANCELED = 202,
	FAIL_SECURITY_NOT_AVAILABLE_ALLOWED = 203,
                                                                                   
	FAIL_FIND_EID = 300,
	FAIL_INVALID_TICKER_ACTION = 301,
	FAIL_ERROR_PARSING_STOP = 302,
	FAIL_INVALID_ACTION = 303,
	FAIL_INVALID_ACCOUNT_VALUE = 304,
	FAIL_REQUEST_PARSING_ERROR = 305,
	FAIL_DDE_REQUEST = 306,
	FAIL_INVALID_REQUEST_TOPIC = 307,
	FAIL_UNABLE_TO_CREATE_PAGE = 308,
	FAIL_MAX_NUMBER_MARKET_DEPTH_REQUESTS_REACHED = 309,
	FAIL_FIND_SUBSCRIBED_MARKET_DEPTH = 310,
	FAIL_ORIGIN_INVALID = 311,
	FAIL_COMBO_DETAILS_INVALID = 312,
	FAIL_COMBO_DETAILS_FOR_LEG_INVALID = 313,
	FAIL_SECURITY_TYPE_REQUIRES_COMBO_LEG_DETAILS = 314,
	FAIL_COMBO_LEGS_ROUTING_RESTRICTED = 315,
	FAIL_MARKET_DEPTH_DATA_HALTED = 316,
	FAIL_MARKET_DEPTH_DATA_RESET = 317,
	FAIL_INVALID_LOG_LEVEL = 319,
	FAIL_SERVER_ERROR_READING_REQUEST = 320,
	FAIL_SERVER_ERROR_VALIDATING_REQUEST = 321,
	FAIL_SERVER_ERROR_PROCESSING_REQUEST = 322,
	FAIL_SERVER_ERROR = 323,
	FAIL_SERVER_ERROR_READING_DDE_REQUEST = 324,
	FAIL_DISCRETIONARY_ORDERS_NOT_SUPPORTED = 325,
	FAIL_UNABLE_TO_CONNECT_ID_IN_USE = 326,
	FAIL_CANNOT_SET_AUTO_BIND_PROPERTY = 327,
	FAIL_CANNOT_ATTACH_TRAILING_STOP_ORDERS = 328,
	FAIL_ORDER_MODIFY_FAILED = 329,
	FAIL_ONLY_FA_OR_STL_CUSTOMERS = 330,
	FAIL_FA_OR_STL_INTERNAL_ERROR = 331,
	FAIL_INVALID_ACCOUNT_CODES_FOR_PROFILE = 332,
	FAIL_INVALID_SHARE_ALLOCATION_SYNTAX = 333,
	FAIL_INVALID_GOOD_TILL_DATE_ORDER = 334,
	FAIL_DELTA_OUT_OF_RANGE = 335,
	FAIL_TIME_OR_TIME_ZONE_INVALID = 336,
	FAIL_DATE_TIME_INVALID = 337,
	FAIL_GOOD_AFTER_TIME_ORDERS_DISABLED = 338,
	FAIL_FUTURES_SPREAD_NOT_SUPPORTED = 339,
	FAIL_INVALID_IMPROVEMENT_AMOUNT = 340,
	FAIL_INVALID_DELTA = 341,
	FAIL_PEGGED_ORDER_NOT_SUPPORTED = 342,
	FAIL_DATE_TIME_ENTERED_INVALID = 343,
	FAIL_ACCOUNT_NOT_FINANCIAL_ADVISOR = 344,
	FAIL_GENERIC_COMBO_NOT_SUPPORTED = 345,
	FAIL_NOT_PRIVILEGED_ACCOUNT = 346,
	FAIL_SHORT_SALE_SLOT_VALUE = 347,
	FAIL_ORDER_NOT_SHORT_SALE = 348,
	FAIL_COMBO_DOES_NOT_SUPPORT_GOOD_AFTER = 349,
	FAIL_MINIMUM_QUANTITY_NOT_SUPPORTED = 350,
	FAIL_REGULAR_TRADING_HOURS_ONLY = 351,
	FAIL_SHORT_SALE_SLOT_REQUIRES_LOCATION = 352,
	FAIL_SHORT_SALE_SLOT_REQUIRES_NO_LOCATION = 353,
	FAIL_NOT_SUBSCRIBED_TO_MARKET_DATA = 354,
	FAIL_ORDER_SIZE_DOES_NOT_CONFORM_RULE = 355,
	FAIL_SMART_COMBO_ORDER_NOT_SUPPORT_OCA = 356,
	FAIL_CLIENT_VERSION_OUT_OF_DATE = 357,
	FAIL_COMBO_CHILD_ORDER_NOT_SUPPORTED = 358,
	FAIL_COMBO_ORDER_REDUCED_SUPPORT = 359,
	FAIL_NO_WHATIF_CHECK_SUPPORT = 360,
	FAIL_INVALID_TRIGGER_PRICE = 361,
	FAIL_INVALID_ADJUSTED_STOP_PRICE = 362,
	FAIL_INVALID_ADJUSTED_STOP_LIMIT_PRICE = 363,
	FAIL_INVALID_ADJUSTED_TRAILING_AMOUNT = 364,
	FAIL_NO_SCANNER_SUBSCRIPTION_FOUND = 365,
	FAIL_NO_HISTORICAL_DATA_QUERY_FOUND = 366,
	FAIL_VOLATILITY_TYPE = 367,
	FAIL_REFERENCE_PRICE_TYPE = 368,
	FAIL_VOLATILITY_ORDERS_US_ONLY = 369,
	FAIL_DYNAMIC_VOLATILITY_ORDERS_ROUTING = 370,
	FAIL_VOL_ORDER_REQUIRES_POSITIVE_VOLATILITY = 371,
	FAIL_DYNAMIC_VOL_ATTRIBUTE_ON_NON_VOL_ORDER = 372,
	FAIL_CANNOT_SET_STOCK_RANGE_ATTRIBUTE = 373,
	FAIL_STOCK_RANGE_ATTRIBUTES_INVALID = 374,
	FAIL_STOCK_RANGE_ATTRIBUTES_NEGATIVE = 375,
	FAIL_ORDER_NOT_ELIGIBLE_FOR_CONTINUOUS_UPDATE = 376,
	FAIL_MUST_SPECIFY_VALID_DELTA_HEDGE_PRICE = 377,
	FAIL_DELTA_HEDGE_REQUIRES_AUX_PRICE = 378,
	FAIL_DELTA_HEDGE_REQUIRES_NO_AUX_PRICE = 379,
	FAIL_ORDER_TYPE_NOT_ALLOWED = 380,
	FAIL_DDE_DLL_NEEDS_TO_UPGRADED = 381,
	FAIL_PRICE_VIOLATES_TICKS_CONSTRAINT = 382,
	FAIL_SIZE_VIOLATES_SIZE_CONSTRAINT = 383,
	FAIL_INVALID_DDE_ARRAY_REQUEST = 384,
	FAIL_DUPLICATE_TICKER_ID_FOR_SCANNER = 385,
	FAIL_DUPLICATE_TICKER_ID_FOR_HISTORICAL_DATA = 386,
	FAIL_UNSUPPORTED_ORDER_TYPE_FOR_EXCHANGE = 387,
	FAIL_ORDER_SIZE_BELOW_REQUIREMENT = 388,
	FAIL_ROUTED_ORDER_ID_NOT_UNIQUE = 389,
	FAIL_ROUTED_ORDER_ID_INVALID = 390,
	FAIL_TIME_OR_TIME_ZONE_ENTERED_INVALID = 391,
	FAIL_INVALID_ORDER_CONTRACT_EXPIRED = 392,
	FAIL_SHORT_SALE_SLOT_FOR_DELTA_HEDGE_ONLY = 393,
	FAIL_INVALID_PROCESS_TIME = 394,
	FAIL_OCA_GROUPS_CURRENTLY_NOT_ACCEPTED = 395,
	FAIL_ONLY_MARKET_AND_LIMIT_ORDERS = 396,
	FAIL_ONLY_MARKET_AND_LIMIT_ORDERS_SUPPORT = 397,
	FAIL_CONDITION_TRIGGER = 398,
	FAIL_ORDER_MESSAGE = 399,
                                                                                   
	FAIL_ALGO_ORDER_ERROR = 400,
	FAIL_LENGTH_RESTRICTION = 401,
	FAIL_CONDITIONS_NOT_ALLOWED_FOR_CONTRACT = 402,
	FAIL_INVALID_STOP_PRICE = 403,
	FAIL_SHARES_NOT_AVAILABLE = 404,
	FAIL_CHILD_ORDER_QUANTITY_INVALID = 405,
	FAIL_CURRENCY_NOT_ALLOWED = 406,
	FAIL_INVALID_SYMBOL = 407,
	FAIL_INVALID_SCALE_ORDER_INCREMENT = 408,
	FAIL_INVALID_SCALE_ORDER_COMPONENT_SIZE = 409,
	FAIL_INVALID_SUBSEQUENT_COMPONENT_SIZE = 410,
	FAIL_OUTSIDE_REGULAR_TRADING_HOURS = 411,
	FAIL_CONTRACT_NOT_AVAILABLE = 412,
	FAIL_WHAT_IF_ORDER_TRANSMIT_FLAG = 413,
	FAIL_MARKET_DATA_SUBSCRIPTION_NOT_APPLICABLE = 414,
	FAIL_WAIT_UNTIL_PREVIOUS_RFQ_FINISHES = 415,
	FAIL_RFQ_NOT_APPLICABLE = 416,
	FAIL_INVALID_INITIAL_COMPONENT_SIZE = 417,
	FAIL_INVALID_SCALE_ORDER_PROFIT_OFFSET = 418,
	FAIL_MISSING_INITIAL_COMPONENT_SIZE = 419,
	FAIL_INVALID_REAL_TIME_QUERY = 420,
	FAIL_INVALID_ROUTE = 421,
	FAIL_ACCOUNT_AND_CLEARING_ATTRIBUTES = 422,
	FAIL_CROSS_ORDER_RFQ_EXPIRED = 423,
	FAIL_FA_ORDER_REQUIRES_ALLOCATION = 424,
	FAIL_FA_ORDER_REQUIRES_MANUAL_ALLOCATIONS = 425,
	FAIL_ACCOUNTS_LACK_SHARES = 426,
	FAIL_MUTUAL_FUND_REQUIRES_MONETARY_VALUE = 427,
	FAIL_MUTUAL_FUND_SELL_REQUIRES_SHARES = 428,
	FAIL_DELTA_NEUTRAL_ORDERS_NOT_SUPPORTED = 429,
	FAIL_FUNDAMENTALS_DATA_NOT_AVAILABLE = 430,
	FAIL_WHAT_TO_SHOW_FIELD = 431,
	FAIL_COMMISSION_NEGATIVE = 432,
	FAIL_INVALID_RESTORE_SIZE = 433,
	FAIL_ORDER_SIZE_ZERO = 434,
	FAIL_MUST_SPECIFY_ACCOUNT = 435,
	FAIL_MUST_SPECIFY_ALLOCATION = 436,
	FAIL_ORDER_TOO_MANY_FLAGS = 437,
	FAIL_APPLICATION_LOCKED = 438,
	FAIL_ALGORITHM_DEFINITION_NOT_FOUND = 439,
	FAIL_ALGORITHM_MODIFIED = 440,
	FAIL_ALGO_ATTRIBUTES_VALIDATION_FAILED = 441,
	FAIL_ALGORITHM_NOT_ALLOWED = 442,
	FAIL_UNKNOWN_ALGO_ATTRIBUTE = 443,
	FAIL_VOLATILITY_COMBO_ORDER_SUBMIT_CHANGES = 444,
	FAIL_RFQ_NO_LONGER_VALID = 445,
	FAIL_MISSING_SCALE_ORDER_PROFIT_OFFSET = 446,
	FAIL_MISSING_SCALE_PRICE_ADJUSTMENT = 447,
	FAIL_INVALID_SCALE_PRICE_ADJUSTMENT_INTERVAL = 448,
	FAIL_UNEXPECTED_SCALE_PRICE_ADJUSTMENT = 449,
	FAIL_DIVIDEND_SCHEDULE_QUERY = 450,
                                                                                   
	/* System Message Codes */
	FAIL_IB_TWS_CONNECTIVITY_LOST = 1100,
	FAIL_IB_TWS_CONNECTIVITY_RESTORED = 1101,
	FAIL_IB_TWS_CONNECTIVITY_RESTORED_WITH_DATA = 1102,
	FAIL_TWS_SOCKET_PORT_RESET = 1300,
                                                                                   
	/* Warning Message Codes */
	FAIL_NEW_ACCOUNT_DATA_REQUESTED = 2100,
	FAIL_UNABLE_TO_SUBSCRIBE_TO_ACCOUNT = 2101,
	FAIL_UNABLE_TO_MODIFY_ORDER = 2102,
	FAIL_MARKET_DATA_FARM_DISCONNECTED = 2103,
	FAIL_MARKET_DATA_FARM_CONNECTED = 2104,
	FAIL_HISTORICAL_DATA_FARM_DISCONNECTED = 2105,
	FAIL_HISTORICAL_DATA_FARM_CONNECTED = 2106,
	FAIL_HISTORICAL_DATA_FARM_INACTIVE = 2107,
	FAIL_MARKET_DATA_FARM_CONNECTION_INACTIVE = 2108,
	FAIL_ORDER_EVENT_ATTRIBUTE_IGNORED = 2109,
	FAIL_IB_TWS_CONNECTIVITY_BROKEN = 2110,

} twsclient_error_code_t;


#ifdef __cplusplus
}
#endif



#endif /* TWS_DEFINITIONS */

#if TWS_DEFINITIONS > 1



/* public C API */
#define TWSCLIENT_VERSION 57

/*
API orders only mimic the behavior of Trader Workstation (TWS). Test each 
order type, ensuring that you can successfully submit each one in TWS, 
before you submit the same order using the API.

Order Type											Abbreviation
==================================================+================== 
Limit Risk
--------------------------------------------------+------------------ 
 
Bracket
  
Market-to-Limit										MTL
 
Market with Protection								MKT PRT
 
Request for Quote									QUOTE
 
Stop												STP
 
Stop Limit											STPLMT
 
Trailing Limit if Touched							TRAILLIT
 
Trailing Market If Touched							TRAILMIT
 
Trailing Stop										TRAIL
 
Trailing Stop Limit									TRAILLMT
 
==================================================+================== 
Speed of Execution
--------------------------------------------------+------------------ 
 
At Auction
 
Discretionary
 
Market												MKT
 
Market-if-Touched									MIT
 
Market-on-Close										MOC
 
Market-on-Open										MOO
 
Pegged-to-Market									PEG MKT
 
Relative											REL
 
Sweep-to-Fill										 
 
==================================================+================== 
Price Improvement
--------------------------------------------------+------------------ 
 
Box Top												BOX TOP
 
Price Improvement Auction
 
Block
 
Limit-on-Close										LOC
 
Limit-on-Open										LOO
 
Limit if Touched									LIT
 
Pegged-to-Midpoint									PEG MID
 
==================================================+================== 
Privacy
--------------------------------------------------+------------------ 
 
Hidden
 
Iceberg/Reserve
 
VWAP - Guaranteed									VWAP
 
==================================================+================== 
Time to Market
--------------------------------------------------+------------------ 
 
All-or-None
 
Fill-or-Kill
 
Good-after-Time/Date								GAT
 
Good-till-Date/Time									GTD
 
Good-till-Canceled									GTC
 
Immediate-or-Cancel									IOC
 
==================================================+================== 
Advanced Trading
--------------------------------------------------+------------------ 
 
One-Cancels-All										OCA
 
Spreads
 
Volatility											VOL
 
==================================================+================== 
Algorithmic Trading (Algos)
--------------------------------------------------+------------------ 
 
Arrival Price
 
Balance Impact and Risk
 
Minimize Impact
 
Percent of volume
 
Scale                                               SCALE   
 
TWAP
 
VWAP - Best Effort
 
Accumulate/Distribute
  
*/ 
#define ORDER_TYPE_MARKET_TO_LIMIT										"MTL"
#define ORDER_TYPE_MARKET_WITH_PROTECTION								"MKT PRT"
#define ORDER_TYPE_REQUEST_FOR_QUOTE									"QUOTE"
#define ORDER_TYPE_STOP													"STP"
#define ORDER_TYPE_STOP_LIMIT											"STPLMT"
#define ORDER_TYPE_TRAILING_LIMIT_IF_TOUCHED							"TRAILLIT"
#define ORDER_TYPE_TRAILING_MARKET_IF_TOUCHED							"TRAILMIT"
#define ORDER_TYPE_TRAILING_STOP										"TRAIL"
#define ORDER_TYPE_TRAILING_STOP_LIMIT									"TRAILLMT" /* "TRAILLIMIT" */
#define ORDER_TYPE_MARKET												"MKT"
#define ORDER_TYPE_MARKET_IF_TOUCHED									"MIT"
#define ORDER_TYPE_MARKET_ON_CLOSE										"MOC" /* "MKTCLS" */
#define ORDER_TYPE_MARKET_ON_OPEN										"MOO"
#define ORDER_TYPE_PEGGED_TO_MARKET										"PEGMKT"
#define ORDER_TYPE_RELATIVE												"REL"
#define ORDER_TYPE_BOX_TOP												"BOX TOP"
#define ORDER_TYPE_LIMIT_ON_CLOSE										"LOC" /* "LMTCLS" */
#define ORDER_TYPE_LIMIT_ON_OPEN										"LOO"
#define ORDER_TYPE_LIMIT_IF_TOUCHED										"LIT"
#define ORDER_TYPE_PEGGED_TO_MIDPOINT									"PEG MID"
#define ORDER_TYPE_VWAP_GUARANTEED										"VWAP"
#define ORDER_TYPE_GOOD_AFTER_TIME_DATE									"GAT"
#define ORDER_TYPE_GOOD_TILL_DATE_TIME									"GTD"
#define ORDER_TYPE_GOOD_TILL_CANCELED									"GTC"
#define ORDER_TYPE_IMMEDIATE_OR_CANCEL									"IOC"
#define ORDER_TYPE_ONE_CANCELS_ALL										"OCA"
#define ORDER_TYPE_VOLATILITY											"VOL"

#define ORDER_TYPE_LIMIT												"LMT"

#define ORDER_TYPE_ACTIVETIM											"ACTIVETIM"
#define ORDER_TYPE_ADJUST												"ADJUST"
#define ORDER_TYPE_ALERT												"ALERT"
#define ORDER_TYPE_ALLOC												"ALLOC"
#define ORDER_TYPE_AVGCOST												"AVGCOST"
#define ORDER_TYPE_BASKET												"BASKET"
#define ORDER_TYPE_COND													"COND"
#define ORDER_TYPE_CONDORDER											"CONDORDER"
#define ORDER_TYPE_CONSCOST												"CONSCOST"
#define ORDER_TYPE_DAY													"DAY"
#define ORDER_TYPE_DEACT												"DEACT"
#define ORDER_TYPE_DEACTDIS												"DEACTDIS"
#define ORDER_TYPE_DEACTEOD												"DEACTEOD"
#define ORDER_TYPE_GTT													"GTT"
#define ORDER_TYPE_HID													"HID"
#define ORDER_TYPE_LTH													"LTH"
#define ORDER_TYPE_NONALGO												"NONALGO"
#define ORDER_TYPE_SCALE												"SCALE"
#define ORDER_TYPE_SCALERST												"SCALERST"
#define ORDER_TYPE_WHATIF												"WHATIF"


/*
scanner scan codes:
*/
#define TWS_SCANNER_BOND_CUSIP_AZ                           "BOND_CUSIP_AZ"
#define TWS_SCANNER_BOND_CUSIP_ZA                           "BOND_CUSIP_ZA"
#define TWS_SCANNER_FAR_MATURITY_DATE                       "FAR_MATURITY_DATE"
#define TWS_SCANNER_HALTED                                  "HALTED"
#define TWS_SCANNER_HIGHEST_SLB_BID                         "HIGHEST_SLB_BID"
#define TWS_SCANNER_HIGH_BOND_ASK_CURRENT_YIELD_ALL         "HIGH_BOND_ASK_CURRENT_YIELD_ALL"
#define TWS_SCANNER_HIGH_BOND_ASK_YIELD_ALL                 "HIGH_BOND_ASK_YIELD_ALL"
#define TWS_SCANNER_HIGH_BOND_DEBT_2_BOOK_RATIO             "HIGH_BOND_DEBT_2_BOOK_RATIO"
#define TWS_SCANNER_HIGH_BOND_DEBT_2_EQUITY_RATIO           "HIGH_BOND_DEBT_2_EQUITY_RATIO"
#define TWS_SCANNER_HIGH_BOND_DEBT_2_TAN_BOOK_RATIO         "HIGH_BOND_DEBT_2_TAN_BOOK_RATIO"
#define TWS_SCANNER_HIGH_BOND_EQUITY_2_BOOK_RATIO           "HIGH_BOND_EQUITY_2_BOOK_RATIO"
#define TWS_SCANNER_HIGH_BOND_EQUITY_2_TAN_BOOK_RATIO       "HIGH_BOND_EQUITY_2_TAN_BOOK_RATIO"
#define TWS_SCANNER_HIGH_BOND_SPREAD_ALL                    "HIGH_BOND_SPREAD_ALL"
#define TWS_SCANNER_HIGH_COUPON_RATE                        "HIGH_COUPON_RATE"
#define TWS_SCANNER_HIGH_DIVIDEND_YIELD                     "HIGH_DIVIDEND_YIELD"
#define TWS_SCANNER_HIGH_DIVIDEND_YIELD_IB                  "HIGH_DIVIDEND_YIELD_IB"
#define TWS_SCANNER_HIGH_GROWTH_RATE                        "HIGH_GROWTH_RATE"
#define TWS_SCANNER_HIGH_MOODY_RATING_ALL                   "HIGH_MOODY_RATING_ALL"
#define TWS_SCANNER_HIGH_OPEN_GAP                           "HIGH_OPEN_GAP"
#define TWS_SCANNER_HIGH_OPT_IMP_VOLAT                      "HIGH_OPT_IMP_VOLAT"
#define TWS_SCANNER_HIGH_OPT_IMP_VOLAT_OVER_HIST            "HIGH_OPT_IMP_VOLAT_OVER_HIST"
#define TWS_SCANNER_HIGH_OPT_OPEN_INTEREST_PUT_CALL_RATIO   "HIGH_OPT_OPEN_INTEREST_PUT_CALL_RATIO"
#define TWS_SCANNER_HIGH_OPT_OPEN_INT_PUT_CALL_RATIO        "HIGH_OPT_OPEN_INT_PUT_CALL_RATIO"
#define TWS_SCANNER_HIGH_OPT_VOLUME_PUT_CALL_RATIO          "HIGH_OPT_VOLUME_PUT_CALL_RATIO"
#define TWS_SCANNER_HIGH_PE_RATIO                           "HIGH_PE_RATIO"
#define TWS_SCANNER_HIGH_PRICE_2_BOOK_RATIO                 "HIGH_PRICE_2_BOOK_RATIO"
#define TWS_SCANNER_HIGH_PRICE_2_TAN_BOOK_RATIO             "HIGH_PRICE_2_TAN_BOOK_RATIO"
#define TWS_SCANNER_HIGH_QUICK_RATIO                        "HIGH_QUICK_RATIO"
#define TWS_SCANNER_HIGH_RETURN_ON_EQUITY                   "HIGH_RETURN_ON_EQUITY"
#define TWS_SCANNER_HIGH_SYNTH_BID_REV_NAT_YIELD            "HIGH_SYNTH_BID_REV_NAT_YIELD"
#define TWS_SCANNER_HIGH_VS_13W_HL                          "HIGH_VS_13W_HL"
#define TWS_SCANNER_HIGH_VS_26W_HL                          "HIGH_VS_26W_HL"
#define TWS_SCANNER_HIGH_VS_52W_HL                          "HIGH_VS_52W_HL"
#define TWS_SCANNER_HOT_BY_OPT_VOLUME                       "HOT_BY_OPT_VOLUME"
#define TWS_SCANNER_HOT_BY_PRICE                            "HOT_BY_PRICE"
#define TWS_SCANNER_HOT_BY_PRICE_RANGE                      "HOT_BY_PRICE_RANGE"
#define TWS_SCANNER_HOT_BY_VOLUME                           "HOT_BY_VOLUME"
#define TWS_SCANNER_HOT_VOLUME                              "HOT_BY_VOLUME"
#define TWS_SCANNER_LIMIT_UP_DOWN                           "LIMIT_UP_DOWN"
#define TWS_SCANNER_LOWEST_SLB_ASK                          "LOWEST_SLB_ASK"
#define TWS_SCANNER_LOW_BOND_BID_CURRENT_YIELD_ALL          "LOW_BOND_BID_CURRENT_YIELD_ALL"
#define TWS_SCANNER_LOW_BOND_BID_YIELD_ALL                  "LOW_BOND_BID_YIELD_ALL"
#define TWS_SCANNER_LOW_BOND_DEBT_2_BOOK_RATIO              "LOW_BOND_DEBT_2_BOOK_RATIO"
#define TWS_SCANNER_LOW_BOND_DEBT_2_EQUITY_RATIO            "LOW_BOND_DEBT_2_EQUITY_RATIO"
#define TWS_SCANNER_LOW_BOND_DEBT_2_TAN_BOOK_RATIO          "LOW_BOND_DEBT_2_TAN_BOOK_RATIO"
#define TWS_SCANNER_LOW_BOND_EQUITY_2_BOOK_RATIO            "LOW_BOND_EQUITY_2_BOOK_RATIO"
#define TWS_SCANNER_LOW_BOND_EQUITY_2_TAN_BOOK_RATIO        "LOW_BOND_EQUITY_2_TAN_BOOK_RATIO"
#define TWS_SCANNER_LOW_BOND_SPREAD_ALL                     "LOW_BOND_SPREAD_ALL"
#define TWS_SCANNER_LOW_COUPON_RATE                         "LOW_COUPON_RATE"
#define TWS_SCANNER_LOW_GROWTH_RATE                         "LOW_GROWTH_RATE"
#define TWS_SCANNER_LOW_MOODY_RATING_ALL                    "LOW_MOODY_RATING_ALL"
#define TWS_SCANNER_LOW_OPEN_GAP                            "LOW_OPEN_GAP"
#define TWS_SCANNER_LOW_OPT_IMP_VOLAT                       "LOW_OPT_IMP_VOLAT"
#define TWS_SCANNER_LOW_OPT_IMP_VOLAT_OVER_HIST             "LOW_OPT_IMP_VOLAT_OVER_HIST"
#define TWS_SCANNER_LOW_OPT_OPEN_INTEREST_PUT_CALL_RATIO    "LOW_OPT_OPEN_INTEREST_PUT_CALL_RATIO"
#define TWS_SCANNER_LOW_OPT_OPEN_INT_PUT_CALL_RATIO         "LOW_OPT_OPEN_INT_PUT_CALL_RATIO"
#define TWS_SCANNER_LOW_OPT_VOLUME_PUT_CALL_RATIO           "LOW_OPT_VOLUME_PUT_CALL_RATIO"
#define TWS_SCANNER_LOW_PE_RATIO                            "LOW_PE_RATIO"
#define TWS_SCANNER_LOW_PRICE_2_BOOK_RATIO                  "LOW_PRICE_2_BOOK_RATIO"
#define TWS_SCANNER_LOW_PRICE_2_TAN_BOOK_RATIO              "LOW_PRICE_2_TAN_BOOK_RATIO"
#define TWS_SCANNER_LOW_QUICK_RATIO                         "LOW_QUICK_RATIO"
#define TWS_SCANNER_LOW_RETURN_ON_EQUITY                    "LOW_RETURN_ON_EQUITY"
#define TWS_SCANNER_LOW_SYNTH_ASK_REV_NAT_YIELD             "LOW_SYNTH_ASK_REV_NAT_YIELD"
#define TWS_SCANNER_LOW_VS_13W_HL                           "LOW_VS_13W_HL"
#define TWS_SCANNER_LOW_VS_26W_HL                           "LOW_VS_26W_HL"
#define TWS_SCANNER_LOW_VS_52W_HL                           "LOW_VS_52W_HL"
#define TWS_SCANNER_LOW_WAR_REL_IMP_VOLAT                   "LOW_WAR_REL_IMP_VOLAT"
#define TWS_SCANNER_MOST_ACTIVE                             "MOST_ACTIVE"
#define TWS_SCANNER_MOST_ACTIVE_AVG_USD                     "MOST_ACTIVE_AVG_USD"
#define TWS_SCANNER_MOST_ACTIVE_USD                         "MOST_ACTIVE_USD"
#define TWS_SCANNER_NEAR_MATURITY_DATE                      "NEAR_MATURITY_DATE"
#define TWS_SCANNER_NOT_OPEN                                "NOT_OPEN"
#define TWS_SCANNER_OPT_OPEN_INTEREST_MOST_ACTIVE           "OPT_OPEN_INTEREST_MOST_ACTIVE"
#define TWS_SCANNER_OPT_VOLUME_MOST_ACTIVE                  "OPT_VOLUME_MOST_ACTIVE"
#define TWS_SCANNER_PMONITOR_AVAIL_CONTRACTS                "PMONITOR_AVAIL_CONTRACTS"
#define TWS_SCANNER_PMONITOR_CTT                            "PMONITOR_CTT"
#define TWS_SCANNER_PMONITOR_RFQ                            "PMONITOR_RFQ"
#define TWS_SCANNER_TOP_OPEN_PERC_GAIN                      "TOP_OPEN_PERC_GAIN"
#define TWS_SCANNER_TOP_OPEN_PERC_LOSE                      "TOP_OPEN_PERC_LOSE"
#define TWS_SCANNER_TOP_OPT_IMP_VOLAT_GAIN                  "TOP_OPT_IMP_VOLAT_GAIN"
#define TWS_SCANNER_TOP_OPT_IMP_VOLAT_LOSE                  "TOP_OPT_IMP_VOLAT_LOSE"
#define TWS_SCANNER_TOP_PERC_GAIN                           "TOP_PERC_GAIN"
#define TWS_SCANNER_TOP_PERC_LOSE                           "TOP_PERC_LOSE"
#define TWS_SCANNER_TOP_PRICE_RANGE                         "TOP_PRICE_RANGE"
#define TWS_SCANNER_TOP_TRADE_COUNT                         "TOP_TRADE_COUNT"
#define TWS_SCANNER_TOP_TRADE_RATE                          "TOP_TRADE_RATE"
#define TWS_SCANNER_TOP_VOLUME_RATE                         "TOP_VOLUME_RATE"
#define TWS_SCANNER_WSH_NEXT_ANALYST_MEETING                "WSH_NEXT_ANALYST_MEETING"
#define TWS_SCANNER_WSH_NEXT_EARNINGS                       "WSH_NEXT_EARNINGS"
#define TWS_SCANNER_WSH_NEXT_EVENT                          "WSH_NEXT_EVENT"
#define TWS_SCANNER_WSH_NEXT_MAJOR_EVENT                    "WSH_NEXT_MAJOR_EVENT"
#define TWS_SCANNER_WSH_PREV_ANALYST_MEETING                "WSH_PREV_ANALYST_MEETING"
#define TWS_SCANNER_WSH_PREV_EARNINGS                       "WSH_PREV_EARNINGS"
#define TWS_SCANNER_WSH_PREV_EVENT                          "WSH_PREV_EVENT"


/* what the heck are these? */
#define OPT_UNKNOWN           "?"
#define OPT_BROKER_DEALER     "b"
#define OPT_CUSTOMER          "c"
#define OPT_FIRM              "f"
#define OPT_ISEMM             "m"
#define OPT_FARMM             "n"
#define OPT_SPECIALIST        "y"



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
#define  MIN_SERVER_VER_SCALE_ORDERS3 60
#define  MIN_SERVER_VER_ORDER_COMBO_LEGS_PRICE 61
#define  MIN_SERVER_VER_TRAILING_PERCENT 62




#endif /* TWS_DEFINITIONS */

#if TWS_DEFINITIONS > 2



#ifdef __cplusplus
namespace tws {
	extern "C" {
#endif


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
    char *         c_sectype;                           /* security type ("BAG" -> transmits combo legs, "" -> does not transmit combo legs, "BOND" -> strike/expiry/right/multiplier/local_symbol must be set) */
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

typedef struct tr_order_combo_leg 
{
	double cl_price; /* price per leg */
} tr_order_combo_leg_t;

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
    double   o_trailing_percent;
    double   o_basis_points;
    double   o_scale_price_increment;					/* For SCALE orders only */
    double   o_scale_price_adjust_value;				/* For SCALE orders only */
    double   o_scale_profit_offset;						/* For SCALE orders only */
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

	tr_order_combo_leg_t *o_combo_legs;

    int      o_algo_params_count;                       /* how many tag values are in o_algo_params, 0 if unused */
	int      o_smart_combo_routing_params_count;        /* how many tag values are in o_smart_combo_routing_params, 0 if unused */
	int      o_combo_legs_count;                  /* how many tag values are in o_combo_legs, 0 if unused */

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
    int      o_scale_subs_level_size;					/* For SCALE orders only */
    int      o_scale_init_level_size;					/* For SCALE orders only */
    int      o_scale_price_adjust_interval;				/* For SCALE orders only */
    int      o_scale_init_position;						/* For SCALE orders only */
    int      o_scale_init_fill_qty;						/* For SCALE orders only */
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
    unsigned o_scale_auto_reset: 1;						/* For SCALE orders only */
    unsigned o_scale_random_percent: 1;					/* For SCALE orders only */
} tr_order_t;

/*
OrderState
*/
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

typedef struct tr_commission_report
{
    char  *cr_exec_id;
    char  *cr_currency;
    double cr_commission;
    double cr_realized_pnl;
    double cr_yield;
    int    cr_yield_redemption_date; /* YYYYMMDD format */
} tr_commission_report_t;


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


typedef struct twsclient_errmsg {
    twsclient_error_code_t err_code;
    const char *err_msg;
} twsclient_errmsg_t;


#ifdef __cplusplus
	}
}
#endif




#endif /* TWS_DEFINITIONS */

#if TWS_DEFINITIONS > 3



#ifdef __cplusplus
namespace tws {
	extern "C" {
#endif


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

void   tws_init_exec_filter(tws_instance_t *tws, tr_exec_filter_t *filter);
void   tws_destroy_exec_filter(tws_instance_t *tws, tr_exec_filter_t *filter);

void   tws_init_scanner_subscription(tws_instance_t *tws, tr_scanner_subscription_t *ss);
void   tws_destroy_scanner_subscription(tws_instance_t *tws, tr_scanner_subscription_t *ss);

void   tws_init_tag_value(tws_instance_t *tws, tr_tag_value_t *t);
void   tws_destroy_tag_value(tws_instance_t *tws, tr_tag_value_t *t);

void   tws_init_order_combo_leg(tws_instance_t *tws, tr_order_combo_leg_t *ocl);
void   tws_destroy_order_combo_leg(tws_instance_t *tws, tr_order_combo_leg_t *ocl);

void   tws_init_under_comp(tws_instance_t *tws, under_comp_t *u);
void   tws_destroy_under_comp(tws_instance_t *tws, under_comp_t *u);

void   tws_init_order(tws_instance_t *tws, tr_order_t *o);
void   tws_destroy_order(tws_instance_t *tws, tr_order_t *o);

void   tws_init_contract(tws_instance_t *ti, tr_contract_t *c);
void   tws_destroy_contract(tws_instance_t *ti, tr_contract_t *c);


/* 
  helper: as all strings (char * pointers) in the tws structs are initialized to dedicated storage
  when you invoke the appropriate tws_init_....() call, you'll need the safe tws_strcpy() to copy
  any string content into those structs.

  tws_strcpy() also accepts src == NULL, which is treated as a src=="" value.

  The function always returns the 'tws_string_ref' pointer. 

  Use tws_strcpy() to prevent structure member overruns when copying string data into initialized
  tws structs.
*/
char *tws_strcpy(char *tws_string_ref, const char *src);

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
void event_receive_fa(void *opaque, tr_fa_msg_type_t fa_data_type, const char cxml[]);
/* fired by: HISTORICAL_DATA (possibly multiple times per incoming message) */
void event_historical_data(void *opaque, int reqid, const char date[], double open, double high, double low, double close, int volume, int bar_count, double wap, int has_gaps);
/* fired by: HISTORICAL_DATA  (once, after one or more invocations of event_historical_data()) */
void event_historical_data_end(void *opaque, int reqid, const char completion_from[], const char completion_to[]);
/* fired by: SCANNER_PARAMETERS */
void event_scanner_parameters(void *opaque, const char xml[]);
/* fired by: SCANNER_DATA (possibly multiple times per incoming message) */
void event_scanner_data(void *opaque, int ticker_id, int rank, tr_contract_details_t *cd, const char distance[], const char benchmark[], const char projection[], const char legs_str[]);
/* fired by: SCANNER_DATA (once, after one or more invocations of event_scanner_data()) */
void event_scanner_data_end(void *opaque, int ticker_id, int num_elements);
/* fired by: SCANNER_DATA (once, before any invocations of event_scanner_data()) */
void event_scanner_data_start(void *opaque, int ticker_id, int num_elements);
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
/* fired by: COMMISSION_REPORT */
void event_commission_report(void *opaque, tr_commission_report_t *report);



#ifdef TWSAPI_GLOBALS
twsclient_errmsg_t twsclient_err_indication[] = {
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
    { FAIL_SEND_EXCERCISE_OPTIONS, "Exercise Options Sending Error - "},

    /* -------------------------------------------------------------- */
    /* The following items are taken from the TWS API on-line documentation: the error code strings for errors reported by the TWS client in the non-5xx range: */

    /* Error Codes */
	{ FAIL_MAX_RATE_OF_MESSAGES_EXCEEDED /* 100 */, "Max rate of messages per second has been exceeded." },
	{ FAIL_MAX_NUMBER_OF_TICKERS_REACHED /* 101 */, "Max number of tickers has been reached." },
	{ FAIL_DUPLICATE_TICKER_ID /* 102 */, "Duplicate ticker ID." },
	{ FAIL_DUPLICATE_ORDER_ID /* 103 */, "Duplicate order ID." },
	{ FAIL_MODIFY_FILLED_ORDER /* 104 */, "Can't modify a filled order." },
	{ FAIL_MODIFIED_ORDER_DOES_NOT_MATCH_ORIGINAL /* 105 */, "Order being modified does not match original order." },
	{ FAIL_TRANSMIT_ORDER_ID /* 106 */, "Can't transmit order ID:" },
	{ FAIL_TRANSMIT_INCOMPLETE_ORDER /* 107 */, "Cannot transmit incomplete order." },
	{ FAIL_PRICE_OUT_OF_RANGE /* 109 */, "Price is out of the range defined by the Percentage setting at order defaults frame. The order will not be transmitted." },
	{ FAIL_PRICE_DOES_NOT_CONFORM_TO_VARIATION /* 110 */, "The price does not conform to the minimum price variation for this contract." },
	{ FAIL_TIF_AND_ORDER_TYPE_ARE_INCOMPATIBLE /* 111 */, "The TIF (Tif type) and the order type are incompatible." },
	{ FAIL_ILLEGAL_TIF_OPTION /* 113 */, "The Tif option should be set to DAY for MOC and LOC orders." },
	{ FAIL_RELATIVE_ORDERS_INVALID /* 114 */, "Relative orders are valid for stocks only." },
	{ FAIL_SUBMIT_RELATIVE_ORDERS /* 115 */, "Relative orders for US stocks can only be submitted to SMART, SMART_ECN, INSTINET, or PRIMEX." },
	{ FAIL_DEAD_EXCHANGE /* 116 */, "The order cannot be transmitted to a dead exchange." },
	{ FAIL_BLOCK_ORDER_SIZE /* 117 */, "The block order size must be at least 50." },
	{ FAIL_VWAP_ORDERS_ROUTING /* 118 */, "VWAP orders must be routed through the VWAP exchange." },
	{ FAIL_NON_VWAP_ORDERS_ON_VWAP_EXCHANGE /* 119 */, "Only VWAP orders may be placed on the VWAP exchange." },
	{ FAIL_VWAP_ORDER_TOO_LATE /* 120 */, "It is too late to place a VWAP order for today." },
	{ FAIL_INVALID_BD_FLAG /* 121 */, "Invalid BD flag for the order. Check \"Destination\" and \"BD\" flag." },
	{ FAIL_NO_REQUEST_TAG /* 122 */, "No request tag has been found for order:" },
	{ FAIL_NO_RECORD_AVAILABLE /* 123 */, "No record is available for conid:" },
	{ FAIL_NO_MARKET_RULE_AVAILABLE /* 124 */, "No market rule is available for conid:" },
	{ FAIL_BUY_PRICE_INCORRECT /* 125 */, "Buy price must be the same as the best asking price." },
	{ FAIL_SELL_PRICE_INCORRECT /* 126 */, "Sell price must be the same as the best bidding price." },
	{ FAIL_VWAP_ORDER_PLACED_AFTER_START_TIME /* 129 */, "VWAP orders must be submitted at least three minutes before the start time." },
	{ FAIL_SWEEP_TO_FILL_INCORRECT /* 131 */, "The sweep-to-fill flag and display size are only valid for US stocks routed through SMART, and will be ignored." },
	{ FAIL_CLEARING_ACCOUNT_MISSING /* 132 */, "This order cannot be transmitted without a clearing account." },
	{ FAIL_SUBMIT_NEW_ORDER /* 133 */, "Submit new order failed." },
	{ FAIL_MODIFY_ORDER /* 134 */, "Modify order failed." },
	{ FAIL_FIND_ORDER /* 135 */, "Can't find order with ID =" },
	{ FAIL_ORDER_CANNOT_CANCEL /* 136 */, "This order cannot be canceled." },
	{ FAIL_CANNOT_CANCEL_VWAP_ORDER /* 137 */, "VWAP orders can only be canceled up to three minutes before the start time." },
	{ FAIL_COULD_NOT_PARSE_TICKER_REQUEST /* 138 */, "Could not parse ticker request:" },
	{ FAIL_PARSING_ERROR /* 139 */, "Parsing error:" },
	{ FAIL_SIZE_VALUE_NOT_INTEGER /* 140 */, "The size value should be an integer:" },
	{ FAIL_PRICE_VALUE_NOT_FLTPOINT /* 141 */, "The price value should be a double:" },
	{ FAIL_MISSING_INSTITUTIONAL_ACCOUNT_INFO /* 142 */, "Institutional customer account does not have account info" },
	{ FAIL_REQUESTED_ID_NOT_INTEGER /* 143 */, "Requested ID is not an integer number." },
	{ FAIL_ORDER_SIZE_DOES_NOT_MATCH_ALLOCATION /* 144 */, "Order size does not match total share allocation.  To adjust the share allocation, right-click on the order and select “Modify > Share Allocation.”" },
	{ FAIL_INVALID_ENTRY_FIELDS /* 145 */, "Error in validating entry fields -" },
	{ FAIL_INVALID_TRIGGER_METHOD /* 146 */, "Invalid trigger method." },
	{ FAIL_CONDITIONAL_CONTRACT_INFO_INCOMPLETE /* 147 */, "The conditional contract info is incomplete." },
	{ FAIL_CANNOT_SUBMIT_CONDITIONAL_ORDER /* 148 */, "A conditional order can only be submitted when the order type is set to limit or market." },
	{ FAIL_ORDER_TRANSMITTED_WITHOUT_USER_NAME /* 151 */, "This order cannot be transmitted without a user name." },
	{ FAIL_INVALID_HIDDEN_ORDER_ATTRIBUTE /* 152 */, "The \"hidden\" order attribute may not be specified for this order." },
	{ FAIL_EFPS_CAN_ONLY_LIMIT_ORDERS /* 153 */, "EFPs can only be limit orders." },
	{ FAIL_ORDERS_TRANSMITTED_FOR_HALTED_SECURITY /* 154 */, "Orders cannot be transmitted for a halted security." },
	{ FAIL_SIZEOP_ORDER_MUST_HAVE_USERNAME_AND_ACCOUNT /* 155 */, "A sizeOp order must have a username and account." },
	{ FAIL_SIZEOP_ORDER_MUST_GO_TO_IBSX /* 156 */, "A SizeOp order must go to IBSX" },
	{ FAIL_ICEBERG_OR_DISCRETIONARY /* 157 */, "An order can be EITHER Iceberg or Discretionary. Please remove either the Discretionary amount or the Display size." },
	{ FAIL_MUST_SPECIFY_OFFSET /* 158 */, "You must specify an offset amount or a percent offset value." },
	{ FAIL_PERCENT_OFFSET_VALUE_OUT_OF_RANGE /* 159 */, "The percent offset value must be between 0% and 100%." },
	{ FAIL_SIZE_VALUE_ZERO /* 160 */, "The size value cannot be zero." },
	{ FAIL_ORDER_NOT_IN_CANCELLABLE_STATE /* 161 */, "Cancel attempted when order is not in a cancellable state. Order permId =" },
	{ FAIL_HISTORICAL_MARKET_DATA_SERVICE /* 162 */, "Historical market data Service error message." },
	{ FAIL_VIOLATE_PERCENTAGE_IN_ORDER_SETTINGS /* 163 */, "The price specified would violate the percentage constraint specified in the default order settings." },
	{ FAIL_NO_MARKET_DATA_TO_CHECK_VIOLATIONS /* 164 */, "There is no market data to check price percent violations." },
	{ INFO_HISTORICAL_MARKET_DATA_SERVICE_QUERY /* 165 */, "Historical market Data Service query message." },
	{ FAIL_HMDS_EXPIRED_CONTRACT_VIOLATION /* 166 */, "HMDS Expired Contract Violation." },
	{ FAIL_VWAP_ORDER_NOT_IN_FUTURE /* 167 */, "VWAP order time must be in the future." },
	{ FAIL_DISCRETIONARY_AMOUNT_MISMATCH /* 168 */, "Discretionary amount does not conform to the minimum price variation for this contract." },
                                                                                   
	{ FAIL_NO_SECURITY_DEFINITION /* 200 */, "No security definition has been found for the request." },
	{ FAIL_ORDER_REJECTED /* 201 */, "Order rejected - Reason:" },
	{ FAIL_ORDER_CANCELED /* 202 */, "Order canceled - Reason:" },
	{ FAIL_SECURITY_NOT_AVAILABLE_ALLOWED /* 203 */, "The security <security> is not available or allowed for this account." },
                                                                                   
	{ FAIL_FIND_EID /* 300 */, "Can't find EId with ticker Id:" },
	{ FAIL_INVALID_TICKER_ACTION /* 301 */, "Invalid ticker action:" },
	{ FAIL_ERROR_PARSING_STOP /* 302 */, "Error parsing stop ticker string:" },
	{ FAIL_INVALID_ACTION /* 303 */, "Invalid action:" },
	{ FAIL_INVALID_ACCOUNT_VALUE /* 304 */, "Invalid account value action:" },
	{ FAIL_REQUEST_PARSING_ERROR /* 305 */, "Request parsing error, the request has been ignored." },
	{ FAIL_DDE_REQUEST /* 306 */, "Error processing DDE request:" },
	{ FAIL_INVALID_REQUEST_TOPIC /* 307 */, "Invalid request topic:" },
	{ FAIL_UNABLE_TO_CREATE_PAGE /* 308 */, "Unable to create the 'API' page in TWS as the maximum number of pages already exists." },
	{ FAIL_MAX_NUMBER_MARKET_DEPTH_REQUESTS_REACHED /* 309 */, "Max number (3) of market depth requests has been reached.   Note: TWS currently limits users to a maximum of 3 distinct market depth requests. This same restriction applies to API clients, however API clients may make multiple market depth requests for the same security." },
	{ FAIL_FIND_SUBSCRIBED_MARKET_DEPTH /* 310 */, "Can't find the subscribed market depth with tickerId:" },
	{ FAIL_ORIGIN_INVALID /* 311 */, "The origin is invalid." },
	{ FAIL_COMBO_DETAILS_INVALID /* 312 */, "The combo details are invalid." },
	{ FAIL_COMBO_DETAILS_FOR_LEG_INVALID /* 313 */, "The combo details for leg '<leg number>' are invalid." },
	{ FAIL_SECURITY_TYPE_REQUIRES_COMBO_LEG_DETAILS /* 314 */, "Security type 'BAG' requires combo leg details." },
	{ FAIL_COMBO_LEGS_ROUTING_RESTRICTED /* 315 */, "Stock combo legs are restricted to SMART order routing." },
	{ FAIL_MARKET_DEPTH_DATA_HALTED /* 316 */, "Market depth data has been HALTED. Please re-subscribe." },
	{ FAIL_MARKET_DEPTH_DATA_RESET /* 317 */, "Market depth data has been RESET. Please empty deep book contents before applying any new entries." },
	{ FAIL_INVALID_LOG_LEVEL /* 319 */, "Invalid log level <log level>" },
	{ FAIL_SERVER_ERROR_READING_REQUEST /* 320 */, "Server error when reading an API client request." },
	{ FAIL_SERVER_ERROR_VALIDATING_REQUEST /* 321 */, "Server error when validating an API client request." },
	{ FAIL_SERVER_ERROR_PROCESSING_REQUEST /* 322 */, "Server error when processing an API client request." },
	{ FAIL_SERVER_ERROR /* 323 */, "Server error: cause - %s" },
	{ FAIL_SERVER_ERROR_READING_DDE_REQUEST /* 324 */, "Server error when reading a DDE client request (missing information)." },
	{ FAIL_DISCRETIONARY_ORDERS_NOT_SUPPORTED /* 325 */, "Discretionary orders are not supported for this combination of exchange and order type." },
	{ FAIL_UNABLE_TO_CONNECT_ID_IN_USE /* 326 */, "Unable to connect as the client id is already in use. Retry with a unique client id." },
	{ FAIL_CANNOT_SET_AUTO_BIND_PROPERTY /* 327 */, "Only API connections with clientId set to 0 can set the auto bind TWS orders property." },
	{ FAIL_CANNOT_ATTACH_TRAILING_STOP_ORDERS /* 328 */, "Trailing stop orders can be attached to limit or stop-limit orders only." },
	{ FAIL_ORDER_MODIFY_FAILED /* 329 */, "Order modify failed. Cannot change to the new order type." },
	{ FAIL_ONLY_FA_OR_STL_CUSTOMERS /* 330 */, "Only FA or STL customers can request managed accounts list." },
	{ FAIL_FA_OR_STL_INTERNAL_ERROR /* 331 */, "Internal error. FA or STL does not have any managed accounts." },
	{ FAIL_INVALID_ACCOUNT_CODES_FOR_PROFILE /* 332 */, "The account codes for the order profile are invalid." },
	{ FAIL_INVALID_SHARE_ALLOCATION_SYNTAX /* 333 */, "Invalid share allocation syntax." },
	{ FAIL_INVALID_GOOD_TILL_DATE_ORDER /* 334 */, "Invalid Good Till Date order" },
	{ FAIL_DELTA_OUT_OF_RANGE /* 335 */, "Invalid delta: The delta must be between 0 and 100." },
	{ FAIL_TIME_OR_TIME_ZONE_INVALID /* 336 */, "The time or time zone is invalid.   The correct format is 'hh:mm:ss xxx' where 'xxx' is an optionally specified time-zone. E.g.: 15:59:00 EST.   Note that there is a space between the time and the time zone.   If no time zone is specified, local time is assumed." },
	{ FAIL_DATE_TIME_INVALID /* 337 */, "The date, time, or time-zone entered is invalid. The correct format is yyyymmdd hh:mm:ss xxx where yyyymmdd and xxx are optional. E.g.: 20031126 15:59:00 EST.   Note that there is a space between the date and time, and between the time and time-zone.   If no date is specified, current date is assumed.   If no time-zone is specified, local time-zone is assumed." },
	{ FAIL_GOOD_AFTER_TIME_ORDERS_DISABLED /* 338 */, "Good After Time orders are currently disabled on this exchange." },
	{ FAIL_FUTURES_SPREAD_NOT_SUPPORTED /* 339 */, "Futures spread are no longer supported. Please use combos instead." },
	{ FAIL_INVALID_IMPROVEMENT_AMOUNT /* 340 */, "Invalid improvement amount for box auction strategy." },
	{ FAIL_INVALID_DELTA /* 341 */, "Invalid delta. Valid values are from 1 to 100.   You can set the delta from the \"Pegged to Stock\" section of the Order Ticket Panel, or by selecting Page/Layout from the main menu and adding the Delta column." },
	{ FAIL_PEGGED_ORDER_NOT_SUPPORTED /* 342 */, "Pegged order is not supported on this exchange." },
	{ FAIL_DATE_TIME_ENTERED_INVALID /* 343 */, "The date, time, or time-zone entered is invalid. The correct format is yyyymmdd hh:mm:ss xxx where yyyymmdd and xxx are optional. E.g.: 20031126 15:59:00 EST.   Note that there is a space between the date and time, and between the time and time-zone.   If no date is specified, current date is assumed.   If no time-zone is specified, local time-zone is assumed." },
	{ FAIL_ACCOUNT_NOT_FINANCIAL_ADVISOR /* 344 */, "The account logged into is not a financial advisor account." },
	{ FAIL_GENERIC_COMBO_NOT_SUPPORTED /* 345 */, "Generic combo is not supported for FA advisor account." },
	{ FAIL_NOT_PRIVILEGED_ACCOUNT /* 346 */, "Not an institutional account or an away clearing account." },
	{ FAIL_SHORT_SALE_SLOT_VALUE /* 347 */, "Short sale slot value must be 1 (broker holds shares) or 2 (delivered from elsewhere)." },
	{ FAIL_ORDER_NOT_SHORT_SALE /* 348 */, "Order not a short sale -- type must be SSHORT to specify short sale slot." },
	{ FAIL_COMBO_DOES_NOT_SUPPORT_GOOD_AFTER /* 349 */, "Generic combo does not support \"Good After\" attribute." },
	{ FAIL_MINIMUM_QUANTITY_NOT_SUPPORTED /* 350 */, "Minimum quantity is not supported for best combo order." },
	{ FAIL_REGULAR_TRADING_HOURS_ONLY /* 351 */, "The \"Regular Trading Hours only\" flag is not valid for this order." },
	{ FAIL_SHORT_SALE_SLOT_REQUIRES_LOCATION /* 352 */, "Short sale slot value of 2 (delivered from elsewhere) requires location." },
	{ FAIL_SHORT_SALE_SLOT_REQUIRES_NO_LOCATION /* 353 */, "Short sale slot value of 1 requires no location be specified." },
	{ FAIL_NOT_SUBSCRIBED_TO_MARKET_DATA /* 354 */, "Not subscribed to requested market data." },
	{ FAIL_ORDER_SIZE_DOES_NOT_CONFORM_RULE /* 355 */, "Order size does not conform to market rule." },
	{ FAIL_SMART_COMBO_ORDER_NOT_SUPPORT_OCA /* 356 */, "Smart-combo order does not support OCA group." },
	{ FAIL_CLIENT_VERSION_OUT_OF_DATE /* 357 */, "Your client version is out of date." },
	{ FAIL_COMBO_CHILD_ORDER_NOT_SUPPORTED /* 358 */, "Smart combo child order not supported." },
	{ FAIL_COMBO_ORDER_REDUCED_SUPPORT /* 359 */, "Combo order only supports reduce on fill without block(OCA)." },
	{ FAIL_NO_WHATIF_CHECK_SUPPORT /* 360 */, "No whatif check support for smart combo order." },
	{ FAIL_INVALID_TRIGGER_PRICE /* 361 */, "Invalid trigger price." },
	{ FAIL_INVALID_ADJUSTED_STOP_PRICE /* 362 */, "Invalid adjusted stop price." },
	{ FAIL_INVALID_ADJUSTED_STOP_LIMIT_PRICE /* 363 */, "Invalid adjusted stop limit price." },
	{ FAIL_INVALID_ADJUSTED_TRAILING_AMOUNT /* 364 */, "Invalid adjusted trailing amount." },
	{ FAIL_NO_SCANNER_SUBSCRIPTION_FOUND /* 365 */, "No scanner subscription found for ticker id:" },
	{ FAIL_NO_HISTORICAL_DATA_QUERY_FOUND /* 366 */, "No historical data query found for ticker id:" },
	{ FAIL_VOLATILITY_TYPE /* 367 */, "Volatility type if set must be 1 or 2 for VOL orders. Do not set it for other order types." },
	{ FAIL_REFERENCE_PRICE_TYPE /* 368 */, "Reference Price Type must be 1 or 2 for dynamic volatility management. Do not set it for non-VOL orders." },
	{ FAIL_VOLATILITY_ORDERS_US_ONLY /* 369 */, "Volatility orders are only valid for US options." },
	{ FAIL_DYNAMIC_VOLATILITY_ORDERS_ROUTING /* 370 */, "Dynamic Volatility orders must be SMART routed, or trade on a Price Improvement Exchange." },
	{ FAIL_VOL_ORDER_REQUIRES_POSITIVE_VOLATILITY /* 371 */, "VOL order requires positive floating point value for volatility. Do not set it for other order types." },
	{ FAIL_DYNAMIC_VOL_ATTRIBUTE_ON_NON_VOL_ORDER /* 372 */, "Cannot set dynamic VOL attribute on non-VOL order." },
	{ FAIL_CANNOT_SET_STOCK_RANGE_ATTRIBUTE /* 373 */, "Can only set stock range attribute on VOL or RELATIVE TO STOCK order." },
	{ FAIL_STOCK_RANGE_ATTRIBUTES_INVALID /* 374 */, "If both are set, the lower stock range attribute must be less than the upper stock range attribute." },
	{ FAIL_STOCK_RANGE_ATTRIBUTES_NEGATIVE /* 375 */, "Stock range attributes cannot be negative." },
	{ FAIL_ORDER_NOT_ELIGIBLE_FOR_CONTINUOUS_UPDATE /* 376 */, "The order is not eligible for continuous update. The option must trade on a cheap-to-reroute exchange." },
	{ FAIL_MUST_SPECIFY_VALID_DELTA_HEDGE_PRICE /* 377 */, "Must specify valid delta hedge order aux. price." },
	{ FAIL_DELTA_HEDGE_REQUIRES_AUX_PRICE /* 378 */, "Delta hedge order type requires delta hedge aux. price to be specified." },
	{ FAIL_DELTA_HEDGE_REQUIRES_NO_AUX_PRICE /* 379 */, "Delta hedge order type requires that no delta hedge aux. price be specified." },
	{ FAIL_ORDER_TYPE_NOT_ALLOWED /* 380 */, "This order type is not allowed for delta hedge orders." },
	{ FAIL_DDE_DLL_NEEDS_TO_UPGRADED /* 381 */, "Your DDE.dll needs to be upgraded." },
	{ FAIL_PRICE_VIOLATES_TICKS_CONSTRAINT /* 382 */, "The price specified violates the number of ticks constraint specified in the default order settings." },
	{ FAIL_SIZE_VIOLATES_SIZE_CONSTRAINT /* 383 */, "The size specified violates the size constraint specified in the default order settings." },
	{ FAIL_INVALID_DDE_ARRAY_REQUEST /* 384 */, "Invalid DDE array request." },
	{ FAIL_DUPLICATE_TICKER_ID_FOR_SCANNER /* 385 */, "Duplicate ticker ID for API scanner subscription." },
	{ FAIL_DUPLICATE_TICKER_ID_FOR_HISTORICAL_DATA /* 386 */, "Duplicate ticker ID for API historical data query." },
	{ FAIL_UNSUPPORTED_ORDER_TYPE_FOR_EXCHANGE /* 387 */, "Unsupported order type for this exchange and security type." },
	{ FAIL_ORDER_SIZE_BELOW_REQUIREMENT /* 388 */, "Order size is smaller than the minimum requirement." },
	{ FAIL_ROUTED_ORDER_ID_NOT_UNIQUE /* 389 */, "Supplied routed order ID is not unique." },
	{ FAIL_ROUTED_ORDER_ID_INVALID /* 390 */, "Supplied routed order ID is invalid." },
	{ FAIL_TIME_OR_TIME_ZONE_ENTERED_INVALID /* 391 */, "The time or time-zone entered is invalid. The correct format is hh:mm:ss xxx where xxx is an optionally specified time-zone. E.g.: 15:59:00 EST.   Note that there is a space between the time and the time zone.   If no time zone is specified, local time is assumed.   " },
	{ FAIL_INVALID_ORDER_CONTRACT_EXPIRED /* 392 */, "Invalid order: contract expired." },
	{ FAIL_SHORT_SALE_SLOT_FOR_DELTA_HEDGE_ONLY /* 393 */, "Short sale slot may be specified for delta hedge orders only." },
	{ FAIL_INVALID_PROCESS_TIME /* 394 */, "Invalid Process Time: must be integer number of milliseconds between 100 and 2000.  Found:" },
	{ FAIL_OCA_GROUPS_CURRENTLY_NOT_ACCEPTED /* 395 */, "Due to system problems, orders with OCA groups are currently not being accepted." },
	{ FAIL_ONLY_MARKET_AND_LIMIT_ORDERS /* 396 */, "Due to system problems, application is currently accepting only Market and Limit orders for this contract." },
	{ FAIL_ONLY_MARKET_AND_LIMIT_ORDERS_SUPPORT /* 397 */, "Due to system problems, application is currently accepting only Market and Limit orders for this contract." },
	{ FAIL_CONDITION_TRIGGER /* 398 */, "< > cannot be used as a condition trigger." },
	{ FAIL_ORDER_MESSAGE /* 399 */, "Order message error" },
                                                                                   
	{ FAIL_ALGO_ORDER_ERROR /* 400 */, "Algo order error." },
	{ FAIL_LENGTH_RESTRICTION /* 401 */, "Length restriction." },
	{ FAIL_CONDITIONS_NOT_ALLOWED_FOR_CONTRACT /* 402 */, "Conditions are not allowed for this contract." },
	{ FAIL_INVALID_STOP_PRICE /* 403 */, "Invalid stop price." },
	{ FAIL_SHARES_NOT_AVAILABLE /* 404 */, "Shares for this order are not immediately available for short sale. The order will be held while we attempt to locate the shares." },
	{ FAIL_CHILD_ORDER_QUANTITY_INVALID /* 405 */, "The child order quantity should be equivalent to the parent order size." },
	{ FAIL_CURRENCY_NOT_ALLOWED /* 406 */, "The currency < > is not allowed." },
	{ FAIL_INVALID_SYMBOL /* 407 */, "The symbol should contain valid non-unicode characters only." },
	{ FAIL_INVALID_SCALE_ORDER_INCREMENT /* 408 */, "Invalid scale order increment." },
	{ FAIL_INVALID_SCALE_ORDER_COMPONENT_SIZE /* 409 */, "Invalid scale order. You must specify order component size." },
	{ FAIL_INVALID_SUBSEQUENT_COMPONENT_SIZE /* 410 */, "Invalid subsequent component size for scale order." },
	{ FAIL_OUTSIDE_REGULAR_TRADING_HOURS /* 411 */, "The \"Outside Regular Trading Hours\" flag is not valid for this order." },
	{ FAIL_CONTRACT_NOT_AVAILABLE /* 412 */, "The contract is not available for trading." },
	{ FAIL_WHAT_IF_ORDER_TRANSMIT_FLAG /* 413 */, "What-if order should have the transmit flag set to true." },
	{ FAIL_MARKET_DATA_SUBSCRIPTION_NOT_APPLICABLE /* 414 */, "Snapshot market data subscription is not applicable to generic ticks." },
	{ FAIL_WAIT_UNTIL_PREVIOUS_RFQ_FINISHES /* 415 */, "Wait until previous RFQ finishes and try again." },
	{ FAIL_RFQ_NOT_APPLICABLE /* 416 */, "RFQ is not applicable for the contract. Order ID:" },
	{ FAIL_INVALID_INITIAL_COMPONENT_SIZE /* 417 */, "Invalid initial component size for scale order." },
	{ FAIL_INVALID_SCALE_ORDER_PROFIT_OFFSET /* 418 */, "Invalid scale order profit offset." },
	{ FAIL_MISSING_INITIAL_COMPONENT_SIZE /* 419 */, "Missing initial component size for scale order." },
	{ FAIL_INVALID_REAL_TIME_QUERY /* 420 */, "Invalid real-time query." },
	{ FAIL_INVALID_ROUTE /* 421 */, "Invalid route." },
	{ FAIL_ACCOUNT_AND_CLEARING_ATTRIBUTES /* 422 */, "The account and clearing attributes on this order may not be changed." },
	{ FAIL_CROSS_ORDER_RFQ_EXPIRED /* 423 */, "Cross order RFQ has been expired. THI committed size is no longer available. Please open order dialog and verify liquidity allocation." },
	{ FAIL_FA_ORDER_REQUIRES_ALLOCATION /* 424 */, "FA Order requires allocation to be specified." },
	{ FAIL_FA_ORDER_REQUIRES_MANUAL_ALLOCATIONS /* 425 */, "FA Order requires per-account manual allocations because there is no common clearing instruction. Please use order dialog Adviser tab to enter the allocation." },
	{ FAIL_ACCOUNTS_LACK_SHARES /* 426 */, "None of the accounts have enough shares." },
	{ FAIL_MUTUAL_FUND_REQUIRES_MONETARY_VALUE /* 427 */, "Mutual Fund order requires monetary value to be specified." },
	{ FAIL_MUTUAL_FUND_SELL_REQUIRES_SHARES /* 428 */, "Mutual Fund Sell order requires shares to be specified." },
	{ FAIL_DELTA_NEUTRAL_ORDERS_NOT_SUPPORTED /* 429 */, "Delta neutral orders are only supported for combos (BAG security type)." },
	{ FAIL_FUNDAMENTALS_DATA_NOT_AVAILABLE /* 430 */, "We are sorry, but fundamentals data for the security specified is not available." },
	{ FAIL_WHAT_TO_SHOW_FIELD /* 431 */, "What to show field is missing or incorrect." },
	{ FAIL_COMMISSION_NEGATIVE /* 432 */, "Commission must not be negative." },
	{ FAIL_INVALID_RESTORE_SIZE /* 433 */, "Invalid \"Restore size after taking profit\" for multiple account allocation scale order." },
	{ FAIL_ORDER_SIZE_ZERO /* 434 */, "The order size cannot be zero." },
	{ FAIL_MUST_SPECIFY_ACCOUNT /* 435 */, "You must specify an account." },
	{ FAIL_MUST_SPECIFY_ALLOCATION /* 436 */, "You must specify an allocation (either a single account, group, or profile)." },
	{ FAIL_ORDER_TOO_MANY_FLAGS /* 437 */, "Order can have only one flag Outside RTH or Allow PreOpen." },
	{ FAIL_APPLICATION_LOCKED /* 438 */, "The application is now locked." },
	{ FAIL_ALGORITHM_DEFINITION_NOT_FOUND /* 439 */, "Order processing failed. Algorithm definition not found." },
	{ FAIL_ALGORITHM_MODIFIED /* 440 */, "Order modify failed. Algorithm cannot be modified." },
	{ FAIL_ALGO_ATTRIBUTES_VALIDATION_FAILED /* 441 */, "Algo attributes validation failed:" },
	{ FAIL_ALGORITHM_NOT_ALLOWED /* 442 */, "Specified algorithm is not allowed for this order." },
	{ FAIL_UNKNOWN_ALGO_ATTRIBUTE /* 443 */, "Order processing failed. Unknown algo attribute." },
	{ FAIL_VOLATILITY_COMBO_ORDER_SUBMIT_CHANGES /* 444 */, "Volatility Combo order is not yet acknowledged. Cannot submit changes at this time." },
	{ FAIL_RFQ_NO_LONGER_VALID /* 445 */, "The RFQ for this order is no longer valid." },
	{ FAIL_MISSING_SCALE_ORDER_PROFIT_OFFSET /* 446 */, "Missing scale order profit offset." },
	{ FAIL_MISSING_SCALE_PRICE_ADJUSTMENT /* 447 */, "Missing scale price adjustment amount or interval." },
	{ FAIL_INVALID_SCALE_PRICE_ADJUSTMENT_INTERVAL /* 448 */, "Invalid scale price adjustment interval." },
	{ FAIL_UNEXPECTED_SCALE_PRICE_ADJUSTMENT /* 449 */, "Unexpected scale price adjustment amount or interval." },
	{ FAIL_DIVIDEND_SCHEDULE_QUERY /* 450 */, "Dividend schedule query failed." },                                       /* was listed as '40' on the IB/TWS web site */
                                                                                   
	 /* System Message Codes */
	{ FAIL_IB_TWS_CONNECTIVITY_LOST /* 1100 */, "Connectivity between IB and TWS has been lost." },
	{ FAIL_IB_TWS_CONNECTIVITY_RESTORED /* 1101 */, "Connectivity between IB and TWS has been restored - data lost. Market and account data subscription requests must be resubmitted." },
	{ FAIL_IB_TWS_CONNECTIVITY_RESTORED_WITH_DATA /* 1102 */, "Connectivity between IB and TWS has been restored - data maintained." },
	{ FAIL_TWS_SOCKET_PORT_RESET /* 1300 */, "TWS socket port has been reset and this connection is being dropped. Please reconnect on the new port." },
                                                                                   
	 /* Warning Message Codes */
	{ FAIL_NEW_ACCOUNT_DATA_REQUESTED /* 2100 */, "New account data requested from TWS.  API client has been unsubscribed from account data." },
	{ FAIL_UNABLE_TO_SUBSCRIBE_TO_ACCOUNT /* 2101 */, "Unable to subscribe to account as the following clients are subscribed to a different account." },
	{ FAIL_UNABLE_TO_MODIFY_ORDER /* 2102 */, "Unable to modify this order as it is still being processed." },
	{ FAIL_MARKET_DATA_FARM_DISCONNECTED /* 2103 */, "A market data farm is disconnected." },
	{ FAIL_MARKET_DATA_FARM_CONNECTED /* 2104 */, "A market data farm is connected." },
	{ FAIL_HISTORICAL_DATA_FARM_DISCONNECTED /* 2105 */, "A historical data farm is disconnected." },
	{ FAIL_HISTORICAL_DATA_FARM_CONNECTED /* 2106 */, "A historical data farm is connected." },
	{ FAIL_HISTORICAL_DATA_FARM_INACTIVE /* 2107 */, "A historical data farm connection has become inactive but should be available upon demand." },
	{ FAIL_MARKET_DATA_FARM_CONNECTION_INACTIVE /* 2108 */, "A market data farm connection has become inactive but should be available upon demand." },
	{ FAIL_ORDER_EVENT_ATTRIBUTE_IGNORED /* 2109 */, "Order Event Warning: Attribute “Outside Regular Trading Hours” is ignored based on the order type and destination. PlaceOrder is now processed." },
	{ FAIL_IB_TWS_CONNECTIVITY_BROKEN /* 2110 */, "Connectivity between TWS and server is broken. It will be restored automatically." },
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
    "delta_neutral_validation", "tick_snapshot_end", "market_data_type", "commission_report"
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

const char *fa_msg_name[] = { "(unknown)", "GROUPS", "PROFILES", "ALIASES" };
const char *tws_market_data_type_name[] = { "(unknown)", "Real Time", "Frozen" };
static const unsigned int d_nan[2] = {~0U, ~(1U<<31)};
const double *dNAN = (const double *)(const void *) d_nan;
#else
/* do NOT export the global string arrays, use the getter functions instead */
#endif /* TWSAPI_GLOBALS */

const twsclient_errmsg_t *tws_strerror(int errcode);

double get_NAN(void);

const char *fa_msg_type_name(tr_fa_msg_type_t x);
const char *tick_type_name(tr_tick_type_t x);
const char *market_data_type_name(market_data_type_t x);


#ifdef __cplusplus
	}
}
#endif



#endif /* TWS_DEFINITIONS */

#endif /* TWSAPI_H_ */
