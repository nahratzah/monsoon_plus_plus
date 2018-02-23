#ifndef MONSOON_OBJPIPE_DETAIL_FWD_H
#define MONSOON_OBJPIPE_DETAIL_FWD_H

///\file
///\ingroup objpipe_detail

namespace monsoon::objpipe::detail {


template<typename Source, typename... Pred>
class filter_op;

template<typename Source, typename... Fn>
class transform_op;

template<typename Source, typename Fn>
class assertion_op;

template<typename Source>
class flatten_op;

template<typename Source>
class adapter_t;


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_FWD_H */
