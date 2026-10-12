// ARYAN SHOKEEN
// MT26116

#pragma once
// Money: fixed-point currency amount stored in minor units.
// Amounts with different currency codes cannot be combined or compared.

#include <utility>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace bookmgmt {

class Money {
public:
    Money() = default;

    // Default currency is INR for backward compatibility.
    static Money fromMinor(std::int64_t minor,
                       std::string currency = "INR") {
    return Money(minor, std::move(currency));
    }

    static Money of(std::int64_t major, int minor = 0,
                    std::string currency = "INR");

    constexpr std::int64_t minorUnits() const { return minor_; }
    const std::string& currency() const { return currency_; }

    double toDouble() const {
        return static_cast<double>(minor_) / 100.0;
    }

    std::string toString() const;

    constexpr bool isZero() const { return minor_ == 0; }
    constexpr bool isNegative() const { return minor_ < 0; }

    Money& operator+=(const Money& other);
    Money& operator-=(const Money& other);
    Money& operator*=(std::int64_t k) {
        minor_ *= k;
        return *this;
    }

    friend Money operator+(Money a, const Money& b) { return a += b; }
    friend Money operator-(Money a, const Money& b) { return a -= b; }
    friend Money operator*(Money a, std::int64_t k) { return a *= k; }
    friend Money operator*(std::int64_t k, Money a) { return a *= k; }

    friend bool operator==(const Money& a, const Money& b);
    friend bool operator!=(const Money& a, const Money& b);
    friend bool operator<(const Money& a, const Money& b);
    friend bool operator<=(const Money& a, const Money& b);
    friend bool operator>(const Money& a, const Money& b);
    friend bool operator>=(const Money& a, const Money& b);

private:
    Money(std::int64_t minor, std::string currency)
        : minor_(minor), currency_(std::move(currency)) {}

    std::int64_t minor_ = 0;
    std::string currency_ = "INR";
};

std::ostream& operator<<(std::ostream& os, const Money& m);

}  // namespace bookmgmt