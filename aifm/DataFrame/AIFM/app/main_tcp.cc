// Download dataset at
// https://www1.nyc.gov/site/tlc/about/tlc-trip-record-data.page.  The
// following code is implemented based on the format of 2016 datasets.

extern "C" {
#include <runtime/runtime.h>
}
#include "deref_scope.hpp"
#include "device.hpp"
#include "manager.hpp"

#include <DataFrame/DataFrame.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <string>

using namespace hmdf;
using namespace far_memory;

constexpr uint64_t kCacheGBs            = 1;
constexpr uint64_t kCacheSize           = kCacheGBs << 30;
constexpr uint64_t kFarMemSize          = (1ULL << 30);  // 1 GB. Not relevant here.
constexpr uint64_t kNumGCThreads        = 40;
constexpr uint64_t kNumElementsPerScope = 1024;
constexpr uint64_t kNumConnections      = 400;

using Index_t = unsigned long long;

static double haversine(double lat1, double lon1, double lat2, double lon2)
{
    // Distance between latitudes and longitudes
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;

    // Convert to radians.
    lat1 = lat1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;

    // Apply formulae.
    double a   = pow(sin(dLat / 2), 2) + pow(sin(dLon / 2), 2) * cos(lat1) * cos(lat2);
    double rad = 6371;
    double c   = 2 * asin(sqrt(a));
    return rad * c;
}

StdDataFrame<Index_t> load_data(FarMemManager* manager)
{
    return read_csv<-1, int, SimpleTime, SimpleTime, int, double, double, double, int, char, double,
                    double, int, double, double, double, double, double, double, double>(
        manager, "/mnt/all.csv", "VendorID", "tpep_pickup_datetime", "tpep_dropoff_datetime",
        "passenger_count", "trip_distance", "pickup_longitude", "pickup_latitude", "RatecodeID",
        "store_and_fwd_flag", "dropoff_longitude", "dropoff_latitude", "payment_type",
        "fare_amount", "extra", "mta_tax", "tip_amount", "tolls_amount", "improvement_surcharge",
        "total_amount");
}

void print_number_vendor_ids_and_unique(FarMemManager* manager, StdDataFrame<Index_t>& df)
{
    std::cout << "print_number_vendor_ids_and_unique()" << std::endl;
    std::cout << "Number of vendor_ids in the train dataset: "
              << df.get_column<int>("VendorID").size() << std::endl;
    std::cout << "Number of unique vendor_ids in the train dataset:"
              << df.get_col_unique_values<int>(manager, "VendorID").size() << std::endl;
    std::cout << std::endl;
}

void print_passage_counts_by_vendor_id(FarMemManager* manager, StdDataFrame<Index_t>& df,
                                       int vendor_id)
{
    std::cout << "print_passage_counts_by_vendor_id(vendor_id), vendor_id = " << vendor_id
              << std::endl;

    auto sel_vendor_functor = [&](const Index_t&, const int& vid) -> bool {
        return vid == vendor_id;
    };
    auto sel_df =
        df.get_data_by_sel<int, decltype(sel_vendor_functor), int, SimpleTime, double, char>(
            manager, "VendorID", sel_vendor_functor);
    auto& passage_count_vec = sel_df.get_column<int>("passenger_count");
    std::map<int, int> passage_count_map;
    {
        DerefScope scope;
        auto it = passage_count_vec.cfbegin(scope);
        for (Index_t i = 0; i < passage_count_vec.size(); ++i, ++it) {
            if (unlikely(i % kNumElementsPerScope == 0)) {
                scope.renew();
                it.renew(scope);
            }
            passage_count_map[*it]++;
        }
    }
    for (auto& [passage_count, cnt] : passage_count_map) {
        std::cout << "passage_count= " << passage_count << ", cnt = " << cnt << std::endl;
    }
    std::cout << std::endl;
}

void calculate_trip_duration(far_memory::FarMemManager* manager, StdDataFrame<Index_t>& df)
{
    std::cout << "calculate_trip_duration()" << std::endl;

    auto& pickup_time_vec  = df.get_column<SimpleTime>("tpep_pickup_datetime");
    auto& dropoff_time_vec = df.get_column<SimpleTime>("tpep_dropoff_datetime");
    assert(pickup_time_vec.size() == dropoff_time_vec.size());
    auto duration_vec = manager->allocate_dataframe_vector<unsigned long long>();
    duration_vec.resize(pickup_time_vec.size());
    {
        DerefScope scope;
        auto pickup_it   = pickup_time_vec.cfbegin(scope);
        auto dropoff_it  = dropoff_time_vec.cfbegin(scope);
        auto duration_it = duration_vec.fbegin(scope);
        for (Index_t i = 0; i < pickup_time_vec.size();
             ++i, ++pickup_it, ++dropoff_it, ++duration_it) {
            if (unlikely(i % kNumElementsPerScope == 0)) {
                scope.renew();
                pickup_it.renew(scope);
                dropoff_it.renew(scope);
                duration_it.renew(scope);
            }
            auto pickup_time_second  = pickup_it->to_second();
            auto dropoff_time_second = dropoff_it->to_second();
            *duration_it =
                static_cast<unsigned long long>(dropoff_time_second - pickup_time_second);
        }
        df.load_column(manager, "duration", std::move(duration_vec),
                       nan_policy::dont_pad_with_nans);
    }
    MaxVisitor<unsigned long long> max_visitor;
    MinVisitor<unsigned long long> min_visitor;
    MeanVisitor<unsigned long long> mean_visitor;
    df.multi_visit(std::make_pair("duration", &max_visitor),
                   std::make_pair("duration", &min_visitor),
                   std::make_pair("duration", &mean_visitor));
    std::cout << "Mean duration = " << mean_visitor.get_result() << " seconds" << std::endl;
    std::cout << "Min duration = " << min_visitor.get_result() << " seconds" << std::endl;
    std::cout << "Max duration = " << max_visitor.get_result() << " seconds" << std::endl;
    std::cout << std::endl;
}

void calculate_distribution_store_and_fwd_flag(FarMemManager* manager, StdDataFrame<Index_t>& df)
{
    std::cout << "calculate_distribution_store_and_fwd_flag()" << std::endl;

    auto sel_N_saff_functor = [&](const Index_t&, const char& saff) -> bool { return saff == 'N'; };
    auto N_df =
        df.get_data_by_sel<char, decltype(sel_N_saff_functor), int, SimpleTime, double, char>(
            manager, "store_and_fwd_flag", sel_N_saff_functor);
    std::cout << static_cast<double>(N_df.get_index().size()) / df.get_index().size() << std::endl;

    auto sel_Y_saff_functor = [&](const Index_t&, const char& saff) -> bool { return saff == 'Y'; };
    auto Y_df =
        df.get_data_by_sel<char, decltype(sel_Y_saff_functor), int, SimpleTime, double, char>(
            manager, "store_and_fwd_flag", sel_Y_saff_functor);
    auto unique_vendor_id_vec = Y_df.get_col_unique_values<int>(manager, "VendorID");
    std::cout << '{';
    {
        DerefScope scope;
        auto it = unique_vendor_id_vec.cfbegin(scope);
        for (Index_t i = 0; i < unique_vendor_id_vec.size(); ++i, ++it) {
            if (unlikely(i % kNumElementsPerScope == 0)) {
                scope.renew();
                it.renew(scope);
            }
            std::cout << *it << ", ";
        }
    }
    std::cout << '}' << std::endl;

    std::cout << std::endl;
}

void calculate_haversine_distance_column(FarMemManager* manager, StdDataFrame<Index_t>& df)
{
    std::cout << "calculate_haversine_distance_column()" << std::endl;

    auto& pickup_longitude_vec  = df.get_column<double>("pickup_longitude");
    auto& pickup_latitude_vec   = df.get_column<double>("pickup_latitude");
    auto& dropoff_longitude_vec = df.get_column<double>("dropoff_longitude");
    auto& dropoff_latitude_vec  = df.get_column<double>("dropoff_latitude");
    assert(pickup_longitude_vec.size() == pickup_latitude_vec.size());
    assert(pickup_longitude_vec.size() == dropoff_longitude_vec.size());
    assert(pickup_longitude_vec.size() == dropoff_latitude_vec.size());
    auto haversine_distance_vec = manager->allocate_dataframe_vector<double>();
    haversine_distance_vec.resize(pickup_longitude_vec.size());
    {
        DerefScope scope;
        auto pickup_lat_it  = pickup_latitude_vec.cfbegin(scope);
        auto pickup_lon_it  = pickup_longitude_vec.cfbegin(scope);
        auto dropoff_lat_it = dropoff_latitude_vec.cfbegin(scope);
        auto dropoff_lon_it = dropoff_longitude_vec.cfbegin(scope);
        auto dis_it         = haversine_distance_vec.fbegin(scope);
        for (Index_t i = 0; i < pickup_longitude_vec.size();
             ++i, ++pickup_lat_it, ++pickup_lon_it, ++dropoff_lat_it, ++dropoff_lon_it, ++dis_it) {
            if (unlikely(i % kNumElementsPerScope == 0)) {
                scope.renew();
            }
            *dis_it = haversine(*pickup_lat_it, *pickup_lon_it, *dropoff_lat_it, *dropoff_lon_it);
        }
    }
    df.load_column(manager, "haversine_distance", std::move(haversine_distance_vec),
                   nan_policy::dont_pad_with_nans);
    auto sel_functor = [&](const Index_t&, const double& dist) -> bool { return dist > 100; };
    auto sel_df = df.get_data_by_sel<double, decltype(sel_functor), int, SimpleTime, double, char>(
        manager, "haversine_distance", sel_functor);
    std::cout << "Number of rows that have haversine_distance > 100 KM = "
              << sel_df.get_index().size() << std::endl;

    std::cout << std::endl;
}

void analyze_trip_timestamp(FarMemManager* manager, StdDataFrame<Index_t>& df)
{
    std::cout << "analyze_trip_timestamp()" << std::endl;

    MaxVisitor<SimpleTime> max_visitor;
    MinVisitor<SimpleTime> min_visitor;
    df.multi_visit(std::make_pair("tpep_pickup_datetime", &max_visitor),
                   std::make_pair("tpep_pickup_datetime", &min_visitor));
    std::cout << max_visitor.get_result() << std::endl;
    std::cout << min_visitor.get_result() << std::endl;

    auto pickup_hour_vec  = manager->allocate_dataframe_vector<char>();
    auto pickup_day_vec   = manager->allocate_dataframe_vector<char>();
    auto pickup_month_vec = manager->allocate_dataframe_vector<char>();
    std::map<char, int> pickup_hour_map;
    std::map<char, int> pickup_day_map;
    std::map<char, int> pickup_month_map;
    auto& pickup_time_vec = df.get_column<SimpleTime>("tpep_pickup_datetime");
    pickup_hour_vec.resize(pickup_time_vec.size());
    pickup_day_vec.resize(pickup_time_vec.size());
    pickup_month_vec.resize(pickup_time_vec.size());
    {
        DerefScope scope;
        auto hour_it  = pickup_hour_vec.fbegin(scope);
        auto day_it   = pickup_day_vec.fbegin(scope);
        auto month_it = pickup_month_vec.fbegin(scope);
        auto time_it  = pickup_time_vec.cfbegin(scope);
        for (Index_t i = 0; i < pickup_time_vec.size();
             ++i, ++hour_it, ++day_it, ++month_it, ++time_it) {
            if (unlikely(i % kNumElementsPerScope == 0)) {
                scope.renew();
                hour_it.renew(scope);
                day_it.renew(scope);
                month_it.renew(scope);
            }
            auto time = *time_it;
            pickup_hour_map[time.hour_]++;
            *hour_it = time.hour_;
            pickup_day_map[time.day_]++;
            *day_it = time.day_;
            pickup_month_map[time.month_]++;
            *month_it = time.month_;
        }
    }
    df.load_column(manager, "pickup_hour", std::move(pickup_hour_vec),
                   nan_policy::dont_pad_with_nans);
    df.load_column(manager, "pickup_day", std::move(pickup_day_vec),
                   nan_policy::dont_pad_with_nans);
    df.load_column(manager, "pickup_month", std::move(pickup_month_vec),
                   nan_policy::dont_pad_with_nans);

    std::cout << "Print top 10 rows." << std::endl;
    auto top_10_df = df.get_data_by_idx<int, SimpleTime, double, char>(
        manager, Index2D<StdDataFrame<Index_t>::IndexType>{0, 9});
    top_10_df.write<std::ostream, int, SimpleTime, double, char>(std::cout, false, io_format::json);
    std::cout << std::endl;

    for (auto& [hour, cnt] : pickup_hour_map) {
        std::cout << "pickup_hour = " << static_cast<int>(hour) << ", cnt = " << cnt << std::endl;
    }
    std::cout << std::endl;
    for (auto& [day, cnt] : pickup_day_map) {
        std::cout << "pickup_day = " << static_cast<int>(day) << ", cnt = " << cnt << std::endl;
    }
    std::cout << std::endl;
    for (auto& [month, cnt] : pickup_month_map) {
        std::cout << "pickup_month = " << static_cast<int>(month) << ", cnt = " << cnt << std::endl;
    }

    std::cout << std::endl;
}

template <typename T_Key>
void analyze_trip_durations_of_timestamps(far_memory::FarMemManager* manager,
                                          StdDataFrame<Index_t>& df, const char* key_col_name)
{
    std::cout << "analyze_trip_durations_of_timestamps() on key = " << key_col_name << std::endl;

    StdDataFrame<Index_t> df_key_duration(manager);
    auto copy_index        = df.get_index();
    auto copy_key_col      = df.get_column<T_Key>(key_col_name);
    auto copy_key_duration = df.get_column<unsigned long long>("duration");
    df_key_duration.load_data(manager, std::move(copy_index),
                              std::make_pair(key_col_name, std::move(copy_key_col)),
                              std::make_pair("duration", std::move(copy_key_duration)));

    StdDataFrame<unsigned long long> groupby_key =
        df_key_duration.groupby<GroupbyMedian, T_Key, T_Key, unsigned long long>(
            manager, GroupbyMedian(), key_col_name);

    auto& key_vec      = groupby_key.get_column<T_Key>(key_col_name);
    auto& duration_vec = groupby_key.get_column<unsigned long long>("duration");
    {
        DerefScope scope;
        auto key_it      = key_vec.cfbegin(scope);
        auto duration_it = duration_vec.cfbegin(scope);
        for (unsigned long long i = 0; i < key_vec.size(); ++i, ++key_it, ++duration_it) {
            if (unlikely(i % kNumElementsPerScope == 0)) {
                scope.renew();
                key_it.renew(scope);
                duration_it.renew(scope);
            }
            std::cout << static_cast<int>(*key_it) << " " << *duration_it << std::endl;
        }
    }

    std::cout << std::endl;
}

void do_work(FarMemManager* manager)
{
    std::chrono::time_point<std::chrono::steady_clock> times[10];
    auto df  = load_data(manager);
    times[0] = std::chrono::steady_clock::now();
    print_number_vendor_ids_and_unique(manager, df);
    times[1] = std::chrono::steady_clock::now();
    print_passage_counts_by_vendor_id(manager, df, 1);
    times[2] = std::chrono::steady_clock::now();
    print_passage_counts_by_vendor_id(manager, df, 2);
    times[3] = std::chrono::steady_clock::now();
    calculate_trip_duration(manager, df);
    times[4] = std::chrono::steady_clock::now();
    calculate_distribution_store_and_fwd_flag(manager, df);
    times[5] = std::chrono::steady_clock::now();
    calculate_haversine_distance_column(manager, df);
    times[6] = std::chrono::steady_clock::now();
    analyze_trip_timestamp(manager, df);
    times[7] = std::chrono::steady_clock::now();
    analyze_trip_durations_of_timestamps<char>(manager, df, "pickup_day");
    times[8] = std::chrono::steady_clock::now();
    analyze_trip_durations_of_timestamps<char>(manager, df, "pickup_month");
    times[9] = std::chrono::steady_clock::now();

    for (uint32_t i = 1; i < std::size(times); i++) {
        std::cout << "Step " << i << ": "
                  << std::chrono::duration_cast<std::chrono::microseconds>(times[i] - times[i - 1])
                         .count()
                  << " us" << std::endl;
    }
    std::cout << "Total: "
              << std::chrono::duration_cast<std::chrono::microseconds>(times[9] - times[0]).count()
              << " us" << std::endl;
}

void _main(void* arg)
{
    char** argv = (char**)arg;
    std::string ip_addr_port(argv[1]);
    auto raddr = helpers::str_to_netaddr(ip_addr_port);
    std::unique_ptr<FarMemManager> manager =
        std::unique_ptr<FarMemManager>(FarMemManagerFactory::build(
            kCacheSize, kNumGCThreads, new TCPDevice(raddr, kNumConnections, kFarMemSize)));
    do_work(manager.get());
}

int main(int _argc, char* argv[])
{
    int ret;

    if (_argc < 3) {
        std::cerr << "usage: [cfg_file] [ip_addr:port]" << std::endl;
        return -EINVAL;
    }

    char conf_path[strlen(argv[1]) + 1];
    strcpy(conf_path, argv[1]);
    for (int i = 2; i < _argc; i++) {
        argv[i - 1] = argv[i];
    }

    ret = runtime_init(conf_path, _main, argv);
    if (ret) {
        std::cerr << "failed to start runtime" << std::endl;
        return ret;
    }

    return 0;
}
