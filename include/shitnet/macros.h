#pragma once

#define fn auto
#define cfn extern "C" auto
#define let auto

#define CPP_MATCH_CASE_1(T) [&](T const &)
#define CPP_MATCH_CASE_2(T, name) [&](T const &name)
#define CPP_MATCH_GET(_1, _2, NAME, ...) NAME
#define case_(...)                                                             \
    CPP_MATCH_GET(__VA_ARGS__, CPP_MATCH_CASE_2, CPP_MATCH_CASE_1)(__VA_ARGS__)
