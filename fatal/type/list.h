/*
 *  Copyright (c) 2014, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 */

#pragma once

#include <fatal/type/pair.h>
#include <fatal/type/tag.h>
#include <fatal/type/traits.h>

#include <limits>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace ftl {

/**
 * READ ME FIRST: You probably want to jump to
 * the line that says `type_list IMPLEMENTATION`
 */

///////////////////////////
// type_list DECLARATION //
///////////////////////////

template <typename...> struct type_list;

/////////////////////
// SUPPORT LIBRARY //
/////////////////////

//////////////
// type_get //
//////////////

/**
 * Specialization of `type_get_traits` so that `type_get` supports `type_list`.
 *
 * @author: Marcelo Juchem <marcelo@fb.com>
 */
template <std::size_t Index, typename... Args>
struct type_get_traits<type_list<Args...>, Index> {
  typedef typename type_list<Args...>::template at<Index> type;
};


////////////////////
// type_list_from //
////////////////////

/**
 * A convenience `type_list` builder based on transforms.
 * See members below for more info.
 *
 * @author: Marcelo Juchem <marcelo@fb.com>
 */
template <template <typename...> class... TTransforms>
struct type_list_from {
  /**
   * Creates a `type_list` from a given type `T` by applying a series of
   * transforms `TTransforms`.
   *
   * The resulting list will be comprised of the result of the transforms when
   * applied to `T`, in the order they are given.
   *
   * Example:
   *
   *  template <typename TFirst, typename TSecond, typename TThird>
   *  struct Foo {
   *    using first = TFirst;
   *    using second = TThird;
   *    using third = TThird;
   *  };
   *
   *  using foo = Foo<int, void, double>;
   *
   *  template <typename T> using get_first = typename T::first;
   *  template <typename T> using get_second = typename T::second;
   *  template <typename T> using get_third = typename T::third;
   *
   *  // yields `type_list<int, void, double>`
   *  using result = type_list_from<get_first, get_second, get_third>::type<foo>;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename T>
  using type = type_list<TTransforms<T>...>;
};

/**
 * A convenience comparer implementation for `binary_search()` that compares a
 * variable against an `std::integral_constant`-like type.
 *
 * Returns `-1`, `0` or `1` when `lhs` is, respectively, less than, equal to or
 * greater than `RHS`.
 *
 * Example:
 *
 *  // `Index` is not important for this example, using `0`.
 *  template <int N>
 *  using rhs = indexed_type_tag<std::integral_constant<int, N>, 0>;
 *
 *  // yields `-1`
 *  type_value_comparer::compare(5, rhs<7>{});
 *
 *  // yields `0`
 *  type_value_comparer::compare(5, rhs<5>{});
 *
 *  // yields `1`
 *  type_value_comparer::compare(5, rhs<3>{});
 *
 * @author: Marcelo Juchem <marcelo@fb.com>
 */
struct type_value_comparer {
  template <typename TLHS, typename TRHS, std::size_t Index>
  static constexpr int compare(TLHS &&lhs, indexed_type_tag<TRHS, Index>) {
    return lhs < TRHS::value
      ? -1
      : TRHS::value < lhs
        ? 1
        : 0;
  }
};

/**
 * A convenience comparer implementation for `binary_search()` that compares a
 * variable against the `indexed_type_tag`'s `Index`.
 *
 * Returns `-1`, `0` or `1` when `lhs` is, respectively, less than, equal to or
 * greater than `RHS`.
 *
 * Example:
 *
 *  // `T` is not important for this example, using `void`.
 *  template <std::size_t Index>
 *  using rhs = indexed_type_tag<void, Index>;
 *
 *  // yields `-1`
 *  type_value_comparer::compare(5, rhs<7>{});
 *
 *  // yields `0`
 *  type_value_comparer::compare(5, rhs<5>{});
 *
 *  // yields `1`
 *  type_value_comparer::compare(5, rhs<3>{});
 *
 * @author: Marcelo Juchem <marcelo@fb.com>
 */
struct index_value_comparer {
  template <typename TLHS, typename TRHS, std::size_t Index>
  static constexpr int compare(TLHS &&lhs, indexed_type_tag<TRHS, Index>) {
    return lhs < Index
      ? -1
      : Index < lhs
        ? 1
        : 0;
  }
};

/**
 * A convenience visitor that accpets any parameters and does nothing.
 *
 * @author: Marcelo Juchem <marcelo@fb.com>
 */
struct no_op_visitor {
  template <typename... UArgs>
  void operator ()(UArgs &&...) {}
};

////////////////////////////
// IMPLEMENTATION DETAILS //
////////////////////////////

namespace detail {
namespace type_list_impl {

////////
// at //
////////

template <std::size_t, typename...> struct at;

template <std::size_t Distance, typename U, typename... UArgs>
struct at<Distance, U, UArgs...> {
  typedef typename at<Distance - 1, UArgs...>::type type;
};

template <typename U, typename... UArgs>
struct at<0, U, UArgs...> { typedef U type; };

////////////
// try_at //
////////////

template <std::size_t, typename...> struct try_at;

template <std::size_t Distance>
struct try_at<Distance> {
  static_assert(Distance == 0, "index out of bounds");
  typedef type_list<> type;
};

template <std::size_t Distance, typename U, typename... UArgs>
struct try_at<Distance, U, UArgs...>:
  public std::conditional<
    Distance == 0,
    type_list<U>,
    typename try_at<Distance ? Distance - 1 : 0, UArgs...>::type
  >
{};

//////////////
// index_of //
//////////////

template <std::size_t, typename, typename...> struct index_of;

template <std::size_t Index, typename T>
struct index_of<Index, T>:
  public std::integral_constant<std::size_t, Index>
{};

template <std::size_t Index, typename T, typename U, typename... UArgs>
struct index_of<Index, T, U, UArgs...>:
  public std::conditional<
    std::is_same<T, U>::value,
    std::integral_constant<std::size_t, Index>,
    index_of<Index + 1, T, UArgs...>
  >::type
{};

/////////////
// type_at //
/////////////

template <typename...> struct type_at;

template <typename U> struct type_at<U> {
  static constexpr std::type_info const &at(std::size_t) {
    return typeid(U);
  }
};

template <typename U, typename... UArgs> struct type_at<U, UArgs...> {
  static constexpr std::type_info const &at(std::size_t index) {
    return index ? type_at<UArgs...>::at(index - 1) : type_at<U>::at(index);
  }
};

//////////////
// contains //
//////////////

template <typename, typename...>
struct contains {
  typedef std::false_type type;
};

template <typename T, typename THead, typename... Args>
struct contains<T, THead, Args...> {
  typedef typename std::conditional<
    std::is_same<T, THead>::value,
    std::true_type,
    typename contains<T, Args...>::type
  >::type type;
};

///////////////////////
// indexed_transform //
///////////////////////

template <
  template <typename, std::size_t, typename...> class,
  typename TList, std::size_t
>
struct indexed_transform_helper{
  static_assert(std::is_same<type_list<>, TList>::value, "unexpected");
  typedef type_list<> type;
};

template <
  template <typename, std::size_t, typename...> class T,
  typename U, std::size_t Index
>
struct indexed_transform_helper<T, type_list<U>, Index> {
  typedef type_list<T<U, Index>> type;
};

template <
  template <typename, std::size_t, typename...> class T,
  typename TList, std::size_t Position = TList::size
>
class indexed_transform{
  static_assert(Position > 0, "unexpected");
  typedef typename indexed_transform<
    T, TList, Position - 1
  >::type previous;
  typedef typename indexed_transform_helper<
    T, typename TList::template try_at<Position - 1>, Position - 1
  >::type current;
public:
  typedef typename previous::template concat<current> type;
};

template <
  template <typename, std::size_t, typename...> class T, typename TList
>
struct indexed_transform<T, TList, 0> { typedef type_list<> type; };

////////////////
// foreach_if //
////////////////

template <bool>
struct conditional_visit {
  template <typename V, typename... VArgs>
  static constexpr std::size_t visit(V &&, VArgs &&...) { return 0; }
};

template <>
struct conditional_visit<true> {
  template <typename V, typename... VArgs>
  static constexpr std::size_t visit(V &&visitor, VArgs &&...args) {
    // hack to accommodate C++11's constexpr constraints
    return visitor(std::forward<VArgs>(args)...), 1;
  }
};

template <
  template <typename...> class TCondition, std::size_t Index, typename...
>
struct foreach_if {
  template <typename V, typename... VArgs>
  static constexpr std::size_t visit(V &&, VArgs &&...) { return 0; }
};

template <
  template <typename...> class TCondition,
  std::size_t Index, typename U, typename... UArgs
>
struct foreach_if<TCondition, Index, U, UArgs...> {
  template <typename V, typename... VArgs>
  static constexpr std::size_t visit(V &&visitor, VArgs &&...args) {
    return conditional_visit<TCondition<U>::value>::visit(
      std::forward<V>(visitor),
      indexed_type_tag<U, Index>(),
      std::forward<VArgs>(args)...
    ) + foreach_if<TCondition, Index + 1, UArgs...>::visit(
      std::forward<V>(visitor),
      std::forward<VArgs>(args)...
    );
  }
};

///////////
// visit //
///////////

template <std::size_t Index, typename... Args> struct visit;

template <std::size_t Index, typename T, typename... Args>
struct visit<Index, T, Args...> {
  template <typename V, typename... VArgs>
  static constexpr bool at(
    fast_pass<std::size_t> index,
    V &&visitor,
    VArgs &&...args
  ) {
    return index == Index
      ? (
        visitor(
          indexed_type_tag<T, Index>(),
          std::forward<VArgs>(args)...
        ),
        true
      )
      : visit<Index + 1, Args...>::at(
        index,
        std::forward<V>(visitor),
        std::forward<VArgs>(args)...
      );
  }
};

template <std::size_t Index>
struct visit<Index> {
  template <typename V, typename... VArgs>
  static constexpr bool at(fast_pass<std::size_t>, V &&, VArgs &&...) {
    return false;
  }
};

//////////
// left //
//////////

template <std::size_t, typename...> struct left;

template <typename... UArgs>
struct left<0, UArgs...> { typedef type_list<> type; };

template <typename U,typename... UArgs>
struct left<0, U, UArgs...> { typedef type_list<> type; };

template <std::size_t Size, typename U, typename... UArgs>
struct left<Size, U, UArgs...> {
  static_assert(Size <= sizeof...(UArgs) + 1, "index out of bounds");
  typedef typename left<
    Size - 1, UArgs...
  >::type::template push_front<U> type;
};

///////////
// slice //
///////////

template <std::size_t, std::size_t, typename...> struct slice;

template <std::size_t End, typename... UArgs>
struct slice<0, End, UArgs...> {
  typedef typename left<End, UArgs...>::type type;
};

template <std::size_t End, typename U, typename... UArgs>
struct slice<0, End, U, UArgs...> {
  typedef typename left<End, U, UArgs...>::type type;
};

template <std::size_t Begin, std::size_t End, typename U, typename... UArgs>
struct slice<Begin, End, U, UArgs...> {
  static_assert(End >= Begin, "invalid range");
  typedef typename slice<Begin - 1, End - 1, UArgs...>::type type;
};

///////////
// right //
///////////

template <std::size_t Size, typename... UArgs>
struct right {
  static_assert(Size <= sizeof...(UArgs), "index out of bounds");
  typedef typename slice<
    sizeof...(UArgs) - Size, sizeof...(UArgs), UArgs...
  >::type type;
};

////////////
// filter //
////////////

template <template <typename...> class, typename...> struct filter;

template <template <typename...> class TFilter>
struct filter<TFilter> {
  typedef type_pair<type_list<>, type_list<>> type;
};

template <
  template <typename...> class TFilter,
  typename U, typename... UArgs
>
struct filter<TFilter, U, UArgs...> {
  typedef typename filter<TFilter, UArgs...>::type tail;

  typedef typename std::conditional<
    TFilter<U>::value,
    type_pair<
      typename tail::first::template push_front<U>,
      typename tail::second
    >,
    type_pair<
      typename tail::first,
      typename tail::second::template push_front<U>
    >
  >::type type;
};

/////////
// zip //
/////////

template <typename, std::size_t, typename...> struct zip;

template <typename TRightList, std::size_t Index>
struct zip<TRightList, Index> {
  typedef typename TRightList::template slice<Index, TRightList::size> type;
};

template <
  typename TRightList, std::size_t Index, typename U, typename... ULeftArgs
>
struct zip<TRightList, Index, U, ULeftArgs...> {
  typedef typename std::conditional<
    Index == TRightList::size,
    type_list<U, ULeftArgs...>,
    typename TRightList::template try_at<Index>
      ::template push_front<U>
      ::template concat<
        typename zip<
          TRightList,
          Index == TRightList::size ? Index : Index + 1,
          ULeftArgs...
        >::type
      >
  >::type type;
};

//////////
// skip //
//////////

template <std::size_t, std::size_t, typename...> struct skip;

template <std::size_t Next, std::size_t Step>
struct skip<Next, Step> { typedef type_list<> type; };

template <std::size_t Next, std::size_t Step, typename U, typename... UArgs>
struct skip<Next, Step, U, UArgs...> {
  typedef typename skip<
    Next == 0 ? Step : Next - 1, Step, UArgs...
  >::type tail;

  typedef typename std::conditional<
    Next == 0,
    typename tail::template push_front<U>,
    tail
  >::type type;
};

template <std::size_t Step>
struct curried_skip{
  template <typename... UArgs>
  using type = skip<0, Step, UArgs...>;
};

////////////
// search //
////////////

template <template <typename...> class, typename, typename...>
struct search;

template <template <typename...> class TFilter, typename TDefault>
struct search<TFilter, TDefault> { typedef TDefault type; };

template <
  template <typename...> class TFilter,
  typename TDefault, typename U, typename... UArgs
>
struct search<TFilter, TDefault, U, UArgs...> {
  typedef typename std::conditional<
    TFilter<U>::value,
    U,
    typename search<TFilter, TDefault, UArgs...>::type
  >::type type;
};

/////////////
// combine //
/////////////

template <
  typename,
  std::size_t,
  template <typename...> class,
  typename...
> struct combine;

template <
  typename TRightList,
  std::size_t Index,
  template <typename...> class TCombiner
>
struct combine<TRightList, Index, TCombiner> {
  static_assert(
    Index == TRightList::size,
    "right type list is larger than left one"
  );
  typedef type_list<> type;
};

template <
  typename TRightList,
  std::size_t Index,
  template <typename...> class TCombiner,
  typename U,
  typename... UArgs
>
struct combine<TRightList, Index, TCombiner, U, UArgs...> {
  static_assert(
    Index < TRightList::size,
    "left type list is larger than right one"
  );
  typedef typename combine<
    TRightList, Index + 1, TCombiner, UArgs...
  >::type::template push_front<
    TCombiner<U, typename TRightList::template at<Index>>
  > type;
};

/////////////
// flatten //
/////////////

template <std::size_t Depth, std::size_t MaxDepth, typename... Args>
struct flatten;

template <std::size_t Depth, std::size_t MaxDepth, typename T, typename... Args>
struct flatten<Depth, MaxDepth, T, Args...> {
  using type = typename std::conditional<
    Depth == MaxDepth,
    type_list<T, Args...>,
    typename flatten<
      Depth, MaxDepth, Args...
    >::type::template push_front<T>
  >::type;
};

template <std::size_t Depth, std::size_t MaxDepth>
struct flatten<Depth, MaxDepth> {
  using type = type_list<>;
};

template <
  std::size_t Depth, std::size_t MaxDepth, typename... Args, typename... UArgs
>
struct flatten<Depth, MaxDepth, type_list<Args...>, UArgs...> {
  using type = typename std::conditional<
    Depth == MaxDepth,
    type_list<type_list<Args...>, UArgs...>,
    typename flatten<
      Depth + 1, MaxDepth, Args...
    >::type::template concat<
      typename flatten<Depth, MaxDepth, UArgs...>::type
    >
  >::type;
};

///////////////////
// insert_sorted //
///////////////////

template <template <typename...> class, typename, typename...>
struct insert_sorted;

template <template <typename...> class TLessComparer, typename T>
struct insert_sorted<TLessComparer, T> { typedef type_list<T> type; };

template <
  template <typename...> class TLessComparer,
  typename T,
  typename THead,
  typename... TTail
>
struct insert_sorted<TLessComparer, T, THead, TTail...> {
  typedef typename std::conditional<
    TLessComparer<T, THead>::value,
    type_list<T, THead, TTail...>,
    typename insert_sorted<
      TLessComparer, T, TTail...
    >::type::template push_front<THead>
  >::type type;
};

///////////////////////
// replace_transform //
///////////////////////

template <typename TFrom, typename TTo>
struct replace_transform {
  template <typename U>
  using type = typename std::conditional<
    std::is_same<U, TFrom>::value, TTo, U
  >::type;
};

//////////
// tail //
//////////

template <std::size_t, typename...> struct tail;

template <> struct tail<0> { typedef type_list<> type; };

template <typename T, typename... Args>
struct tail<0, T, Args...> { typedef type_list<T, Args...> type; };

template <std::size_t Index, typename T, typename... Args>
struct tail<Index, T, Args...> {
  static_assert(Index <= sizeof...(Args) + 1, "index out of bounds");
  typedef typename tail<Index - 1, Args...>::type type;
};

///////////
// split //
///////////

template <std::size_t, typename...> struct split;

template <>
struct split<0> { typedef type_pair<type_list<>, type_list<>> type; };

template <typename T, typename... Args>
struct split<0, T, Args...> {
  typedef type_pair<type_list<>, type_list<T, Args...>> type;
};

template <std::size_t Index, typename T, typename... Args>
struct split<Index, T, Args...> {
  typedef split<Index - 1, Args...> tail;

public:
  typedef type_pair<
    typename tail::type::first::template push_front<T>,
    typename tail::type::second
  > type;
};

///////////////
// is_sorted //
///////////////

template <template <typename...> class, typename...>
struct is_sorted: public std::true_type {};

template <
  template <typename...> class TLessComparer,
  typename TLHS, typename TRHS, typename... Args
>
struct is_sorted<TLessComparer, TLHS, TRHS, Args...>:
  public logical_and_constants<
    negate_constant<TLessComparer<TRHS, TLHS>>,
    is_sorted<TLessComparer, TRHS, Args...>
  >
{};

///////////
// merge //
///////////

template <template <typename...> class, typename, typename...> struct merge;

template <template <typename...> class TLessComparer>
struct merge<TLessComparer, type_list<>> { typedef type_list<> type; };

template <template <typename...> class TLessComparer, typename TRHSList>
struct merge<TLessComparer, TRHSList> { typedef TRHSList type; };

template <
  template <typename...> class TLessComparer,
  typename TLHS, typename... TLHSArgs
>
struct merge<TLessComparer, type_list<>, TLHS, TLHSArgs...> {
  typedef type_list<TLHS, TLHSArgs...> type;
};

template <
  template <typename...> class TLessComparer,
  typename TRHSList, typename TLHS, typename... TLHSArgs
>
struct merge<TLessComparer, TRHSList, TLHS, TLHSArgs...> {
  typedef typename TRHSList::template at<0> rhs;
  typedef TLessComparer<rhs, TLHS> right_merge;
  typedef typename std::conditional<right_merge::value, rhs, TLHS>::type head;
  typedef typename std::conditional<
    right_merge::value,
    merge<
      TLessComparer, typename TRHSList::template tail<1>, TLHS, TLHSArgs...
    >,
    merge<TLessComparer, TRHSList, TLHSArgs...>
  >::type::type tail;

public:
  typedef typename tail::template push_front<head> type;
};

template <
  template <typename...> class TLessComparer,
  typename TRHSList, typename... TLHSArgs
>
struct merge_entry_point {
#   ifndef NDEBUG
  // debug only due to compilation times
  static_assert(
    type_list<TLHSArgs...>::template is_sorted<TLessComparer>::value,
    "left hand side list is not sorted"
  );

  static_assert(
    TRHSList::template is_sorted<TLessComparer>::value,
    "right hand side list is not sorted"
  );
#   endif // NDEBUG
  typedef typename merge<TLessComparer, TRHSList, TLHSArgs...>::type type;
};

////////////////
// merge_sort //
////////////////

template <template <typename...> class TLessComparer, typename TList>
struct merge_sort {
  typedef typename TList::template split<TList::size / 2> unsorted;
  typedef typename merge_sort<
    TLessComparer, typename unsorted::first
  >::type sorted_left;
  typedef typename merge_sort<
    TLessComparer, typename unsorted::second
  >::type sorted_right;

public:
  static_assert(TList::size > 1, "invalid specialization");
  typedef typename sorted_left::template merge<
    sorted_right, TLessComparer
  > type;
};

template <template <typename...> class TLessComparer>
struct merge_sort<TLessComparer, type_list<>> {
  typedef type_list<> type;
};

template <template <typename...> class TLessComparer, typename T>
struct merge_sort<TLessComparer, type_list<T>> {
  typedef type_list<T> type;
};

////////////
// unique //
////////////

template <typename, typename...> struct unique;
template <typename TResult> struct unique<TResult> { using type = TResult; };

template <typename TResult, typename T, typename... Args>
struct unique<TResult, T, Args...> {
  using type = typename unique<
    typename std::conditional<
      TResult::template contains<T>::value,
      TResult,
      typename TResult::template push_back<T>
    >::type,
    Args...
  >::type;
};

/////////////////////////
// binary_search_exact //
/////////////////////////

template <typename... Args>
struct binary_search_exact {
  typedef type_list<Args...> list;

  typedef typename list:: template split<list::size / 2> split;
  static_assert(!split::second::empty, "invalid specialization");

  typedef typename split::first left;
  typedef typename split::second::template tail<1> right;

  template <std::size_t Offset> using pivot = indexed_type_tag<
    typename split::second::template at<0>,
    Offset + left::size
  >;

  template <
    typename TComparer, std::size_t Offset, typename TComparison,
    typename TNeedle, typename TVisitor, typename... VArgs
  >
  static constexpr bool impl(
    TComparison &&comparison, TNeedle &&needle,
    TVisitor &&visitor, VArgs &&...args
  ) {
    // ternary needed due to C++11's constexpr restrictions
    return comparison < 0
      ? left::template apply<
          ::ftl::detail::type_list_impl::binary_search_exact
        >::template search<TComparer, Offset>(
          std::forward<TNeedle>(needle),
          std::forward<TVisitor>(visitor),
          std::forward<VArgs>(args)...
        )
      : (0 < comparison
        ? right::template apply<
            ::ftl::detail::type_list_impl::binary_search_exact
          >::template search<TComparer, Offset + left::size + 1>(
            std::forward<TNeedle>(needle),
            std::forward<TVisitor>(visitor),
            std::forward<VArgs>(args)...
          )
        : (
          // comma operator needed due to C++11's constexpr restrictions
          visitor(
            pivot<Offset>{},
            std::forward<TNeedle>(needle),
            std::forward<VArgs>(args)...
          ), true
        )
      );
  }

  template <
    typename TComparer, std::size_t Offset,
    typename TNeedle, typename TVisitor, typename... VArgs
  >
  static constexpr bool search(
    TNeedle &&needle, TVisitor &&visitor, VArgs &&...args
  ) {
    return impl<TComparer, Offset>(
      TComparer::compare(std::forward<TNeedle>(needle), pivot<Offset>{}),
      std::forward<TNeedle>(needle),
      std::forward<TVisitor>(visitor),
      std::forward<VArgs>(args)...
    );
  }
};

template <>
struct binary_search_exact<> {
  template <
    typename, std::size_t,
    typename TNeedle, typename TVisitor, typename... VArgs
  >
  static constexpr bool search(TNeedle &&, TVisitor &&, VArgs &&...) {
    return false;
  }
};

///////////////////////////////
// binary_search_lower_bound //
///////////////////////////////

template <typename... Args>
struct binary_search_lower_bound {
  typedef type_list<Args...> list;
  typedef typename list:: template split<list::size / 2> split;
  typedef typename split::first left;
  typedef typename split::second right;
  template <std::size_t Offset> using pivot = indexed_type_tag<
    typename right::template at<0>,
    Offset + left::size
  >;

  template <
    typename TComparer, std::size_t Offset,
    typename TNeedle, typename TVisitor, typename... VArgs
  >
  static constexpr bool recursion(
    TNeedle &&needle, TVisitor &&visitor, VArgs &&...args
  ) {
    // ternary needed due to C++11's constexpr restrictions
    return TComparer::compare(
      std::forward<TNeedle>(needle), pivot<Offset>{}
    ) < 0
      ? left::template apply<
          ::ftl::detail::type_list_impl::binary_search_lower_bound
        >::template recursion<TComparer, Offset>(
          std::forward<TNeedle>(needle),
          std::forward<TVisitor>(visitor),
          std::forward<VArgs>(args)...
        )
      : right::template apply<
          ::ftl::detail::type_list_impl::binary_search_lower_bound
        >::template recursion<TComparer, Offset + left::size>(
          std::forward<TNeedle>(needle),
          std::forward<TVisitor>(visitor),
          std::forward<VArgs>(args)...
        );
  }

  template <
    typename TComparer, typename TNeedle, typename TVisitor, typename... VArgs
  >
  static constexpr bool search(
    TNeedle &&needle, TVisitor &&visitor, VArgs &&...args
  ) {
    typedef indexed_type_tag<typename list::template at<0>, 0> first;
    return TComparer::compare(std::forward<TNeedle>(needle), first{}) >= 0
      && recursion<TComparer, 0>(
        std::forward<TNeedle>(needle),
        std::forward<TVisitor>(visitor),
        std::forward<VArgs>(args)...
      );
  }
};

template <typename T>
struct binary_search_lower_bound<T> {
  template <
    typename TComparer, std::size_t Offset,
    typename TNeedle, typename TVisitor, typename... VArgs
  >
  static constexpr bool recursion(
    TNeedle &&needle, TVisitor &&visitor, VArgs &&...args
  ) {
    // comma operator needed due to C++11's constexpr restrictions
    return visitor(
      indexed_type_tag<T, Offset>{},
      std::forward<TNeedle>(needle),
      std::forward<VArgs>(args)...
    ), true;
  }

  template <
    typename TComparer, typename TNeedle, typename TVisitor, typename... VArgs
  >
  static constexpr bool search(
    TNeedle &&needle, TVisitor &&visitor, VArgs &&...args
  ) {
    return TComparer::compare(
      std::forward<TNeedle>(needle),
      indexed_type_tag<T, 0>{}
    ) >= 0 && recursion<TComparer, 0>(
      std::forward<TNeedle>(needle),
      std::forward<TVisitor>(visitor),
      std::forward<VArgs>(args)...
    );
  }
};

template <>
struct binary_search_lower_bound<> {
  template <
    typename, std::size_t,
    typename TNeedle, typename TVisitor, typename... VArgs
  >
  static constexpr bool recursion(TNeedle &&, TVisitor &&, VArgs &&...) {
    return false;
  }

  template <typename, typename TNeedle, typename TVisitor, typename... VArgs>
  static constexpr bool search(TNeedle &&, TVisitor &&, VArgs &&...) {
    return false;
  }
};

///////////////////////////////
// binary_search_upper_bound //
///////////////////////////////

template <typename... Args>
struct binary_search_upper_bound {
  typedef type_list<Args...> list;
  typedef typename list:: template split<(list::size + 1) / 2> split;
  typedef typename split::first left;
  typedef typename split::second right;
  template <std::size_t Offset> using pivot = indexed_type_tag<
    typename left::template at<left::size - 1>,
    Offset + (left::size - 1)
  >;

  template <
    typename TComparer, std::size_t Offset,
    typename TNeedle, typename TVisitor, typename... VArgs
  >
  static constexpr bool search(
    TNeedle &&needle, TVisitor &&visitor, VArgs &&...args
  ) {
    // ternary needed due to C++11's constexpr restrictions
    return TComparer::compare(
      std::forward<TNeedle>(needle),
      pivot<Offset>{}
    ) < 0
      ? left::template apply<
          ::ftl::detail::type_list_impl::binary_search_upper_bound
        >::template search<TComparer, Offset>(
          std::forward<TNeedle>(needle),
          std::forward<TVisitor>(visitor),
          std::forward<VArgs>(args)...
        )
      : right::template apply<
          ::ftl::detail::type_list_impl::binary_search_upper_bound
        >::template search<TComparer, Offset + left::size>(
          std::forward<TNeedle>(needle),
          std::forward<TVisitor>(visitor),
          std::forward<VArgs>(args)...
        );
  }
};

template <typename T>
struct binary_search_upper_bound<T> {
  template <
    typename TComparer, std::size_t Offset,
    typename TNeedle, typename TVisitor, typename... VArgs
  >
  static constexpr bool search(
    TNeedle &&needle, TVisitor &&visitor, VArgs &&...args
  ) {
    return TComparer::compare(
      std::forward<TNeedle>(needle), indexed_type_tag<T, Offset>{}
    ) < 0 && (
      // comma operator needed due to C++11's constexpr restrictions
      visitor(
        indexed_type_tag<T, Offset>{},
        std::forward<TNeedle>(needle),
        std::forward<VArgs>(args)...
      ), true
    );
  }
};

template <>
struct binary_search_upper_bound<> {
  template <
    typename, std::size_t,
    typename TNeedle, typename TVisitor, typename... VArgs
  >
  static constexpr bool search(TNeedle &&, TVisitor &&, VArgs &&...) {
    return false;
  }
};

} // namespace type_list_impl {
} // namespace detail {

//////////////////////////////
// type_list IMPLEMENTATION //
//////////////////////////////

/**
 * Type list for template metaprogramming.
 *
 * Most operations, unless noted otherwise, are compile-time evaluated.
 *
 * Compile-time operations have no side-effects. I.e.: operations that would
 * mutate the list upon which they are performed actually create a new list.
 *
 * @author: Marcelo Juchem <marcelo@fb.com>
 */
template <typename... Args>
struct type_list {
  /**
   * The size of this list.
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  static constexpr std::size_t size = sizeof...(Args);

  /**
   * A boolean telling whether this list is empty or not.
   * This is the same as `type_list::size == 0`.
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  static constexpr bool empty = sizeof...(Args) == 0;

  /**
   * Returns the type at the given index.
   * Yields a compilation error when not found.
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <std::size_t Index>
  using at = typename std::enable_if<
    !empty,
    typename detail::type_list_impl::at<Index, Args...>::type
  >::type;

  /**
   * Returns a unitary list with the type at the given index, or an empty
   * list if not found.
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <std::size_t Index>
  using try_at = typename detail::type_list_impl::try_at<Index, Args...>::type;

  /**
   * Returns a std::integral_const with the 0-based index of the given type
   * in this list, or type_list::size if not found.
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename T>
  using index_of = detail::type_list_impl::index_of<0, T, Args...>;

  /**
   * RTTI for the type at the given index.
   *
   * No bounds checking performed, returns the last
   * type of the type list when index >= size.
   *
   * Note: this is a runtime facility.
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  static constexpr std::type_info const &type_at(std::size_t index) {
    return detail::type_list_impl::type_at<Args...>::at(index);
  }

  /**
   * Returns a boolean std::integral_constant telling whether this
   * list contains the given type.
   *
   * Example:
   *
   *  typedef type_list<A, B, C> types;
   *
   *  // yields `true`
   *  types::contains<A>::value
   *
   *  // yields `false`
   *  types::contains<D>::value
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename T>
  using contains = typename detail::type_list_impl::contains<T, Args...>::type;

  /**
   * Appends the types `Suffix...` to the end of this list.
   *
   * Example:
   *
   *  typedef type_list<A, B, C> types;
   *
   *  // yields `type_list<A, B, C, D, E>`
   *  typedef types::push_back<D, E> result;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... Suffix>
  using push_back = type_list<Args..., Suffix...>;

  /**
   * Prepends the types `Prefix...` to the start of this list.
   *
   * Example:
   *
   *  typedef type_list<C, D, E> types;
   *
   *  // yields `type_list<A, B, C, D, E>`
   *  typedef types::push_front<A, B> result;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... Prefix>
  using push_front = type_list<Prefix..., Args...>;

  /**
   * Appends the types from the given `type_list` to the end of this one.
   *
   * Example:
   *
   *  typedef type_list<A, B> lhs;
   *  type_list<C, D> rhs;
   *
   *  // yields `type_list<A, B, C, D>`
   *  typedef lhs::concat<rhs> result;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename TList>
  using concat = typename TList::template push_front<Args...>;

  /**
   * Inserts the type `T` in its sorted position from the beginning of
   * the list, according to the order relation defined by `TLessComparer`.
   *
   * `TLessComparer` defaults to `constants_comparison_lt` when omitted.
   *
   * Example:
   *
   *  template <int Value>
   *  using val = std::integral_constant<int, Value>;
   *
   *  typedef type_list<val<0>, val<1>, val<3>> list;
   *
   *  // yields `type_list<val<0>, val<1>, val<2>, val<3>>`
   *  typedef list::insert_sorted<val<2>> result1;
   *
   *  template <typename LHS, typename RHS> struct cmp
   *  using gt = std::integral_constant<bool, (LHS::value > RHS::value)>;
   *
   *  typedef type_list<val<5>, val<3>, val<1>> list;
   *
   *  // yields `type_list<val<5>, val<4>, val<3>, val<1>>`
   *  typedef list::insert_sorted<val<4>, gt> result2;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <
    typename T,
    template <typename...> class TLessComparer = constants_comparison_lt
  >
  using insert_sorted = typename detail::type_list_impl::insert_sorted<
    TLessComparer, T, Args...
  >::type;

  /**
   * Applies the elements of this list to the variadic template `T`. There's
   * an optional transform `TTransform` that can be applied to each element
   * of this list beforehand.
   *
   * When `TTransform` is specified, this is the same as
   *
   *  type_list::transform<TTransform>::apply<T>
   *
   * Example:
   *
   *  typedef type_list<A, B, C> types;
   *
   *  // yields `std::tuple<A, B, C>`
   *  types::apply<std::tuple>
   *
   *  template <typename> struct Foo {};
   *
   *  // yields `std::tuple<Foo<A>, Foo<B>, Foo<C>>`
   *  types::apply<std::tuple, Foo>
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <
    template <typename...> class T,
    template <typename...> class TTransform = ftl::transform::identity
  >
  using apply = T<TTransform<Args>...>;

  /**
   * Uses the std::integral_constant-like class `TGetter` to extract
   * values of type `T` from each of this list's types, then applies
   * them to the variadic template `TTo`. The type `T` is not applied,
   * only the values.
   *
   * The default `TGetter` simply extracts the static
   * constexpr member `value` from the types.
   *
   * Example:
   *
   *  template <int... Values> struct int_sequence;
   *  typedef type_list<
   *    std::integral_constant<int, 0>,
   *    std::integral_constant<int, 1>,
   *    std::integral_constant<int, 2>
   *  > list;
   *  template <typename T>
   *  struct double_getter: public std::integral_constant<int, T::value * 2> {};
   *
   *  // yields `int_sequence<0, 1, 2>`
   *  typedef list::apply_values<int, int_sequence> result1;
   *
   *  // yields `int_sequence<0, 2, 4>`
   *  typedef list::apply_values<int, int_sequence, double_getter> result2;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <
    typename T, template <T...> class TTo,
    template <typename...> class TGetter = ftl::transform::identity
  >
  using apply_values = TTo<TGetter<Args>::value...>;

  /**
   * Uses the std::integral_constant-like class `TGetter` to extract
   * values of type `T` from each of this list's types, then applies
   * them to the variadic template `TTo`, with the type `T` being the
   * first template parameter.
   *
   * The default `TGetter` simply extracts the static
   * constexpr member `value` from the types.
   *
   * Example:
   *
   *  template <typename T, T... Values> struct sequence;
   *  typedef type_list<
   *    std::integral_constant<int, 0>,
   *    std::integral_constant<int, 1>,
   *    std::integral_constant<int, 2>
   *  > list;
   *  template <typename T>
   *  struct double_getter: public std::integral_constant<int, T::value * 2> {};
   *
   *  // yields `sequence<int, 0, 1, 2>`
   *  typedef list::apply_type_values<int, sequence> result1;
   *
   *  // yields `sequence<int, 0, 2, 4>`
   *  typedef list::apply_type_values<int, sequence, double_getter> result2;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <
    typename T, template <typename, T...> class TTo,
    template <typename...> class TGetter = ftl::transform::identity
  >
  using apply_type_values = TTo<T, TGetter<Args>::value...>;

  /**
   * A shorted and cheaper version of
   *
   *  this_list::concat<type_list<Suffix...>>::apply<T>
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <template <typename...> class T, typename... Suffix>
  using apply_back = T<Args..., Suffix...>;

  /**
   * A shorted and cheaper version of
   *
   *  type_list<Prefix...>::concat<this_list>::apply<T>
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <template <typename...> class T, typename... Prefix>
  using apply_front = T<Prefix..., Args...>;

  /**
   * Calls the given visitor for each type in the list that yields true when
   * fed to the given condition `TCondition`.
   *
   * No code for calling the visitor will be generated when the condition is
   * `false`.
   *
   * The first parameter given to the visitor is `indexed_type_tag` with the
   * list's type and its index, followed by a perfect forward of `args`.
   *
   * This function returns the amount of types visited (i.e.: the amount of
   * calls to the visitor).
   *
   * Note: this is a runtime facility.
   *
   * Example:
   *
   *  struct visitor {
   *    template <std::size_t Index>
   *    void operator ()(indexed_type_tag<int, Index>, char const *message) {
   *      std::cout << "overload for int at index " << Index << ": "
   *        << message << std::endl;
   *    }
   *
   *    template <std::size_t Index>
   *    void operator ()(indexed_type_tag<long, Index>, char const *message) {
   *      std::cout << "overload for long at index " << Index << ": "
   *        << message << std::endl;
   *    }
   *
   *    // calling `operator ()` for types other than `int` and `long` would
   *    // yield a compilation error since there are no such overloads
   *  };
   *
   *  typedef type_list<double, float, long, std::string, int> list;
   *
   *  // yields `2` and prints `
   *  //  overload for long at index 2: visited!
   *  //  overload for int at index 4: visited!
   *  // `
   *  list::foreach_if<std::is_integral>(visitor(), "visited!");
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <
    template <typename...> class TCondition, typename V, typename... VArgs
  >
  static constexpr std::size_t foreach_if(V &&visitor, VArgs &&...args) {
    return detail::type_list_impl::foreach_if<TCondition, 0, Args...>::visit(
      std::forward<V>(visitor),
      std::forward<VArgs>(args)...
    );
  };

  /**
   * Calls the given visitor for each type in the list.
   *
   * The first parameter given to the visitor is `indexed_type_tag`
   * with the list's type and its index, followed by a perfect forward
   * of `args`.
   *
   * This function returns `true` if the list is not empty (visitor has been
   * called for all types in the list, in order) or `false` if the list is empty
   * (visitor hasn't beel called).
   *
   * Note: this is a runtime facility.
   *
   * Example:
   *
   *  template <typename> struct t2s;
   *  template <> struct t2s<int> { char const *s() { return "int"; } };
   *  template <> struct t2s<long> { char const *s() { return "long"; } };
   *  template <> struct t2s<double> { char const *s() { return "double"; } };
   *  template <> struct t2s<float> { char const *s() { return "float"; } };
   *  template <> struct t2s<bool> { char const *s() { return "bool"; } };
   *
   *  struct visitor {
   *    template <typename T, std::size_t Index>
   *    void operator ()(indexed_type_tag<T, Index>, char const *message) {
   *      std::cout << "overload for " << t2s<T>::s() << " at index " << Index
   *        << ": " << message << std::endl;
   *    }
   *  };
   *
   *  typedef type_list<double, float, long, bool, int> list;
   *
   *  // yields `true` and prints `
   *  //  overload for double at index 0: visited!
   *  //  overload for float at index 1: visited!
   *  //  overload for long at index 2: visited!
   *  //  overload for bool at index 3: visited!
   *  //  overload for int at index 4: visited!
   *  // `
   *  list::foreach(visitor(), "visited!");
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename V, typename... VArgs>
  static constexpr bool foreach(V &&visitor, VArgs &&...args) {
    return 0 < foreach_if<
      ftl::transform::fixed<std::true_type>::template type
    >(std::forward<V>(visitor), std::forward<VArgs>(args)...);
  };

  /**
   * Calls the given visitor for the type at position `index` in the list.
   *
   * This is a way to obtain a given type from an index that's only known at
   * runtime.
   *
   * The first parameter given to the visitor is `indexed_type_tag`
   * with the list's type and its index, followed by a perfect forward
   * of `args`.
   *
   * This function returns `true` if the list has an element at `index` (visitor
   * has been called) or `false` if the list is shorter than `index` (visitor
   * hasn't beel called)`
   *
   * Note: this is a runtime facility.
   *
   * Example:
   *
   *  template <typename> struct t2s;
   *  template <> struct t2s<int> { char const *s() { return "int"; } };
   *  template <> struct t2s<long> { char const *s() { return "long"; } };
   *  template <> struct t2s<double> { char const *s() { return "double"; } };
   *  template <> struct t2s<float> { char const *s() { return "float"; } };
   *  template <> struct t2s<bool> { char const *s() { return "bool"; } };
   *
   *  struct visitor {
   *    template <typename T, std::size_t Index>
   *    void operator ()(indexed_type_tag<T, Index>, char const *message) {
   *      std::cout << "overload for " << t2s<T>::s() << " at index " << Index
   *        << ": " << message << std::endl;
   *    }
   *  };
   *
   *  typedef type_list<double, float, long, bool, int> list;
   *
   *  // yields `true` and prints `overload for double at index 0: visited!`
   *  list::visit(0, visitor(), "visited!");
   *
   *  // yields `true` and prints `overload for float at index 1: visited!`
   *  list::visit(1, visitor(), "visited!");
   *
   *  // yields `true` and prints `overload for long at index 2: visited!`
   *  list::visit(2, visitor(), "visited!");
   *
   *  // yields `true` and prints `overload for bool at index 3: visited!`
   *  list::visit(3, visitor(), "visited!");
   *
   *  // yields `true` and prints `overload for int at index 4: visited!`
   *  list::visit(4, visitor(), "visited!");
   *
   *  // yields `false`
   *  list::visit(5, visitor(), "visited!");
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename V, typename... VArgs>
  static constexpr bool visit(
    fast_pass<std::size_t> index,
    V &&visitor,
    VArgs &&...args
  ) {
    return detail::type_list_impl::visit<0, Args...>::at(
      index,
      std::forward<V>(visitor),
      std::forward<VArgs>(args)...
    );
  }

  /**
   * Applies T to each element of this list.
   *
   * Example:
   *
   *  template <typename> struct T {};
   *  typedef type_list<A, B, C> types;
   *
   *  // yields `type_list<T<A>, T<B>, T<C>>`
   *  types::transform<T>
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <template <typename...> class T>
  using transform = type_list<T<Args>...>;

  /**
   * Applies T to each element/index pair of this list.
   *
   * Example:
   *
   *  template <typenam, std::size_t> struct T {};
   *  typedef type_list<A, B, C> types;
   *
   *  // yields `type_list<T<A, 0>, T<B, 1>, T<C, 2>>`
   *  types::transform<T>
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <template <typename, std::size_t, typename...> class T>
  using indexed_transform = typename detail::type_list_impl::indexed_transform<
    T, type_list
  >::type;

  /**
   * Replaces all occurences of `TFrom` with `TTo`.
   *
   * Example:
   *
   *  typedef type_list<int, void, int, bool> list;
   *
   *  // yields `type_list<double, void, double, bool>`
   *  typedef list::replace<int, double> result;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename TFrom, typename TTo>
  using replace = transform<
    detail::type_list_impl::replace_transform<TFrom, TTo>::template type
  >;

  /**
   * Returns a list with the rightmost types of this list,
   * starting at the index `Offset`.
   *
   * This is the same as `right<size - Offset>` or `slice<Offse, size>`.
   *
   * Example:
   *
   *  typedef type_list<A, B, C, D> types;
   *
   *  // yields `type_list<B, C, D>`
   *  typedef types::tail<1> result1;
   *
   *  // yields `type_list<A, B, C, D>`
   *  typedef types::tail<0> result2;
   *
   *  // yields `type_list<>`
   *  typedef types::tail<types::size> result3;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <std::size_t Offset>
  using tail = typename detail::type_list_impl::tail<Offset, Args...>::type;

  /**
   * Returns a type pair with two sublists resulting from splitting this
   * list at `Index`.
   *
   * `Index` defaults to `size / 2` which is equivalent to splitting the
   * list in half.
   *
   * This is the same as `type_pair<left<Index>, tail<Index>>`.
   *
   * Example:
   *
   *  typedef type_list<A, B, C, D> types;
   *
   *  // yields `type_pair<type_list<A, B>, type_list<C, D>>`
   *  typedef types::split<2> result1;
   *
   *  // yields `type_pair<type_list<>, type_list<A, B, C, D>>`
   *  typedef types::split<0> result1;
   *
   *  // yields `type_pair<type_list<A, B, C, D>, type_list<>>`
   *  typedef types::split<types::size> result1;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <std::size_t Index = (size / 2)>
  using split = typename detail::type_list_impl::split<Index, Args...>::type;

  /**
   * Returns a list with the `Size` leftmost types of this list.
   *
   * Example:
   *
   *  typedef type_list<A, B, C, D> types;
   *
   *  // yields `type_list<A, B>`
   *  typedef types::left<2> result1;
   *
   *  // yields `type_list<>`
   *  typedef types::left<0> result2;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <std::size_t Size>
  using left = typename detail::type_list_impl::left<Size, Args...>::type;

  /**
   * Returns a sublist with the types whose positional
   * index is in the range [Begin, End).
   *
   * Example:
   *
   *  typedef type_list<A, B, C, D> types;
   *
   *  // yields `type_list<>`
   *  typedef types::slice<2, 0> result1;
   *
   *  // yields `type_list<C>`
   *  typedef types::slice<2, 1> result1;
   *
   *  // yields `type_list<C, D>`
   *  typedef types::slice<2, 2> result1;
   *
   *  // yields `type_list<B, C, D>`
   *  typedef types::slice<1, 3> result2;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <std::size_t Begin, std::size_t End>
  using slice = typename detail::type_list_impl::slice<
    Begin, End, Args...
  >::type;

  /**
   * Returns a list with the `Size` rightmost types of this list.
   *
   * Example:
   *
   *  typedef type_list<A, B, C, D> types;
   *
   *  // yields `type_list<C, D>`
   *  typedef types::right<2> result1;
   *
   *  // yields `type_list<>`
   *  typedef types::right<0> result2;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <std::size_t Size>
  using right = typename detail::type_list_impl::right<Size, Args...>::type;

  /**
   * Returns a pair with two `type_list`s. One (first) with the types that
   * got accepted by the filter and the other (second) with the types that
   * weren't accepted by it.
   *
   * TFilter is a std::integral_constant-like template whose value
   * evaluates to a boolean when fed with an element from this list.
   *
   * Example:
   *
   *  typedef type_list<int, std::string, double, long> types;
   *
   *  // yields `type_pair<
   *  //   type_list<int, long>,
   *  //   type_list<std::string, double>
   *  // >`
   *  typedef types::filter<std::is_integral> filtered;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <template <typename...> class TFilter>
  using filter = typename detail::type_list_impl::filter<
    TFilter, Args...
  >::type;

  /**
   * Removes all occurences of given types from the type list.
   *
   * Example:
   *
   *  typedef type_list<int, bool, int, float, void> types;
   *
   *  // yields `type_list<bool, float>`
   *  typedef types::remove<int, void> result;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... UArgs>
  using remove = typename filter<
    type_list<UArgs...>::template contains
  >::second;

  /**
   * Interleaves the types from both lists.
   *
   * Example:
   *
   *  typedef type_list<E, D, G> l1;
   *  typedef type_list<B, A> l2;
   *
   *  // yields type_list<E, B, D, A, G>
   *  typedef l2::apply<l1::zip> result1;
   *
   *  // yields type_list<B, E, A, D, G>
   *  typedef l1::apply<l2::zip> result2;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... UArgs>
  using zip = typename detail::type_list_impl::zip<
    type_list<UArgs...>, 0, Args...
  >::type;

  /**
   * Returns a type_list with a pattern of types based on their position.
   * Picks types starting at `Offset` and skipping `Step` positions.
   *
   * Example:
   *
   *  typedef type_list<A, B, C, D, E, F, G, H, I> list;
   *
   *  // yields type_list<A, D, G>
   *  typedef list::unzip<2> result;
   *
   *  // yields type_list<C, E, G, I>
   *  typedef list::unzip<1, 2> result;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <std::size_t Step, std::size_t Offset = 0>
  using unzip = typename tail<(Offset < size) ? Offset : size>::template apply<
    detail::type_list_impl::curried_skip<Step>::template type
  >::type;

  /**
   * Searches for the first type that satisfiest the given filter.
   *
   * `TFilter` is a std::integral_constant-like template whose value tells
   * whether a given type satisfies the match or not.
   *
   * If there's no type in this list that satisfies the filter, then `TDefault`
   * is returned (defaults to `type_not_found_tag` when omitted).
   *
   * Example:
   *
   *  typedef type_list<int, std::string, double, long> types;
   *
   *  // yields `int`
   *  typedef types::search<std::is_integral, void> result1;
   *
   *  // yields `void`
   *  typedef types::search<std::is_unsigned, void> result2;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <
    template <typename...> class TFilter,
    typename TDefault = type_not_found_tag
  >
  using search = typename detail::type_list_impl::search<
    TFilter, TDefault, Args...
  >::type;

  /**
   * Combines two type lists of equal size into a single type list using the
   * `TCombiner` template.
   *
   * Example:
   *
   *  typedef type_list<A, B, C> left;
   *  typedef type_list<D, E, F> right;
   *  template <typename, typename> struct combiner {};
   *
   *  // yields `type_list<combiner<A, D>, combiner<B, E>, combiner<C, F>>`
   *  typedef left::combine<right, combiner> result;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename TRightList, template <typename...> class TCombiner>
  using combine = typename detail::type_list_impl::combine<
    TRightList, 0, TCombiner, Args...
  >::type;

  /**
   * Flattens elements from sublists into a single, topmost list.
   *
   * The topmost list's elements will be layed out as if traversing using a
   * recursive iterator on the lists.
   *
   * Up to `Depth` recursion levels are flattened. 0 means the original list.
   *
   * Example:
   *
   *  using list = type_list<
   *    int,
   *    type_list<
   *      double,
   *      float,
   *      type_list<
   *        type_list<std::string>,
   *        type_list<std::size_t, std::vector<int>>,
   *        short
   *      >,
   *    >,
   *    bool
   *  >;
   *
   *  // yields `type_list<
   *  //   int,
   *  //   double,
   *  //   float,
   *  //   std::string,
   *  //   std::size_t, std::vector<int>,
   *  //   short
   *  //   bool
   *  // >`
   *  using result = list::flatten<>;
   *
   *  // yields the same as `list`
   *  using result0 = list::flatten<0>;
   *
   *  // yields `type_list<
   *  //   int,
   *  //   double,
   *  //   float,
   *  //   type_list<
   *  //     type_list<std::string>,
   *  //     type_list<std::size_t, std::vector<int>>,
   *  //     short
   *  //   >,
   *  //   bool
   *  // >`
   *  using result1 = list::flatten<1>;
   *
   *  // yields `type_list<
   *  //   int,
   *  //   double,
   *  //   float,
   *  //   type_list<std::string>,
   *  //   type_list<std::size_t, std::vector<int>>,
   *  //   short
   *  //   bool
   *  // >`
   *  using result2 = list::flatten<2>;
   *
   *  // yields `type_list<
   *  //   int,
   *  //   double,
   *  //   float,
   *  //   std::string,
   *  //   std::size_t, std::vector<int>,
   *  //   short
   *  //   bool
   *  // >`
   *  using result3 = list::flatten<3>;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <std::size_t Depth = std::numeric_limits<std::size_t>::max()>
  using flatten = typename detail::type_list_impl::flatten<
    0, Depth, Args...
  >::type;

  /**
   * Tells whether this type_list is sorted according to the given type
   * comparer `TLessComparer`.
   *
   * `TLessComparer` receives two types, `TLHS` and `TRHS`, as template
   * arguments, and must provide a static constexpr boolean member `value`
   * telling whether `TLHS` is smaller than `TRHS` or not.
   *
   * `TLessComparer` must represent a total order relation between all types.
   *
   * The default comparer is `constants_comparison_lt`.
   *
   * Example:
   *
   *  template <int X> struct T {};
   *  template <typename, typename> struct comparer;
   *  template <int L, int R>
   *  struct comparer<T<L>, T<R>>:
   *    public std::integral_constant<bool, (L < R)>
   *  {};
   *
   *  // yields `false`
   *  type_list<T<1>, T<0>, T<2>>::is_sorted<comparer>::value
   *
   *  // yields `true`
   *  type_list<T<0>, T<1>, T<2>>::is_sorted<comparer>::value
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <
    template <typename...> class TLessComparer = constants_comparison_lt
  >
  using is_sorted = typename detail::type_list_impl::is_sorted<
    TLessComparer, Args...
  >::type;

  /**
   * Merges two sorted type lists into a new sorted type list, according to
   * the given type comparer `TLessComparer`.
   *
   * `TLessComparer` must represent a total order relation between all types.
   *
   * The default comparer is `constants_comparison_lt`.
   *
   * Example:
   *
   *  typedef type_list<T<0>, T<1>, T<2>> lhs;
   *  typedef type_list<T<3>, T<4>, T<5>> rhs;
   *
   *  // yields `type_list<T<0>, T<1>, T<2>, T<3>, T<4>, T<5>>`
   *  typedef lhs::merge<rhs, constants_comparison_lt> result;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <
    typename TList,
    template <typename...> class TLessComparer = constants_comparison_lt
  >
  using merge = typename detail::type_list_impl::merge_entry_point<
    TLessComparer, TList, Args...
  >::type;

  /**
   * Sorts this `type_list` using the stable merge sort algorithm, according to
   * the given type comparer `TLessComparer`.
   *
   * `TLessComparer` must represent a total order relation between all types.
   *
   * The default comparer is `constants_comparison_lt`.
   *
   * Example:
   *
   *  typedef type_list<T<0>, T<5>, T<4>, T<2>, T<1>, T<3>> list;
   *
   *  // yields `type_list<T<0>, T<1>, T<2>, T<3>, T<4>, T<5>>`
   *  typedef list::merge_sort<> result;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <
    template <typename...> class TLessComparer = constants_comparison_lt
  >
  using merge_sort = typename detail::type_list_impl::merge_sort<
    TLessComparer, type_list
  >::type;

  /**
   * Removes all duplicate elements from this list.
   *
   * This algorithm is stable: only the first occurence of a type is kept.
   *
   * An optional transform `TTransform` can be applied to each element of this
   * list before processing the duplicates.
   *
   * Example:
   *
   *  using list1 = type_list<int, double, double, int, bool, int, bool, float>;
   *
   *  // yields `type_list<int, double, bool, float>`
   *  using result1 = list::unique<>;
   *
   *  template <int... Values> using int_seq = type_list<
   *    std::integral_constant<int, Values>...
   *  >;
   *
   *  using list2 = int_seq<0, 1, 4, 3, 2, 6, 1, 2, 4, 3, 1, 2, 3, 0, 1, 2>;
   *
   *  // yields `int_seq<0, 1, 4, 3, 2, 6>`
   *  using result2 = list2::unique<>;
   *
   *  template <typename T>
   *  using double_val = std::integral_constant<int, T::value * 2>;
   *
   *  // yields `int_seq<0, 2, 8, 6, 4, 12>`
   *  using result3 = list2::unique<double_val>;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <template <typename...> class TTransform = ftl::transform::identity>
  using unique = typename detail::type_list_impl::unique<
    type_list<>, TTransform<Args>...
  >::type;

  /**
   * Performs a binary search on this list's types (assumes the list is sorted),
   * comparing against the given `needle`.
   *
   * If a matching type is found, the visitor is called with the following
   * arguments:
   *  - an instance of `indexed_type_tag<MatchingType, Index>`
   *  - the perfectly forwarded `needle`
   *  - the perfectly forwarded list of additional arguments `args` given to
   *    the visitor
   *
   * in other words, with this general signature:
   *
   *  template <
   *    typename T,
   *    std::size_t Index,
   *    typename TNeedle,
   *    typename... VArgs
   *  >
   *  void operator ()(
   *    indexed_type_tag<T, Index>,
   *    TNeedle &&needle,
   *    VArgs &&...args
   *  );
   *
   * Returns `true` when found, `false` otherwise.
   *
   * The comparison is performed using the given `TComparer`'s method, whose
   * signature must follow this pattern:
   *
   *  template <typename TNeedle, typename T, std::size_t Index>
   *  static int compare(TNeedle &&needle, indexed_type_tag<T, Index>);
   *
   * which effectivelly compares `needle` against the type `T`. The result must
   * be < 0, > 0 or == 0 if `needle` is, respectively, less than, greather than
   * or equal to `T`. `Index` is the position of `T` in this type list and can
   * also be used in the comparison if needed.
   *
   * `TComparer` defaults to `type_value_comparer` which compares an
   * std::integral_constant-like value to a runtime value.
   *
   * The sole purpose of `args` is to be passed along to the visitor, it is not
   * used by this method in any way. It could also be safely omitted.
   *
   * The visitor will be called at most once, iff a match is found. Otherwise it
   * will not be called. The boolean returned by this method can be used to tell
   * whether the visitor has been called or not.
   *
   * See each operation's documentation below for further details.
   *
   * Note: this is a runtime facility.
   *
   * Example comparer used on examples below:
   *
   *  template <char c> using chr = std::integral_constant<char, c>;
   *  template <char... s> using str = type_list<chr<s>...>;
   *
   *  template <int n> using int_val = std::integral_constant<int, n>;
   *  template <int... s> using int_seq = type_list<int_val<s>...>;
   *
   *  struct cmp {
   *    template <char c, std::size_t Index>
   *    static int compare(char needle, indexed_type_tag<chr<c>, Index>) {
   *      return static_cast<int>(needle) - c;
   *    };
   *
   *    template <int n, std::size_t Index>
   *    static int compare(int needle, indexed_type_tag<int_val<n>, Index>) {
   *      return needle - n;
   *    };
   *  };
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename TComparer = type_value_comparer>
  struct binary_search {
    /**
     * Performs a binary search for an element
     * that is an exact match of the `needle`.
     *
     * Refer to the `binary_search` documentation above for more details.
     *
     * Note: this is a runtime facility.
     *
     * Example:
     *
     *  struct visitor {
     *    template <char c, std::size_t Index>
     *    void operator ()(indexed_type_tag<chr<c>, Index>, char needle) {
     *      assert(c == needle);
     *      std::cout << "needle '" << needle << "' found at index " << Index
     *        << std::endl;
     *    };
     *  };
     *
     *  typedef str<'a', 'e', 'i', 'o', 'u'> list;
     *
     *  // yields `false`
     *  list::binary_search<cmp>::exact('x', visitor());
     *
     *  // yields `true` and prints `"needle 'i' found at index 2"`
     *  list::binary_search<cmp>::exact('i', visitor());
     *
     * @author: Marcelo Juchem <marcelo@fb.com>
     */
    template <typename TNeedle, typename TVisitor, typename... VArgs>
    static constexpr bool exact(
      TNeedle &&needle, TVisitor &&visitor, VArgs &&...args
    ) {
      return type_list::template apply<
        detail::type_list_impl::binary_search_exact
      >::template search<TComparer, 0>(
        std::forward<TNeedle>(needle),
        std::forward<TVisitor>(visitor),
        std::forward<VArgs>(args)...
      );
    }

    /**
     * Performs a binary search for the greatest element that is less than
     * or equal to (<=) the `needle`. This is analogous to `std::lower_bound`.
     *
     * Refer to the `binary_search` documentation above for more details.
     *
     * Note: this is a runtime facility.
     *
     * Example:
     *
     *  struct visitor {
     *    template <int n, std::size_t Index>
     *    void operator ()(indexed_type_tag<int_val<n>, Index>, int needle) {
     *      assert(n <= needle);
     *      std::cout << needle << "'s lower bound " << n
     *        << " found at index " << Index
     *        << std::endl;
     *    };
     *  };
     *
     *  typedef int_seq<10, 30, 50, 70> list;
     *
     *  // yields `false`
     *  list::binary_search<cmp>::lower_bound(5, visitor());
     *
     *  // yields `true` and prints `"11's lower bound 10 found at index 0"`
     *  list::binary_search<cmp>::lower_bound(11, visitor());
     *
     *  // yields `true` and prints `"68's lower bound 50 found at index 2"`
     *  list::binary_search<cmp>::lower_bound(68, visitor());
     *
     *  // yields `true` and prints `"70's lower bound 70 found at index 3"`
     *  list::binary_search<cmp>::lower_bound(70, visitor());
     *
     * @author: Marcelo Juchem <marcelo@fb.com>
     */
    template <typename TNeedle, typename TVisitor, typename... VArgs>
    static constexpr bool lower_bound(
      TNeedle &&needle, TVisitor &&visitor, VArgs &&...args
    ) {
      return type_list::template apply<
        detail::type_list_impl::binary_search_lower_bound
      >::template search<TComparer>(
        std::forward<TNeedle>(needle),
        std::forward<TVisitor>(visitor),
        std::forward<VArgs>(args)...
      );
    }

    /**
     * Performs a binary search for the least element that is greater
     * than (>) the `needle`. This is analogous to `std::upper_bound`.
     *
     * Refer to the `binary_search` documentation above for more details.
     *
     * Note: this is a runtime facility.
     *
     * Example:
     *
     *  struct visitor {
     *    template <int n, std::size_t Index>
     *    void operator ()(indexed_type_tag<int_val<n>, Index>, int needle) {
     *      assert(n > needle);
     *      std::cout << needle << "'s upper bound " << n
     *        << " found at index " << Index
     *        << std::endl;
     *    };
     *  };
     *
     *  typedef int_seq<10, 30, 50, 70> list;
     *
     *  // yields `false`
     *  list::binary_search<cmp>::upper_bound(70, visitor());
     *
     *  // yields `true` and prints `"5's upper bound 10 found at index 0"`
     *  list::binary_search<cmp>::upper_bound(5, visitor());
     *
     *  // yields `true` and prints `"31's upper bound 50 found at index 2"`
     *  list::binary_search<cmp>::upper_bound(31, visitor());
     *
     * @author: Marcelo Juchem <marcelo@fb.com>
     */
    template <typename TNeedle, typename TVisitor, typename... VArgs>
    static constexpr bool upper_bound(
      TNeedle &&needle, TVisitor &&visitor, VArgs &&...args
    ) {
      return type_list::template apply<
        detail::type_list_impl::binary_search_upper_bound
      >::template search<TComparer, 0>(
        std::forward<TNeedle>(needle),
        std::forward<TVisitor>(visitor),
        std::forward<VArgs>(args)...
      );
    }
  };
};

///////////////////////////////
// STATIC MEMBERS DEFINITION //
///////////////////////////////

template <typename... Args> constexpr std::size_t type_list<Args...>::size;
template <typename... Args> constexpr bool type_list<Args...>::empty;

} // namespace ftl
