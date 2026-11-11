// SPDX-License-Identifier: GPL-3.0-or-later
#include "clockcore/Validation.hpp"

#include <cctype>

namespace clockcore
{

namespace
{

// ---- POSIX TZ parsing helpers -------------------------------------------------

// Parses a zone abbreviation at s[i]. Returns false on a malformed name.
bool parseTzName(const std::string& s, size_t& i)
{
    if (i < s.size() && s[i] == '<')
    {
        size_t start = ++i;
        while (i < s.size() && s[i] != '>')
        {
            if (!(std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '+' || s[i] == '-'))
            {
                return false;
            }

            ++i;
        }

        if (i >= s.size() || i - start < 3)
        {
            return false;
        }

        ++i; // '>'
        return true;
    }

    size_t start = i;
    while (i < s.size() && std::isalpha(static_cast<unsigned char>(s[i])))
    {
        ++i;
    }

    return i - start >= 3;
}

bool parseNumber(const std::string& s, size_t& i, int maxDigits, int& value)
{
    size_t start = i;
    value = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])) && int(i - start) < maxDigits)
    {
        value = value * 10 + (s[i] - '0');
        ++i;
    }

    return i > start;
}

// [+-]hh[:mm[:ss]]
bool parseTzTime(const std::string& s, size_t& i, int maxHours)
{
    if (i < s.size() && (s[i] == '+' || s[i] == '-'))
    {
        ++i;
    }

    int v;
    if (!parseNumber(s, i, 3, v) || v > maxHours)
    {
        return false;
    }

    for (int part = 0; part < 2 && i < s.size() && s[i] == ':'; ++part)
    {
        ++i;
        if (!parseNumber(s, i, 2, v) || v > 59)
        {
            return false;
        }
    }

    return true;
}

// Mm.w.d[/time] | Jn[/time] | n[/time]
bool parseTzRule(const std::string& s, size_t& i)
{
    int v;
    if (i < s.size() && s[i] == 'M')
    {
        ++i;
        if (!parseNumber(s, i, 2, v) || v < 1 || v > 12)
        {
            return false;
        }

        if (i >= s.size() || s[i++] != '.')
        {
            return false;
        }

        if (!parseNumber(s, i, 1, v) || v < 1 || v > 5)
        {
            return false;
        }

        if (i >= s.size() || s[i++] != '.')
        {
            return false;
        }

        if (!parseNumber(s, i, 1, v) || v > 6)
        {
            return false;
        }
    }
    else if (i < s.size() && s[i] == 'J')
    {
        ++i;
        if (!parseNumber(s, i, 3, v) || v < 1 || v > 365)
        {
            return false;
        }
    }
    else
    {
        if (!parseNumber(s, i, 3, v) || v > 365)
        {
            return false;
        }
    }

    if (i < s.size() && s[i] == '/')
    {
        ++i;
        if (!parseTzTime(s, i, 167))
        {
            return false;
        }
    }

    return true;
}

// Returns the position where the std part ends (0 on error) and whether the rule is valid.
bool parsePosix(const std::string& s, size_t& stdEnd)
{
    size_t i = 0;
    if (!parseTzName(s, i) || !parseTzTime(s, i, 24))
    {
        return false;
    }

    stdEnd = i;
    if (i == s.size())
    {
        return true;
    }

    if (!parseTzName(s, i))
    {
        return false;
    }

    if (i < s.size() && s[i] != ',')
    {
        if (!parseTzTime(s, i, 24))
        {
            return false;
        }
    }

    if (i == s.size())
    {
        return true; // DST name without rules: libc uses its default rule
    }

    if (s[i++] != ',' || !parseTzRule(s, i))
    {
        return false;
    }

    if (i >= s.size() || s[i++] != ',' || !parseTzRule(s, i))
    {
        return false;
    }

    return i == s.size();
}

} // namespace

std::string stripDst(const std::string& posix)
{
    size_t stdEnd = 0;
    if (!parsePosix(posix, stdEnd))
    {
        return std::string();
    }

    return posix.substr(0, stdEnd);
}

bool isValidPosixTz(const std::string& posix)
{
    size_t stdEnd = 0;
    return !posix.empty() && posix.size() <= 64 && parsePosix(posix, stdEnd);
}

bool isValidIPv4(const std::string& s)
{
    int parts = 0;
    size_t i = 0;
    while (parts < 4)
    {
        int v;
        size_t start = i;
        if (!parseNumber(s, i, 3, v) || v > 255)
        {
            return false;
        }

        if (i - start > 1 && s[start] == '0')
        {
            return false; // no leading zeros
        }

        ++parts;
        if (parts < 4)
        {
            if (i >= s.size() || s[i] != '.')
            {
                return false;
            }

            ++i;
        }
    }

    return i == s.size();
}

bool isValidHostName(const std::string& s)
{
    if (s.empty() || s.size() > 63)
    {
        return false;
    }

    size_t labelLen = 0;
    char prev = '.';
    for (char c : s)
    {
        if (c == '.')
        {
            if (labelLen == 0 || prev == '-')
            {
                return false;
            }

            labelLen = 0;
        }
        else if (std::isalnum(static_cast<unsigned char>(c)) || c == '-')
        {
            if (labelLen == 0 && c == '-')
            {
                return false;
            }

            ++labelLen;
        }
        else
        {
            return false;
        }

        prev = c;
    }

    return labelLen > 0 && prev != '-';
}

bool isValidToken(const std::string& s)
{
    if (s.size() < 16 || s.size() > 64)
    {
        return false;
    }

    for (char c : s)
    {
        if (!std::isxdigit(static_cast<unsigned char>(c)))
        {
            return false;
        }
    }

    return true;
}

} // namespace clockcore
