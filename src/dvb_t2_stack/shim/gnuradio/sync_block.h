#pragma once
#include <gnuradio/block.h>
namespace gr {
class sync_block : public block {
public:
    sync_block(const std::string& name, io_signature::sptr in, io_signature::sptr out) : block(name, in, out) {}
    virtual int work(int noutput_items, gr_vector_const_void_star& in, gr_vector_void_star& out) = 0;
    int general_work(int n, gr_vector_int&, gr_vector_const_void_star& in, gr_vector_void_star& out) override {
        int r = work(n, in, out);
        consume_each(r);
        return r;
    }
protected:
    sync_block() {}
};
} // namespace gr
