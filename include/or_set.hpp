#pragma once

#include <map>
#include <set>
#include <string>
#include <random>
#include <cstdio>
#include <cstdint>
#include <nlohmann/json.hpp>

template<typename T>
requires requires(T t, nlohmann::json j)
{
  { nlohmann::json(t) };
  { j.get<T>() };
}

class ORSet {
  public:
    void add(const T& element) {
      elements_[element].insert(generate_uuid());
    }

    void remove(const T& element) {
      auto it = elements_.find(element);
      if (it != elements_.end()) {
        it->second.clear();
      }
    }

    [[nodiscard]] bool contains(const T& element) const {
      auto it = elements_.find(element);
      return it != elements_.end() && !it->second.empty();
    }

    [[nodiscard]] std::set<T> value() const {
      std::set<T> result;
      for (const auto& [elem, tags] : elements_) {
        if (!tags.empty()) {
          result.insert(elem);
        }
      }
      return result;
    }

    [[nodiscard]] std::size_t size() const {
      std::size_t count = 0;
      for (const auto& [elem, tags] : elements_) {
        if (!tags.empty()) {
          count++;
        }      }
      return count;
    }

    [[nodiscard]] ORSet<T> merge(const ORSet<T>& other) const {
      ORSet<T> result;
      result.elements_ = elements_;
      for (const auto& [elem, tags] : other.elements_) {
        for (const auto& tag : tags) {
          result.elements_[elem].insert(tag);
        }
      }
      return result;
    }

    bool operator==(const ORSet<T>& other) const {
      return elements_ == other.elements_;
    }

    [[nodiscard]] nlohmann::json to_json() const {
      nlohmann::json arr = nlohmann::json::array();
      for (const auto& [elem, tags] : elements_) {
        arr.push_back({
          {"element", elem},
          {"tags", tags}
        });
      }
      return arr;
    }

    static ORSet<T> from_json(const nlohmann::json& j) {
      ORSet<T> result;
      for (const auto& entry : j) {
        T elem = entry.at("element").get<T>();
        for (const auto& tag : entry.at("tags")) {
          result.elements_[elem].insert(tag.get<std::string>());
        }
      }
      return result;
    }

  private:
    std::map<T, std::set<std::string>> elements_;

    static std::string generate_uuid() {
      static std::random_device rd;
      static std::mt19937 gen(rd());
      static std::uniform_int_distribution<uint32_t> dist;

      uint32_t p1 = dist(gen);
      uint32_t p2 = dist(gen);
      uint32_t p3 = dist(gen);
      uint32_t p4 = dist(gen);

      uint32_t f3 = (p2 & 0x0FFFu) | 0x4000u;
      uint32_t f4 = ((p3 >> 16) & 0x0FFFu) | 0x8000u;

      char buf[37];
      std::snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%04x%08x",
        static_cast<unsigned>(p1),
        static_cast<unsigned>(p2 >> 16),
        static_cast<unsigned>(f3),
        static_cast<unsigned>(f4),
        static_cast<unsigned>(p3 & 0xFFFFu),
        static_cast<unsigned>(p4));
      return std::string(buf);
    }
};