//
// TODO: Check permissions on config file before using it.
//
#include <algorithm>
#include <fstream>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include "Config.h"

#include "utils.h"
#include "HashPrefixes.h"

#define SECTION_ACCOUNT_PROBE_MAX		"account_probe_max"
#define SECTION_CLUSTER_NODES			"cluster_nodes"
#define SECTION_DATABASE_PATH			"database_path"
#define SECTION_DEBUG_LOGFILE			"debug_logfile"
#define SECTION_FEE_DEFAULT				"fee_default"
#define SECTION_FEE_NICKNAME_CREATE		"fee_nickname_create"
#define SECTION_FEE_OFFER				"fee_offer"
#define SECTION_FEE_OPERATION			"fee_operation"
#define SECTION_FEE_ACCOUNT_RESERVE		"fee_account_reserve"
#define SECTION_FEE_OWNER_RESERVE		"fee_owner_reserve"
#define SECTION_LEDGER_HISTORY			"ledger_history"
#define SECTION_IPS						"ips"
#define SECTION_NETWORK_QUORUM			"network_quorum"
#define SECTION_NODE_SEED				"node_seed"
#define SECTION_PEER_CONNECT_LOW_WATER	"peer_connect_low_water"
#define SECTION_PEER_IP					"peer_ip"
#define SECTION_PEER_PORT				"peer_port"
#define SECTION_PEER_PRIVATE			"peer_private"
#define SECTION_PEER_SCAN_INTERVAL_MIN	"peer_scan_interval_min"
#define SECTION_PEER_SSL_CIPHER_LIST	"peer_ssl_cipher_list"
#define SECTION_PEER_START_MAX			"peer_start_max"
#define SECTION_RPC_ALLOW_REMOTE		"rpc_allow_remote"
#define SECTION_RPC_IP					"rpc_ip"
#define SECTION_RPC_PORT				"rpc_port"
#define SECTION_RPC_STARTUP				"rpc_startup"
#define SECTION_SNTP					"sntp_servers"
#define SECTION_VALIDATORS_FILE			"validators_file"
#define SECTION_VALIDATION_QUORUM		"validation_quorum"
#define SECTION_VALIDATION_SEED			"validation_seed"
#define SECTION_WEBSOCKET_PUBLIC_IP		"websocket_public_ip"
#define SECTION_WEBSOCKET_PUBLIC_PORT	"websocket_public_port"
#define SECTION_WEBSOCKET_IP			"websocket_ip"
#define SECTION_WEBSOCKET_PORT			"websocket_port"
#define SECTION_WEBSOCKET_SECURE		"websocket_secure"
#define SECTION_WEBSOCKET_SSL_CERT		"websocket_ssl_cert"
#define SECTION_WEBSOCKET_SSL_CHAIN		"websocket_ssl_chain"
#define SECTION_WEBSOCKET_SSL_KEY		"websocket_ssl_key"
#define SECTION_VALIDATORS				"validators"
#define SECTION_VALIDATORS_SITE			"validators_site"

// Fees are in XRP.
#define DEFAULT_FEE_DEFAULT				10
#define DEFAULT_FEE_ACCOUNT_RESERVE		200*SYSTEM_CURRENCY_PARTS
#define DEFAULT_FEE_OWNER_RESERVE		50*SYSTEM_CURRENCY_PARTS
#define DEFAULT_FEE_NICKNAME_CREATE		1000
#define DEFAULT_FEE_OFFER				DEFAULT_FEE_DEFAULT
#define DEFAULT_FEE_OPERATION			1

Config theConfig;
const char* ALPHABET = "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz";

void Config::setup(const std::string& strConf, bool bTestNet, bool bQuiet)
{
	boost::system::error_code	ec;
	std::string					strDbPath, strConfFile;

	//
	// Determine the config and data directories.
	// If the config file is found in the current working directory, use the current working directory as the config directory and
	// that with "db" as the data directory.
	//

	TESTNET	= bTestNet;
	QUIET	= bQuiet;

	// TESTNET forces a "testnet-" prefix on the conf file and db directory.
	strDbPath			= TESTNET ? "testnet-db" : "db";
	strConfFile			= boost::str(boost::format(TESTNET ? "testnet-%s" : "%s")
							% (strConf.empty() ? CONFIG_FILE_NAME : strConf));

	VALIDATORS_BASE		= boost::str(boost::format(TESTNET ? "testnet-%s" : "%s")
							% VALIDATORS_FILE_NAME);
	VALIDATORS_URI		= boost::str(boost::format("/%s") % VALIDATORS_BASE);

	SIGN_TRANSACTION	= TESTNET ? sHP_TestNetTransactionSign	: sHP_TransactionSign;
	SIGN_VALIDATION		= TESTNET ? sHP_TestNetValidation		: sHP_Validation;
	SIGN_PROPOSAL		= TESTNET ? sHP_TestNetProposal			: sHP_Proposal;

	if (TESTNET)
		ALPHABET = "RPShNAF39wBUDnEGHJKLM4pQrsT7VWXYZ2bcdeCg65jkm8ofqi1tuvaxyz";

	if (!strConf.empty())
	{
		// --conf=<path> : everything is relative that file.
		CONFIG_FILE				= strConfFile;
		CONFIG_DIR				= CONFIG_FILE;
			CONFIG_DIR.remove_filename();
		DATA_DIR				= CONFIG_DIR / strDbPath;
	}
	else
	{
		CONFIG_DIR				= boost::filesystem::current_path();
		CONFIG_FILE				= CONFIG_DIR / strConfFile;
		DATA_DIR				= CONFIG_DIR / strDbPath;

		if (exists(CONFIG_FILE)
			// Can we figure out XDG dirs?
			|| (!getenv("HOME") && (!getenv("XDG_CONFIG_HOME") || !getenv("XDG_DATA_HOME"))))
		{
			// Current working directory is fine, put dbs in a subdir.
			nothing();
		}
		else
		{
			// Construct XDG config and data home.
			// http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
			std::string	strHome				= strGetEnv("HOME");
			std::string	strXdgConfigHome	= strGetEnv("XDG_CONFIG_HOME");
			std::string	strXdgDataHome		= strGetEnv("XDG_DATA_HOME");

			if (strXdgConfigHome.empty())
			{
				// $XDG_CONFIG_HOME was not set, use default based on $HOME.
				strXdgConfigHome	= str(boost::format("%s/.config") % strHome);
			}

			if (strXdgDataHome.empty())
			{
				// $XDG_DATA_HOME was not set, use default based on $HOME.
				strXdgDataHome	= str(boost::format("%s/.local/share") % strHome);
			}

			CONFIG_DIR			= str(boost::format("%s/" SYSTEM_NAME) % strXdgConfigHome);
			CONFIG_FILE			= CONFIG_DIR / strConfFile;
			DATA_DIR			= str(boost::format("%s/" SYSTEM_NAME) % strXdgDataHome);

			boost::filesystem::create_directories(CONFIG_DIR, ec);

			if (ec)
				throw std::runtime_error(str(boost::format("Can not create %s") % CONFIG_DIR));
		}
	}

	// Update default values
	load();

	// std::cerr << "CONFIG FILE: " << CONFIG_FILE << std::endl;
	// std::cerr << "CONFIG DIR: " << CONFIG_DIR << std::endl;
	// std::cerr << "DATA DIR: " << DATA_DIR << std::endl;

	boost::filesystem::create_directories(DATA_DIR, ec);

	if (ec)
		throw std::runtime_error(str(boost::format("Can not create %s") % DATA_DIR));
}

Config::Config()
{
	//
	// Defaults
	//

	TESTNET					= false;
	NETWORK_START_TIME		= 1319844908;

	PEER_PORT				= SYSTEM_PEER_PORT;
	RPC_PORT				= 5001;
	WEBSOCKET_PORT			= SYSTEM_WEBSOCKET_PORT;
	WEBSOCKET_PUBLIC_PORT	= SYSTEM_WEBSOCKET_PUBLIC_PORT;
	WEBSOCKET_SECURE		= false;
	NUMBER_CONNECTIONS		= 30;

	// a new ledger every minute
	LEDGER_SECONDS			= 60;
	LEDGER_CREATOR			= false;

	RPC_USER				= "admin";
	RPC_PASSWORD			= "pass";
	RPC_ALLOW_REMOTE		= false;

	PEER_SSL_CIPHER_LIST	= DEFAULT_PEER_SSL_CIPHER_LIST;
	PEER_SCAN_INTERVAL_MIN	= DEFAULT_PEER_SCAN_INTERVAL_MIN;

	PEER_START_MAX			= DEFAULT_PEER_START_MAX;
	PEER_CONNECT_LOW_WATER	= DEFAULT_PEER_CONNECT_LOW_WATER;

	PEER_PRIVATE			= false;

	TRANSACTION_FEE_BASE	= DEFAULT_FEE_DEFAULT;

	NETWORK_QUORUM			= 0;	// Don't need to see other nodes
	VALIDATION_QUORUM		= 1;	// Only need one node to vouch

	FEE_ACCOUNT_RESERVE		= DEFAULT_FEE_ACCOUNT_RESERVE;
	FEE_OWNER_RESERVE		= DEFAULT_FEE_OWNER_RESERVE;
	FEE_NICKNAME_CREATE		= DEFAULT_FEE_NICKNAME_CREATE;
	FEE_OFFER				= DEFAULT_FEE_OFFER;
	FEE_DEFAULT				= DEFAULT_FEE_DEFAULT;
	FEE_CONTRACT_OPERATION  = DEFAULT_FEE_OPERATION;

	LEDGER_HISTORY			= 256;

	ACCOUNT_PROBE_MAX		= 10;

	VALIDATORS_SITE			= DEFAULT_VALIDATORS_SITE;

	RUN_STANDALONE			= false;
	START_UP				= NORMAL;
}

void Config::load()
{
	if (!QUIET)
		std::cerr << "Loading: " << CONFIG_FILE << std::endl;

	std::ifstream	ifsConfig(CONFIG_FILE.c_str(), std::ios::in);

	if (!ifsConfig)
	{
		std::cerr << "Failed to open '" << CONFIG_FILE << "'." << std::endl;
	}
	else
	{
		std::string	strConfigFile;

		strConfigFile.assign((std::istreambuf_iterator<char>(ifsConfig)),
			std::istreambuf_iterator<char>());

		if (ifsConfig.bad())
		{
			std::cerr << "Failed to read '" << CONFIG_FILE << "'." << std::endl;
		}
		else
		{
			section		secConfig	= ParseSection(strConfigFile, true);
			std::string	strTemp;

			// XXX Leak
			section::mapped_type*	smtTmp;

			smtTmp	= sectionEntries(secConfig, SECTION_VALIDATORS);
			if (smtTmp)
			{
				VALIDATORS	= *smtTmp;
				// sectionEntriesPrint(&VALIDATORS, SECTION_VALIDATORS);
			}

			smtTmp = sectionEntries(secConfig, SECTION_CLUSTER_NODES);
			if (smtTmp)
			{
				CLUSTER_NODES = *smtTmp;
				// sectionEntriesPrint(&CLUSTER_NODES, SECTION_CLUSTER_NODES);
			}

			smtTmp	= sectionEntries(secConfig, SECTION_IPS);
			if (smtTmp)
			{
				IPS	= *smtTmp;
				// sectionEntriesPrint(&IPS, SECTION_IPS);
			}

			smtTmp = sectionEntries(secConfig, SECTION_SNTP);
			if (smtTmp)
			{
				SNTP_SERVERS = *smtTmp;
			}

			smtTmp	= sectionEntries(secConfig, SECTION_RPC_STARTUP);
			if (smtTmp)
			{
				BOOST_FOREACH(const std::string& strJson, *smtTmp)
				{
					Json::Reader	jrReader;
					Json::Value		jvCommand;

					if (!jrReader.parse(strJson, jvCommand))
						throw std::runtime_error(boost::str(boost::format("Couldn't parse ["SECTION_RPC_STARTUP"] command: %s") % strJson));

					RPC_STARTUP.push_back(jvCommand);
				}
			}

			if (sectionSingleB(secConfig, SECTION_DATABASE_PATH, DATABASE_PATH))
				DATA_DIR	= DATABASE_PATH;

			(void) sectionSingleB(secConfig, SECTION_VALIDATORS_SITE, VALIDATORS_SITE);

			(void) sectionSingleB(secConfig, SECTION_PEER_IP, PEER_IP);

			if (sectionSingleB(secConfig, SECTION_PEER_PORT, strTemp))
				PEER_PORT			= boost::lexical_cast<int>(strTemp);

			if (sectionSingleB(secConfig, SECTION_PEER_PRIVATE, strTemp))
				PEER_PRIVATE		= boost::lexical_cast<bool>(strTemp);

			(void) sectionSingleB(secConfig, SECTION_RPC_IP, RPC_IP);

			if (sectionSingleB(secConfig, SECTION_RPC_PORT, strTemp))
				RPC_PORT = boost::lexical_cast<int>(strTemp);

			if (sectionSingleB(secConfig, "ledger_creator" , strTemp))
				LEDGER_CREATOR = boost::lexical_cast<bool>(strTemp);

			if (sectionSingleB(secConfig, SECTION_RPC_ALLOW_REMOTE, strTemp))
				RPC_ALLOW_REMOTE	= boost::lexical_cast<bool>(strTemp);

			(void) sectionSingleB(secConfig, SECTION_WEBSOCKET_IP, WEBSOCKET_IP);

			if (sectionSingleB(secConfig, SECTION_WEBSOCKET_PORT, strTemp))
				WEBSOCKET_PORT		= boost::lexical_cast<int>(strTemp);

			(void) sectionSingleB(secConfig, SECTION_WEBSOCKET_PUBLIC_IP, WEBSOCKET_PUBLIC_IP);

			if (sectionSingleB(secConfig, SECTION_WEBSOCKET_PUBLIC_PORT, strTemp))
				WEBSOCKET_PUBLIC_PORT	= boost::lexical_cast<int>(strTemp);

			if (sectionSingleB(secConfig, SECTION_WEBSOCKET_SECURE, strTemp))
				WEBSOCKET_SECURE	= boost::lexical_cast<bool>(strTemp);

			sectionSingleB(secConfig, SECTION_WEBSOCKET_SSL_CERT, WEBSOCKET_SSL_CERT);
			sectionSingleB(secConfig, SECTION_WEBSOCKET_SSL_CHAIN, WEBSOCKET_SSL_CHAIN);
			sectionSingleB(secConfig, SECTION_WEBSOCKET_SSL_KEY, WEBSOCKET_SSL_KEY);

			if (sectionSingleB(secConfig, SECTION_VALIDATION_SEED, strTemp))
			{
				VALIDATION_SEED.setSeedGeneric(strTemp);
				if (VALIDATION_SEED.isValid())
				{
					VALIDATION_PUB = RippleAddress::createNodePublic(VALIDATION_SEED);
					VALIDATION_PRIV = RippleAddress::createNodePrivate(VALIDATION_SEED);
				}
			}
			if (sectionSingleB(secConfig, SECTION_NODE_SEED, strTemp))
			{
				NODE_SEED.setSeedGeneric(strTemp);
				if (NODE_SEED.isValid())
				{
					NODE_PUB = RippleAddress::createNodePublic(NODE_SEED);
					NODE_PRIV = RippleAddress::createNodePrivate(NODE_SEED);
				}
			}

			(void) sectionSingleB(secConfig, SECTION_PEER_SSL_CIPHER_LIST, PEER_SSL_CIPHER_LIST);

			if (sectionSingleB(secConfig, SECTION_PEER_SCAN_INTERVAL_MIN, strTemp))
				// Minimum for min is 60 seconds.
				PEER_SCAN_INTERVAL_MIN = std::max(60, boost::lexical_cast<int>(strTemp));

			if (sectionSingleB(secConfig, SECTION_PEER_START_MAX, strTemp))
				PEER_START_MAX		= std::max(1, boost::lexical_cast<int>(strTemp));

			if (sectionSingleB(secConfig, SECTION_PEER_CONNECT_LOW_WATER, strTemp))
				PEER_CONNECT_LOW_WATER = std::max(1, boost::lexical_cast<int>(strTemp));

			if (sectionSingleB(secConfig, SECTION_NETWORK_QUORUM, strTemp))
				NETWORK_QUORUM		= std::max(0, boost::lexical_cast<int>(strTemp));

			if (sectionSingleB(secConfig, SECTION_VALIDATION_QUORUM, strTemp))
				VALIDATION_QUORUM	= std::max(0, boost::lexical_cast<int>(strTemp));

			if (sectionSingleB(secConfig, SECTION_FEE_ACCOUNT_RESERVE, strTemp))
				FEE_ACCOUNT_RESERVE	= boost::lexical_cast<uint64>(strTemp);

			if (sectionSingleB(secConfig, SECTION_FEE_OWNER_RESERVE, strTemp))
				FEE_OWNER_RESERVE	= boost::lexical_cast<uint64>(strTemp);

			if (sectionSingleB(secConfig, SECTION_FEE_NICKNAME_CREATE, strTemp))
				FEE_NICKNAME_CREATE	= boost::lexical_cast<int>(strTemp);

			if (sectionSingleB(secConfig, SECTION_FEE_OFFER, strTemp))
				FEE_OFFER			= boost::lexical_cast<int>(strTemp);

			if (sectionSingleB(secConfig, SECTION_FEE_DEFAULT, strTemp))
				FEE_DEFAULT			= boost::lexical_cast<int>(strTemp);

			if (sectionSingleB(secConfig, SECTION_FEE_OPERATION, strTemp))
				FEE_CONTRACT_OPERATION	= boost::lexical_cast<int>(strTemp);

			if (sectionSingleB(secConfig, SECTION_LEDGER_HISTORY, strTemp))
			{
				boost::to_lower(strTemp);
				if (strTemp == "none")
					LEDGER_HISTORY = 0;
				else if (strTemp == "full")
					LEDGER_HISTORY = 1000000000u;
				else
					LEDGER_HISTORY = boost::lexical_cast<uint32>(strTemp);
			}

			if (sectionSingleB(secConfig, SECTION_ACCOUNT_PROBE_MAX, strTemp))
				ACCOUNT_PROBE_MAX	= boost::lexical_cast<int>(strTemp);

			if (sectionSingleB(secConfig, SECTION_VALIDATORS_FILE, strTemp))
				VALIDATORS_FILE		= strTemp;

			if (sectionSingleB(secConfig, SECTION_DEBUG_LOGFILE, strTemp))
				DEBUG_LOGFILE		= strTemp;
		}
	}
}

// vim:ts=4
