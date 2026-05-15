#pragma once

#include <set>
#include <algorithm>
#include <stdexcept>
#include <nlohmann/json.hpp>

template <typename T>
concept JsonSerializable = requires(T t, nlohmann::json j)
{
  { nlohmann::json(t) };
  { j.get<T>() };
};

template <typename T>
requires JsonSerializable<T>
class GSet {
  public:
    void add(const T& element) {
      elements_.insert(element);
    }

    void remove(const T&) {
      throw std::logic_error("G-Set is grow-only");
    }

    [[nodiscard]] bool contains(const T& element) const {
      return elements_.count(element) > 0;
    }

    [[nodiscard]] std::size_t size() const {
      return elements_.size();
    }

    [[nodiscard]] const std::set<T>& value() const {
      return elements_;
    }

    [[nodiscard]] GSet<T> merge(const GSet<T>& other) const {
      GSet<T> result;
      result.elements_ = elements_;
      result.elements_.insert(other.elements_.begin(), other.elements_.end());
      return result;
    }

    bool operator==(const GSet<T>& other) const {
      return elements_ == other.elements_;
    }

    [[nodiscard]] nlohmann::json to_json() const {
      return nlohmann::json(elements_);
    }

    static GSet<T> from_json(const nlohmann::json& j) {
      GSet<T> result;
      for (const auto& item : j) {
        result.elements_.insert(item.get<T>());
      }
      return result;
    }

  private:
    std::set<T> elements_;
};