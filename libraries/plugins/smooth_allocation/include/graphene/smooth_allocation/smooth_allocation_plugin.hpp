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
#pragma once

#include <graphene/app/plugin.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/asset_limitation_object.hpp>
#include <graphene/utilities/key_conversion.hpp>

using std::vector;

namespace graphene
{
namespace smooth_allocation
{

namespace smooth_allocation_condition
{
enum smooth_allocation_condition_enum
{
    initial_allocation_produced = 0,
    approve_allocation_produced = 1,
    initial_allocation_completed = 2,
    approve_allocation_completed = 3,
    exception_allocation = 4,
    stop_smooth_allocation = 5
};
}

class smooth_allocation_plugin : public graphene::app::plugin
{
public:
    ~smooth_allocation_plugin() { stop_allocation(); }

    virtual std::string plugin_name() const override;

    virtual void plugin_set_program_options(
        boost::program_options::options_description &command_line_options,
        boost::program_options::options_description &config_file_options) override;

    void stop_allocation();

    virtual void plugin_initialize(const boost::program_options::variables_map &options) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

private:
    vector<chain::property_object> get_all_backed_assets(chain::database &db) const;
    const chain::asset_limitation_object &get_asset_limitation(chain::database &db, std::string symbol) const;
    const chain::property_object &get_backed_asset(chain::database &db, uint32_t backed_asset_id) const;
    const chain::property_object &get_backed_asset(chain::database &db, protocol::property_id_type backed_asset_id_type) const;
    const int64_t get_asset_supply(chain::database &db, std::string symbol) const;

    void allocate_price_limitation(std::string backed_by_asset_symbol, uint32_t backed_asset_id, double_t value);
    void increase_backed_asset_allocation_progress(uint32_t backed_asset_id, double_t increase_value);
    void schedule_allocation_loop();
    void result_viewer(smooth_allocation_condition::smooth_allocation_condition_enum result, fc::limited_mutable_variant_object &capture);

    void allocation_loop();
    smooth_allocation_condition::smooth_allocation_condition_enum maybe_allocate_price(chain::property_object &backed_asset, double_t allocation_percent, fc::limited_mutable_variant_object &capture);

    boost::program_options::variables_map _options;
    bool _shutting_down = false;

    protocol::account_id_type meta1_account_id;
    fc::optional<fc::ecc::private_key> privkey;

    vector<chain::property_object> backed_assets_local_storage;
    vector<chain::property_object> initial_smooth_backed_assets;
    vector<chain::property_object> approve_smooth_backed_assets;

    fc::future<void> _smooth_allocation_task;
};

} // namespace smooth_allocation
} // namespace graphene
