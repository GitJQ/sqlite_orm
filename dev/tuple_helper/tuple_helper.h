#pragma once

#include <cstddef>
#include <type_traits>  //  std::false_type, std::true_type
#include <tuple>  //  std::tuple, std::get, std::tuple_element, std::tuple_size
#include <array>

#include "../static_magic.h"

namespace sqlite_orm {

    //  got from here http://stackoverflow.com/questions/25958259/how-do-i-find-out-if-a-tuple-contains-a-type
    namespace tuple_helper {

        /**
         *  HAS_TYPE type trait
         */
        template<typename T, typename Tuple>
        struct has_type;

        template<typename T>
        struct has_type<T, std::tuple<>> : std::false_type {};

        template<typename T, typename U, typename... Ts>
        struct has_type<T, std::tuple<U, Ts...>> : has_type<T, std::tuple<Ts...>> {};

        template<typename T, typename... Ts>
        struct has_type<T, std::tuple<T, Ts...>> : std::true_type {};

        template<typename T, typename Tuple>
        using tuple_contains_type = typename has_type<T, Tuple>::type;

        /**
         *  HAS_SOME_TYPE type trait
         */
        template<template<class> class TT, typename Tuple>
        struct has_some_type;

        template<template<class> class TT>
        struct has_some_type<TT, std::tuple<>> : std::false_type {};

        template<template<class> class TT, typename U, typename... Ts>
        struct has_some_type<TT, std::tuple<U, Ts...>> : has_some_type<TT, std::tuple<Ts...>> {};

        template<template<class> class TT, typename T, typename... Ts>
        struct has_some_type<TT, std::tuple<TT<T>, Ts...>> : std::true_type {};

        template<template<class> class TT, typename Tuple>
        using tuple_contains_some_type = typename has_some_type<TT, Tuple>::type;

        template<size_t N, class... Args>
        struct iterator_impl {

            template<class L>
            void operator()(const std::tuple<Args...>& tuple, const L& lambda, bool reverse = true) {
                if(reverse) {
                    lambda(std::get<N>(tuple));
                    iterator_impl<N - 1, Args...>()(tuple, lambda, reverse);
                } else {
                    iterator_impl<N - 1, Args...>()(tuple, lambda, reverse);
                    lambda(std::get<N>(tuple));
                }
            }

            template<class L>
            void operator()(const L& lambda) {
                iterator_impl<N - 1, Args...>()(lambda);
                lambda((const typename std::tuple_element<N - 1, std::tuple<Args...>>::type*)nullptr);
            }
        };

        template<class... Args>
        struct iterator_impl<0, Args...> {

            template<class L>
            void operator()(const std::tuple<Args...>& tuple, const L& lambda, bool /*reverse*/ = true) {
                lambda(std::get<0>(tuple));
            }

            template<class L>
            void operator()(const L& lambda) {
                lambda((const typename std::tuple_element<0, std::tuple<Args...>>::type*)nullptr);
            }
        };

        template<size_t N>
        struct iterator_impl<N> {

            template<class L>
            void operator()(const std::tuple<>&, const L&, bool /*reverse*/ = true) {
                //..
            }

            template<class L>
            void operator()(const L&) {
                //..
            }
        };

        template<class... Args>
        struct iterator_impl2;

        template<>
        struct iterator_impl2<> {

            template<class L>
            void operator()(const L&) const {
                //..
            }
        };

        template<class H, class... Tail>
        struct iterator_impl2<H, Tail...> {

            template<class L>
            void operator()(const L& lambda) const {
                lambda((const H*)nullptr);
                iterator_impl2<Tail...>{}(lambda);
            }
        };

        template<class T>
        struct iterator;

        template<class... Args>
        struct iterator<std::tuple<Args...>> {

            template<class L>
            void operator()(const L& lambda) const {
                iterator_impl2<Args...>{}(lambda);
            }
        };
    }

    namespace internal {

        //  got it form here https://stackoverflow.com/questions/7858817/unpacking-a-tuple-to-call-a-matching-function-pointer
        template<class Function, class FunctionPointer, class Tuple, size_t... I>
        auto call_impl(Function& f, FunctionPointer functionPointer, Tuple t, std::index_sequence<I...>) {
            return (f.*functionPointer)(std::get<I>(move(t))...);
        }

        template<class Function, class FunctionPointer, class Tuple>
        auto call(Function& f, FunctionPointer functionPointer, Tuple t) {
            static constexpr auto size = std::tuple_size<Tuple>::value;
            return call_impl(f, functionPointer, move(t), std::make_index_sequence<size>{});
        }

        template<class Function, class Tuple>
        auto call(Function& f, Tuple t) {
            return call(f, &Function::operator(), move(t));
        }

        template<size_t N, size_t I, class L, class R>
        void move_tuple_impl(L& lhs, R& rhs) {
            std::get<I>(lhs) = std::move(std::get<I>(rhs));
            static_if<std::integral_constant<bool, N != I + 1>{}>([](auto& l, auto& r) {
                move_tuple_impl<N, I + 1>(l, r);
            })(lhs, rhs);
        }

        template<size_t N, class L, class R>
        void move_tuple(L& lhs, R& rhs) {
            using bool_type = std::integral_constant<bool, N != 0>;
            static_if<bool_type{}>([](auto& l, auto& r) {
                move_tuple_impl<N, 0>(l, r);
            })(lhs, rhs);
        }

        template<class L, class... Args>
        void iterate_tuple(const std::tuple<Args...>& tuple, const L& lambda) {
            using tuple_type = std::tuple<Args...>;
            tuple_helper::iterator_impl<std::tuple_size<tuple_type>::value - 1, Args...>()(tuple, lambda, false);
        }

        template<class T, class L>
        void iterate_tuple(const L& lambda) {
            tuple_helper::iterator<T>{}(lambda);
        }

        template<typename... input_t>
        using tuple_cat_t = decltype(std::tuple_cat(std::declval<input_t>()...));

        template<class... Args>
        struct conc_tuple {
            using type = tuple_cat_t<Args...>;
        };

        template<class T, template<class C> class F>
        struct tuple_filter;

        template<class... Args, template<class C> class F>
        struct tuple_filter<std::tuple<Args...>, F>
            : conc_tuple<std::conditional_t<F<Args>::value, std::tuple<Args>, std::tuple<>>...> {};

        template<class Tpl, template<class C> class F>
        using filter_tuple_t = typename tuple_filter<Tpl, F>::type;

        /** 
         *  Get the value of an index_sequence at a specific position.
         */
        template<size_t... Idx>
        SQLITE_ORM_CONSTEVAL size_t index_sequence_value(size_t pos, std::index_sequence<Idx...>) {
            constexpr std::array<size_t, sizeof...(Idx)> values{Idx...};
            return values[pos];
        }

        template<class... Seq>
        struct concat_idx_seq;

        template<size_t... Idx>
        struct concat_idx_seq<std::index_sequence<Idx...>> {
            using type = std::index_sequence<Idx...>;
        };

        template<size_t... As, size_t... Bs, class... Seq>
        struct concat_idx_seq<std::index_sequence<As...>, std::index_sequence<Bs...>, Seq...>
            : concat_idx_seq<std::index_sequence<As..., Bs...>, Seq...> {};

        template<class Tpl, template<class...> class F, class Seq>
        struct filter_tuple_sequence;

        template<class Tpl, template<class...> class F, size_t... Idx>
        struct filter_tuple_sequence<Tpl, F, std::index_sequence<Idx...>>
            : concat_idx_seq<std::conditional_t<F<std::tuple_element_t<Idx, Tpl>>::value,
                                                std::index_sequence<Idx>,
                                                std::index_sequence<>>...> {};

        template<class Tpl, template<class...> class F>
        using filter_tuple_sequence_t =
            typename filter_tuple_sequence<Tpl, F, std::make_index_sequence<std::tuple_size<Tpl>::value>>::type;
    }
}
