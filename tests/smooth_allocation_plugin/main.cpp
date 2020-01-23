
#include <graphene/app/application.hpp>
#include <graphene/app/plugin.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/utilities/tempdir.hpp>

#include <graphene/witness/witness.hpp>
#include <graphene/egenesis/egenesis.hpp>
#include <graphene/wallet/wallet.hpp>
#include <graphene/smooth_allocation/smooth_allocation_plugin.hpp>

#include <fc/thread/thread.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/crypto/base58.hpp>

#include <fc/crypto/aes.hpp>

#ifdef _WIN32
   #ifndef _WIN32_WINNT
      #define _WIN32_WINNT 0x0501
   #endif
   #include <winsock2.h>
   #include <WS2tcpip.h>
#else
   #include <sys/socket.h>
   #include <netinet/ip.h>
   #include <sys/types.h>
#endif
#include <thread>

#include <boost/filesystem/path.hpp>

#define BOOST_TEST_MODULE Test Application
#include <boost/test/included/unit_test.hpp>

/*****
 * Global Initialization for Windows
 * ( sets up Winsock stuf )
 */
#ifdef _WIN32
int sockInit(void)
{
   WSADATA wsa_data;
   return WSAStartup(MAKEWORD(1,1), &wsa_data);
}
int sockQuit(void)
{
   return WSACleanup();
}
#endif

/*********************
 * Helper Methods
 *********************/

#include "../common/genesis_file_util.hpp"

using std::exception;
using std::cerr;

#define INVOKE(test) ((struct test*)this)->test_method();

//////
/// @brief attempt to find an available port on localhost
/// @returns an available port number, or -1 on error
/////
int get_available_port()
{
   struct sockaddr_in sin;
   int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
   if (socket_fd == -1)
      return -1;
   sin.sin_family = AF_INET;
   sin.sin_port = 0;
   sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (::bind(socket_fd, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) == -1)
      return -1;
   socklen_t len = sizeof(sin);
   if (getsockname(socket_fd, (struct sockaddr *)&sin, &len) == -1)
      return -1;
#ifdef _WIN32
   closesocket(socket_fd);
#else
   close(socket_fd);
#endif
   return ntohs(sin.sin_port);
}

///////////
/// @brief Start the application
/// @param app_dir the temporary directory to use
/// @param server_port_number to be filled with the rpc endpoint port number
/// @returns the application object
//////////
std::shared_ptr<graphene::app::application> start_application(fc::temp_directory& app_dir, int& server_port_number, bool& allocation_plugin_mode) {
   std::shared_ptr<graphene::app::application> app1(new graphene::app::application{});
   app1->register_plugin< graphene::witness_plugin::witness_plugin >(true);
   app1->register_plugin< graphene::smooth_allocation::smooth_allocation_plugin>(true);
   app1->startup_plugins();
   boost::program_options::variables_map cfg;
#ifdef _WIN32
   sockInit();
#endif
   server_port_number = get_available_port();
   cfg.emplace(
      "rpc-endpoint",
      boost::program_options::variable_value(string("127.0.0.1:" + std::to_string(server_port_number)), false)
   );
   cfg.emplace("genesis-json", boost::program_options::variable_value(create_genesis_file(app_dir), false));
   cfg.emplace("seed-nodes", boost::program_options::variable_value(string("[]"), false));
   cfg.emplace("meta1-private-key",  boost::program_options::variable_value(string("5HuCDiMeESd86xrRvTbexLjkVg2BEoKrb7BAA5RLgXizkgV3shs"), false));

   /// @param allocation_plugin_mode : true = test_mode , false = production_mode
   cfg.emplace("smooth-allocation-plugin-testing-mode",  boost::program_options::variable_value(allocation_plugin_mode, false));
   app1->initialize(app_dir.path(), cfg);

   app1->initialize_plugins(cfg);
   app1->startup_plugins();

   app1->startup();
   fc::usleep(fc::milliseconds(500));
   return app1;
}

///////////
/// @brief a class to make connecting to the application server easier
///////////
class client_connection
{
public:
   /////////
   // constructor
   /////////
   client_connection(
      std::shared_ptr<graphene::app::application> app,
      const fc::temp_directory& data_dir,
      const int server_port_number
   )
   {
      wallet_data.chain_id = app->chain_database()->get_chain_id();
      wallet_data.ws_server = "ws://127.0.0.1:" + std::to_string(server_port_number);
      wallet_data.ws_user = "";
      wallet_data.ws_password = "";
      websocket_connection  = websocket_client.connect( wallet_data.ws_server );

      api_connection = std::make_shared<fc::rpc::websocket_api_connection>( websocket_connection,
                                                                            GRAPHENE_MAX_NESTED_OBJECTS );

      remote_login_api = api_connection->get_remote_api< graphene::app::login_api >(1);
      BOOST_CHECK(remote_login_api->login( wallet_data.ws_user, wallet_data.ws_password ) );

      wallet_api_ptr = std::make_shared<graphene::wallet::wallet_api>(wallet_data, remote_login_api);
      wallet_filename = data_dir.path().generic_string() + "/wallet.json";
      wallet_api_ptr->set_wallet_filename(wallet_filename);

      wallet_api = fc::api<graphene::wallet::wallet_api>(wallet_api_ptr);

      wallet_cli = std::make_shared<fc::rpc::cli>(GRAPHENE_MAX_NESTED_OBJECTS);
      for( auto& name_formatter : wallet_api_ptr->get_result_formatters() )
         wallet_cli->format_result( name_formatter.first, name_formatter.second );

      boost::signals2::scoped_connection closed_connection(websocket_connection->closed.connect([=]{
         cerr << "Server has disconnected us.\n";
         wallet_cli->stop();
      }));
      (void)(closed_connection);
   }
   ~client_connection()
   {
      // wait for everything to finish up
      fc::usleep(fc::milliseconds(500));
   }
public:
   fc::http::websocket_client websocket_client;
   graphene::wallet::wallet_data wallet_data;
   fc::http::websocket_connection_ptr websocket_connection;
   std::shared_ptr<fc::rpc::websocket_api_connection> api_connection;
   fc::api<login_api> remote_login_api;
   std::shared_ptr<graphene::wallet::wallet_api> wallet_api_ptr;
   fc::api<graphene::wallet::wallet_api> wallet_api;
   std::shared_ptr<fc::rpc::cli> wallet_cli;
   std::string wallet_filename;
};

///////////////////////////////
// Cli Wallet Fixture
///////////////////////////////

/// @brief used to launch allocation plugin in test mode
/// @param production_mode defines timeouts between allocation
//In production mode, allocation speed  = 1 minute; 
//In test mode, allocation speed        = 100 milliseconds;
struct test_cli_fixture
{
   class dummy
   {
   public:
      ~dummy()
      {
         // wait for everything to finish up
         fc::usleep(fc::milliseconds(500));
      }
   };
   dummy dmy;
   int server_port_number;
   fc::temp_directory app_dir;
   std::shared_ptr<graphene::app::application> app1;
   client_connection con;
   std::vector<std::string> nathan_keys;
   bool flag;
   /// @param allocation_plugin_mode : true = test_mode , false = production_mode
   test_cli_fixture(bool allocation_plugin_mode = true) :
      server_port_number(0),
      app_dir( graphene::utilities::temp_directory_path() ),
      app1( start_application(app_dir, server_port_number,allocation_plugin_mode) ),
      con( app1, app_dir, server_port_number ),
      nathan_keys( {"5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"} )
   {
      BOOST_TEST_MESSAGE("Setup cli_wallet::boost_fixture_test_case");
      using namespace graphene::chain;
      using namespace graphene::app;

      try
      {
         BOOST_TEST_MESSAGE("Setting wallet password");
         con.wallet_api_ptr->set_password("supersecret");
         con.wallet_api_ptr->unlock("supersecret");

         // import Nathan account
         BOOST_TEST_MESSAGE("Importing nathan key");
         BOOST_CHECK_EQUAL(nathan_keys[0], "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3");
         BOOST_CHECK(con.wallet_api_ptr->import_key("nathan", nathan_keys[0]));
      } catch( fc::exception& e ) {
         edump((e.to_detail_string()));
         throw;
      }
   }

   ~test_cli_fixture()
   {
      BOOST_TEST_MESSAGE("Cleanup cli_wallet::boost_fixture_test_case");

      // wait for everything to finish up
      fc::usleep(fc::seconds(1));

      app1->shutdown();
#ifdef _WIN32
      sockQuit();
#endif
   }
};

/// @brief used to launch allocation plugin in production mode
/// @param production_mode defines timeouts between allocation
//In production mode, allocation speed  = 1 minute; 
//In test mode, allocation speed        = 100 milliseconds;
struct production_cli_fixture
{
   class dummy
   {
   public:
      ~dummy()
      {
         // wait for everything to finish up
         fc::usleep(fc::milliseconds(500));
      }
   };
   dummy dmy;
   int server_port_number;
   fc::temp_directory app_dir;
   std::shared_ptr<graphene::app::application> app1;
   client_connection con;
   std::vector<std::string> nathan_keys;
   bool flag;
   //allocation_plugin mode: true = test_mode , false = production_mode
   production_cli_fixture(bool allocation_plugin_mode = false) :
      server_port_number(0),
      app_dir( graphene::utilities::temp_directory_path() ),
      app1( start_application(app_dir, server_port_number,allocation_plugin_mode) ),
      con( app1, app_dir, server_port_number ),
      nathan_keys( {"5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"} )
   {
      BOOST_TEST_MESSAGE("Setup cli_wallet::boost_fixture_test_case");
      using namespace graphene::chain;
      using namespace graphene::app;

      try
      {
         BOOST_TEST_MESSAGE("Setting wallet password");
         con.wallet_api_ptr->set_password("supersecret");
         con.wallet_api_ptr->unlock("supersecret");

         // import Nathan account
         BOOST_TEST_MESSAGE("Importing nathan key");
         BOOST_CHECK_EQUAL(nathan_keys[0], "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3");
         BOOST_CHECK(con.wallet_api_ptr->import_key("nathan", nathan_keys[0]));
      } catch( fc::exception& e ) {
         edump((e.to_detail_string()));
         throw;
      }
   }

   ~production_cli_fixture()
   {
      BOOST_TEST_MESSAGE("Cleanup cli_wallet::boost_fixture_test_case");

      // wait for everything to finish up
      fc::usleep(fc::seconds(1));

      app1->shutdown();
#ifdef _WIN32
      sockQuit();
#endif
   }
};
///////////////////////////////
// Tests
///////////////////////////////

BOOST_FIXTURE_TEST_CASE( upgrade_nathan_account, test_cli_fixture )
{
   try
   {
      BOOST_TEST_MESSAGE("Upgrade Nathan's account");

      account_object nathan_acct_before_upgrade, nathan_acct_after_upgrade;
      std::vector<signed_transaction> import_txs;
      signed_transaction upgrade_tx;

      BOOST_TEST_MESSAGE("Importing nathan's balance");
      import_txs = con.wallet_api_ptr->import_balance("nathan", nathan_keys, true);
      nathan_acct_before_upgrade = con.wallet_api_ptr->get_account("nathan");

      // upgrade nathan
      BOOST_TEST_MESSAGE("Upgrading Nathan to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("nathan", true);
      nathan_acct_after_upgrade = con.wallet_api_ptr->get_account("nathan");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
         std::not_equal_to<uint32_t>(),
         (nathan_acct_before_upgrade.membership_expiration_date.sec_since_epoch())
         (nathan_acct_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(nathan_acct_after_upgrade.is_lifetime_member());
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_FIXTURE_TEST_CASE(create_asset_limitation, test_cli_fixture)
{
   try
   {
      INVOKE(upgrade_nathan_account);

      // create a new account for meta1
      {
         graphene::wallet::brain_key_info bki = con.wallet_api_ptr->suggest_brain_key();
         BOOST_CHECK(!bki.brain_priv_key.empty());
         signed_transaction create_acct_tx = con.wallet_api_ptr->create_account_with_brain_key(bki.brain_priv_key, "meta1",
                                                                                               "nathan", "nathan", true);
         // save the private key for this new account in the wallet file
         BOOST_CHECK(con.wallet_api_ptr->import_key("meta1", bki.wif_priv_key));
         con.wallet_api_ptr->save_wallet_file(con.wallet_filename);
         // attempt to give meta1 some bitsahres
         BOOST_TEST_MESSAGE("Transferring meta1 from Nathan to meta1");
         signed_transaction transfer_tx = con.wallet_api_ptr->transfer("nathan", "meta1", "100000", "1.3.0",
                                                                       "Here are some CORE token for your new account", true);
         
         signed_transaction upgrade_tx = con.wallet_api_ptr->upgrade_account("meta1", true);

      }
      // create_asset_limitation for META1 asset
      asset_limitation_options asset_limitation_ops = {
          "0.00000000000000",
          "0.0000000000000001",
      };
      signed_transaction asset_limitation_trx = con.wallet_api_ptr->create_asset_limitation("meta1", "META1", asset_limitation_ops, true);
      //get asset_limitation for META1 asset
      auto asset_limitation_meta1 = con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1");
      BOOST_CHECK(asset_limitation_meta1->limit_symbol == "META1");
      BOOST_CHECK(asset_limitation_meta1->issuer == con.wallet_api_ptr->get_account("meta1").get_id());
      BOOST_CHECK(asset_limitation_meta1->options.sell_limit == "0.00000000000000");
      BOOST_CHECK(asset_limitation_meta1->options.buy_limit == "0.0000000000000001");
   }
   catch (fc::exception &e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

const int64_t get_asset_supply(asset_object asset) 
{
   //calc supply for smooth allocation formula supply of coin / 10
   return asset.options.max_supply.value / 10 / std::pow(10, asset.precision);
}

bool double_equals(double a, double b, double epsilon = 0.000001)
{
    return std::abs(a - b) < epsilon;
}

/////////////////////////////////////////////////////////////////////
//https://github.com/meta1-blockchain/meta1-core/issues/18         //
/////////////////////////////////////////////////////////////////////

// (Case A) 
// An asset is added on Date 1. It is appraised at a certain value. 
// It remains in the "in processing contract" but it is never verified. 
// Demonstrate that the "Smooth Allocation Smart Contract" increases the value over time.

///////////
/// @brief in_time_test tests that the plugin is in production mode,
///        allocates the value exactly every minute and checks that progress value are calculated correctly.
///
///        Test duration: 3 minutes
//////////
BOOST_FIXTURE_TEST_CASE(in_time_test, production_cli_fixture)
{
   try
   {
      INVOKE(create_asset_limitation);
      ilog("asset_limitation for META1 created");

      property_options property_ops = {
          "some description",
          "some title",
          "my@email.com",
          "you",
          "https://fsf.com",
          "https://purepng.com/metal-1701528976849tkdsl.png",
          "not approved",
          "222",
          1000000000,
          1,
          33104,
          "52",
          "0.000000000000",
          "META1",
      };

      signed_transaction create_backing_asset = con.wallet_api_ptr->create_property("meta1", property_ops, true);
      time_point start_time = fc::time_point::now();
      int loop_counter = 0;

      ilog("Start time:${t}", ("t", start_time));
      auto backing_asset_create = create_backing_asset.operations.back().get<property_create_operation>();

      //create_backing_asset && wait 60s.for allocation META1 sell_limitation
      ilog("Backing Asset created\nINFO:\n ${b}\n", ("b", backing_asset_create.common_options));

      ilog("META1 sell limitation:${l}", ("l", con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit));
      ilog("Waiting first minute for allocation progress\n");
      //First minute
      fc::usleep(fc::minutes(1));
      ++loop_counter;

      //checking that the current time - 1 minute == creation time of new backing asset
      BOOST_CHECK(fc::time_point::now().sec_since_epoch() - 60 * loop_counter == start_time.sec_since_epoch());
      ilog("Time:${t}", ("t", fc::time_point::now()));
      double_t price = ((double)backing_asset_create.common_options.appraised_property_value / get_asset_supply(con.wallet_api_ptr->get_asset("META1"))) * 0.25;
      double_t timeline = boost::lexical_cast<double_t>(backing_asset_create.common_options.smooth_allocation_time) * 7 * 24 * 60 * 0.25;
      double_t increase_value = price / timeline;
      double_t progress = increase_value;

      ilog("META1 sell limitation:${l}", ("l", con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit));
      ilog("Price to allocate    :${p}", ("p", price));
      ilog("Timeline             :${t}", ("t", timeline));
      ilog("Increase_value       :${i}", ("i", increase_value));
      ilog("Progress             :${p}", ("p", progress));

      //check that allocation progress in backing asset && META1 sell_limitation are same double_t progress
      BOOST_CHECK(boost::lexical_cast<double_t>(con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit) == progress);
      BOOST_CHECK(boost::lexical_cast<double_t>(con.wallet_api_ptr->get_property(backing_asset_create.property_id).options.allocation_progress) == progress);

      ilog("Waiting second minute for allocation progress\n");
      //Second minute
      fc::usleep(fc::minutes(1));
      ++loop_counter;

      progress += increase_value;

      //checking that the current time - 2 minute == creation time of new backing asset
      BOOST_CHECK(fc::time_point::now().sec_since_epoch() - 60 * loop_counter == start_time.sec_since_epoch());

      ilog("Time:${t}", ("t", fc::time_point::now()));
      ilog("META1 sell limitation:${l}", ("l", con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit));
      ilog("Price to allocate    :${p}", ("p", price));
      ilog("Timeline             :${t}", ("t", timeline));
      ilog("Increase_value       :${i}", ("i", increase_value));
      ilog("Progress             :${p}", ("p", progress));

      //check that allocation progress in backing asset && META1 sell_limitation are same double_t progress
      BOOST_CHECK(boost::lexical_cast<double_t>(con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit) == progress);
      BOOST_CHECK(boost::lexical_cast<double_t>(con.wallet_api_ptr->get_property(backing_asset_create.property_id).options.allocation_progress) == progress);

      ilog("Waiting third minute for allocation progress\n");
      //Third minute
      fc::usleep(fc::minutes(1));
      ++loop_counter;

      progress += increase_value;

      //checking that the current time - 3 minute == creation time of new backing asset
      BOOST_CHECK(fc::time_point::now().sec_since_epoch() - 60 * loop_counter == start_time.sec_since_epoch());

      ilog("Time:${t}", ("t", fc::time_point::now()));
      ilog("META1 sell limitation:${l}", ("l", con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit));
      ilog("Price to allocate    :${p}", ("p", price));
      ilog("Timeline             :${t}", ("t", timeline));
      ilog("Increase_value       :${i}", ("i", increase_value));
      ilog("Progress             :${p}", ("p", progress));

      //check that allocation progress in backing asset && META1 sell_limitation are same double_t progress
      BOOST_CHECK(boost::lexical_cast<double_t>(con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit) == progress);
      BOOST_CHECK(boost::lexical_cast<double_t>(con.wallet_api_ptr->get_property(backing_asset_create.property_id).options.allocation_progress) == progress);
   }
   catch (fc::exception &e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

///////////
/// @brief end_of_the_vesting_period_test tests that the plugin is in test mode (speed up allocation mode)
///        allocates backing asset progress && META1 price_limit value using 1 minute progress with  100 milliseconds speed.
///        Demonstrate at the end of the vesting period that the smooth allocation has allocated 0.25 of the appraisal value 
///        Demonstrate after the end of the vesting period that the smooth allocation has not allocated more than 0.25 of the appraisal value
///        
///        Test duration: ~5 minutes
//////////
BOOST_FIXTURE_TEST_CASE(end_of_the_vesting_period_test, test_cli_fixture)
{
   try
   {
      INVOKE(create_asset_limitation);
      ilog("asset_limitation for META1 created");

      property_options property_ops = {
          "some description",
          "some title",
          "my@email.com",
          "you",
          "https://fsf.com",
          "https://purepng.com/metal-1701528976849tkdsl.png",
          "not approved",
          "222",
          1000000000,
          1,
          33104,
          "1",
          "0.000000000000",
          "META1",
      };
      //Price to allocate
      double_t price = ((double)property_ops.appraised_property_value / get_asset_supply(con.wallet_api_ptr->get_asset("META1"))) * 0.25;
      double_t timeline = boost::lexical_cast<double_t>(property_ops.smooth_allocation_time) * 7 * 24 * 60 * 0.25;
      double_t increase_value = price / timeline;
      double_t progress = 0.0;

      //Calculated value of the end of allocation, in milliseconds
      int loop_ends_in_milliseconds = price / increase_value * 100;
      time_point start_time = fc::time_point::now();

      signed_transaction create_backing_asset = con.wallet_api_ptr->create_property("meta1", property_ops, true);
      auto backing_asset_create = create_backing_asset.operations.back().get<property_create_operation>();

      //loop for checking allocation_progress && sell_limit progress
      for (int i = 0; i < loop_ends_in_milliseconds; i += 100)
      {
         //////////
         //Waiting for new allocation progress.
         //99 milliseconds.
         //Due to the fact that allocation occurs very quickly, we have a slight delay in the tests thread.
         //It will be enough to expect every cycle 1 millisecond less.
         fc::usleep(fc::milliseconds(99));

         progress += increase_value;

         ilog("Price to allocate    :${p}", ("p", price));
         ilog("META1 sell limitation:${l}", ("l", con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit));
         ilog("Time:${t}", ("t", fc::time_point::now()));
         ilog("progress:${t}", ("t", progress));

         //Check that allocation progress in backing asset && META1 sell_limitation are same double_t progress
         BOOST_CHECK(boost::lexical_cast<double_t>(con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit) == progress);
         BOOST_CHECK(boost::lexical_cast<double_t>(con.wallet_api_ptr->get_property(backing_asset_create.property_id).options.allocation_progress) == progress);
      }

      fc::usleep(fc::milliseconds(500));

      ilog("Price to allocate    :${p}", ("p", price));
      ilog("META1 sell limitation:${l}", ("l", con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit));
      ilog("Time:${t}", ("t", fc::time_point::now()));
      ilog("progress:${t}", ("t", progress));

      //Check that allocatation not produced out of price(Price to allocate) range
      BOOST_CHECK(boost::lexical_cast<double_t>(con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit) == price);
      BOOST_CHECK(boost::lexical_cast<double_t>(con.wallet_api_ptr->get_property(backing_asset_create.property_id).options.allocation_progress) == price);
   }
   catch (fc::exception &e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

// OLD TESTS//
///////////////////////////////////////////////////////////////////
//smooth_allocation_plugin TEST                                  //
//Use test_cli_fixture as an easy way to send transactions for testing//
///////////////////////////////////////////////////////////////////
BOOST_FIXTURE_TEST_CASE(smooth_allocation_plugin, production_cli_fixture)
{
   try
   {
      INVOKE(create_asset_limitation);
      property_options property_ops = {
         "some description",
         "some title",
         "my@email.com",
         "you",
         "https://fsf.com",
         "https://purepng.com/public/uploads/large/purepng.com-gold-bargoldatomic-number-79chemical-elementgroup-11-elementaurumgold-dustprecious-metal-1701528976849tkdsl.png",
         "not approved",
         "222",
         1000000000,
         1,
         33104,
         "1",
         "0.000000000000",
         "META1",
      };

      signed_transaction create_backing_asset = con.wallet_api_ptr->create_property("meta1",property_ops,true);
      auto backing_asset_create = create_backing_asset.operations.back().get<property_create_operation>();

      //create_backing_asset && wait 60s.for allocation META1 sell_limitation
      ilog("Create backing Asset");
      ilog("META1 sell limitation:${l}", ("l", con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit));
      ilog("Waiting first minute");
      fc::usleep(fc::seconds(60));
      double_t price = ((double)backing_asset_create.common_options.appraised_property_value / get_asset_supply(con.wallet_api_ptr->get_asset("META1"))) * 0.25;
      double_t timeline = boost::lexical_cast<double_t>(backing_asset_create.common_options.smooth_allocation_time) * 7 * 24 * 60 * 0.25;
      double_t increase_value = price / timeline;
      double_t progress = increase_value;

      //check that allocation progress in backing asset && META1 sell_limitation are same double_t progress
      BOOST_CHECK( boost::lexical_cast<double_t>(con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit) == progress);
      BOOST_CHECK( boost::lexical_cast<double_t>(con.wallet_api_ptr->get_property(backing_asset_create.property_id).options.allocation_progress) == progress);

      ilog("Waiting second minute");
      fc::usleep(fc::seconds(60));
      progress+=increase_value;
      ilog("test_progress :${b}", ("b", progress));
      //check that allocation progress in backing asset && META1 sell_limitation are same double_t progress*2
      BOOST_CHECK( boost::lexical_cast<double_t>(con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit) == progress);
      BOOST_CHECK( boost::lexical_cast<double_t>(con.wallet_api_ptr->get_property(backing_asset_create.property_id).options.allocation_progress) == progress);

      //approve_backing_asset
      signed_transaction approve_backing_asset = con.wallet_api_ptr->approve_property(backing_asset_create.property_id,true);
      ilog("Waiting third minute");
      ilog("Approve backing Asset");
      fc::usleep(fc::seconds(60));
      progress+=increase_value;
      ilog("test_progress for initial 3 min${b}", ("b", progress));

      price = ((double)backing_asset_create.common_options.appraised_property_value / get_asset_supply(con.wallet_api_ptr->get_asset("META1")));
      timeline = boost::lexical_cast<double_t>(backing_asset_create.common_options.smooth_allocation_time) * 7 * 24 * 60 * 0.75;
      progress+=  price / timeline;

      ilog("test_progress for initial && approve allocation 3 min:${b}", ("b", progress));
       //check that allocation progress in backing asset && META1 sell_limitation are same double_t progress with initial && approve allocation at the same time
      BOOST_CHECK( boost::lexical_cast<double_t>(con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit) == progress);
      BOOST_CHECK( boost::lexical_cast<double_t>(con.wallet_api_ptr->get_property(backing_asset_create.property_id).options.allocation_progress) == progress);

      ilog("Waiting fourth minute");
      fc::usleep(fc::seconds(60));
      progress+=increase_value;
      progress+=  price / timeline;

      ilog("test_progress for initial && approve allocation 4 min:${b}", ("b", progress));
       //check that allocation progress in backing asset && META1 sell_limitation are same double_t progress with initial && approve allocation at the same time
      BOOST_CHECK( boost::lexical_cast<double_t>(con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit) == progress);
      BOOST_CHECK( boost::lexical_cast<double_t>(con.wallet_api_ptr->get_property(backing_asset_create.property_id).options.allocation_progress) == progress);

      //if we pass previous test of allocation backing_asset progress state, NOW we will check allocation producing with 2 backing_assets
      ilog("create second backing_asset");
      signed_transaction create_backing_asset_second = con.wallet_api_ptr->create_property("meta1",property_ops,true);
      auto backing_asset_create_second = create_backing_asset_second.operations.back().get<property_create_operation>();
      ilog("Waiting fifth minute");
      fc::usleep(fc::seconds(60));
      //progress from first backing_asset
      progress+=increase_value;
      progress+=  price / timeline;
      //progress from second backing_asset that is not approved and have same stats as the first one 
      progress+=increase_value;

      BOOST_CHECK( boost::lexical_cast<double_t>(con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit) == progress);

      //approve second property
      approve_backing_asset = con.wallet_api_ptr->approve_property(backing_asset_create_second.property_id,true);
      ilog("approve second property");
      ilog("Waiting sixth minute");
      fc::usleep(fc::seconds(60));
       //progress from first backing_asset
      progress+=increase_value;
      progress+=  price / timeline;
      //progress from second approved backing_asset  and have same stats as the first one 
      progress+=increase_value;
      progress+=  price / timeline;
       wlog("META1 limit: ${l}",("l",con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")));
      BOOST_CHECK( boost::lexical_cast<double_t>(con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit) == progress);
      
      //erase_backed_asset plugin storage if delete backing_asset and stop allocation from this asset to sell_limit
      //and recount sell_limit -allocation progress of deleted backing asset
      //delete_backing_asset first
      ilog("delete_backing_asset");
      signed_transaction delete_backing_asset = con.wallet_api_ptr->delete_property(backing_asset_create.property_id,true);
      ilog("Waiting seventh minute");
      fc::usleep(fc::seconds(60));
      progress+=increase_value;
      progress+=  price / timeline;
      double_t second_backing_asset_progress = boost::lexical_cast<double_t>(con.wallet_api_ptr->get_property(backing_asset_create_second.property_id).options.allocation_progress);

      //rolling back progress from first backing_asset in property_delete operation
      //check that now META1 sell_limit == allocations only from existing second backing_asset
      BOOST_CHECK(double_equals( boost::lexical_cast<double_t>(con.wallet_api_ptr->get_asset_limitaion_by_symbol("META1")->options.sell_limit),second_backing_asset_progress));
   
   }
   catch (fc::exception &e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}
