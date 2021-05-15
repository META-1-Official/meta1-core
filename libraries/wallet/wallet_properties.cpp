/*
 * Copyright META1 (c) 2020-2021
 */

#include <graphene/wallet/wallet.hpp>
#include "wallet_api_impl.hpp"
#include <random>

namespace graphene { namespace wallet { namespace detail {

   bool wallet_api_impl::is_property_exists(uint32_t id) const
   {
      bool flag = _remote_db->is_property_exists(id);
      return flag;
   }

    signed_transaction wallet_api_impl::create_property(string issuer,
                                       uint64_t appraised_property_value,
                                       uint32_t allocation_duration_minutes,
                                       string backed_by_asset_symbol,
                                       property_options common,
                                       bool broadcast)
   {
      try
      {
         FC_ASSERT(find_asset(backed_by_asset_symbol).valid(), "Asset with that symbol not exists!");
         std::random_device rd;
         std::mt19937 mersenne(rd());
         uint32_t rand_id;
         bool flag;
         account_object issuer_account = get_account(issuer);
         property_create_operation create_op;

         //generating new property id and regenerate if such id is exists
         do 
         {
            rand_id = mersenne();
            ilog(" backed asset id : ${i}",("i",rand_id));
            flag = is_property_exists(rand_id);
         }
         while(flag);

         create_op.property_id = rand_id;
         create_op.issuer = issuer_account.id;
         create_op.appraised_property_value = appraised_property_value;
         create_op.allocation_duration_minutes = allocation_duration_minutes;
         create_op.backed_by_asset_symbol = backed_by_asset_symbol;
         create_op.common_options = common;
         signed_transaction tx;
         tx.operations.push_back(create_op);

         set_operation_fees(tx, _remote_db->get_global_properties().parameters.get_current_fees());

         tx.validate();
         auto transaction_result = sign_transaction(tx, broadcast);
         return transaction_result;
      }
      FC_CAPTURE_AND_RETHROW((issuer)(common)(broadcast))
   }


   signed_transaction wallet_api_impl::update_property(uint32_t id,
                                      property_options new_options,
                                      bool broadcast)
   {
      try
      {
         optional<property_object> property_to_update = find_property(id);
         if (!property_to_update)
            FC_THROW("No property with that id exists!");

         property_update_operation update_op;
         update_op.issuer = property_to_update->issuer;
         update_op.property_to_update = property_to_update->id;
         update_op.new_options = new_options;

         signed_transaction tx;
         tx.operations.push_back(update_op);
         set_operation_fees(tx, _remote_db->get_global_properties().parameters.get_current_fees());
         tx.validate();

         return sign_transaction(tx, broadcast);
      }
      FC_CAPTURE_AND_RETHROW((id)(new_options)(broadcast))
   }

   signed_transaction wallet_api_impl::approve_property(uint32_t id,
                                       bool broadcast)
   {
      try
      {
         optional<property_object> property_to_update = find_property(id);
         if (!property_to_update)
            FC_THROW("No property with that id exists!");

         property_approve_operation approve_op;
         approve_op.issuer = property_to_update->issuer;
         approve_op.property_to_approve = property_to_update->get_id();

         signed_transaction tx;
         tx.operations.push_back(approve_op);
         set_operation_fees(tx, _remote_db->get_global_properties().parameters.get_current_fees());
         tx.validate();

         return sign_transaction(tx, broadcast);
      }
      FC_CAPTURE_AND_RETHROW((id)(broadcast))
   }

   signed_transaction wallet_api_impl::delete_property(uint32_t property_id, bool broadcast)
   {
      try
      {
         FC_ASSERT(!is_locked());
         signed_transaction trx;

         property_delete_operation op;
         op.fee_paying_account = get_property(property_id).issuer;
         op.property = get_property(property_id).id;
         trx.operations = {op};
         set_operation_fees(trx, _remote_db->get_global_properties().parameters.get_current_fees());

         trx.validate();
         return sign_transaction(trx, broadcast);
      }
      FC_CAPTURE_AND_RETHROW((property_id))
   }

   optional<property_object> wallet_api_impl::find_property(uint32_t id) const
   {
      auto rec = _remote_db->get_properties({id}).front();
      return rec;
   }

   property_object wallet_api_impl::get_property(uint32_t id) const
   {
      auto rec = _remote_db->get_properties({id});
      FC_ASSERT(rec.front(), "no property with such id");
      return *rec.front();
   }

   vector<property_object> wallet_api_impl::get_all_properties() const
   {
      auto result = _remote_db->get_all_properties();
      return result;
   }

   vector<property_object> wallet_api_impl::get_properties_by_backed_asset_symbol(string symbol) const
   {
      auto result = _remote_db->get_properties_by_backed_asset_symbol(symbol);
      return result;
   }

   pair<uint32_t,uint32_t> wallet_api_impl::get_property_allocation_progress(uint32_t property_id) const
   {
      auto property_progress = get_property(property_id).get_allocation_progress();
      return std::make_pair(property_progress.numerator(),property_progress.denominator());
   }


   bool wallet_api_impl::is_asset_limitation_exists(string limit_symbol)const
   {
      bool flag = _remote_db->is_asset_limitation_exists(limit_symbol);
      return flag;
   }

   signed_transaction wallet_api_impl::create_asset_limitation(string issuer,
                                              string limit_symbol,
                                              bool broadcast)
   {
      try
      {
         FC_ASSERT(!is_asset_limitation_exists(limit_symbol),"Limitation for Asset with that symbol is already exists!");
         account_object issuer_account = get_account(issuer);
         asset_limitation_object_create_operation create_op;

         create_op.limit_symbol = limit_symbol;
         create_op.issuer = issuer_account.id;
         signed_transaction tx;
         tx.operations.push_back(create_op);

         set_operation_fees(tx, _remote_db->get_global_properties().parameters.get_current_fees());

         tx.validate();
         auto transaction_result = sign_transaction(tx, broadcast);
         return transaction_result;
      }
      FC_CAPTURE_AND_RETHROW((issuer)(limit_symbol)(broadcast))
   }

   optional<asset_limitation_object> wallet_api_impl::get_asset_limitaion_by_symbol( string limit_symbol ) const
   {
      auto result = _remote_db->get_asset_limitaion_by_symbol(limit_symbol);

      return result;
   }

   uint64_t wallet_api_impl::get_asset_limitation_value(const string symbol_or_id)const
   {
      uint64_t result = _remote_db->get_asset_limitation_value(symbol_or_id);

      return result;
   }

}}} // namespace graphene::wallet::detail
