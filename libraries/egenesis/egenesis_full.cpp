//                                   _           _    __ _ _        //
//                                  | |         | |  / _(_) |       //
//    __ _  ___ _ __   ___ _ __ __ _| |_ ___  __| | | |_ _| | ___   //
//   / _` |/ _ \ '_ \ / _ \ '__/ _` | __/ _ \/ _` | |  _| | |/ _ \  //
//  | (_| |  __/ | | |  __/ | | (_| | ||  __/ (_| | | | | | |  __/  //
//   \__, |\___|_| |_|\___|_|  \__,_|\__\___|\__,_| |_| |_|_|\___|  //
//    __/ |                                                         //
//   |___/                                                          //
//                                                                  //
// Generated by:  libraries/chain_id/identify_chain.cpp             //
//                                                                  //
// Warning: This is a generated file, any changes made here will be //
// overwritten by the build process.  If you need to change what    //
// is generated here, you should use the CMake variable             //
// GRAPHENE_EGENESIS_JSON to specify an embedded genesis state.     //
//                                                                  //

/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <graphene/protocol/types.hpp>
#include <graphene/egenesis/egenesis.hpp>

namespace graphene { namespace egenesis {

using namespace graphene::chain;

static const char genesis_json_array[244][40+1] =
{
"{\n  \"initial_timestamp\": \"2019-02-14T20:",
"32:55\",\n  \"max_core_supply\": \"4500000000",
"0000\",\n  \"initial_parameters\": {\n    \"cu",
"rrent_fees\": {\n      \"parameters\": [[\n  ",
"        0,{\n            \"fee\": 2000,\n   ",
"         \"price_per_kbyte\": 1000\n       ",
"   }\n        ],[\n          1,{\n         ",
"   \"fee\": 500\n          }\n        ],[\n  ",
"        2,{\n            \"fee\": 0\n       ",
"   }\n        ],[\n          3,{\n         ",
"   \"fee\": 2000\n          }\n        ],[\n ",
"         4,{}\n        ],[\n          5,{\n",
"            \"basic_fee\": 500,\n          ",
"  \"premium_fee\": 20000,\n            \"pri",
"ce_per_kbyte\": 100\n          }\n        ]",
",[\n          6,{\n            \"fee\": 2000",
",\n            \"price_per_kbyte\": 100\n   ",
"       }\n        ],[\n          7,{\n     ",
"       \"fee\": 300\n          }\n        ],",
"[\n          8,{\n            \"membership_",
"annual_fee\": 200000,\n            \"member",
"ship_lifetime_fee\": 100000\n          }\n ",
"       ],[\n          9,{\n            \"fe",
"e\": 50000\n          }\n        ],[\n      ",
"    10,{\n            \"symbol3\": \"5000000",
"0\",\n            \"symbol4\": \"30000000\",\n ",
"           \"long_symbol\": 500000,\n      ",
"      \"price_per_kbyte\": 10\n          }\n",
"        ],[\n          11,{\n            \"",
"fee\": 50000,\n            \"price_per_kbyt",
"e\": 10\n          }\n        ],[\n         ",
" 12,{\n            \"fee\": 50000\n         ",
" }\n        ],[\n          13,{\n          ",
"  \"fee\": 50000\n          }\n        ],[\n ",
"         14,{\n            \"fee\": 2000,\n ",
"           \"price_per_kbyte\": 100\n      ",
"    }\n        ],[\n          15,{\n       ",
"     \"fee\": 2000\n          }\n        ],[",
"\n          16,{\n            \"fee\": 100\n ",
"         }\n        ],[\n          17,{\n  ",
"          \"fee\": 10000\n          }\n     ",
"   ],[\n          18,{\n            \"fee\":",
" 50000\n          }\n        ],[\n         ",
" 19,{\n            \"fee\": 100\n          }",
"\n        ],[\n          20,{\n            ",
"\"fee\": 500000\n          }\n        ],[\n  ",
"        21,{\n            \"fee\": 2000\n   ",
"       }\n        ],[\n          22,{\n    ",
"        \"fee\": 2000,\n            \"price_",
"per_kbyte\": 10\n          }\n        ],[\n ",
"         23,{\n            \"fee\": 2000,\n ",
"           \"price_per_kbyte\": 10\n       ",
"   }\n        ],[\n          24,{\n        ",
"    \"fee\": 100\n          }\n        ],[\n ",
"         25,{\n            \"fee\": 100\n   ",
"       }\n        ],[\n          26,{\n    ",
"        \"fee\": 100\n          }\n        ]",
",[\n          27,{\n            \"fee\": 200",
"0,\n            \"price_per_kbyte\": 10\n   ",
"       }\n        ],[\n          28,{\n    ",
"        \"fee\": 0\n          }\n        ],[",
"\n          29,{\n            \"fee\": 50000",
"0\n          }\n        ],[\n          30,{",
"\n            \"fee\": 2000\n          }\n   ",
"     ],[\n          31,{\n            \"fee",
"\": 100\n          }\n        ],[\n         ",
" 32,{\n            \"fee\": 100\n          }",
"\n        ],[\n          33,{\n            ",
"\"fee\": 2000\n          }\n        ],[\n    ",
"      34,{\n            \"fee\": 500000\n   ",
"       }\n        ],[\n          35,{\n    ",
"        \"fee\": 100,\n            \"price_p",
"er_kbyte\": 10\n          }\n        ],[\n  ",
"        36,{\n            \"fee\": 100\n    ",
"      }\n        ],[\n          37,{}\n    ",
"    ],[\n          38,{\n            \"fee\"",
": 2000,\n            \"price_per_kbyte\": 1",
"0\n          }\n        ],[\n          39,{",
"\n            \"fee\": 500,\n            \"pr",
"ice_per_output\": 500\n          }\n       ",
" ],[\n          40,{\n            \"fee\": 5",
"00,\n            \"price_per_output\": 500\n",
"          }\n        ],[\n          41,{\n ",
"           \"fee\": 500\n          }\n      ",
"  ],[\n          42,{}\n        ],[\n      ",
"    43,{\n            \"fee\": 2000\n       ",
"   }\n        ],[\n          44,{}\n       ",
" ],[\n          45,{\n            \"fee\": 2",
"000\n          }\n        ],[\n          46",
",{}\n        ],[\n          47,{\n         ",
"   \"fee\": 2000\n          }\n        ],[\n ",
"         48,{\n            \"fee\": 2000\n  ",
"        }\n        ]\n      ],\n      \"scal",
"e\": 10000\n    },\n    \"block_interval\": 5",
",\n    \"maintenance_interval\": 86400,\n   ",
" \"maintenance_skip_slots\": 3,\n    \"commi",
"ttee_proposal_review_period\": 1209600,\n ",
"   \"maximum_transaction_size\": 2048,\n   ",
" \"maximum_block_size\": 2000000,\n    \"max",
"imum_time_until_expiration\": 86400,\n    ",
"\"maximum_proposal_lifetime\": 2419200,\n  ",
"  \"maximum_asset_whitelist_authorities\":",
" 10,\n    \"maximum_asset_feed_publishers\"",
": 10,\n    \"maximum_witness_count\": 1001,",
"\n    \"maximum_committee_count\": 1001,\n  ",
"  \"maximum_authority_membership\": 10,\n  ",
"  \"reserve_percent_of_fee\": 2000,\n    \"n",
"etwork_percent_of_fee\": 2000,\n    \"lifet",
"ime_referrer_percent_of_fee\": 3000,\n    ",
"\"cashback_vesting_period_seconds\": 31536",
"000,\n    \"cashback_vesting_threshold\": 1",
"0000000,\n    \"count_non_member_votes\": t",
"rue,\n    \"allow_non_member_whitelists\": ",
"false,\n    \"witness_pay_per_block\": 1000",
"000,\n    \"worker_budget_per_day\": \"50000",
"000000\",\n    \"max_predicate_opcode\": 1,\n",
"    \"fee_liquidation_threshold\": 1000000",
"0,\n    \"accounts_per_fee_scale\": 1000,\n ",
"   \"account_fee_scale_bitshifts\": 4,\n   ",
" \"max_authority_depth\": 2,\n    \"extensio",
"ns\": []\n  },\n  \"initial_accounts\": [{\n  ",
"    \"name\": \"init0\",\n      \"owner_key\": ",
"\"META16MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8B",
"htHuGYqET5GDW5CV\",\n      \"active_key\": \"",
"META16MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8Bh",
"tHuGYqET5GDW5CV\",\n      \"is_lifetime_mem",
"ber\": true\n    },{\n      \"name\": \"init1\"",
",\n      \"owner_key\": \"META16MRyAjQq8ud7h",
"VNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV\",\n",
"      \"active_key\": \"META16MRyAjQq8ud7hV",
"NYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV\",\n ",
"     \"is_lifetime_member\": true\n    },{\n",
"      \"name\": \"init2\",\n      \"owner_key\"",
": \"META16MRyAjQq8ud7hVNYcfnVPJqcVpscN5So",
"8BhtHuGYqET5GDW5CV\",\n      \"active_key\":",
" \"META16MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8",
"BhtHuGYqET5GDW5CV\",\n      \"is_lifetime_m",
"ember\": true\n    },{\n      \"name\": \"init",
"3\",\n      \"owner_key\": \"META16MRyAjQq8ud",
"7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV\"",
",\n      \"active_key\": \"META16MRyAjQq8ud7",
"hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV\",",
"\n      \"is_lifetime_member\": true\n    },",
"{\n      \"name\": \"init4\",\n      \"owner_ke",
"y\": \"META16MRyAjQq8ud7hVNYcfnVPJqcVpscN5",
"So8BhtHuGYqET5GDW5CV\",\n      \"active_key",
"\": \"META16MRyAjQq8ud7hVNYcfnVPJqcVpscN5S",
"o8BhtHuGYqET5GDW5CV\",\n      \"is_lifetime",
"_member\": true\n    },{\n      \"name\": \"in",
"it5\",\n      \"owner_key\": \"META16MRyAjQq8",
"ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5C",
"V\",\n      \"active_key\": \"META16MRyAjQq8u",
"d7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV",
"\",\n      \"is_lifetime_member\": true\n    ",
"},{\n      \"name\": \"init6\",\n      \"owner_",
"key\": \"META16MRyAjQq8ud7hVNYcfnVPJqcVpsc",
"N5So8BhtHuGYqET5GDW5CV\",\n      \"active_k",
"ey\": \"META16MRyAjQq8ud7hVNYcfnVPJqcVpscN",
"5So8BhtHuGYqET5GDW5CV\",\n      \"is_lifeti",
"me_member\": true\n    },{\n      \"name\": \"",
"init7\",\n      \"owner_key\": \"META16MRyAjQ",
"q8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW",
"5CV\",\n      \"active_key\": \"META16MRyAjQq",
"8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5",
"CV\",\n      \"is_lifetime_member\": true\n  ",
"  },{\n      \"name\": \"init8\",\n      \"owne",
"r_key\": \"META16MRyAjQq8ud7hVNYcfnVPJqcVp",
"scN5So8BhtHuGYqET5GDW5CV\",\n      \"active",
"_key\": \"META16MRyAjQq8ud7hVNYcfnVPJqcVps",
"cN5So8BhtHuGYqET5GDW5CV\",\n      \"is_life",
"time_member\": true\n    },{\n      \"name\":",
" \"init9\",\n      \"owner_key\": \"META16MRyA",
"jQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5G",
"DW5CV\",\n      \"active_key\": \"META16MRyAj",
"Qq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GD",
"W5CV\",\n      \"is_lifetime_member\": true\n",
"    },{\n      \"name\": \"init10\",\n      \"o",
"wner_key\": \"META16MRyAjQq8ud7hVNYcfnVPJq",
"cVpscN5So8BhtHuGYqET5GDW5CV\",\n      \"act",
"ive_key\": \"META16MRyAjQq8ud7hVNYcfnVPJqc",
"VpscN5So8BhtHuGYqET5GDW5CV\",\n      \"is_l",
"ifetime_member\": true\n    },{\n      \"nam",
"e\": \"nathan\",\n      \"owner_key\": \"META16",
"MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYq",
"ET5GDW5CV\",\n      \"active_key\": \"META16M",
"RyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqE",
"T5GDW5CV\",\n      \"is_lifetime_member\": f",
"alse\n    }\n  ],\n  \"initial_assets\": [],\n",
"  \"initial_balances\": [{\n      \"owner\": ",
"\"META1FAbAx7yuxt725qSZvfwWqkdCwp9ZnUama\"",
",\n      \"asset_symbol\": \"META1\",\n      \"",
"amount\": \"45000000000000\"\n    }\n  ],\n  \"",
"initial_vesting_balances\": [],\n  \"initia",
"l_active_witnesses\": 11,\n  \"initial_witn",
"ess_candidates\": [{\n      \"owner_name\": ",
"\"init0\",\n      \"block_signing_key\": \"MET",
"A16MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHu",
"GYqET5GDW5CV\"\n    },{\n      \"owner_name\"",
": \"init1\",\n      \"block_signing_key\": \"M",
"ETA16MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8Bht",
"HuGYqET5GDW5CV\"\n    },{\n      \"owner_nam",
"e\": \"init2\",\n      \"block_signing_key\": ",
"\"META16MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8B",
"htHuGYqET5GDW5CV\"\n    },{\n      \"owner_n",
"ame\": \"init3\",\n      \"block_signing_key\"",
": \"META16MRyAjQq8ud7hVNYcfnVPJqcVpscN5So",
"8BhtHuGYqET5GDW5CV\"\n    },{\n      \"owner",
"_name\": \"init4\",\n      \"block_signing_ke",
"y\": \"META16MRyAjQq8ud7hVNYcfnVPJqcVpscN5",
"So8BhtHuGYqET5GDW5CV\"\n    },{\n      \"own",
"er_name\": \"init5\",\n      \"block_signing_",
"key\": \"META16MRyAjQq8ud7hVNYcfnVPJqcVpsc",
"N5So8BhtHuGYqET5GDW5CV\"\n    },{\n      \"o",
"wner_name\": \"init6\",\n      \"block_signin",
"g_key\": \"META16MRyAjQq8ud7hVNYcfnVPJqcVp",
"scN5So8BhtHuGYqET5GDW5CV\"\n    },{\n      ",
"\"owner_name\": \"init7\",\n      \"block_sign",
"ing_key\": \"META16MRyAjQq8ud7hVNYcfnVPJqc",
"VpscN5So8BhtHuGYqET5GDW5CV\"\n    },{\n    ",
"  \"owner_name\": \"init8\",\n      \"block_si",
"gning_key\": \"META16MRyAjQq8ud7hVNYcfnVPJ",
"qcVpscN5So8BhtHuGYqET5GDW5CV\"\n    },{\n  ",
"    \"owner_name\": \"init9\",\n      \"block_",
"signing_key\": \"META16MRyAjQq8ud7hVNYcfnV",
"PJqcVpscN5So8BhtHuGYqET5GDW5CV\"\n    },{\n",
"      \"owner_name\": \"init10\",\n      \"blo",
"ck_signing_key\": \"META16MRyAjQq8ud7hVNYc",
"fnVPJqcVpscN5So8BhtHuGYqET5GDW5CV\"\n    }",
"\n  ],\n  \"initial_committee_candidates\": ",
"[{\n      \"owner_name\": \"init0\"\n    },{\n ",
"     \"owner_name\": \"init1\"\n    },{\n     ",
" \"owner_name\": \"init2\"\n    },{\n      \"ow",
"ner_name\": \"init3\"\n    },{\n      \"owner_",
"name\": \"init4\"\n    },{\n      \"owner_name",
"\": \"init5\"\n    },{\n      \"owner_name\": \"",
"init6\"\n    },{\n      \"owner_name\": \"init",
"7\"\n    },{\n      \"owner_name\": \"init8\"\n ",
"   },{\n      \"owner_name\": \"init9\"\n    }",
",{\n      \"owner_name\": \"init10\"\n    }\n  ",
"],\n  \"initial_worker_candidates\": [],\n  ",
"\"immutable_parameters\": {\n    \"min_commi",
"ttee_member_count\": 11,\n    \"min_witness",
"_count\": 11,\n    \"num_special_accounts\":",
" 0,\n    \"num_special_assets\": 0\n  }\n}\n"
};

chain_id_type get_egenesis_chain_id()
{
   return chain_id_type( "3dbdca4078e2988c2a31202e2aa3cbeae7a8aa755a31e0f49d6f4b2a262e9c3e" );
}

void compute_egenesis_json( std::string& result )
{
   result.reserve( 9758 );
   result.resize(0);
   for( size_t i=0; i<244-1; i++ )
   {
      result.append( genesis_json_array[i], 40 );
   }
   result.append( std::string( genesis_json_array[ 244-1 ] ) );
}

fc::sha256 get_egenesis_json_hash()
{
   return fc::sha256( "3dbdca4078e2988c2a31202e2aa3cbeae7a8aa755a31e0f49d6f4b2a262e9c3e" );
}

} }
