#pragma once
#include <memory>
namespace gr {
class io_signature {
public:
    typedef std::shared_ptr<io_signature> sptr;
    static sptr make(int, int, int size) { auto s = std::make_shared<io_signature>(); s->d_size = size; return s; }
    int sizeof_stream_item(int) const { return d_size; }
    int d_size = 0;
};
} // namespace gr
