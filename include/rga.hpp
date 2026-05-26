#pragma once

#include "lamport_clock.hpp"
#include <functional>
#include <list>
#include <string>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <nlohmann/json.hpp>

struct RGANode {
  char value;
  uint8_t author;
  uint64_t timestamp;
  bool deleted;
  uint8_t pred_author;
  uint64_t pred_timestamp;
};

class RGA {
  public:
    static constexpr uint8_t SENTINEL_AUTHOR = 0xFF;
    static constexpr uint64_t SENTINEL_TIMESTAMP = 0ULL;

    RGA(uint8_t node_id, LamportClock& clock) : node_id_(node_id), clock_(clock) {}

    void insert(std::size_t pos, char value) {
      uint64_t ts = clock_.get().tick();

      uint8_t pa = SENTINEL_AUTHOR;
      uint64_t pts = SENTINEL_TIMESTAMP;

      if (pos > 0) {
        auto it = nth_visible(pos - 1);
        pa = it->author;
        pts = it->timestamp;
      }

      list_.push_back({value, node_id_, ts, false, pa, pts});
      reorder();
    }

    void remove(std::size_t pos) {
      auto it = nth_visible(pos);
      if (it != list_.end()) it->deleted = true;
    }

    [[nodiscard]] std::string value() const {
      std::string s;
      for (const auto& n : list_) {
        if (!n.deleted) s += n.value;
      }
      return s;
    }

    [[nodiscard]] std::size_t size() const {
      std::size_t c = 0;
      for (const auto& n : list_) {
        if (!n.deleted) ++c;
      }
      return c;
    }

    [[nodiscard]] RGA merge(const RGA& other) const {
      RGA result(node_id_, clock_.get());

      std::vector<RGANode> nodes(list_.begin(), list_.end());

      for (const auto& n : other.list_) {
        auto it = std::find_if(nodes.begin(), nodes.end(), [&](const RGANode& x) {
          return x.author == n.author && x.timestamp == n.timestamp;
        });

        if (it != nodes.end()) {
          if (n.deleted) it->deleted = true;
        } else {
          nodes.push_back(n);
        }
      }

      build_ordered(SENTINEL_AUTHOR, SENTINEL_TIMESTAMP, nodes, result.list_);

      return result;
    }

    bool operator==(const RGA& other) const {
      return value() == other.value();
    }

    [[nodiscard]] nlohmann::json to_json() const {
      nlohmann::json arr = nlohmann::json::array();
      for (const auto& n : list_) {
        arr.push_back({
          {"value", std::string(1, n.value)},
          {"author", n.author},
          {"timestamp", n.timestamp},
          {"deleted", n.deleted},
          {"pred_author", n.pred_author},
          {"pred_timestamp", n.pred_timestamp}
        });
      }
      return arr;
    }

    static RGA from_json(const nlohmann::json& j, uint8_t node_id, LamportClock& clock) {
      RGA result(node_id, clock);
      for (const auto& e: j) {
        RGANode n;
        n.value = e.at("value").get<std::string>()[0];
        n.author = e.at("author").get<uint8_t>();
        n.timestamp = e.at("timestamp").get<uint64_t>();
        n.deleted = e.at("deleted").get<bool>();
        n.pred_author = e.at("pred_author").get<uint8_t>();
        n.pred_timestamp = e.at("pred_timestamp").get<uint64_t>();
        result.list_.push_back(n);
        (void)clock.update(n.timestamp);
      }
      return result;
    }

  private:
    uint8_t node_id_;
    std::reference_wrapper<LamportClock> clock_;
    std::list<RGANode> list_;

    std::list<RGANode>::iterator nth_visible(std::size_t n) {
      std::size_t c = 0;
      for (auto it = list_.begin(); it != list_.end(); ++it) {
        if (!it->deleted) {
          if (c == n) return it;
          ++c;
        }
      }
      return list_.end();
    }

    std::list<RGANode>::const_iterator nth_visible(std::size_t n) const {
      std::size_t c = 0;
      for (auto it = list_.cbegin(); it != list_.cend(); ++it) {
        if (!it->deleted) {
          if (c == n) return it;
          ++c;
        }
      }
      return list_.cend();
    }

    void reorder() {
      std::vector<RGANode> nodes(list_.begin(), list_.end());
      list_.clear();
      build_ordered(SENTINEL_AUTHOR, SENTINEL_TIMESTAMP, nodes, list_);
    }

    static void build_ordered(uint8_t pa, uint64_t pts, std::vector<RGANode>& nodes, std::list<RGANode>& out) {
      std::vector<RGANode*> children;
      for (auto& n : nodes) {
        if (n.pred_author == pa && n.pred_timestamp == pts) {
          children.push_back(&n);
        }
      }

      std::sort(children.begin(), children.end(), [](const RGANode* a, const RGANode* b) {
        if (a->timestamp != b->timestamp) return a->timestamp > b->timestamp;
        return a->author > b->author;
      });

      for (auto* ch : children) {
        out.push_back(*ch);
        build_ordered(ch->author, ch->timestamp, nodes, out);
      }
    }
};