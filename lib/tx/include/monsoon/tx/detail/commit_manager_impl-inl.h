#ifndef MONSOON_TX_DETAIL_COMMIT_MANAGER_IMPL_INL_H
#define MONSOON_TX_DETAIL_COMMIT_MANAGER_IMPL_INL_H

#include <utility>

namespace monsoon::tx::detail {


inline commit_manager_impl::write_id_state_impl_::write_id_state_impl_(commit_id seq, txfile::transaction&& tx) noexcept
: write_id_state_(std::move(seq), std::move(tx))
{}


} /* namespace monsoon::tx::detail */

#endif /* MONSOON_TX_DETAIL_COMMIT_MANAGER_IMPL_INL_H */
