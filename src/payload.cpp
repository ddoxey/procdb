#include "payload.hpp"

void Field::Serialize(std::vector<char>& buffer) const {
  uint32_t label_size = label.size();
  buffer.insert(
      buffer.end(), reinterpret_cast<const char*>(&label_size),
      reinterpret_cast<const char*>(&label_size) + sizeof(label_size));
  buffer.insert(buffer.end(), label.begin(), label.end());

  bool has_value = value.has_value();
  buffer.insert(buffer.end(), reinterpret_cast<const char*>(&has_value),
                reinterpret_cast<const char*>(&has_value) + sizeof(has_value));
  if (has_value) {
    double val = value.value();
    buffer.insert(buffer.end(), reinterpret_cast<const char*>(&val),
                  reinterpret_cast<const char*>(&val) + sizeof(val));
  }
}

Field Field::Deserialize(const std::vector<char>& buffer, size_t& offset) {
  Field field;

  uint32_t label_size;
  std::memcpy(&label_size, &buffer[offset], sizeof(label_size));
  offset += sizeof(label_size);

  field.label.resize(label_size);
  std::memcpy(field.label.data(), &buffer[offset], label_size);
  offset += label_size;

  bool has_value;
  std::memcpy(&has_value, &buffer[offset], sizeof(has_value));
  offset += sizeof(has_value);

  if (has_value) {
    double val;
    std::memcpy(&val, &buffer[offset], sizeof(val));
    offset += sizeof(val);
    field.value = val;
  }

  return field;
}

// Payload constructor
Payload::Payload(PayloadType type, uint32_t count, std::vector<Field> fields)
    : type(type), field_count(count), fields(std::move(fields)) {}

void Payload::Serialize(std::vector<char>& buffer) const {
  buffer.insert(buffer.end(), reinterpret_cast<const char*>(&type),
                reinterpret_cast<const char*>(&type) + sizeof(type));
  buffer.insert(
      buffer.end(), reinterpret_cast<const char*>(&field_count),
      reinterpret_cast<const char*>(&field_count) + sizeof(field_count));

  for (const auto& field : fields) {
    field.Serialize(buffer);
  }
}

Payload Payload::Deserialize(const std::vector<char>& buffer) {
  size_t offset = 0;

  PayloadType type;
  std::memcpy(&type, &buffer[offset], sizeof(type));
  offset += sizeof(type);

  uint32_t field_count;
  std::memcpy(&field_count, &buffer[offset], sizeof(field_count));
  offset += sizeof(field_count);

  std::vector<Field> fields;
  for (uint32_t i = 0; i < field_count; ++i) {
    fields.push_back(Field::Deserialize(buffer, offset));
  }

  return Payload(type, field_count, std::move(fields));
}
