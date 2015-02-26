/*
 *  Copyright (c) 2014, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 */

#pragma once

#include <fatal/math/numerics.h>
#include <fatal/type/list.h>
#include <fatal/type/traits.h>

#include <type_traits>

#include <cassert>

namespace fatal {

/**
 * A type-safe way to represent a set of boolean flags.
 *
 * Assume, in the examples below, that these types are available:
 *
 *  struct my_flag_1 {};
 *  struct my_flag_2 {};
 *  struct my_flag_3 {};
 *  struct my_flag_4 {};
 *
 * @author: Marcelo Juchem <marcelo@fb.com>
 */
template <typename... TFlags>
struct flag_set {
  /**
   * The list of supported flags.
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  typedef type_list<TFlags...> tag_list;

  /**
   * The integral representation of the flag_set.
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  typedef smallest_fast_unsigned_integral<tag_list::size> flags_type;

  static_assert(
    tag_list::size <= data_bits<flags_type>::value,
    "too many flags"
  );

  static_assert(
    logical_and_constants<
      std::true_type,
      std::is_same<TFlags, typename std::decay<TFlags>::type>...
    >::value,
    "unsupported tag"
  );

private:
  typedef mersenne_number<tag_list::size> range_mask;

  template <bool IgnoreUnsupported, typename... UFlags>
  using mask_for = bitwise_or_constants<
    std::integral_constant<flags_type, 0>,
    std::integral_constant<
      typename std::enable_if<
        IgnoreUnsupported || tag_list::template contains<UFlags>::value,
        flags_type
      >::type,
      (tag_list::template index_of<UFlags>::value < tag_list::size
        ? (flags_type(1) << tag_list::template index_of<UFlags>::value)
        : 0
      )
    >...
  >;

  template <typename... UFlags>
  struct import_foreign {
    typedef flag_set<UFlags...> foreign_set;

    struct visitor {
      template <typename UFlag, std::size_t Index>
      void operator ()(
        indexed_type_tag<UFlag, Index>,
        foreign_set const &foreign,
        flags_type &out
      ) {
        if (foreign.template is_set<UFlag>()) {
          out |= mask_for<true, UFlag>::value;
        }
      }
    };

    static flags_type import(foreign_set const &foreign) {
      flags_type flags(0);
      foreign_set::tag_list::foreach(visitor(), foreign, flags);
      assert((flags & range_mask::value) == flags);
      return flags;
    }
  };

public:
  /**
   * Default constructor that start with all flags unset.
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  flag_set(): flags_(0) {}

  flag_set(flag_set const &) = default;
  flag_set(flag_set &&) = default;

  /**
   * Constructor that takes a list of flags to be initialized as set.
   *
   * Example:
   *
   *  // will initialize the set with `my_flag_1` and `my_flag_2`
   *  flag_set<my_flag_1, my_flag_2, my_flag_3> s{my_flag_1(), my_flag_2()};
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <
    typename... UFlags,
    typename X = safe_ctor_overload_t<flag_set, UFlags...>
  >
  explicit flag_set(UFlags &&...):
    flags_(mask_for<false, typename std::decay<UFlags>::type...>::value)
  {}

  /**
   * Unsets all the flags in this `flag_set`.
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  void clear() { flags_ = 0; }

  /**
   * Initializes this set setting all supported flags that are set in `other`,
   * ignoring the unsupported ones.
   *
   * Example:
   *
   *  flag_set<my_flag_1, my_flag_2, my_flag_3> s{my_flag_1(), my_flag_3()};
   *
   *  // only `my_flag_3` will be initially set since that's the only
   *  // supported flag that's set in `s`.
   *  flag_set<my_flag_2, my_flag_3, my_flag_4> r(s);
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... UFlags>
  flag_set(flag_set<UFlags...> const &other):
    flags_(import_foreign<UFlags...>::import(other))
  {}

  /**
   * Sets the given flags.
   *
   * Example:
   *
   *  flag_set<my_flag_1, my_flag_2, my_flag_3> s;
   *
   *  // sets `my_flag_1` and `my_flag_2`
   *  s.set(my_flag_1(), my_flag_2());
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... UFlags>
  flag_set &set(UFlags &&...) & {
    set<typename std::decay<UFlags>::type...>();
    return *this;
  }

  /**
   * Sets the given flags.
   *
   * Example:
   *
   *  // sets `my_flag_1` and `my_flag_2`
   *  auto s = flag_set<my_flag_1, my_flag_2, my_flag_3>()
   *    .set(my_flag_1(), my_flag_2());
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... UFlags>
  flag_set &&set(UFlags &&...) && {
    set<typename std::decay<UFlags>::type...>();
    return std::move(*this);
  }

  /**
   * Sets the given flags.
   *
   * Example:
   *
   *  flag_set<my_flag_1, my_flag_2, my_flag_3> s;
   *
   *  // sets `my_flag_1` and `my_flag_2`
   *  s.set<my_flag_1, my_flag_2>();
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... UFlags>
  flag_set &set() & {
    flags_ |= mask_for<false, UFlags...>::value;
    assert((flags_ & range_mask::value) == flags_);
    return *this;
  }

  /**
   * Sets the given flags.
   *
   * Example:
   *
   *  // sets `my_flag_1` and `my_flag_2`
   *  auto s = flag_set<my_flag_1, my_flag_2, my_flag_3>()
   *    .set<my_flag_1, my_flag_2>();
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... UFlags>
  flag_set &&set() && {
    flags_ |= mask_for<false, UFlags...>::value;
    assert((flags_ & range_mask::value) == flags_);
    return std::move(*this);
  }

  /**
   * Sets the `UFlag` flag iff the `condition` is true, otherwise leave it
   * unchanged.
   *
   * Example:
   *
   *  flag_set<my_flag_1, my_flag_2, my_flag_3> s;
   *
   *  // sets `my_flag_1`
   *  s.set_if<my_flag_1>(true);
   *
   *  // leaves `my_flag_2` unchanged
   *  s.set_if<my_flag_2>(false);
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename UFlag>
  flag_set &set_if(bool condition) & {
    if (condition) {
      set<UFlag>();
    }

    return *this;
  }


  /**
   * Sets the `UFlag` flag iff the `condition` is true, otherwise leave it
   * unchanged.
   *
   * Example:
   *
   *  // sets `my_flag_1` but leaves `my_flag_2` unchanged
   *  auto s = flag_set<my_flag_1, my_flag_2, my_flag_3>()
   *    .set_if<my_flag_1>(true)
   *    .set_if<my_flag_2>(false);
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename UFlag>
  flag_set &&set_if(bool condition) && {
    if (condition) {
      set<UFlag>();
    }

    return std::move(*this);
  }

  /**
   * Resets resets this flag set to contain exactly the given flags.
   *
   * This is the same as calling `clear()` then `set(flags...)`.
   *
   * Example:
   *
   *  flag_set<my_flag_1, my_flag_2, my_flag_3, my_flag_4> s{
   *    my_flag_1(), my_flag_3()
   *  };
   *
   *  // leaves only `my_flag_2` and `my_flag_4` set
   *  s.reset(my_flag_2(), my_flag_4());
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... UFlags>
  void reset(UFlags &&...) { reset<typename std::decay<UFlags>::type...>(); }

  /**
   * Resets resets this flag set to contain exactly the given flags.
   *
   * This is the same as calling `clear()` then `set<UFlags...>()`.
   *
   * Example:
   *
   *  flag_set<my_flag_1, my_flag_2, my_flag_3, my_flag_4> s{
   *    my_flag_1(), my_flag_3()
   *  };
   *
   *  // leaves only `my_flag_2` and `my_flag_4` set
   *  s.reset<my_flag_2, my_flag_4>();
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... UFlags>
  void reset() {
    flags_ = mask_for<false, UFlags...>::value;
    assert((flags_ & range_mask::value) == flags_);
  }

  /**
   * Tells whether all given flag are set.
   *
   * Example:
   *
   *  flag_set<my_flag_1, my_flag_2, my_flag_3> s{my_flag_1(), my_flag_3()};
   *
   *  // yields `true`
   *  s.is_set(my_flag_1(), my_flag_3());
   *
   *  // yields `true`
   *  s.is_set(my_flag_1());
   *
   *  // yields `false`
   *  s.is_set(my_flag_1(), my_flag_2());
   *
   *  // yields `false`
   *  s.is_set(my_flag_2());
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... UFlags>
  bool is_set(UFlags &&...) const {
    return is_set<typename std::decay<UFlags>::type...>();
  }

  /**
   * Tells whether all given flag are set.
   *
   * Example:
   *
   *  flag_set<my_flag_1, my_flag_2, my_flag_3> s{my_flag_1(), my_flag_3()};
   *
   *  // yields `true`
   *  s.is_set<my_flag_1, my_flag_3>();
   *
   *  // yields `true`
   *  s.is_set<my_flag_1>();
   *
   *  // yields `false`
   *  s.is_set<my_flag_1, my_flag_2>();
   *
   *  // yields `false`
   *  s.is_set<my_flag_2>();
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... UFlags>
  bool is_set() const {
    typedef mask_for<false, UFlags...> mask;
    return (flags_ & mask::value) == mask::value;
  }

  /**
   * Adds `UFlag` to the list of supported flags (in which case it returns a
   * new type), if not supported already (in which case it returns the same
   * type).
   *
   * This is the type returned by `expand()`.
   *
   *  // yields `flag_set<my_flag_1>`
   *  typedef flag_set<>::expanded<my_flag_1> result0;
   *
   *  typedef flag_set<my_flag_2, my_flag_3> my_set;
   *
   *  // yields `flag_set<my_flag_2, my_flag_3, my_flag_4>`
   *  typedef my_set::expanded<my_flag_4> result1;
   *
   *  // yields `flag_set<my_flag_2, my_flag_3, my_flag_1>`
   *  typedef my_set::expanded<my_flag_1> result2;
   *
   *  // yields `flag_set<my_flag_2, my_flag_3>`
   *  typedef my_set::expanded<my_flag_2> result3;
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename UFlag>
  using expanded = typename std::conditional<
    tag_list::template contains<UFlag>::value,
    flag_set,
    typename tag_list::template apply_back<fatal::flag_set, UFlag>
  >::type;

  /**
   * Expands the list of supported flags with `UFlag` and sets is in addition
   * to what's already set.
   *
   * Returns the result as a new set, leaving this set unchanged.
   *
   * Refer to `expanded` for more details.
   *
   * Example:
   *
   *  flag_set<> e;
   *
   *  // yields `flag_set<my_flag_1>` with `my_flag_1` set
   *  auto s0 = e.expand<my_flag_1>();
   *
   *  // yields `flag_set<my_flag_1>` with `my_flag_1` set
   *  auto s1 = flag_set<>().expand<my_flag_1>();
   *
   *  flag_set<my_flag_2, my_flag_3> s{my_flag_2()};
   *
   *  // yields `flag_set<my_flag_2, my_flag_3, my_flag_4>`
   *  // with `my_flag_2` and `my_flag_4` set
   *  typedef s.expand<my_flag_4>();
   *
   *  // yields `flag_set<my_flag_2, my_flag_3, my_flag_1>`
   *  // with `my_flag_2` and `my_flag_1` set
   *  typedef s.expand<my_flag_1>();
   *
   *  // yields `flag_set<my_flag_2, my_flag_3>` with `my_flag_2`
   *  typedef s.expand<my_flag_2>();
   *
   *  // yields `flag_set<my_flag_2, my_flag_3>`
   *  // with `my_flag_2` and `my_flag_3` set
   *  typedef s.expand<my_flag_3>();
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename UFlag>
  expanded<UFlag> expand() const {
    return expanded<UFlag>(*this).template set<UFlag>();
  }

  /**
   * Expands the list of supported flags with `UFlag` and leaves set whatever
   * was already set. If the condition is `true`, also sets `UFlag`.
   *
   * Returns the result as a new set, leaving this set unchanged.
   *
   * Refer to `expanded` and `expand` for more details.
   *
   * Example:
   *
   *  flag_set<> e;
   *
   *  // yields `flag_set<my_flag_1>` with `my_flag_1` set
   *  auto s0 = e.expand_if<my_flag_1>(true);
   *
   *  // yields `flag_set<my_flag_1>` with nothing set
   *  auto s1 = e.expand_if<my_flag_1>(false);
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename UFlag>
  expanded<UFlag> expand_if(bool condition) const {
    return expanded<UFlag>(*this).template set_if<UFlag>(condition);
  }

  /**
   * Gets the integral representation of this `flag_set`.
   *
   * The supported flags are set from the lowest to the highest significant bit.
   *
   * Example:
   *
   *  // yields `0b111`
   *  flag_set<my_flag_1, my_flag_2, my_flag_3>{
   *    my_flag_1(), my_flag_3(), my_flag_3()
   *  }.get();
   *
   *  // yields `0b101`
   *  flag_set<my_flag_1, my_flag_2, my_flag_3>{my_flag_1(), my_flag_3()}.get();
   *
   *  // yields `0b001`
   *  flag_set<my_flag_1, my_flag_2, my_flag_3>{my_flag_1()}.get();
   *
   *  // yields `0b100`
   *  flag_set<my_flag_1, my_flag_2, my_flag_3>{ my_flag_3()}.get();
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  fast_pass<flags_type> get() const { return flags_; }

  /**
   * Assignment operator. Sets this `flag_set` with exactly what's set in `rhs`.
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  flag_set &operator =(flag_set const &rhs) {
    flags_ = rhs.flags_;
    assert((flags_ & range_mask::value) == flags_);

    return *this;
  }

  /**
   * Assignment operator. Sets this `flag_set` with exactly what's set in `rhs`,
   * ignoring unsupported types.
   *
   * @author: Marcelo Juchem <marcelo@fb.com>
   */
  template <typename... UFlags>
  flag_set &operator =(flag_set<UFlags...> const &rhs) {
    flags_ = import_foreign<UFlags...>::import(rhs);
    return *this;
  }

private:
  flags_type flags_;
};

} // namespace fatal {
