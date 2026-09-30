#include "units.hpp"

#include "semantic_error.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <set>
#include <stdexcept>
#include <vector>

namespace semantic::units
{
  namespace
  {
    auto checked_multiply(const std::int64_t left, const std::int64_t right) -> std::int64_t
    {
      std::int64_t result{};
      if (__builtin_mul_overflow(left, right, &result))
        throw std::overflow_error("Exact unit ratio exceeds Int64 range");
      return result;
    }

    auto checked_add(const std::int64_t left, const std::int64_t right) -> std::int64_t
    {
      std::int64_t result{};
      if (__builtin_add_overflow(left, right, &result))
        throw std::overflow_error("Exact unit ratio exceeds Int64 range");
      return result;
    }

    auto base(std::string name) -> dimensions { return {{std::move(name), 1}}; }

    class expression_parser
    {
      std::string_view text_;
      std::size_t current_{};
      const registry &registry_;
      parser::span range_;

      auto space() -> void
      {
        while (current_ < text_.size() && text_[current_] == ' ') ++current_;
      }

      auto identifier() -> std::string
      {
        space();
        const auto begin = current_;
        while (current_ < text_.size() && text_[current_] != ' ' && text_[current_] != '*' &&
               text_[current_] != '/' && text_[current_] != '^' && text_[current_] != '(' &&
               text_[current_] != '>') ++current_;
        if (begin == current_) throw semantic_error("Expected a unit name", range_);
        return std::string(text_.substr(begin, current_ - begin));
      }

      auto factor() -> descriptor
      {
        space();
        if (text_.substr(current_).starts_with("Delta<"))
        {
          current_ += 6;
          auto value = expression();
          space();
          if (current_ >= text_.size() || text_[current_++] != '>')
            throw semantic_error("Expected '>' after Delta unit", range_);
          return difference_of(value);
        }
        if (current_ < text_.size() && text_[current_] == '(')
        {
          ++current_;
          auto value = expression();
          space();
          if (current_ >= text_.size() || text_[current_++] != ')')
            throw semantic_error("Expected ')' in unit expression", range_);
          return value;
        }
        const auto name = identifier();
        std::int64_t scale{};
        const auto parsed = std::from_chars(name.data(), name.data() + name.size(), scale);
        if (parsed.ec == std::errc{} && parsed.ptr == name.data() + name.size())
          return descriptor{"", {}, {}, {scale, 1}, {0, 1}, category::linear, ""};
        if (const auto *value = registry_.find(name)) return *value;
        if (const auto value = registry_.prefixed(name)) return *value;
        if (const auto *value = registry_.quantity_dimensions(name))
          return descriptor{name, *value, name, {1, 1}, {0, 1}, category::linear, ""};
        if (const auto *value = registry_.dimension_dimensions(name))
          return descriptor{name, *value, {}, {1, 1}, {0, 1}, category::linear, ""};
        throw semantic_error("Unknown unit '" + name + "'", range_);
      }

      auto powered() -> descriptor
      {
        auto value = factor();
        space();
        if (current_ >= text_.size() || text_[current_] != '^') return value;
        ++current_;
        space();
        const auto begin = current_;
        if (current_ < text_.size() && text_[current_] == '-') ++current_;
        while (current_ < text_.size() && text_[current_] >= '0' && text_[current_] <= '9') ++current_;
        int exponent{};
        const auto parsed = std::from_chars(text_.data() + begin, text_.data() + current_, exponent);
        if (parsed.ec != std::errc{} || exponent == 0)
          throw semantic_error("Unit exponent must be a nonzero integer", range_);
        descriptor result{"", {}, {}, {1, 1}, {0, 1}, category::linear, ""};
        const auto count = static_cast<unsigned int>(std::abs(exponent));
        for (unsigned int index = 0; index < count; ++index)
          result = combine(result, value, exponent > 0 ? '*' : '/', registry_);
        return result;
      }

    public:
      expression_parser(std::string_view text, const registry &units, const parser::span range)
          : text_(text), registry_(units), range_(range) {}

      auto expression() -> descriptor
      {
        auto value = powered();
        while (true)
        {
          space();
          if (current_ >= text_.size() || (text_[current_] != '*' && text_[current_] != '/')) break;
          const char operation = text_[current_++];
          value = combine(value, powered(), operation, registry_);
        }
        return value;
      }

      auto complete() -> descriptor
      {
        auto value = expression();
        space();
        if (current_ != text_.size()) throw semantic_error("Invalid unit expression", range_);
        return value;
      }
    };
  }

  auto rational::normalized() const -> rational
  {
    if (denominator == 0) throw std::domain_error("Unit ratio denominator cannot be zero");
    const auto divisor = std::gcd(numerator, denominator);
    const auto sign = denominator < 0 ? -1 : 1;
    auto result = rational{numerator / divisor * sign, denominator / divisor * sign,
                           decimal_exponent, pi_exponent};
    if (result.numerator == 0) return {0, 1};
    while (result.numerator != 0 && result.numerator % 10 == 0)
    {
      result.numerator /= 10;
      ++result.decimal_exponent;
    }
    while (result.denominator % 10 == 0)
    {
      result.denominator /= 10;
      --result.decimal_exponent;
    }
    return result;
  }

  auto multiply(const rational left, const rational right) -> rational
  {
    return rational{checked_multiply(left.numerator, right.numerator),
                    checked_multiply(left.denominator, right.denominator),
                    left.decimal_exponent + right.decimal_exponent,
                    left.pi_exponent + right.pi_exponent}.normalized();
  }
  auto divide(const rational left, const rational right) -> rational
  {
    return multiply(left, {right.denominator, right.numerator,
                           -right.decimal_exponent, -right.pi_exponent});
  }
  auto add(const rational left, const rational right) -> rational
  {
    if (left.decimal_exponent != right.decimal_exponent || left.pi_exponent != right.pi_exponent)
      throw std::domain_error("Adding unlike symbolic unit ratios is not supported");
    return rational{checked_add(checked_multiply(left.numerator, right.denominator),
                                checked_multiply(right.numerator, left.denominator)),
                    checked_multiply(left.denominator, right.denominator),
                    left.decimal_exponent, left.pi_exponent}.normalized();
  }

  registry::registry()
  {
    for (const std::string name : {"Length", "Time", "Mass", "ElectricCurrent", "Temperature",
                                   "AmountOfSubstance", "LuminousIntensity", "Angle"})
      dimensions_.emplace(name, base(name));
    const auto quantity = [&](std::string name, dimensions value)
    {
      quantities_.emplace(std::move(name), std::move(value));
    };
    quantity("Area", {{"Length", 2}});
    quantity("Volume", {{"Length", 3}});
    quantity("Angle", {{"Angle", 1}});
    quantity("SolidAngle", {{"Angle", 2}});
    quantity("Frequency", {{"Time", -1}});
    quantity("Speed", {{"Length", 1}, {"Time", -1}});
    quantity("Acceleration", {{"Length", 1}, {"Time", -2}});
    quantity("Wavenumber", {{"Length", -1}});
    quantity("Density", {{"Length", -3}, {"Mass", 1}});
    quantity("Momentum", {{"Length", 1}, {"Mass", 1}, {"Time", -1}});
    quantity("Force", {{"Length", 1}, {"Mass", 1}, {"Time", -2}});
    quantity("Pressure", {{"Length", -1}, {"Mass", 1}, {"Time", -2}});
    quantity("Energy", {{"Length", 2}, {"Mass", 1}, {"Time", -2}});
    quantity("Torque", {{"Length", 2}, {"Mass", 1}, {"Time", -2}});
    quantity("Power", {{"Length", 2}, {"Mass", 1}, {"Time", -3}});
    quantity("ElectricCharge", {{"ElectricCurrent", 1}, {"Time", 1}});
    quantity("Voltage", {{"ElectricCurrent", -1}, {"Length", 2}, {"Mass", 1}, {"Time", -3}});
    quantity("Capacitance", {{"ElectricCurrent", 2}, {"Length", -2}, {"Mass", -1}, {"Time", 4}});
    quantity("Resistance", {{"ElectricCurrent", -2}, {"Length", 2}, {"Mass", 1}, {"Time", -3}});
    quantity("Conductance", {{"ElectricCurrent", 2}, {"Length", -2}, {"Mass", -1}, {"Time", 3}});
    quantity("MagneticFlux", {{"ElectricCurrent", -1}, {"Length", 2}, {"Mass", 1}, {"Time", -2}});
    quantity("MagneticFluxDensity", {{"ElectricCurrent", -1}, {"Mass", 1}, {"Time", -2}});
    quantity("Inductance", {{"ElectricCurrent", -2}, {"Length", 2}, {"Mass", 1}, {"Time", -2}});
    quantity("LuminousFlux", {{"Angle", 2}, {"LuminousIntensity", 1}});
    quantity("Illuminance", {{"Angle", 2}, {"Length", -2}, {"LuminousIntensity", 1}});
    quantity("Radioactivity", {{"Time", -1}});
    quantity("AbsorbedDose", {{"Length", 2}, {"Time", -2}});
    quantity("EquivalentDose", {{"Length", 2}, {"Time", -2}});
    quantity("CatalyticActivity", {{"AmountOfSubstance", 1}, {"Time", -1}});
    quantity("DynamicViscosity", {{"Length", -1}, {"Mass", 1}, {"Time", -1}});
    quantity("KinematicViscosity", {{"Length", 2}, {"Time", -1}});

    prefixes_ = {{"quecto", {1, 1, -30}}, {"ronto", {1, 1, -27}}, {"yocto", {1, 1, -24}},
                 {"zepto", {1, 1, -21}}, {"atto", {1, 1, -18}}, {"femto", {1, 1, -15}},
                 {"pico", {1, 1, -12}}, {"nano", {1, 1, -9}}, {"micro", {1, 1, -6}},
                 {"milli", {1, 1, -3}}, {"centi", {1, 1, -2}}, {"deci", {1, 1, -1}},
                 {"deca", {1, 1, 1}}, {"hecto", {1, 1, 2}}, {"kilo", {1, 1, 3}},
                 {"mega", {1, 1, 6}}, {"giga", {1, 1, 9}}, {"tera", {1, 1, 12}},
                 {"peta", {1, 1, 15}}, {"exa", {1, 1, 18}}, {"zetta", {1, 1, 21}},
                 {"yotta", {1, 1, 24}}, {"ronna", {1, 1, 27}}, {"quetta", {1, 1, 30}}};
    const auto linear = [&](std::string name, dimensions dimension, rational scale,
                            std::optional<std::string> named_quantity = {}, std::string symbol = {},
                            const bool prefixable = false)
    {
      const std::string key = name;
      units_.emplace(key, descriptor{std::move(name), std::move(dimension), std::move(named_quantity),
                                     scale.normalized(), {0, 1}, category::linear, std::move(symbol)});
      if (prefixable) prefixable_.insert(key);
    };
    linear("meter", base("Length"), {1, 1}, {}, "m", true);
    linear("second", base("Time"), {1, 1}, {}, "s", true);
    linear("gram", base("Mass"), {1, 1, -3}, {}, "g", true);
    linear("kilogram", base("Mass"), {1, 1}, {}, "kg");
    linear("ampere", base("ElectricCurrent"), {1, 1}, {}, "A", true);
    linear("kelvin", base("Temperature"), {1, 1}, {}, "K", true);
    linear("mole", base("AmountOfSubstance"), {1, 1}, {}, "mol", true);
    linear("candela", base("LuminousIntensity"), {1, 1}, {}, "cd", true);
    linear("radian", base("Angle"), {1, 1}, "Angle", "rad", true);
    linear("steradian", {{"Angle", 2}}, {1, 1}, "SolidAngle", "sr", true);

    const auto derived = [&](std::string name, std::string quantity_name, std::string symbol)
    {
      linear(std::move(name), quantities_.at(quantity_name), {1, 1}, quantity_name,
             std::move(symbol), true);
    };
    derived("hertz", "Frequency", "Hz");
    derived("newton", "Force", "N");
    derived("pascal", "Pressure", "Pa");
    derived("joule", "Energy", "J");
    derived("newtonMeterTorque", "Torque", "N·m");
    derived("watt", "Power", "W");
    derived("coulomb", "ElectricCharge", "C");
    derived("volt", "Voltage", "V");
    derived("farad", "Capacitance", "F");
    derived("ohm", "Resistance", "Ω");
    derived("siemens", "Conductance", "S");
    derived("weber", "MagneticFlux", "Wb");
    derived("tesla", "MagneticFluxDensity", "T");
    derived("henry", "Inductance", "H");
    derived("lumen", "LuminousFlux", "lm");
    derived("lux", "Illuminance", "lx");
    derived("becquerel", "Radioactivity", "Bq");
    derived("gray", "AbsorbedDose", "Gy");
    derived("sievert", "EquivalentDose", "Sv");
    derived("katal", "CatalyticActivity", "kat");

    linear("minute", base("Time"), {60, 1}, {}, "min");
    linear("hour", base("Time"), {3600, 1}, {}, "h");
    linear("day", base("Time"), {86400, 1}, {}, "d");
    linear("degree", base("Angle"), {1, 180, 0, 1}, "Angle", "°");
    linear("arcminute", base("Angle"), {1, 10800, 0, 1}, "Angle", "′");
    linear("arcsecond", base("Angle"), {1, 648000, 0, 1}, "Angle", "″");
    linear("liter", {{"Length", 3}}, {1, 1, -3}, "Volume", "L", true);
    linear("tonne", base("Mass"), {1000, 1}, {}, "t");
    linear("hectare", {{"Length", 2}}, {10000, 1}, "Area", "ha");
    linear("bar", quantities_.at("Pressure"), {1, 1, 5}, "Pressure", "bar");
    linear("astronomicalUnit", base("Length"), {149597870700, 1}, {}, "au");
    linear("nauticalMile", base("Length"), {1852, 1}, {}, "nmi");
    linear("knot", quantities_.at("Speed"), {463, 900}, "Speed", "kn");
    linear("electronvolt", quantities_.at("Energy"), {1602176634, 1, -28}, "Energy", "eV", true);
    linear("angstrom", base("Length"), {1, 1, -10}, {}, "Å");
    linear("barn", {{"Length", 2}}, {1, 1, -28}, "Area", "b", true);
    linear("gal", quantities_.at("Acceleration"), {1, 1, -2}, "Acceleration", "Gal", true);
    linear("lightYear", base("Length"), {9460730472580800LL, 1}, {}, "ly");
    linear("parsec", base("Length"), {96939420213600000LL, 1, 0, -1}, {}, "pc", true);
    linear("standardAtmosphere", quantities_.at("Pressure"), {101325, 1}, "Pressure", "atm");
    linear("torr", quantities_.at("Pressure"), {101325, 760}, "Pressure", "Torr");
    linear("calorie", quantities_.at("Energy"), {4184, 1, -3}, "Energy", "cal", true);
    linear("kilowattHour", quantities_.at("Energy"), {36, 1, 5}, "Energy", "kWh");
    linear("dyne", quantities_.at("Force"), {1, 1, -5}, "Force", "dyn", true);
    linear("erg", quantities_.at("Energy"), {1, 1, -7}, "Energy", "erg", true);
    linear("gauss", quantities_.at("MagneticFluxDensity"), {1, 1, -4}, "MagneticFluxDensity", "G", true);
    linear("poise", quantities_.at("DynamicViscosity"), {1, 1, -1}, "DynamicViscosity", "P", true);
    linear("stokes", quantities_.at("KinematicViscosity"), {1, 1, -4}, "KinematicViscosity", "St", true);
    linear("inch", base("Length"), {254, 1, -4}, {}, "in");
    linear("foot", base("Length"), {3048, 1, -4}, {}, "ft");
    linear("yard", base("Length"), {9144, 1, -4}, {}, "yd");
    linear("mile", base("Length"), {1609344, 1, -3}, {}, "mi");
    linear("poundMass", base("Mass"), {45359237, 1, -8}, {}, "lb");
    linear("poundForce", quantities_.at("Force"), {44482216152605LL, 1, -13}, "Force", "lbf");
    units_.emplace("Kelvin", descriptor{"Kelvin", base("Temperature"), {}, {1, 1}, {0, 1},
                                         category::affine_point, "K"});
    units_.emplace("Celsius", descriptor{"Celsius", base("Temperature"), {}, {1, 1}, {27315, 100},
                                          category::affine_point, "°C"});
    units_.emplace("Fahrenheit", descriptor{"Fahrenheit", base("Temperature"), {}, {5, 9},
                                             {45967, 180}, category::affine_point, "°F"});
  }

  auto registry::find(const std::string_view name) const -> const descriptor *
  {
    const auto found = units_.find(std::string(name));
    return found == units_.end() ? nullptr : &found->second;
  }

  auto registry::prefixed(const std::string_view name) const -> std::optional<descriptor>
  {
    for (const auto &[prefix, factor] : prefixes_)
    {
      if (!name.starts_with(prefix)) continue;
      const std::string unit_name{name.substr(prefix.size())};
      if (!prefixable_.contains(unit_name)) continue;
      const auto found = units_.find(unit_name);
      if (found == units_.end() || found->second.kind != category::linear) continue;
      auto result = found->second;
      result.name = std::string(name);
      result.scale = multiply(result.scale, factor);
      result.symbol.clear();
      return result;
    }
    return {};
  }

  auto registry::quantity_dimensions(const std::string_view name) const -> const dimensions *
  {
    const auto found = quantities_.find(std::string(name));
    return found == quantities_.end() ? nullptr : &found->second;
  }

  auto registry::dimension_dimensions(const std::string_view name) const -> const dimensions *
  {
    const auto found = dimensions_.find(std::string(name));
    return found == dimensions_.end() ? nullptr : &found->second;
  }

  auto registry::infer_quantity(const dimensions &value) const -> std::optional<std::string>
  {
    std::optional<std::string> result;
    for (const auto &[name, dimension] : quantities_)
      if (dimension == value)
      {
        if (result) return {};
        result = name;
      }
    return result;
  }

  auto registry::resolve(const std::string_view expression, const parser::span range) const -> descriptor
  {
    return expression_parser(expression, *this, range).complete();
  }

  auto registry::add_program(const parser::program &program) -> void
  {
    std::vector<const parser::measurement_declaration *> declarations;
    std::set<std::string> declared_names;
    for (const auto &statement : program.statements)
      if (const auto *entry = dynamic_cast<const parser::measurement_declaration *>(statement.get()))
      {
        if (!declared_names.insert(entry->name).second || dimensions_.contains(entry->name) ||
            quantities_.contains(entry->name) || units_.contains(entry->name))
          throw semantic_error("Duplicate measurement declaration '" + entry->name + "'", entry->range);
        declarations.push_back(entry);
        if (entry->declaration_kind == parser::measurement_declaration::kind::dimension)
        {
          dimensions_.emplace(entry->name, base(entry->name));
        }
      }

    auto pending = declarations;
    std::erase_if(pending, [](const auto *entry)
    {
      return entry->declaration_kind == parser::measurement_declaration::kind::dimension;
    });
    while (!pending.empty())
    {
      bool progressed = false;
      for (auto entry = pending.begin(); entry != pending.end();)
      {
        const auto *declaration = *entry;
        try
        {
          if (declaration->declaration_kind == parser::measurement_declaration::kind::quantity)
          {
            const auto resolved = resolve(*declaration->definition, declaration->range);
            quantities_.emplace(declaration->name, resolved.dimension);
          }
          else
          {
            std::unordered_map<std::string, std::string> properties;
            for (const auto &[name, value] : declaration->properties) properties.insert_or_assign(name, value);
            descriptor value;
            if (declaration->declaration_kind == parser::measurement_declaration::kind::affine_unit)
            {
              if (!declaration->declared_dimension)
                throw semantic_error("Affine unit '" + declaration->name + "' requires a dimension", declaration->range);
              const auto dimension = dimension_dimensions(*declaration->declared_dimension);
              if (!dimension)
                throw semantic_error("Unknown affine dimension '" + *declaration->declared_dimension + "'", declaration->range);
              if (!properties.contains("canonical") || !properties.contains("scale") || !properties.contains("offset"))
                throw semantic_error("Affine unit '" + declaration->name +
                                     "' requires canonical, scale, and offset properties", declaration->range);
              const auto canonical = resolve(properties.at("canonical"), declaration->range);
              if (canonical.dimension != *dimension)
                throw semantic_error("Affine unit '" + declaration->name + "' has an inconsistent canonical unit",
                                     declaration->range);
              value = canonical;
              value.scale = resolve(properties.at("scale"), declaration->range).scale;
              value.offset = resolve(properties.at("offset"), declaration->range).scale;
              value.kind = category::affine_point;
            }
            else if (*declaration->definition == "base")
            {
              if (!declaration->declared_dimension)
                throw semantic_error("Base unit '" + declaration->name + "' requires a dimension", declaration->range);
              const auto dimension = dimension_dimensions(*declaration->declared_dimension);
              if (!dimension) throw semantic_error("Unknown dimension '" + *declaration->declared_dimension + "'", declaration->range);
              value = descriptor{"", *dimension, {}, {1, 1}, {0, 1}, category::linear, ""};
            }
            else value = resolve(*declaration->definition, declaration->range);
            value.name = declaration->name;
            if (declaration->declared_dimension)
            {
              if (const auto quantity = quantity_dimensions(*declaration->declared_dimension))
              {
                if (*quantity != value.dimension)
                  throw semantic_error("Unit '" + declaration->name + "' has an inconsistent quantity", declaration->range);
                value.quantity = *declaration->declared_dimension;
              }
              else if (const auto dimension = dimension_dimensions(*declaration->declared_dimension);
                       !dimension || *dimension != value.dimension)
                throw semantic_error("Unit '" + declaration->name + "' has an inconsistent dimension", declaration->range);
            }
            const auto clean = [](std::string text)
            {
              if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
                return text.substr(1, text.size() - 2);
              return text;
            };
            if (properties.contains("symbol")) value.symbol = clean(properties.at("symbol"));
            units_.emplace(value.name, value);
            if (properties.contains("aliases"))
            {
              std::string aliases = properties.at("aliases");
              for (char &character : aliases) if (character == '[' || character == ']' || character == ',') character = ' ';
              std::size_t begin{};
              while (begin < aliases.size())
              {
                while (begin < aliases.size() && aliases[begin] == ' ') ++begin;
                const auto end = aliases.find(' ', begin);
                if (begin == aliases.size()) break;
                const std::string alias = clean(aliases.substr(begin, end - begin));
                if (units_.contains(alias)) throw semantic_error("Duplicate unit alias '" + alias + "'", declaration->range);
                auto alias_value = value;
                alias_value.name = alias;
                units_.emplace(alias, std::move(alias_value));
                begin = end == std::string::npos ? aliases.size() : end + 1;
              }
            }
            if (properties.contains("prefixes"))
            {
              std::string names = properties.at("prefixes");
              for (char &character : names) if (character == '[' || character == ']' || character == ',') character = ' ';
              std::size_t begin{};
              while (begin < names.size())
              {
                while (begin < names.size() && names[begin] == ' ') ++begin;
                const auto end = names.find(' ', begin);
                if (begin == names.size()) break;
                const std::string prefix = names.substr(begin, end - begin);
                const auto ratio = prefixes_.find(prefix);
                if (ratio == prefixes_.end()) throw semantic_error("Unknown unit prefix '" + prefix + "'", declaration->range);
                auto prefixed = value;
                prefixed.name = prefix + value.name;
                prefixed.scale = multiply(prefixed.scale, ratio->second);
                if (units_.contains(prefixed.name)) throw semantic_error("Duplicate prefixed unit '" + prefixed.name + "'", declaration->range);
                units_.emplace(prefixed.name, std::move(prefixed));
                begin = end == std::string::npos ? names.size() : end + 1;
              }
            }
          }
          entry = pending.erase(entry);
          progressed = true;
        }
        catch (const semantic_error &error)
        {
          const std::string message = error.what();
          if (std::string_view{message}.starts_with("Unknown unit '") ||
              std::string_view{message}.starts_with("Unknown dimension '") ||
              std::string_view{message}.starts_with("Unknown affine dimension '"))
          {
            const auto quote = message.find('\'');
            const auto close = quote == std::string::npos ? std::string::npos : message.find('\'', quote + 1);
            const std::string missing = close == std::string::npos ? std::string{} :
                                        message.substr(quote + 1, close - quote - 1);
            if (declared_names.contains(missing))
            {
              ++entry;
              continue;
            }
          }
          throw;
        }
      }
      if (!progressed)
        throw semantic_error("Cyclic or unresolved measurement declaration involving '" + pending.front()->name + "'",
                             pending.front()->range);
    }
  }

  auto parse_measured_type(const std::string_view type, const registry &unit_registry,
                           const parser::span range) -> std::optional<measured_type>
  {
    const auto open = type.find('<');
    if (open == std::string_view::npos || !type.ends_with('>')) return {};
    const auto numeric = type.substr(0, open);
    if (!numeric.starts_with("Int") && !numeric.starts_with("Float")) return {};
    return measured_type{std::string(numeric), unit_registry.resolve(type.substr(open + 1, type.size() - open - 2), range)};
  }

  auto format_type(const std::string_view numeric, const descriptor &unit) -> std::string
  {
    return std::string(numeric) + '<' + unit.name + '>';
  }

  auto compatible(const descriptor &expected, const descriptor &actual) -> bool
  {
    return expected.dimension == actual.dimension && expected.kind == actual.kind &&
           (!expected.quantity || !actual.quantity || expected.quantity == actual.quantity);
  }

  auto difference_of(const descriptor &point) -> descriptor
  {
    if (point.kind != category::affine_point)
      throw std::domain_error("Delta requires an affine unit");
    auto result = point;
    result.name = "Delta<" + point.name + ">";
    result.offset = {0, 1};
    result.kind = category::affine_difference;
    result.symbol = "Δ" + point.symbol;
    return result;
  }

  auto combine(const descriptor &left, const descriptor &right, const char operation,
               const registry &unit_registry) -> descriptor
  {
    if (left.kind == category::affine_point || right.kind == category::affine_point)
      throw std::domain_error("Absolute affine units cannot be multiplied or divided");
    descriptor result{"", left.dimension, {}, operation == '*' ? multiply(left.scale, right.scale)
                                                               : divide(left.scale, right.scale),
                      {0, 1}, category::linear, ""};
    for (const auto &[name, exponent] : right.dimension)
    {
      result.dimension[name] += operation == '*' ? exponent : -exponent;
      if (result.dimension[name] == 0) result.dimension.erase(name);
    }
    result.quantity = unit_registry.infer_quantity(result.dimension);
    result.name = left.name + ' ' + operation + ' ' + right.name;
    return result;
  }
}
