// Copyright 2019 Oath Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "log_message.h"
#include "exceptions.h"
#include <locale>
#include <sstream>
#include <iostream>

namespace ns_log {

namespace {

std::locale clocale("C");

[[noreturn]] void
bad_tab(const char *tab_name, std::string_view log_line)
{
    std::ostringstream os;
    os << "Bad " << tab_name << " tab: " << log_line;
    throw BadLogLineException(os.str());
}

std::string_view::size_type
find_tab(std::string_view log_line, const char *tab_name, std::string_view::size_type field_pos, bool allowEmpty)
{
    auto tab_pos = log_line.find('\t', field_pos);
    if (tab_pos == std::string_view::npos || (tab_pos == field_pos && !allowEmpty)) {
        bad_tab(tab_name, log_line);
    }
    return tab_pos;
}

int64_t
parse_time_field(std::string time_field)
{
    std::istringstream time_stream(time_field);
    time_stream.imbue(clocale);
    double logtime = 0;
    time_stream >> logtime;
    if (!time_stream.eof()) {
        std::ostringstream os;
        os << "Bad time field: " << time_field;
        throw BadLogLineException(os.str());
    }
    return logtime * 1000000000;
}

struct PidFieldParser
{
    int32_t _process_id;
    int32_t _thread_id;

    PidFieldParser(std::string pid_field);
};

PidFieldParser::PidFieldParser(std::string pid_field)
    : _process_id(0),
      _thread_id(0)
{
    std::istringstream pid_stream(pid_field);
    pid_stream.imbue(clocale);
    pid_stream >> _process_id;
    bool badField = false;
    if (!pid_stream.eof() && pid_stream.good() && pid_stream.peek() == '/') {
        pid_stream.get();
        if (pid_stream.eof()) {
            badField = true;
        } else {
            pid_stream >> _thread_id;
        }
    }
    if (!pid_stream.eof() || pid_stream.fail() || pid_stream.bad() || badField) {
        std::ostringstream os;
        os << "Bad pid field: " << pid_field;
        throw BadLogLineException(os.str());
    }
}

}

LogMessage::LogMessage()
    : _time_nanos(0),
      _hostname(),
      _process_id(0),
      _thread_id(0),
      _service(),
      _component(),
      _level(Logger::LogLevel::NUM_LOGLEVELS),
      _payload()
{
}

LogMessage::~LogMessage() = default;


/* 
 * Parse log line to populate log message class. The parsing is based on
 * LegacyForwarder in logd.
 */
void
LogMessage::parse_log_line(std::string_view log_line)
{
    // time
    auto tab_pos = find_tab(log_line, "1st", 0, false);
    _time_nanos = parse_time_field(std::string(log_line.substr(0, tab_pos)));

    // hostname
    auto field_pos = tab_pos + 1;
    tab_pos = find_tab(log_line, "2nd", field_pos, true);
    _hostname = log_line.substr(field_pos, tab_pos - field_pos);

    // pid
    field_pos = tab_pos + 1;
    tab_pos = find_tab(log_line, "3rd", field_pos, false);
    PidFieldParser pid_field_parser(std::string(log_line.substr(field_pos, tab_pos - field_pos)));
    _process_id = pid_field_parser._process_id;
    _thread_id = pid_field_parser._thread_id;

    // service
    field_pos = tab_pos + 1;
    tab_pos = find_tab(log_line, "4th", field_pos, true);
    _service = log_line.substr(field_pos, tab_pos - field_pos);

    // component
    field_pos = tab_pos + 1;
    tab_pos = find_tab(log_line, "5th", field_pos, false);
    _component = log_line.substr(field_pos, tab_pos -field_pos);

    // level
    field_pos = tab_pos + 1;
    tab_pos = find_tab(log_line, "6th", field_pos, false);
    std::string level_string(log_line.substr(field_pos, tab_pos - field_pos));
    _level = Logger::parseLevel(level_string.c_str());
    _payload = log_line.substr(tab_pos + 1);
}

}
