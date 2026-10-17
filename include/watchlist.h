#pragma once
#include "core.h"
#include <stdlib.h>
namespace dash {
class Watchlist {
public:
  enum Kind : uint8_t { Mac, Oui, Name, Company, Service };
  struct Rule {
    Kind kind;
    uint8_t address[6]{};
    uint16_t id = 0;
    char text[25]{};
  };
  void clear() {
    count_ = 0;
  }
  unsigned size() const {
    return count_;
  }
  bool add(const char *line) {
    if (count_ == 16 || !line || strlen(line) > 80)
      return false;
    char kind[12]{}, value[40]{}, extra;
    if (sscanf(line, "%11[^,],%39[^\r\n]%c", kind, value, &extra) < 2)
      return false;
    Rule r{};
    if (!strcmp(kind, "MAC") || !strcmp(kind, "OUI")) {
      r.kind = !strcmp(kind, "MAC") ? Mac : Oui;
      unsigned length = r.kind == Mac ? 12 : 6;
      if (strlen(value) != length)
        return false;
      for (unsigned i = 0; i < length; ++i)
        if (!isxdigit((unsigned char)value[i]))
          return false;
      for (unsigned i = 0; i < length / 2; ++i) {
        char byte[] = {value[i * 2], value[i * 2 + 1], 0};
        r.address[i] = strtoul(byte, nullptr, 16);
      }
    } else if (!strcmp(kind, "NAME")) {
      if (!*value || strlen(value) > 24)
        return false;
      r.kind = Name;
      strcpy(r.text, value);
    } else if (!strcmp(kind, "COMPANY") || !strcmp(kind, "SERVICE")) {
      if (strlen(value) != 4)
        return false;
      for (unsigned i = 0; i < 4; ++i)
        if (!isxdigit((unsigned char)value[i]))
          return false;
      r.kind = !strcmp(kind, "COMPANY") ? Company : Service;
      r.id = strtoul(value, nullptr, 16);
    } else
      return false;
    rules_[count_++] = r;
    return true;
  }
  Match match(const uint8_t *mac, bool publicAddress, const char *name,
              const uint8_t *adv = nullptr, size_t length = 0) const {
    for (unsigned i = 0; i < count_; ++i) {
      const auto &r = rules_[i];
      bool found = false;
      if (r.kind == Mac)
        found = !memcmp(mac, r.address, 6);
      if (r.kind == Oui)
        found = publicAddress && !(mac[0] & 3) && !memcmp(mac, r.address, 3);
      if (r.kind == Name)
        found = contains(name, r.text);
      if (adv && (r.kind == Company || r.kind == Service)) {
        for (size_t at = 0; at < length;) {
          uint8_t n = adv[at++];
          if (!n)
            break;
          if (n > length - at)
            return {};
          uint8_t type = adv[at];
          const uint8_t *d = adv + at + 1;
          size_t z = n - 1;
          if (r.kind == Company && type == 0xff && z >= 2)
            found |= le16(d) == r.id;
          if (r.kind == Service) {
            unsigned width = (type == 2 || type == 3 || type == 0x16)   ? 2
                             : (type == 4 || type == 5 || type == 0x20) ? 4
                             : (type == 6 || type == 7 || type == 0x21) ? 16
                                                                        : 0;
            bool data = type == 0x16 || type == 0x20 || type == 0x21;
            if (width)
              for (size_t k = 0; k + width <= z; k += width) {
                const uint8_t *v = d + k;
                static const uint8_t base[] = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0,
                                               0,    0x80, 0,    0x10, 0,    0};
                bool assigned = width == 2 || (width == 4 && !v[2] && !v[3]) ||
                                (width == 16 && !memcmp(v, base, 12) && !v[14] && !v[15]);
                if (assigned)
                  found |= le16(v + (width == 16 ? 12 : 0)) == r.id;
                if (data)
                  break;
              }
          }
          at += n;
        }
      }
      if (found)
        return {Family::Watchlist, Confidence::High,
                "User watchlist match; identity not authenticated"};
    }
    return {};
  }

private:
  Rule rules_[16];
  unsigned count_ = 0;
};
} // namespace dash
