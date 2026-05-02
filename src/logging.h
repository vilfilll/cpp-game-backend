#pragma once

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/attributes.hpp>
#include <boost/json.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iomanip>
#include <sstream>

namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace json = boost::json;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

inline std::string FormatTimestamp(const boost::posix_time::ptime& ts) {
    using namespace boost::posix_time;

    const auto date = ts.date();
    const auto tod = ts.time_of_day();

    std::ostringstream ss;

    ss << date.year() << "-";
    ss << std::setw(2) << std::setfill('0') << date.month().as_number() << "-";
    ss << std::setw(2) << std::setfill('0') << date.day() << "T";

    ss << std::setw(2) << tod.hours() << ":";
    ss << std::setw(2) << tod.minutes() << ":";
    ss << std::setw(2) << tod.seconds() << ".";

    ss << std::setw(6) << std::setfill('0') << tod.fractional_seconds();

    return ss.str();
}

inline void InitLogging() {
    logging::add_common_attributes();

    logging::add_console_log(
        std::cout,
        logging::keywords::auto_flush = true,
        logging::keywords::format =
            [](logging::record_view const& rec,
                               logging::formatting_ostream& strm) {

                json::object obj;

                if (auto ts = rec[timestamp]) {
                    obj["timestamp"] =
                        FormatTimestamp(*ts);
                }

                obj["message"] = rec[expr::smessage].get();

                if (auto data = rec[additional_data]) {
                    obj["data"] = *data;
                } else {
                    obj["data"] = json::object{};
                }

                strm << json::serialize(obj);
            }
    );

}
