/*
Risk premia automated trading

##### Instructions ######

Key parameters and settings are controlled using the #define statements below. 

1. Choose to trade US-listed assets or UK-listed assets by setting #define ASSET list to one of these options:
US-listed assets: #define ASSET_LIST "AssetsRP_US"
UK-listed assets: #define ASSET_LIST "AssetsRP_EU"

If you are using US-listed assets, we get historical prices from Alpha Vantage. You'll need to get an Alpha Vantage API key and enter it in ZorroFix.ini.

If you're using UK-listed assets, we get historical prices from Stooq. Anecdotally, the data quality is better for these assets from Stooq. No API key is required. Note that the UK-listed ETFs have a shorter history than the US versions. 

2. Choose a rebalancing approach. Your options are:

a) Manually rebalance (ie submit orders yourself) at user-defined intervals based on today's target position. Just run the script in test mode and observe the output in the message window that looks like this:

######
DESIRED POSITIONS
Data up to and including 30- 7-2020
GLD: 150 shares @ 183.76
TLT: 188 shares @ 171.11
VTI: 88 shares @ 164.68
######

Compare those desired positions to your current positions, and do any necessary trades. If the difference is small, you may decide to skip the rebalance and save on costs. 

b) Have Zorro automatically rebalance based on a user-defined tracking error. Uncomment the TRACKING_ERROR_THRESHOLD definition statement below and set an appropriate threshold. 10% (ie 0.1) is reasonable. Lower thresholds will rebalance more frequently, minimising tracking error at the expense of increased costs. If you have a small account, it is not recommended to rebalance frequently as IB's minimum commission structure will hurt you. 

c) Have Zorro automatically rebalance on the first trading day of the month. Comment the TRACKING_ERROR_THRESHOLD definition statement below. 

3. You may wish to change the default parameters controlling the volatility targeting algorithm - the volatility lookback and volatiltiy target. The defaults are the values that we like. These are controlled by the VOL_LOOKBACK and VOL_TARGET definition statements. 

4. We have set the maximum leverage to 1. If you have a portfolio margin account, you may wish to change this setting. It's controlled by the MAX_LEVERAGE definition statement. 

5. For simulation purposes, we have implemented IB's fixed tier commission model. If yours is different to this, you may wish to change the values controlled by the MIN_COMMISSION and PER_SHARE_COMMISSION definition statements. 

6. Set up the strategy's capital allocation via the slider on the Zorro interface. Note that the slider value is multiplied by 1,000. So for example a slider value of 10 corresponds to $10,000 account equity. 

7. In trade mode, uncomment the LOGGING definition statement and set an approriate log file. Note that data is logged out daily. This will show the delta between current and target positions on a daily basis, as well as days to rebalance under the monthly rebalance approach and the current tracking error.


CHANGELOG
~~~~~~~~~~
2020-08-03: bug fix in logging - tracking_error_threshold not passed to writeLog function correctly

*/
//#include "Strategy\extensions.c" 
string TradeLogFilePath = "Log\\risk-premia-20200821.csv";

// Choose US or EU version: "AssetsRP_US" or "AssetsRP_EU"
#define ASSET_LIST "AssetsRP_US"
#define NUM_ASSETS 3

// Rebalancing approach
// UNCOMMENT to rebalance basis tracking error
// COMMENT to rebalance on the first trading day of the month
#define TRACKING_ERROR_THRESHOLD 0.1

// Key parameters
#define VOL_LOOKBACK 90
#define VOL_TARGET 0.1
#define MAX_LEVERAGE 1.

// Default commission model is IB's fixed tier
#define MIN_COMMISSION 1.
#define PER_SHARE_COMMISSION 0.005

// Control plot output
#define COLOUR_1 CYAN 
#define COLOUR_2 BLUE 
#define COLOUR_3 RED

// uncomment to run with logging - recommended in trademode. If WebFolder is set up, use it: "C:\\inetpub\\wwwroot\\riskpremia_log.txt". Otherwise "Log\\riskpremia.log", or whatever you like. 
//#define LOGGING  "C:\\inetpub\\wwwroot\\riskpremia_log.txt"
#define LOGGING "Log\\riskpremia.log"

static int USHolidays[12] = {0101,1225,20200120,20200217,20200410,20200525,20200703,20200907,20201012,20201111,20201126,0};

void writeLog(string fileName, int current_position, int desired_position, var current_vol, var target_vol, var tracking_error, var tracking_error_threshold)
{
	
	char line[200];
	static bool firsttime = true;
	if(is(INITRUN) and firsttime and !file_length(fileName))
	{
		sprintf(line, "Datetime, Ticker, Open, High, Low, Close, CurrentPosition, Exposure, DesiredPosition, PositionDelta, CurrentVolatility, TargetVolatility, TradingDaysUntilRebal, TrackingError, TrackingErrorThreshold");
		firsttime = false;
		
		file_append(fileName, line);
	}
	else if(!is(LOOKBACK))
	{	 
		sprintf(line,
				"\n%04i/%02i/%02i, %s, %.5f, %.5f, %.5f, %.5f, %i, %.2f, %i, %i, %.3f, %.2f, %i, %.2f, %.2f",  
				year(), 
				month(), 
				day(), 
				Asset,
				priceOpen(), 
				priceHigh(), 
				priceLow(), 
				priceClose(),
				current_position,
				current_position*priceClose(),
				desired_position,
				desired_position - current_position,
				current_vol,
				VOL_TARGET,
				ifelse(tdm() == 1, 0, tom() - tdm() + 1),
				tracking_error,
				tracking_error_threshold
		);
		
		file_append(fileName, line);
	}
}

void rebalance(var current_position, var target_position)
{
	if(is(TRADEMODE) and !is(LOOKBACK))
	{
		brokerCommand(SET_ORDERTYPE, 2);
		brokerCommand(SET_ORDERTEXT, "MOC/");
	}
	
	var position_diff = abs(current_position - target_position);
	if(position_diff > 0)
		Commission = max(PER_SHARE_COMMISSION, MIN_COMMISSION/position_diff);
	if(target_position > current_position)
	{
		ThisTrade = enterLong(position_diff);
		if (ThisTrade != 0)
		{
			ThisTrade->nLots = position_diff; // Needed for resuming trades correctly after script restart
		}
	}
	else if(target_position < current_position)
		exitLong(Asset, 0, position_diff);
}

function run()
{
	set(PLOTNOW+PRELOAD);
	setf(PlotMode, PL_ALL+PL_FINE);
	LookBack = VOL_LOOKBACK;
	BarMode = BR_WEEKEND+BR_FLAT+BR_MARKET+BR_SLEEP+BR_LOGOFF;
	BarZone = ET;
	Holidays = USHolidays;
	StartMarket = 0930;
	EndMarket = 1610;
	MonteCarlo = 0;
	StopFactor = 0;
	BarPeriod = 1440;
	StartDate = 2000;
	
	if(is(INITRUN))
	{
		// choose the US or EU version
		assetList(ASSET_LIST); 
		
		// report selected rebalancing approach 
		#ifdef TRACKING_ERROR_THRESHOLD
			printf("\n#####\nREBALANCING ON %.1f %% TRACKING ERROR\n#####", 100*TRACKING_ERROR_THRESHOLD);
		#else
			printf("\n#####\nREBALANCING MONTHLY\n#####");
		#endif
		
		// load data: 
			// for US ETFs, from Alpha Vantage - set up your AV API key in Zorro.ini or ZorroFix.ini
			// for EU ETFs, from Stooq. Data quality is better.
		string Name;
		int n = 0;
		while(Name = loop(Assets))
		{
			if(!strcmp(ASSET_LIST, "AssetsRP_EU"))
			{
				assetHistory(Name, FROM_STOOQ);
			}
			else if(!strcmp(ASSET_LIST, "AssetsRP_US")) 
			{
				assetHistory(Name, FROM_AV);
			}
			n++;
		}
		if (is(TRADEMODE)) {
			//dataParse(1,TradeHistoryFileFormat,TradeLogFilePath);
			//dataSort(1);
			//readTradeHistory();
		}
	}
	
	// Force IB plugin to use last traded price 
	var price_type = brokerCommand(GET_PRICETYPE, 2);
	if (is(TRADEMODE))
	{
		if (price_type != 2)
			brokerCommand(SET_PRICETYPE, 2);
	}
	
	// calculate theoretical target sizes from volatility
	var theo_size[NUM_ASSETS];
	var ann_vol[NUM_ASSETS];
	int i;
	for(i=0; i<NUM_ASSETS; i++)
	{
		// prices and returns for asset i 
		asset(Assets[i]);
		vars closes = series(priceClose());
		vars returns = series(ROCP(closes, 1)); 
				
		// calculate sizes from vol targets
		ann_vol[i] = sqrt(Moment(returns, VOL_LOOKBACK, 2)*252);
		if (ann_vol[i] == 0.0) { 
			theo_size[i] = 0;
		}
		else 
		{ 
			theo_size[i] = VOL_TARGET/ann_vol[i];
		}
	}
	
	// constrain size according to max leverage
	var total_size = Sum(theo_size, NUM_ASSETS);
	var adj_factor = 1.;
	if(total_size > MAX_LEVERAGE)
	{
		adj_factor = MAX_LEVERAGE/total_size;	
	}
	
	var constrained_size[NUM_ASSETS];
	for(i=0; i<NUM_ASSETS; i++)
	{
		constrained_size[i] = theo_size[i]*adj_factor;
	}
	
	// calculate today's target positions from constrained sizes and capital
	// in TESTMODE, print the most recent desired positions
	// in TRADEMODE and when rebalancing on tracking error, do any necessary rebalancing
	var total_exposure = Command[0];//slider(1, 9, 0, 100, "USDx1k", "Total USD exposure");
	if(is(INITRUN))
	{
		printf("\n### USD exposure: %.0f", total_exposure);
	}

	var target_positions[NUM_ASSETS];
	var current_positions[NUM_ASSETS];
	if(is(EXITRUN) and is(TESTMODE))
			printf("\n######\nDESIRED POSITIONS\nData up to and including %2d-%2d-%4d", day(), month(), year());
	for(i=0; i<NUM_ASSETS; i++)
	{
		asset(Assets[i]);
		target_positions[i] = floor(total_exposure * constrained_size[i] / priceClose());
		current_positions[i] = 0;
		for(current_trades)
		{
			if(TradeIsOpen)
				current_positions[i] += TradeLots;
		}
		// current_positions[i] = LotsPool;
		
		int position_diff = abs(target_positions[i] - current_positions[i]);
		var tracking_error = position_diff/target_positions[i];
		
	#ifdef LOGGING
		#ifdef TRACKING_ERROR_THRESHOLD
			writeLog(LOGGING, current_positions[i], target_positions[i], ann_vol[i], VOL_TARGET, tracking_error, TRACKING_ERROR_THRESHOLD);
		#endif
		#ifndef TRACKING_ERROR_THRESHOLD
			writeLog(LOGGING, current_positions[i], target_positions[i], ann_vol[i], VOL_TARGET, tracking_error, 0);
		#endif
	#endif
	
		// print out most recent desired positions in testmode
		if(is(EXITRUN) and is(TESTMODE))
			printf("\n%s: %.0f shares @ %.2f", Asset, target_positions[i], priceClose());
		
		// print out current and desired positions every day in trademode
		if(is(TRADEMODE) and !is(LOOKBACK))
		{
			printf("\n###%s###\nCurrent Position: %.0f shares\nDesired Position: %.0f shares @ %.2f", Asset, current_positions[i], target_positions[i], priceClose());
		}
			
		// rebalance when tracking error exceeds threhsold
		#ifdef TRACKING_ERROR_THRESHOLD	
			if(ann_vol[i] > 0 and (tracking_error > TRACKING_ERROR_THRESHOLD or (NumOpenLong + NumPendingLong == 0)))
			{
				rebalance(current_positions[i], target_positions[i]);
			}
		#endif	
	}		
	if(is(EXITRUN) and is(TESTMODE))
			printf("\n######");

	// rebalance monthly
	#ifndef TRACKING_ERROR_THRESHOLD
		if(tdm() == 1 or (NumOpenLong + NumPendingLong == 0))
		{
			for(i=0; i<NUM_ASSETS; i++)
			{
				asset(Assets[i]);
				rebalance(current_positions[i], target_positions[i]);
			}
		}
	#endif
	
	// PLOTTING
	
	// asset-wise theoretical size plot
	for(i=0; i<NUM_ASSETS; i++)
	{
		asset(Assets[i]);
		
		char plot_title1[20];
		strcpy(plot_title1, Asset);
		strcat(plot_title1, " Theo.Sze");
		if(i == 0)
		{
			plot(plot_title1, theo_size[i], NEW, color(100*i/(NUM_ASSETS-1), COLOUR_1, COLOUR_2, COLOUR_3));
		}
		else
		{
			plot(plot_title1, theo_size[i], 0, color(100*i/(NUM_ASSETS-1), COLOUR_1, COLOUR_2, COLOUR_3));
		}
	}
	
	// portfolio size plot
	plot("Total Theo.Sze", total_size, NEW, BLACK);
	
	// asset-wise constrained size plot
	for(i=0; i<NUM_ASSETS; i++)
	{
		asset(Assets[i]);
		
		char plot_title2[20];
		strcpy(plot_title2, Asset);
		strcat(plot_title2, " Constr.Sz");
		
		if(i ==0)
		{
			plot(plot_title2, constrained_size[i], NEW, color(100*i/(NUM_ASSETS-1), CYAN, BLUE, RED));
		}
		else
		{
			plot(plot_title2, constrained_size[i], 0, color(100*i/(NUM_ASSETS-1), CYAN, BLUE, RED));
		}
	}
	
	// portfolio constrained size plot
	plot("Total Constr.Sze", Sum(constrained_size, NUM_ASSETS), NEW, BLACK);
}
