/*
 *  Copyright (c) 2014, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 */

#pragma once

#include <fatal/type/list.h>
#include <fatal/type/pair.h>
#include <fatal/type/tag.h>
#include <fatal/type/transform.h>

#include <type_traits>
#include <utility>
#include <typeinfo>

namespace fatal {

//////////////////////////
// type_map DECLARATION //
//////////////////////////

template <typename... Args> class type_map;

/////////////////////
// SUPPORT LIBRARY //
/////////////////////

namespace detail {
namespace type_map_impl {

///////////////////
// pair_resolver //
///////////////////

template <typename... Args> struct pair_resolver;

template <typename TKey, typename TMapped>
struct pair_resolver<TKey, TMapped> {
  typedef type_pair<TKey, TMapped> type;
};

template <typename TKey, typename TMapped>
struct pair_resolver<type_pair<TKey, TMapped>> {
  typedef type_pair<TKey, TMapped> type;
};

////////////////////////
// curried_key_filter //
////////////////////////

template <template <typename...> class TFilter, typename... Args>
struct curried_key_filter {
  template <typename T>
  using type = TFilter<type_get_first<T>, Args...>;
};

///////////////
// transform //
///////////////

template <
  typename TPair,
  template <typename...> class TKeyTransform,
  template <typename...> class TMappedTransform
>
using transform = typename TPair::template transform<
  TKeyTransform, TMappedTransform
>;

//////////////////
// transform_at //
//////////////////

template <
  typename TKey,
  typename TPair,
  template <typename...> class TKeyTransform,
  template <typename...> class TMappedTransform
>
using transform_at = typename std::conditional<
  std::is_same<TKey, typename TPair::first>::value,
  typename TPair::template transform<TKeyTransform, TMappedTransform>,
  TPair
>::type;

////////////
// invert //
////////////

template <
  typename TPair,
  template <typename...> class TKeyTransform,
  template <typename...> class TMappedTransform
>
using invert_transform = typename TPair::template transform<
  TKeyTransform, TMappedTransform
>::invert;

////////////
// filter //
////////////

template <template <typename...> class TFilter, typename TList>
struct filter {
  typedef typename TList::template filter<
    curried_key_filter<TFilter>::template type
  > filtered;

  typedef type_pair<
    typename filtered::first::template apply<type_map>,
    typename filtered::second::template apply<type_map>
  > type;
};

/////////////
// cluster //
/////////////

template <typename T>
struct cluster_transform {
  template <typename U>
  using add = typename U::template push_back<T>;
};

template <typename... Args>
struct cluster {
  typedef type_map<> type;
};

template <typename T, typename... Args>
struct cluster<T, Args...> {
  typedef typename cluster<Args...>::type tail;
  typedef typename T::first key;
  typedef typename T::second mapped;

  typedef typename std::conditional<
    tail::template contains<key>::value,
    typename tail::template transform_at<
      key,
      cluster_transform<mapped>::template add
    >,
    typename tail::template push_back<key, type_list<mapped>>
  >::type type;
};

///////////
// visit //
///////////

struct visit_not_found {};

template <typename TKey, typename TMapped>
struct visit_visitor {
  template <typename TVisitor, typename... VArgs>
  static constexpr bool visit(TVisitor &&visitor, VArgs &&...args) {
    // comma operator needed due to C++11's constexpr restrictions
    return visitor(
      type_pair<TKey, TMapped>(),
      std::forward<VArgs>(args)...
    ), true;
  }
};

template <typename TKey>
struct visit_visitor<TKey, visit_not_found> {
  template <typename TVisitor, typename... VArgs>
  static constexpr bool visit(TVisitor &&, VArgs &&...) { return false; }
};

///////////////////
// binary_search //
///////////////////

template <typename TComparer>
struct binary_search_comparer {
  template <typename TNeedle, typename TKey, typename TValue, std::size_t Index>
  static constexpr int compare(
    TNeedle &&needle,
    indexed_type_tag<type_pair<TKey, TValue>, Index>
  ) {
    return TComparer::compare(
      std::forward<TNeedle>(needle),
      indexed_type_tag<TKey, Index>{}
    );
  }
};

} // namespace type_map_impl {
} // namespace detail {

/**
 * Type map for template metaprogramming.
 *
 * Most operations, unless noted otherwise, are compile-time evaluated.
 *
 * Compile-time operations have no side-effects. I.e.: operations that would
 * mutate the map upon which they are performed actually create a new map.
 *
 * @author: Marcelo Juchem <marcelo@fb.com>
 */
template <typename... Args>
class type_map {
  static_assert(
    logical_and_constants<
      std::true_type,
      is_template<type_pair>::template instantiation<Args>...
    >::value,
    "type_map elements must be instantiations of type_pair"
  );

  // private to `type_map` so that `contains` will never have false negatives
  struct not_found_tag_impl {};

public:
  /**
   * The underlying type_list of type_pair<TKey, TMapped> used for
   *  implementing this map.
   */
  typedef type_list<Args...> contents;

  /**
   * The size of this map.
   */
  static constexpr std::size_t size = sizeof...(Args);
  static_assert(size == contents::size, "size mismatch");

  /**
   * A boolean telling whether this map is empty or not.
   * This is the same as `type_map::size == 0`.
   */
  static constexpr bool empty = sizeof...(Args) == 0;
  static_assert(empty == contents::empty, "empty property mismatch");

  /**
   * A list of all the keys in this map. The order of the keys are guaranteed
   * to follow `contents`'s order.
   */
  typedef typename contents::template transform<type_get_first> keys;

  /**
   * A list of all the mapped types in this map. The order of the mapped types
   * are guaranteed to follow `contents`'s order.
   */
  typedef typename contents::template transform<type_get_second> mapped;

  template <
    template <typename...> class TMappedTransform,
    template <typename...> class TKeyTransform = ::fatal::transform::identity
  >
  using transform = type_map<
    detail::type_map_impl::transform<Args, TKeyTransform, TMappedTransform>...
  >;

  template <
    typename TKey,
    template <typename...> class TMappedTransform,
    template <typename...> class TKeyTransform = ::fatal::transform::identity
  >
  using transform_at = type_map<
    detail::type_map_impl::transform_at<
      TKey, Args, TKeyTransform, TMappedTransform
    >...
  >;

  /**
   * Inverts the key and mapped types of this `type_map`.
   *
   * Optionally, it can apply a transformation to the key (`TKeyTransform`)
   * and mapped value (`TMappedTransform`) before inverting.
   *
   * Example:
   *
   *  typedef type_map<
   *    type_pair<int, bool>,
   *    type_pair<float, long>
   *  > map;
   *
   *  // yields `type_map<
   *  //   type_pair<bool, int>,
   *  //   type_pair<long, float>
   *  // >`
   *  typedef map::invert<> result1;
   *
   *  // yields `type_map<
   *  //   type_pair<Bar<bool>, Foo<int>>,
   *  //   type_pair<Bar<long>, Foo<float>>
   *  // >`
   *  template <typename> struct Foo {};
   *  template <typename> struct Bar {};
   *  typedef map::invert<Foo, Bar> result2;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <
    template <typename...> class TKeyTransform = ::fatal::transform::identity,
    template <typename...> class TMappedTransform = ::fatal::transform::identity
  >
  using invert = type_map<
    detail::type_map_impl::invert_transform<
      Args, TKeyTransform, TMappedTransform
    >...
  >;

  /**
   * Finds the first pair with key `TKey` and returns the mapped type.
   *
   * If there's no pair in this map with key `TKey` then `TDefault` is returned
   * (defaults to `type_not_found_tag` when omitted).
   *
   * Example:
   *
   *  typedef type_map<type_pair<int, double>, type_pair<bool, long>> map;
   *
   *  // yields `double`
   *  typedef map::find<int> result1;
   *
   *  // yields `type_not_found_tag`
   *  typedef map::find<float> result2;
   *
   *  // yields `void`
   *  typedef map::find<float, void> result2;
   */
  template <typename TKey, typename TDefault = type_not_found_tag>
  using find = typename contents::template search<
    detail::type_map_impl::curried_key_filter<
      std::is_same, TKey
    >::template type,
    type_pair<void, TDefault>
  >::second;

  /**
   * Tells whether there is a mapping where T is the key.
   *
   * Example:
   *
   *  typedef type_map<type_pair<int, double>> map;
   *
   *  // yields `true`
   *  map::contains<int>::value
   *
   *  // yields `false`
   *  map::contains<bool>::value
   */
  template <typename T>
  using contains = typename keys::template contains<T>;

  /**
   * Inserts the given `TKey` and `TValue` pair before all elements of this map.
   *
   * There are two overloads, one accepting the key and mapped type
   * (push_front<TKey, TMapped>) and another accepting a `type_pair`
   * (push_front<type_pair<TKey, TMapped>>).
   *
   * Note that keys are not necessarily unique. Inserting an existing key
   * will create duplicate entries for it.
   *
   * Example:
   *
   *  typedef type_map<
   *    type_pair<int, bool>,
   *    type_pair<float, double>
   *  > map;
   *
   *  // yields `type_map<
   *  //   type_pair<short, long>,
   *  //   type_pair<int, bool>,
   *  //   type_pair<float, double>
   *  // >`
   *  typedef map::push_front<short, long> result1;
   *
   *  // yields `type_map<
   *  //   type_pair<short, long>,
   *  //   type_pair<int, bool>,
   *  //   type_pair<float, double>
   *  // >`
   *  typedef map::push_front<type_pair<short, long>> result2;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... UArgs>
  using push_front = type_map<
    typename detail::type_map_impl::pair_resolver<UArgs...>::type,
    Args...
  >;

  /**
   * Inserts the given `TKey` and `TValue` pair after all elements of this map.
   *
   * There are two overloads, one accepting the key and mapped type
   * (push_back<TKey, TMapped>) and another accepting a `type_pair`
   * (push_back<type_pair<TKey, TMapped>>).
   *
   * Note that keys are not necessarily unique. Inserting an existing key
   * will create duplicate entries for it.
   *
   * Example:
   *
   *  typedef type_map<
   *    type_pair<int, bool>,
   *    type_pair<float, double>
   *  > map;
   *
   *  // yields `type_map<
   *  //   type_pair<int, bool>,
   *  //   type_pair<float, double>,
   *  //   type_pair<short, long>
   *  // >`
   *  typedef map::push_back<short, long> result1;
   *
   *  // yields `type_map<
   *  //   type_pair<int, bool>,
   *  //   type_pair<float, double>,
   *  //   type_pair<short, long>
   *  // >`
   *  typedef map::push_back<type_pair<short, long>> result2;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... UArgs>
  using push_back = type_map<
    Args...,
    typename detail::type_map_impl::pair_resolver<UArgs...>::type
  >;

  /**
   * Inserts a new mapping into the type_map, in no specified order.
   *
   * Note that keys are not necessarily unique. Inserting an existing key
   * will create duplicate entries for it. Should you need unique mappings,
   * use `replace` instead.
   *
   * There are two overloads, one accepting the key and mapped type
   * (insert<TKey, TMapped>) and another accepting a `type_pair`
   * (insert<type_pair<TKey, TMapped>>).
   *
   * Example:
   *
   *  typedef type_map<type_pair<int, double>> map;
   *
   *  // yields `type_map<type_pair<int, double>, type_pair<float, long>>`
   *  typedef map::insert<float, long> result1;
   *
   *  // yields `type_map<type_pair<int, double>, type_pair<float, long>>`
   *  typedef type_pair<float, long> pair;
   *  typedef map::insert<pair> result2;
   */
  template <typename... UArgs>
  using insert = typename contents::template push_back<
    typename detail::type_map_impl::pair_resolver<UArgs...>::type
  >::template apply<::fatal::type_map>;

  /**
   * Inserts a new `TKey`, `TValue` mapping into the type_map in its sorted
   * position according to the order provided by `TLessComparer` when applied
   * to the keys.
   *
   * `TLessComparer` defaults to `constants_comparison_lt` when omitted.
   *
   * Example:
   *
   *  template <int Value>
   *  using val = std::integral_constant<int, Value>;
   *
   *  typedef type_map<
   *    type_pair<val<0>, bool>,
   *    type_pair<val<3>, double>
   *  > map;
   *
   *  // yields `type_map<
   *  //   type_pair<val<0>, bool>,
   *  //   type_pair<val<2>, long>,
   *  //   type_pair<val<3>, double>
   *  // >`
   *  typedef map::insert_sorted<constants_comparison_lt, val<2>, long> result;
   */
  template <
    typename TKey, typename TValue,
    template <typename...> class TLessComparer = constants_comparison_lt
  >
  using insert_sorted = typename contents::template insert_sorted<
    type_pair<TKey, TValue>,
    type_get_first_comparer<TLessComparer>::template type
  >::template apply<::fatal::type_map>;

  /**
   * Inserts a new `type_pair` mapping into the type_map in its sorted position
   * according to the order provided by `TLessComparer` when applied to the
   * keys.
   *
   * `TLessComparer` defaults to `constants_comparison_lt` when omitted.
   *
   * Example:
   *
   *  template <int Value>
   *  using val = std::integral_constant<int, Value>;
   *
   *  typedef type_map<
   *    type_pair<val<0>, bool>,
   *    type_pair<val<3>, double>
   *  > map;
   *
   *  // yields `type_map<
   *  //   type_pair<val<0>, bool>,
   *  //   type_pair<val<1>, int>,
   *  //   type_pair<val<3>, double>
   *  // >`
   *  typedef map::insert_pair_sorted<
   *    constants_comparison_lt,
   *    type_pair<val<1>, int>
   *  > result;
   */
  template <
    typename TPair,
    template <typename...> class TLessComparer = constants_comparison_lt
  >
  using insert_pair_sorted = typename contents::template insert_sorted<
    TPair,
    type_get_first_comparer<TLessComparer>::template type
  >::template apply<::fatal::type_map>;

  /**
   * Replaces with `TMapped` the mapped type of all pairs with key `TKey`.
   *
   * Example:
   *
   *  // yields `type_map<type_pair<int, double>>`
   *  typedef type_map<type_pair<int, long>>::replace<int, double> result1;
   *
   *  // yields `typedef type_map<
   *  //   type_pair<int, double>,
   *  //   type_pair<float, short>,
   *  //   type_pair<int, double>
   *  // >`
   *  typedef type_map<
   *    type_pair<int, long>,
   *    type_pair<float, short>,
   *    type_pair<int, bool>
   *  >::replace<int, double> result2;
   */
  template <typename TKey, typename TMapped>
  using replace = typename type_map::template transform_at<
    TKey,
    ::fatal::transform::fixed<TMapped>::template type
  >;

  /**
   * FiltersRemoves all entries where given types are the key from the type map.
   *
   * Example:
   *
   *  typedef type_map<
   *    type_pair<int, bool>,
   *    type_pair<int, float>,
   *    type_pair<void, std::string>,
   *    type_pair<float, double>,
   *    type_pair<bool, bool>
   *  > map;
   *
   *  // yields `type_map<type_pair<float, double>, type_pair<bool, bool>>`
   *  typedef map::remove<int, void> result;
   */
  template <typename... UArgs>
  using remove = typename contents::template filter<
    detail::type_map_impl::curried_key_filter<
      type_list<UArgs...>::template contains
    >::template type
  >::second::template apply<::fatal::type_map>;

  /**
   * Returns a pair with two `type_map`s. One (first) with the pairs whose keys
   * got accepted by the filter and the other (second) with the pairs whose keys
   * weren't accepted by it.
   *
   * TFilter is a std::integral_constant-like template whose value
   * evaluates to a boolean when fed with a key from this map.
   *
   * Example:
   *
   *  typedef type_map<
   *    type_pair<int, bool>,
   *    type_pair<int, float>,
   *    type_pair<void, std::string>,
   *    type_pair<float, double>,
   *    type_pair<bool, bool>
   *  > map;
   *
   *  // yields `type_pair<
   *  //   type_map<
   *  //     type_pair<int, bool>,
   *  //     type_pair<int, float>,
   *  //     type_pair<bool, bool>
   *  //   >,
   *  //   type_map<
   *  //     type_pair<void, std::string>,
   *  //     type_pair<float, double>
   *  //   >,
   *  // >`
   *  typedef map::filter<std::is_integral> filtered;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <template <typename...> class TFilter>
  using filter = typename detail::type_map_impl::filter<
    TFilter, contents
  >::type;

  /**
   * Sorts this `type_map` by key using the stable merge sort algorithm,
   * according to the given type comparer `TLessComparer`.
   *
   * `TLessComparer` must represent a total order relation between all types.
   *
   * The default comparer is `constants_comparison_lt`.
   *
   * Example:
   *
   *  typedef type_map<
   *    type_pair<T<0>, void>,
   *    type_pair<T<1>, short>,
   *    type_pair<T<4>, double>,
   *    type_pair<T<2>, bool>,
   *    type_pair<T<1>, int>,
   *    type_pair<T<3>, float>
   *  > map;
   *
   *  // yields `type_map<
   *  //   type_pair<T<0>, void>,
   *  //   type_pair<T<1>, short>,
   *  //   type_pair<T<1>, int>,
   *  //   type_pair<T<2>, bool>,
   *  //   type_pair<T<3>, float>,
   *  //   type_pair<T<4>, double>
   *  // >`
   *  typedef map::merge_sort<> result;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <
    template <typename...> class TLessComparer = constants_comparison_lt
  >
  using merge_sort = typename contents::template merge_sort<
    type_get_first_comparer<TLessComparer>::template type
  >::template apply<::fatal::type_map>;

  template <
    template <typename...> class TKeyTransform = ::fatal::transform::identity,
    template <typename...> class TMappedTransform = ::fatal::transform::identity
  >
  using cluster = typename detail::type_map_impl::cluster<
    detail::type_map_impl::transform<Args, TKeyTransform, TMappedTransform>...
  >::type;

  template <typename TKey, typename TVisitor, typename... VArgs>
  static constexpr bool visit(TVisitor &&visitor, VArgs &&...args) {
    using mapped = find<TKey, detail::type_map_impl::visit_not_found>;

    return detail::type_map_impl::visit_visitor<TKey, mapped>::visit(
      std::forward<TVisitor>(visitor),
      std::forward<VArgs>(args)...
    );
  }

  /**
   * Performs a binary search on this map's keys (assumes the map is sorted),
   * comparing against the given `needle`.
   *
   * If a matching key is found, the visitor is called with the following
   * arguments:
   *  - an instance of `indexed_type_tag<
   *      type_pair<MatchingKey, MappedType>,
   *      Index
   *    >`
   *  - the perfectly forwarded `needle`
   *  - the perfectly forwarded list of additional arguments `args` given to
   *    the visitor
   *
   * in other words, with this general signature:
   *
   *  template <
   *    typename TKey, typename TMapped, std::size_t Index,
   *    typename TNeedle,
   *    typename... VArgs
   *  >
   *  void operator ()(
   *    indexed_type_tag<type_pair<TKey, TMapped>, Index>,
   *    TNeedle &&needle,
   *    VArgs &&...args
   *  );
   *
   * Returns `true` when found, `false` otherwise.
   *
   * The comparison is performed using the given `TComparer`'s method, whose
   * signature must follow this pattern:
   *
   *  template <typename TNeedle, typename TKey, std::size_t Index>
   *  static int compare(TNeedle &&needle, indexed_type_tag<TKey, Index>);
   *
   * which effectivelly compares `needle` against the key type `TKey`. The
   * result must be < 0, > 0 or == 0 if `needle` is, respectively, less than,
   * greather than or equal to `TKey`. `Index` is the position of the pair with
   * key `TKey` in this type map and can also be used in the comparison if
   * needed.
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
   *  template <int n> using int_val = std::integral_constant<int, n>;
   *
   *  template <char c> using chr = std::integral_constant<char, c>;
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
   */
  template <typename TComparer = type_value_comparer>
  struct binary_search {
    /**
     * Performs a binary search for a key that
     * is an exact match of the `needle`.
     *
     * Refer to the `binary_search` documentation above for more details.
     *
     * Note: this is a runtime facility.
     *
     * Example:
     *
     *  struct visitor {
     *    template <char Key, char Mapped, std::size_t Index>
     *    void operator ()(
     *      indexed_type_tag<type_pair<chr<Key>, chr<Mapped>>, Index>,
     *      char needle
     *    ) {
     *      assert(needle == Key);
     *      std::cout << "key '" << needle << "' found at index " << Index
     *        << " mapping '" << Mapped << '\''
     *        << std::endl;
     *    };
     *  };
     *
     *  typedef type_map<
     *    type_pair<chr<'a'>, chr<'A'>>,
     *    type_pair<chr<'e'>, chr<'E'>>,
     *    type_pair<chr<'i'>, chr<'I'>>,
     *    type_pair<chr<'o'>, chr<'O'>>,
     *    type_pair<chr<'u'>, chr<'U'>>
     *  > map;
     *
     *  // yields `false`
     *  map::binary_search::exact<cmp>('x', visitor());
     *
     *  // yields `true` and prints `"key 'i' found at index 2 mapping 'I'"`
     *  map::binary_search<cmp>::exact('i', visitor());
    */
    template <typename TNeedle, typename TVisitor, typename... VArgs>
    static constexpr bool exact(
      TNeedle &&needle, TVisitor &&visitor, VArgs &&...args
    ) {
      return contents::template binary_search<
        detail::type_map_impl::binary_search_comparer<TComparer>
      >::exact(
        std::forward<TNeedle>(needle),
        std::forward<TVisitor>(visitor),
        std::forward<VArgs>(args)...
      );
    }

    /**
     * Performs a binary search for the greatest key that is less than
     * or equal to (<=) the `needle`. This is analogous to `std::lower_bound`.
     *
     * Refer to the `binary_search` documentation above for more details.
     *
     * Note: this is a runtime facility.
     *
     * Example:
     *
     *  struct visitor {
     *    template <int Key, int Mapped, std::size_t Index>
     *    void operator ()(
     *      indexed_type_tag<type_pair<int_val<Key>, int_val<Mapped>>, Index>,
     *      int needle
     *    ) {
     *      assert(Key <= needle);
     *      std::cout << needle << "'s lower bound " << n
     *        << " found at index " << Index << " mapping " << Mapped
     *        << std::endl;
     *    };
     *  };
     *
     *  typedef type_map<
     *    type_pair<int_val<10>, int_val<100>>,
     *    type_pair<int_val<30>, int_val<300>>,
     *    type_pair<int_val<50>, int_val<500>>,
     *    type_pair<int_val<70>, int_val<700>>
     *  > map;
     *
     *  // yields `false`
     *  map::binary_search<cmp>::lower_bound(5, visitor());
     *
     *  // yields `true` and prints
     *  // `"11's lower bound 10 found at index 0 mapping 100"`
     *  map::binary_search<cmp>::lower_bound(11, visitor());
     *
     *  // yields `true` and prints
     *  // `"68's lower bound 50 found at index 2 mapping 500"`
     *  map::binary_search<cmp>::lower_bound(68, visitor());
     *
     *  // yields `true` and prints
     *  // `"70's lower bound 70 found at index 3 mapping 700"`
     *  map::binary_search<cmp>::lower_bound(70, visitor());
     */
    template <typename TNeedle, typename TVisitor, typename... VArgs>
    static constexpr bool lower_bound(
      TNeedle &&needle, TVisitor &&visitor, VArgs &&...args
    ) {
      return contents::template binary_search<
        detail::type_map_impl::binary_search_comparer<TComparer>
      >::lower_bound(
        std::forward<TNeedle>(needle),
        std::forward<TVisitor>(visitor),
        std::forward<VArgs>(args)...
      );
    }

    /**
     * Performs a binary search for the least key that is greater than (>)
     * the `needle`. This is analogous to `std::upper_bound`.
     *
     * Refer to the `binary_search` documentation above for more details.
     *
     * Note: this is a runtime facility.
     *
     * Example:
     *
     *  struct visitor {
     *    template <int Key, int Mapped, std::size_t Index>
     *    void operator ()(
     *      indexed_type_tag<type_pair<int_val<Key>, int_val<Mapped>>, Index>,
     *      int needle
     *    ) {
     *      assert(Key > needle);
     *      std::cout << needle << "'s upper bound " << n
     *        << " found at index " << Index << " mapping " << Mapped
     *        << std::endl;
     *    };
     *  };
     *
     *  typedef int_seq<10, 30, 50, 70> list;
     *
     *  // yields `false`
     *  map::binary_search<cmp>::upper_bound(70, visitor());
     *
     *  // yields `true` and prints
     *  // `"5's upper bound 10 found at index 0 mapping 100"`
     *  map::binary_search<cmp>::upper_bound(5, visitor());
     *
     *  // yields `true` and prints
     *  // `"31's upper bound 50 found at index 2 mapping 500"`
     *  map::binary_search<cmp>::upper_bound(31, visitor());
     */
    template <typename TNeedle, typename TVisitor, typename... VArgs>
    static constexpr bool upper_bound(
      TNeedle &&needle, TVisitor &&visitor, VArgs &&...args
    ) {
      return contents::template binary_search<
        detail::type_map_impl::binary_search_comparer<TComparer>
      >::upper_bound(
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

template <typename... Args> constexpr std::size_t type_map<Args...>::size;
template <typename... Args> constexpr bool type_map<Args...>::empty;

/////////////////////
// SUPPORT LIBRARY //
/////////////////////

/////////////////////
// type_get_traits //
/////////////////////

/**
 * Specialization of `type_get_traits` so that `type_get` supports `type_map`.
 *
 * @author: Marcelo Juchem <marcelo@fb.com>
 */
template <std::size_t Index, typename... Args>
struct type_get_traits<type_map<Args...>, Index> {
  typedef typename type_map<Args...>::contents::template at<Index> type;
};

////////////////////
// build_type_map //
////////////////////

namespace detail {
namespace type_map_impl {

template <typename... Args>
class builder {
  typedef type_list<Args...> args;

  typedef typename args::template unzip<1, 0> keys;
  typedef typename args::template unzip<1, 1> mapped;

  static_assert(keys::size == mapped::size, "not all keys map to a type");

public:
  typedef typename keys::template combine<mapped, type_pair>::template apply<
    type_map
  > type;
};

} // namespace type_map_impl {
} // namespace detail {

/**
 * Convenience mechanism to construct new type maps.
 *
 * It receives a flat list of `TKey, TMapped` types.
 *
 * Example:
 *
 *  // yields an empty `type_map<>`
 *  typedef build_type_map<> empty;
 *
 *  // yields `type_map<type_pair<int, double>>`
 *  typedef build_type_map<int, double> result1;
 *
 *  // yields
 *  //   type_map<
 *  //     type_pair<int, bool>,
 *  //     type_pair<double, float>,
 *  //     type_pair<void, std::string>,
 *  //   > map
 *  typedef build_type_map<int, bool, double, float, void, std::string> result2;
 */
template <typename... Args>
using build_type_map = typename detail::type_map_impl::builder<Args...>::type;

///////////////////
// type_map_from //
///////////////////

/**
 * Creates a `type_map` out of a `type_list`.
 *
 * For each element `T` of the list, a corresponding key/value pair will be
 * added to the map, by applying an optional `TKeyTransform` to obtain the key.
 * An optional transform can also be applied to `T` to obtain the mapped value.
 * The default transform uses `T` itself.
 *
 * Example:
 *
 *  typedef type_list<int, bool, double> list;
 *
 *  template <typename> struct Foo {};
 *  template <typename> struct Bar {};
 *
 *  // yield `type_map<
 *  //    type_pair<Foo<int>, int>,
 *  //    type_pair<Foo<bool>, bool>,
 *  //    type_pair<Foo<double>, double>
 *  // >`
 *  typedef type_map_from<Foo>::list<list> result1;
 *
 *  // yield `type_map<
 *  //    type_pair<Foo<int>, Bar<int>>,
 *  //    type_pair<Foo<bool>, Bar<bool>>,
 *  //    type_pair<Foo<double>, Bar<double>>
 *  // >`
 *  typedef type_map_from<Foo, Bar>::list<list> result2;
 *
 *  // yield `type_map<
 *  //    type_pair<int, int>,
 *  //    type_pair<bool, bool>,
 *  //    type_pair<double, double>
 *  // >`
 *  typedef type_map_from<>::list<list> result3;
 *
 * @author: Marcelo Juchem <marcelo@fb.com>
 */
template <
  template <typename...> class TKeyTransform = ::fatal::transform::identity,
  template <typename...> class TValueTransform = ::fatal::transform::identity
>
struct type_map_from {
  template <typename... UArgs>
  using args = type_map<
    typename type_pair_from<
      TKeyTransform, TValueTransform
    >::template type<UArgs>...
  >;

  template <typename UList>
  using list = typename UList::template apply<args>;
};

/////////////////////
// clustered_index //
/////////////////////

namespace detail {

template <
  template <typename...> class TTransform,
  template <typename...> class... TTransforms
>
struct clustered_index_impl {
  template <typename TList>
  using type = typename type_map_from<TTransform>
    ::template list<TList>
    ::template cluster<>
    ::template transform<
      clustered_index_impl<TTransforms...>::template type
    >;
};

template <template <typename...> class TTransform>
struct clustered_index_impl<TTransform> {
  template <typename TList>
  using type = typename type_map_from<TTransform>::template list<TList>;
};

} // namespace detail {

/////////////////////
// clustered_index //
/////////////////////

template <
  typename TList,
  template <typename...> class TTransform,
  template <typename...> class... TTransforms
>
using clustered_index = typename detail::clustered_index_impl<
  TTransform, TTransforms...
>::template type<TList>;

////////////////////////////
// IMPLEMENTATION DETAILS //
////////////////////////////

///////////////////////////////
// recursive_type_merge_sort //
///////////////////////////////

namespace transform {

/**
 * Specialization of `recursive_type_merge_sort` for `type_map`.
 *
 * @author: Marcelo Juchem <marcelo@fb.com>
 */
template <typename... T, std::size_t Depth>
struct recursive_type_merge_sort_impl<type_map<T...>, Depth> {
  using type = typename std::conditional<
    (Depth > 0),
    typename type_map<T...>
      ::template merge_sort<>
      ::template transform<
        recursive_type_merge_sort<Depth - 1>::template type
      >,
    type_map<T...>
  >::type;
};

} // namespace transform {
} // namespace fatal {
