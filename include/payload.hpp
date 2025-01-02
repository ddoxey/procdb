#pragma once

#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Enum for payload types
enum class PayloadType : uint8_t {
  PROCESS_STATS = 0,
  NETWORK_STATS = 1,
  PROCESS_LIST = 2
};

// Unordered map for human-readable descriptions
inline const std::unordered_map<PayloadType, std::string> kPayloadTypeToString =
    {{PayloadType::PROCESS_STATS, "Process Statistics"},
     {PayloadType::NETWORK_STATS, "Network Statistics"},
     {PayloadType::PROCESS_LIST, "Process List"}};

// Function to get the string description of a PayloadType
inline std::string PayloadTypeToString(PayloadType type) {
  auto it = kPayloadTypeToString.find(type);
  if (it != kPayloadTypeToString.end()) {
    return it->second;
  }
  return "Unknown Type";
}

// Struct for individual fields
struct Field {
  std::string label;
  std::optional<double> value;

  // Serialize a field into a binary stream
  void Serialize(std::vector<char>& buffer) const;

  // Deserialize a field from a binary stream
  static Field Deserialize(const std::vector<char>& buffer, size_t& offset);
};

// Struct for the payload
struct Payload {
  PayloadType type;
  uint32_t field_count;
  std::vector<Field> fields;

  // Constructor
  Payload(PayloadType type, uint32_t count, std::vector<Field> fields);

  // Serialize the payload into a char buffer
  void Serialize(std::vector<char>& buffer) const;

  // Deserialize the payload from a binary stream
  static Payload Deserialize(const std::vector<char>& buffer);
};
