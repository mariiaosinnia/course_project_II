#include <exception>
#include <stdexcept>

#include <boost/config.hpp>
#include <boost/throw_exception.hpp>

namespace boost {

void throw_exception(std::exception const& e)
{
    throw e;
}

void throw_exception(std::exception const& e, boost::source_location const&)
{
    throw e;
}

} // namespace boost
