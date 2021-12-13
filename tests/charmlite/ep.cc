#include "ep.hh"

#include "collective.hh"

namespace cmk {
entry_table_t entry_table_;
chare_table_t chare_table_;
message_table_t message_table_;
collective_kinds_t collective_kinds_;
collective_table_t collective_table_;
collective_buffer_t collective_buffer_;
std::uint32_t local_collective_count_ = 0;
}  // namespace cmk