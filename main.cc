#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#define isFloat(number)                                                        \
  number.find('.') != number.npos || number.find('e') != number.npos

namespace json
{
struct Node;
using Null = std::monostate;
using Bool = bool;
using String = std::string;
using Int = int64_t;
using Float = double;
using Array = std::vector<Node>;
using Object = std::map<String, Node>;
using Value = std::variant<Null, Bool, Int, Float, String, Array, Object>;

struct Node
{
  Value value;

  Node() : value(Null{}) {}
  Node(Value _value) : value(_value) {}

  auto &operator[](const std::string &key)
  {
    if (auto object = std::get_if<Object>(&value))
      return (*object)[key];
    throw std::runtime_error("not an object !");
  }

  auto operator[](size_t index)
  {
    if (auto array = std::get_if<Array>(&value))
      return array->at(index);
    throw std::runtime_error("not an array !");
  }

  void push(const Node &rhs)
  {
    if (auto array = std::get_if<Array>(&value))
      array->push_back(rhs);
    else
      throw std::runtime_error("not an array !");
  }
};

struct JsonParser
{
  std::string_view json_str;
  size_t pos = 0;

  void parse_whitespace()
  {
    while (pos < json_str.size() &&
           (std::isspace(json_str[pos]) || json_str[pos] == '\n'))
      pos++;
  }

  std::optional<Value> parse_null()
  {
    if (json_str.substr(pos, 4) == "null")
    {
      pos += 4;
      return Null{};
    }
    return {};
  }

  std::optional<Value> parse_true()
  {
    if (json_str.substr(pos, 4) == "true")
    {
      pos += 4;
      return true;
    }
    return {};
  }

  std::optional<Value> parse_false()
  {
    if (json_str.substr(pos, 5) == "false")
    {
      pos += 5;
      return false;
    }
    return {};
  }

  std::optional<Value> parse_number()
  {
    size_t endpos = pos;
    while (endpos < json_str.size() &&
           (std::isdigit(json_str[endpos]) || json_str[endpos] == 'e' ||
            json_str[endpos] == '.'))
      endpos++;
    std::string number = std::string{json_str.substr(pos, endpos - pos)};
    pos = endpos;

    if (isFloat(number))
    {
      try
      {
        Float ret = std::stod(number);
        return ret;
      }
      catch (...)
      {
        return {};
      }
    }
    else
    {
      try
      {
        Int ret = std::stoi(number);
        return ret;
      }
      catch (...)
      {
        return {};
      }
    }
  }

  std::optional<Value> parse_string()
  {
    size_t endpos = ++pos;
    while (pos < json_str.size() && json_str[endpos] != '"')
      endpos++;
    std::string ret = std::string{json_str.substr(pos, endpos - pos)};
    pos = endpos + 1;
    return ret;
  }

  std::optional<Value> parse_value()
  {
    parse_whitespace();
    switch (json_str[pos])
    {
    case 'n':
      return parse_null();
    case 't':
      return parse_true();
    case 'f':
      return parse_false();
    case '"':
      return parse_string();
    case '[':
      return parse_array();
    case '{':
      return parse_object();
    default:
      return parse_number();
    }
  }

  std::optional<Value> parse_array()
  {
    pos++;
    Array arr;
    parse_whitespace();
    while (pos < json_str.size() && json_str[pos] != ']')
    {
      auto value = parse_value();
      arr.push_back(value.value());
      parse_whitespace();
      if (pos < json_str.size() && json_str[pos] == ',')
        pos++;
      parse_whitespace();
    }
    pos++;
    return arr;
  }

  std::optional<Value> parse_object()
  {
    pos++;
    Object obj;
    while (pos < json_str.size() && json_str[pos] != '}')
    {
      auto key = parse_value();

      parse_whitespace();
      if (!std::holds_alternative<String>(key.value()))
        return {};
      if (pos < json_str.size() && json_str[pos] == ':')
        pos++;

      parse_whitespace();
      auto value = parse_value();
      obj[std::get<String>(key.value())] = value.value();

      parse_whitespace();
      if (pos < json_str.size() && json_str[pos] == ',')
        pos++;
      parse_whitespace();
    }
    pos++;
    return obj;
  }

  std::optional<Node> parse()
  {
    parse_whitespace();
    auto value = parse_value();

    if (!value)
      return {};
    return Node{*value};
  }
};

std::optional<Node> parser(std::string_view json_str)
{
  JsonParser p{json_str};
  return p.parse();
}

class JsonGenerator
{
public:
  static std::string generate_string(const String &str)
  {
    std::string json_str = "\"";
    json_str += str;
    json_str += '"';
    return json_str;
  };

  static std::string generate_array(const Array &arr)
  {
    std::string json_str = "[";
    for (const auto &node : arr)
    {
      json_str += generate(node);
      json_str += ',';
    }

    // to get rid of the last comma
    if (!arr.empty())
      json_str.pop_back();
    json_str += ']';
    return json_str;
  };
  static std::string generate_object(const Object &obj)
  {
    std::string json_str = "{";
    for (const auto &[key, node] : obj)
    {
      json_str += generate_string(key);
      json_str += ':';
      json_str += generate(node);
      json_str += ',';
    }
    if (!obj.empty())
      json_str.pop_back();
    json_str += '}';
    return json_str;
  };
  static std::string generate(const Node &node)
  {
    return std::visit(
        [](auto &&arg) -> std::string
        {
          using T = std::decay_t<decltype(arg)>;
          // what is constexpr ?
          if constexpr (std::is_same_v<T, Null>)
            return "null";
          else if constexpr (std::is_same_v<T, Bool>)
            return arg ? "true" : "false";
          else if constexpr (std::is_same_v<T, Int>)
            return std::to_string(arg);
          else if constexpr (std::is_same_v<T, Float>)
            return std::to_string(arg);
          else if constexpr (std::is_same_v<T, String>)
            return generate_string(arg);
          else if constexpr (std::is_same_v<T, Array>)
            return generate_array(arg);
          else if constexpr (std::is_same_v<T, Object>)
            return generate_object(arg);
        },
        node.value
    );
  }
};

inline std::string generate(const Node &node)
{
  return JsonGenerator::generate(node);
}

std::ostream &operator<<(std::ostream &out, const Node &node)
{
  out << generate(node);
  return out;
}
} // namespace json

int main()
{
  std::ifstream fin("test.json");
  std::stringstream ss;
  ss << fin.rdbuf();
  std::string s{ss.str()};
  auto x = json::parser(s).value();
  std::cout << x << "\n";
  x["configurations"].push({true});
  x["configurations"].push({json::Null{}});
  x["version"] = {114514LL};
  std::cout << x << "\n";
}
