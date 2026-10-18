/* Minimal stand-in for gr::block so the gr-dtv DVB-T2 implementation files
 * compile and run without the GNU Radio runtime. The t2mod pipeline driver
 * calls forecast()/general_work() directly. (t2mod, GPLv3) */
#pragma once
#include <gnuradio/types.h>
#include <gnuradio/io_signature.h>
#include <gnuradio/math.h>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
namespace gr {
struct logger {
    template <typename... A> void warn(const std::string& s, A&&...) { fprintf(stderr, "WARN: %s\n", s.c_str()); }
    template <typename... A> void error(const std::string& s, A&&...) { fprintf(stderr, "ERROR: %s\n", s.c_str()); }
    template <typename... A> void fatal(const std::string& s, A&&...) { fprintf(stderr, "FATAL: %s\n", s.c_str()); }
    template <typename... A> void info(const std::string& s, A&&...) {}
    template <typename... A> void debug(const std::string& s, A&&...) {}
};
typedef std::shared_ptr<logger> logger_ptr;
class block {
public:
    block(const std::string& name, io_signature::sptr in, io_signature::sptr out)
        : d_logger(std::make_shared<logger>()), d_name(name), d_in(in), d_out(out) {}
    virtual ~block() {}
    virtual void forecast(int noutput_items, gr_vector_int& ninput_items_required) {
        for (auto& n : ninput_items_required) n = noutput_items;
    }
    virtual int general_work(int, gr_vector_int&, gr_vector_const_void_star&, gr_vector_void_star&) { return 0; }
    void set_output_multiple(int m) { d_output_multiple = m; }
    int output_multiple() const { return d_output_multiple; }
    void consume_each(int n) { d_consumed = n; }
    void consume(int, int n) { d_consumed = n; }
    int in_size() const { return d_in->d_size; }
    int out_size() const { return d_out->d_size; }
    logger_ptr d_logger;
    int d_consumed = 0;
protected:
    block() : d_logger(std::make_shared<logger>()) {}
    std::string d_name;
    io_signature::sptr d_in, d_out;
    int d_output_multiple = 1;
};
} // namespace gr
namespace gnuradio {
template <typename T, typename... A> std::shared_ptr<T> make_block_sptr(A&&... a) {
    return std::make_shared<T>(std::forward<A>(a)...);
}
} // namespace gnuradio
