#include <string_view>
#include <tuple>
#include <algorithm>
#include <array>

template <typename CharT> constexpr CharT lowercase_bit = CharT{'a'} - CharT{'A'}; // 0x20

template <typename CharT> [[msvc::forceinline]] [[gnu::always_inline]] static constexpr auto to_lower(CharT c) noexcept {
    return c | lowercase_bit<CharT>;
}

static_assert(to_lower('a') == 'a');
static_assert(to_lower('A') == 'a');

template <typename CharT> [[msvc::forceinline]] [[gnu::always_inline]] static constexpr bool is_alpha(CharT in) noexcept {
    const CharT c = to_lower(in);
    // this is technically undefined, as EBDIC doesn't have connected [A-Z]
    return (c >= CharT{'a'}) & (c <= CharT{'z'});
}

static_assert(is_alpha('a'));
static_assert(is_alpha('A'));
static_assert(!is_alpha('0'));
static_assert(!is_alpha('@'));

struct case_sensitivity_tag {
    bool value;
    explicit consteval case_sensitivity_tag(bool v) noexcept: value{v} { }

    explicit consteval operator bool() const noexcept {
        return value;
    }
};

constexpr auto case_insensitive = case_sensitivity_tag{false};
constexpr auto case_sensitive = case_sensitivity_tag{true};

template <case_sensitivity_tag CaseSensitive> 
struct wildcard_comparator {
    template <typename CharT> [[msvc::forceinline]] [[gnu::always_inline]] constexpr bool operator()(CharT pattern, CharT subject) noexcept {
        if (pattern == CharT{'?'}) {
            // I would probably replace it only with return true;
            return subject != CharT{'.'};
        }

        if constexpr (CaseSensitive) {
            return pattern == subject;
        } else {
            // if we are ignoring cases...
            return pattern == subject || is_alpha(pattern) & is_alpha(subject) & (to_lower(pattern)) == (to_lower(subject));
        }
    }
};

// take iterators and support sentinels too
template <case_sensitivity_tag CaseSensitive, typename PatternIt, typename SubjectIt>
constexpr bool glob(PatternIt pit, const PatternIt pend, SubjectIt sit, const SubjectIt send) noexcept {
    using char_t = decltype(*pit);

    for (;;) {

        // this will match everything it can and return where the iterators are different or if one is at the end
        std::tie(pit, sit) = std::mismatch(pit, pend, sit, send, wildcard_comparator<CaseSensitive>{});

        // if we are at the end, we can tell it to user
        if (pit == pend) {
            // but only if both are at the end we were successful
            return sit == send;
        }

        // if stopped on anything else than wildcard (questionmark is dealt with in compare function)
        if (*pit != char_t{'*'}) {
            return false;
        }

        // deal with the star, recursively and lazily, which means we will try next char instead and prefer these
        // the recursion will be as much as deep as length of the pattern
        if (glob<CaseSensitive>(std::next(pit), pend, sit, send)) {
            // and if we were succesfull, we are done
            return true;
        }
        
        // we fail if we are at end of the subject...
        if (sit == send) {
            return false;
        }
        
        // otherwise we can try next character in subject
        // as it's processed by the star
        sit = std::next(sit);
    }
}

// just take iterators
template <case_sensitivity_tag CaseSensitive, typename CharT> 
constexpr bool glob(std::basic_string_view<CharT> pattern, std::basic_string_view<CharT> subject) noexcept {
    return glob<CaseSensitive>(pattern.begin(), pattern.end(), subject.begin(), subject.end());
}

template <typename A, typename B> concept convertible_to_same_string_view = requires (const A & a, const B & b) {
    std::basic_string_view{a};
    std::basic_string_view{b};

    requires std::same_as<typename decltype(std::basic_string_view{a})::value_type, typename decltype(std::basic_string_view{b})::value_type>;
};

// convert anything to string_view
template <case_sensitivity_tag CaseSensitive, typename A, typename B> requires convertible_to_same_string_view<A,B>
constexpr bool glob(const A & a, const B & b) noexcept {
    return glob<CaseSensitive>(std::basic_string_view{a}, std::basic_string_view{b});
}

static_assert(!glob<case_sensitive>("abc", "def"));
static_assert(!glob<case_sensitive>("abc", "ABC"));
static_assert(glob<case_insensitive>("abc", "ABC"));
static_assert(glob<case_sensitive>("abc", "abc"));
static_assert(glob<case_sensitive>("a?c", "abc"));
static_assert(glob<case_sensitive>("a*c", "axxxxxc"));
static_assert(glob<case_sensitive>("a*b*c", "axxxbxxc"));
static_assert(!glob<case_sensitive>("a*b*c", "axxxxxxc"));
static_assert(glob<case_insensitive>("*.exe", "aloha.EXE"));
static_assert(glob<case_insensitive>("***abc***", "abc"));
static_assert(glob<case_insensitive>("***a?c***", "xxxxxxxaxcxxxxxx"));
static_assert(glob<case_insensitive>("abc*", "abc"));
static_assert(glob<case_insensitive>("*abc", "abc"));

// support for template fixed_string

template <typename CharT, size_t Length> struct fixed_string: std::array<CharT, Length> {
    using super = std::array<CharT, Length>;

    consteval fixed_string(const CharT (&in)[Length]) noexcept: super(std::to_array(in)) { }

    constexpr size_t size() const noexcept {
        return Length - 1u; // zero-terminated
    }

    constexpr auto to_view() const noexcept {
        return std::basic_string_view<CharT>(super::data(), size());
    }
};

template <fixed_string Pattern, case_sensitivity_tag CaseSensitive = case_insensitive, std::same_as<typename decltype(Pattern)::value_type> CharT>
constexpr bool glob_match(std::basic_string_view<CharT> in) noexcept {
    return glob<CaseSensitive>(Pattern.to_view(), in);
}

using namespace std::string_view_literals;

static_assert(glob_match<"*.exe">("aloha.EXE"sv));
static_assert(glob_match<"ver??.txt">("ver92.txt"sv));

// this is non-constexpr to force instantiation of glob function
bool glob_me(std::string_view pattern, std::string_view subject) noexcept {
    return glob<case_insensitive>(pattern, subject);
}