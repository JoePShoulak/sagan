#include "ast_render.hpp"

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace parser
{
  namespace
  {
    struct visual_node
    {
      std::string label;
      std::string category;
      std::vector<std::unique_ptr<visual_node>> children;
      double x = 0.0;
      double y = 0.0;

      visual_node(std::string node_label, std::string node_category)
          : label(std::move(node_label)), category(std::move(node_category))
      {
      }
    };

    auto make_expression_node(const expression &value) -> std::unique_ptr<visual_node>;
    auto make_statement_node(const statement &value) -> std::unique_ptr<visual_node>;

    auto string_preview(std::string_view text) -> std::string
    {
      std::string result;
      for (const char character : text)
      {
        if (character == '\n') result += " ↵ ";
        else if (character == '\r') result += " ↵ ";
        else if (character == '\t') result += " ⇥ ";
        else result.push_back(character);
        if (result.size() > 30)
        {
          result.resize(30);
          result += "…";
          break;
        }
      }
      return result.empty() ? "(empty)" : result;
    }

    auto make_expression_node(const expression &value) -> std::unique_ptr<visual_node>
    {
      if (const auto *identifier = dynamic_cast<const identifier_expression *>(&value))
      {
        return std::make_unique<visual_node>(visual_node{"Identifier\n" + identifier->name, "identifier"});
      }
      if (const auto *literal = dynamic_cast<const literal_expression *>(&value))
      {
        std::string kind = "Boolean";
        if (literal->literal_kind == literal_expression::kind::integer)
        {
          kind = "Integer";
        }
        else if (literal->literal_kind == literal_expression::kind::floating_point)
        {
          kind = "Float";
        }
        return std::make_unique<visual_node>(visual_node{kind + "\n" + literal->spelling, "literal"});
      }
      if (const auto *measured = dynamic_cast<const measured_expression *>(&value))
      {
        auto result = std::make_unique<visual_node>(visual_node{
            std::string(measured->conversion ? "Convert\n" : "Measure\n") + measured->unit, "type"});
        result->children.push_back(make_expression_node(*measured->value));
        return result;
      }
      if (const auto *grouping = dynamic_cast<const grouping_expression *>(&value))
      {
        auto result = std::make_unique<visual_node>(visual_node{"Group", "expression"});
        result->children.push_back(make_expression_node(*grouping->value));
        return result;
      }
      if (const auto *unary = dynamic_cast<const unary_expression *>(&value))
      {
        const std::string form = unary->postfix ? "Postfix\n" : "Prefix\n";
        auto result = std::make_unique<visual_node>(visual_node{form + unary->operator_text, "operator"});
        result->children.push_back(make_expression_node(*unary->operand));
        return result;
      }
      if (const auto *binary = dynamic_cast<const binary_expression *>(&value))
      {
        auto result = std::make_unique<visual_node>(visual_node{"Binary\n" + binary->operator_text, "operator"});
        result->children.push_back(make_expression_node(*binary->left));
        result->children.push_back(make_expression_node(*binary->right));
        return result;
      }
      if (const auto *conditional = dynamic_cast<const conditional_expression *>(&value))
      {
        auto result = std::make_unique<visual_node>(visual_node{"Conditional\n? ;", "operator"});
        result->children.push_back(make_expression_node(*conditional->condition));
        result->children.push_back(make_expression_node(*conditional->when_true));
        result->children.push_back(make_expression_node(*conditional->when_false));
        return result;
      }
      if (const auto *assignment = dynamic_cast<const assignment_expression *>(&value))
      {
        auto result = std::make_unique<visual_node>(visual_node{"Assign value\n:=", "operator"});
        result->children.push_back(make_expression_node(*assignment->target));
        result->children.push_back(make_expression_node(*assignment->value));
        return result;
      }
      if (const auto *call = dynamic_cast<const call_expression *>(&value))
      {
        auto result = std::make_unique<visual_node>(visual_node{
            "Call\n" + std::to_string(call->arguments.size()) + " argument(s)", "call"});
        result->children.push_back(make_expression_node(*call->callee));
        for (const auto &argument : call->arguments)
        {
          result->children.push_back(make_expression_node(*argument));
        }
        return result;
      }
      if (const auto *index = dynamic_cast<const index_expression *>(&value))
      {
        auto result = std::make_unique<visual_node>(visual_node{"Index\n[]", "access"});
        result->children.push_back(make_expression_node(*index->target));
        result->children.push_back(make_expression_node(*index->index));
        return result;
      }
      if (const auto *member = dynamic_cast<const member_expression *>(&value))
      {
        auto result = std::make_unique<visual_node>(visual_node{
            std::string(member->safe ? "Safe member\n" : "Member\n") + member->member_name, "access"});
        result->children.push_back(make_expression_node(*member->target));
        return result;
      }
      if (const auto *string = dynamic_cast<const string_expression *>(&value))
      {
        const std::string label = string->raw ? "Raw string" : (string->multiline ? "Multiline string" : "String");
        auto result = std::make_unique<visual_node>(visual_node{label, "string"});
        for (const auto &part : string->parts)
        {
          if (part.interpolation)
          {
            auto interpolation = std::make_unique<visual_node>(visual_node{"Interpolation\n${…}", "string"});
            interpolation->children.push_back(make_expression_node(*part.interpolation));
            result->children.push_back(std::move(interpolation));
          }
          else
          {
            result->children.push_back(
                std::make_unique<visual_node>(visual_node{"Text\n" + string_preview(part.text), "text"}));
          }
        }
        return result;
      }
      if (const auto *spread = dynamic_cast<const spread_expression *>(&value))
      {
        auto result = std::make_unique<visual_node>(visual_node{"Spread\n...", "collection"});
        result->children.push_back(make_expression_node(*spread->value));
        return result;
      }
      if (const auto *collection = dynamic_cast<const collection_expression *>(&value))
      {
        const char *kind = collection->collection_kind == collection_expression::kind::array
                               ? "Array"
                           : collection->collection_kind == collection_expression::kind::vector
                               ? "Vector"
                           : collection->collection_kind == collection_expression::kind::point
                               ? "Point"
                           : collection->collection_kind == collection_expression::kind::spherical_vector
                               ? "SphericalVector"
                               : "SphericalPoint";
        auto result = std::make_unique<visual_node>(
            visual_node{std::string(kind) + "\n" + std::to_string(collection->elements.size()) + " element(s)",
                        "collection"});
        for (const auto &element : collection->elements)
        {
          result->children.push_back(make_expression_node(*element));
        }
        return result;
      }
      if (const auto *dictionary = dynamic_cast<const dictionary_expression *>(&value))
      {
        auto result = std::make_unique<visual_node>(visual_node{
            "Dictionary\n" + std::to_string(dictionary->entries.size()) + " entry(s)", "collection"});
        for (const auto &entry : dictionary->entries)
        {
          if (!entry.key)
          {
            result->children.push_back(make_expression_node(*entry.value));
            continue;
          }
          auto item = std::make_unique<visual_node>(visual_node{"Entry\nkey : value", "entry"});
          item->children.push_back(make_expression_node(*entry.key));
          item->children.push_back(make_expression_node(*entry.value));
          result->children.push_back(std::move(item));
        }
        return result;
      }
      if (const auto *lambda = dynamic_cast<const lambda_expression *>(&value))
      {
        std::string label = "Lambda";
        if (lambda->return_type)
        {
          label += "\n: " + *lambda->return_type;
        }
        auto result = std::make_unique<visual_node>(visual_node{std::move(label), "declaration"});
        for (const auto &parameter : lambda->parameters)
        {
          std::string parameter_label = "Parameter\n" + parameter.name;
          if (parameter.type_name)
          {
            parameter_label += ": " + *parameter.type_name;
          }
          result->children.push_back(
              std::make_unique<visual_node>(visual_node{std::move(parameter_label), "declaration"}));
        }
        result->children.push_back(make_expression_node(*lambda->body));
        return result;
      }
      throw std::runtime_error("AST renderer encountered an unsupported expression node");
    }

    auto make_statement_node_content(const statement &value) -> std::unique_ptr<visual_node>
    {
      if (const auto *declaration = dynamic_cast<const let_declaration *>(&value))
      {
        std::string label = "Let\n" + std::string(declaration->weak_member ? "weak " : "") +
                            std::string(declaration->private_member ? "." : "") + declaration->name;
        if (declaration->type_name)
        {
          label += ": " + *declaration->type_name;
        }
        auto node = std::make_unique<visual_node>(visual_node{std::move(label), "declaration"});
        if (declaration->initializer)
        {
          node->children.push_back(make_expression_node(*declaration->initializer));
        }
        return node;
      }
      if (const auto *expression = dynamic_cast<const expression_statement *>(&value))
      {
        auto node = std::make_unique<visual_node>(visual_node{"Expression statement", "statement"});
        node->children.push_back(make_expression_node(*expression->value));
        return node;
      }
      if (const auto *assignment = dynamic_cast<const assignment_statement *>(&value))
      {
        auto node = std::make_unique<visual_node>(visual_node{"Assignment\n" + assignment->operation, "statement"});
        node->children.push_back(make_expression_node(*assignment->target));
        node->children.push_back(make_expression_node(*assignment->value));
        return node;
      }
      if (const auto *block = dynamic_cast<const block_statement *>(&value))
      {
        auto node = std::make_unique<visual_node>(
            visual_node{"Block\n" + std::to_string(block->statements.size()) + " statement(s)", "block"});
        for (const auto &statement : block->statements)
        {
          node->children.push_back(make_statement_node(*statement));
        }
        return node;
      }
      if (const auto *conditional = dynamic_cast<const if_statement *>(&value))
      {
        auto node = std::make_unique<visual_node>(visual_node{"If", "control"});
        auto condition = std::make_unique<visual_node>(visual_node{"Condition", "control"});
        condition->children.push_back(make_expression_node(*conditional->condition));
        node->children.push_back(std::move(condition));
        auto when_true = std::make_unique<visual_node>(visual_node{"Then", "control"});
        when_true->children.push_back(make_statement_node(*conditional->then_branch));
        node->children.push_back(std::move(when_true));
        if (conditional->else_branch)
        {
          auto when_false = std::make_unique<visual_node>(visual_node{"Else", "control"});
          when_false->children.push_back(make_statement_node(*conditional->else_branch));
          node->children.push_back(std::move(when_false));
        }
        return node;
      }
      if (const auto *loop = dynamic_cast<const condition_loop_statement *>(&value))
      {
        const std::string label = loop->loop_kind == condition_loop_statement::kind::while_loop ? "While" : "Until";
        auto node = std::make_unique<visual_node>(visual_node{label, "control"});
        auto condition = std::make_unique<visual_node>(visual_node{"Condition", "control"});
        condition->children.push_back(make_expression_node(*loop->condition));
        node->children.push_back(std::move(condition));
        node->children.push_back(make_statement_node(*loop->body));
        return node;
      }
      if (const auto *loop = dynamic_cast<const for_statement *>(&value))
      {
        auto node = std::make_unique<visual_node>(visual_node{"For\n" + loop->binding, "control"});
        auto iterable = std::make_unique<visual_node>(visual_node{"Iterable", "control"});
        iterable->children.push_back(make_expression_node(*loop->iterable));
        node->children.push_back(std::move(iterable));
        node->children.push_back(make_statement_node(*loop->body));
        return node;
      }
      if (const auto *control = dynamic_cast<const loop_control_statement *>(&value))
      {
        const std::string label = control->control_kind == loop_control_statement::kind::break_loop
                                      ? "Break"
                                      : "Continue";
        return std::make_unique<visual_node>(visual_node{label, "control"});
      }
      if (const auto *returned = dynamic_cast<const return_statement *>(&value))
      {
        auto node = std::make_unique<visual_node>(visual_node{"Return", "control"});
        if (returned->value)
        {
          node->children.push_back(make_expression_node(*returned->value));
        }
        return node;
      }
      if (const auto *yielded = dynamic_cast<const yield_statement *>(&value))
      {
        auto node = std::make_unique<visual_node>(visual_node{"Yield", "control"});
        if (yielded->value)
        {
          node->children.push_back(make_expression_node(*yielded->value));
        }
        return node;
      }
      if (const auto *matched = dynamic_cast<const match_statement *>(&value))
      {
        auto node = std::make_unique<visual_node>(visual_node{"Match", "control"});
        auto subject = std::make_unique<visual_node>(visual_node{"Subject", "control"});
        subject->children.push_back(make_expression_node(*matched->subject));
        node->children.push_back(std::move(subject));
        for (const auto &branch : matched->cases)
        {
          auto case_node = std::make_unique<visual_node>(
              visual_node{branch.pattern ? "Case" : "Case else", "control"});
          if (branch.pattern)
          {
            auto pattern = std::make_unique<visual_node>(visual_node{"Pattern", "control"});
            pattern->children.push_back(make_expression_node(*branch.pattern));
            case_node->children.push_back(std::move(pattern));
          }
          case_node->children.push_back(make_statement_node(*branch.body));
          node->children.push_back(std::move(case_node));
        }
        return node;
      }
      if (const auto *hope = dynamic_cast<const hope_statement *>(&value))
      {
        auto node = std::make_unique<visual_node>(visual_node{"Hope", "control"});
        auto protected_body = std::make_unique<visual_node>(visual_node{"Protected", "control"});
        protected_body->children.push_back(make_statement_node(*hope->protected_body));
        node->children.push_back(std::move(protected_body));
        for (const auto &handler : hope->handlers)
        {
          auto unless = std::make_unique<visual_node>(visual_node{"Unless", "control"});
          auto pattern = std::make_unique<visual_node>(visual_node{"Pattern", "control"});
          pattern->children.push_back(make_expression_node(*handler.pattern));
          unless->children.push_back(std::move(pattern));
          unless->children.push_back(make_statement_node(*handler.body));
          node->children.push_back(std::move(unless));
        }
        if (hope->cleanup)
        {
          auto cleanup = std::make_unique<visual_node>(visual_node{"Finally", "control"});
          cleanup->children.push_back(make_statement_node(*hope->cleanup));
          node->children.push_back(std::move(cleanup));
        }
        return node;
      }
      if (const auto *scream = dynamic_cast<const scream_statement *>(&value))
      {
        auto node = std::make_unique<visual_node>(visual_node{"Scream", "control"});
        node->children.push_back(make_expression_node(*scream->value));
        return node;
      }
      if (const auto *function = dynamic_cast<const function_declaration *>(&value))
      {
        std::string label = std::string(function->constructor_member ? "Constructor\n" : "Function\n") +
                            std::string(function->private_member ? "." : "") + function->name;
        if (!function->type_parameters.empty())
        {
          label += '<';
          for (std::size_t index = 0; index < function->type_parameters.size(); ++index)
          {
            if (index != 0) label += ", ";
            label += function->type_parameters[index];
          }
          label += '>';
        }
        if (function->return_type)
        {
          label += ": " + *function->return_type;
        }
        auto node = std::make_unique<visual_node>(visual_node{std::move(label), "declaration"});
        for (const auto &parameter : function->parameters)
        {
          std::string parameter_label = "Parameter\n" + parameter.name;
          if (parameter.type_name)
          {
            parameter_label += ": " + *parameter.type_name;
          }
          node->children.push_back(
              std::make_unique<visual_node>(visual_node{std::move(parameter_label), "declaration"}));
        }
        if (function->body)
        {
          node->children.push_back(make_statement_node(*function->body));
        }
        else if (function->expression_body)
        {
          auto body = std::make_unique<visual_node>(visual_node{"Expression body\n=>", "declaration"});
          body->children.push_back(make_expression_node(*function->expression_body));
          node->children.push_back(std::move(body));
        }
        else
        {
          node->children.push_back(std::make_unique<visual_node>(visual_node{"Signature", "declaration"}));
        }
        return node;
      }
      if (const auto *type = dynamic_cast<const type_declaration *>(&value))
      {
        const std::string kind_name = type->type_kind == type_declaration::kind::interface_type
                                          ? "Face"
                                      : type->type_kind == type_declaration::kind::class_type ? "Class"
                                                                                              : "Enum";
        auto node = std::make_unique<visual_node>(visual_node{kind_name + "\n" + type->name, "declaration"});
        if (type->composition_keyword)
        {
          auto composition = std::make_unique<visual_node>(
              visual_node{"Composition\n" + *type->composition_keyword, "declaration"});
          for (const auto &interface_name : type->composed_interfaces)
          {
            composition->children.push_back(
                std::make_unique<visual_node>(visual_node{"Interface\n" + interface_name, "declaration"}));
          }
          node->children.push_back(std::move(composition));
        }
        for (const auto &member : type->members)
        {
          node->children.push_back(make_statement_node(*member));
        }
        for (const auto &parameter : type->type_parameters)
          node->children.insert(node->children.begin(), std::make_unique<visual_node>(
              visual_node{"Type parameter\n" + parameter, "type"}));
        for (const auto &member : type->enum_members)
        {
          auto member_node = std::make_unique<visual_node>(
              visual_node{"Enum member\n" + member.name, "declaration"});
          for (const auto &payload : member.payload_types)
            member_node->children.push_back(std::make_unique<visual_node>(
                visual_node{"Payload type\n" + payload, "type"}));
          for (const auto &comment : member.documentation)
          {
            member_node->children.push_back(std::make_unique<visual_node>(
                visual_node{"Documentation\n" + string_preview(comment.text), "documentation"}));
          }
          node->children.push_back(std::move(member_node));
        }
        return node;
      }
      if (const auto *measurement = dynamic_cast<const measurement_declaration *>(&value))
      {
        const char *kind = measurement->declaration_kind == measurement_declaration::kind::dimension
                               ? "Dimension"
                           : measurement->declaration_kind == measurement_declaration::kind::quantity
                               ? "Quantity"
                           : measurement->declaration_kind == measurement_declaration::kind::affine_unit
                               ? "Affine unit"
                               : "Unit";
        auto node = std::make_unique<visual_node>(visual_node{std::string(kind) + "\n" + measurement->name,
                                                              "declaration"});
        if (measurement->declared_dimension)
          node->children.push_back(std::make_unique<visual_node>(
              visual_node{"Dimension\n" + *measurement->declared_dimension, "type"}));
        if (measurement->definition)
          node->children.push_back(std::make_unique<visual_node>(
              visual_node{"Definition\n" + *measurement->definition, "expression"}));
        for (const auto &[property, property_value] : measurement->properties)
          node->children.push_back(std::make_unique<visual_node>(
              visual_node{"Property\n" + property + ": " + property_value, "declaration"}));
        return node;
      }
      if (const auto *module = dynamic_cast<const module_declaration *>(&value))
      {
        return std::make_unique<visual_node>(visual_node{"Module\n" + module->name, "declaration"});
      }
      if (const auto *imported = dynamic_cast<const import_declaration *>(&value))
      {
        std::string label = "Import\n" + imported->imported_name;
        if (imported->source_module)
        {
          label += " from " + *imported->source_module;
        }
        if (imported->alias)
        {
          label += " as " + *imported->alias;
        }
        return std::make_unique<visual_node>(visual_node{std::move(label), "declaration"});
      }
      if (const auto *exported = dynamic_cast<const export_declaration *>(&value))
      {
        std::string label = "Export\n" + exported->exported_name;
        if (exported->alias)
        {
          label += " as " + *exported->alias;
        }
        return std::make_unique<visual_node>(visual_node{std::move(label), "declaration"});
      }
      throw std::runtime_error("AST renderer encountered an unsupported statement node");
    }

    auto make_statement_node(const statement &value) -> std::unique_ptr<visual_node>
    {
      auto node = make_statement_node_content(value);
      std::vector<std::unique_ptr<visual_node>> comments;
      comments.reserve(value.documentation.size());
      for (const auto &comment : value.documentation)
      {
        comments.push_back(std::make_unique<visual_node>(
            visual_node{"Documentation\n" + string_preview(comment.text), "documentation"}));
      }
      node->children.insert(node->children.begin(),
                            std::make_move_iterator(comments.begin()),
                            std::make_move_iterator(comments.end()));
      return node;
    }

    auto make_tree(const program &tree) -> std::unique_ptr<visual_node>
    {
      auto root = std::make_unique<visual_node>(visual_node{"Program", "program"});
      for (const auto &statement : tree.statements)
      {
        root->children.push_back(make_statement_node(*statement));
      }
      return root;
    }

    auto escape_xml(std::string_view text) -> std::string
    {
      std::string result;
      for (const char character : text)
      {
        switch (character)
        {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '"': result += "&quot;"; break;
        case '\'': result += "&#39;"; break;
        default: result.push_back(character); break;
        }
      }
      return result;
    }

    auto escape_dot(std::string_view text) -> std::string
    {
      std::string result;
      for (const char character : text)
      {
        if (character == '\n')
        {
          result += "\\n";
          continue;
        }
        if (character == '"' || character == '\\')
        {
          result.push_back('\\');
        }
        result.push_back(character);
      }
      return result;
    }

    auto assign_positions(visual_node &node, const std::size_t depth, double &next_leaf,
                          std::size_t &maximum_depth) -> void
    {
      maximum_depth = std::max(maximum_depth, depth);
      node.y = 75.0 + static_cast<double>(depth) * 150.0;
      if (node.children.empty())
      {
        node.x = next_leaf;
        next_leaf += 220.0;
        return;
      }
      for (auto &child : node.children)
      {
        assign_positions(*child, depth + 1, next_leaf, maximum_depth);
      }
      node.x = (node.children.front()->x + node.children.back()->x) / 2.0;
    }

    auto node_colors(std::string_view category) -> std::pair<std::string_view, std::string_view>
    {
      if (category == "program") return {"#172554", "#dbeafe"};
      if (category == "declaration") return {"#1d4ed8", "#eff6ff"};
      if (category == "operator") return {"#7e22ce", "#faf5ff"};
      if (category == "call") return {"#0f766e", "#f0fdfa"};
      if (category == "access") return {"#0369a1", "#f0f9ff"};
      if (category == "string") return {"#be185d", "#fdf2f8"};
      if (category == "text") return {"#a16207", "#fefce8"};
      if (category == "documentation") return {"#7c3aed", "#f5f3ff"};
      if (category == "collection") return {"#4338ca", "#eef2ff"};
      if (category == "entry") return {"#0f766e", "#f0fdfa"};
      if (category == "literal") return {"#15803d", "#f0fdf4"};
      if (category == "identifier") return {"#c2410c", "#fff7ed"};
      return {"#475569", "#f8fafc"};
    }

    auto write_edges(std::ostream &output, const visual_node &node) -> void
    {
      for (const auto &child : node.children)
      {
        output << "  <path d=\"M " << node.x << ' ' << node.y + 36.0 << " C " << node.x << ' '
               << node.y + 86.0 << ", " << child->x << ' ' << child->y - 86.0 << ", " << child->x << ' '
               << child->y - 36.0 << "\" class=\"edge\"/>\n";
        write_edges(output, *child);
      }
    }

    auto write_nodes(std::ostream &output, const visual_node &node) -> void
    {
      const auto [stroke, fill] = node_colors(node.category);
      output << "  <g class=\"node\">\n"
             << "    <rect x=\"" << node.x - 82.0 << "\" y=\"" << node.y - 36.0
             << "\" width=\"164\" height=\"72\" rx=\"12\" fill=\"" << fill
             << "\" stroke=\"" << stroke << "\"/>\n";
      std::istringstream lines(node.label);
      std::string line;
      int line_number = 0;
      while (std::getline(lines, line))
      {
        const double line_y = node.y + (line_number == 0 ? -6.0 : 16.0);
        output << "    <text x=\"" << node.x << "\" y=\"" << line_y << "\" class=\"label"
               << (line_number == 0 ? " primary" : "") << "\">" << escape_xml(line) << "</text>\n";
        line_number++;
      }
      output << "  </g>\n";
      for (const auto &child : node.children)
      {
        write_nodes(output, *child);
      }
    }

    auto write_dot_node(std::ostream &output, const visual_node &node, std::size_t &next_id) -> std::size_t
    {
      const std::size_t id = next_id++;
      output << "  n" << id << " [label=\"" << escape_dot(node.label) << "\", class=\"" << node.category
             << "\"];\n";
      for (const auto &child : node.children)
      {
        const std::size_t child_id = write_dot_node(output, *child, next_id);
        output << "  n" << id << " -> n" << child_id << ";\n";
      }
      return id;
    }
  }

  auto render_ast_dot(const program &tree) -> std::string
  {
    const auto root = make_tree(tree);
    std::ostringstream output;
    output << "digraph SaganAST {\n  rankdir=TB;\n  node [shape=box, style=\"rounded,filled\", fontname=\"sans-serif\"];\n";
    std::size_t next_id = 0;
    static_cast<void>(write_dot_node(output, *root, next_id));
    output << "}\n";
    return output.str();
  }

  auto render_ast_svg(const program &tree) -> std::string
  {
    auto root = make_tree(tree);
    double next_leaf = 110.0;
    std::size_t maximum_depth = 0;
    assign_positions(*root, 0, next_leaf, maximum_depth);
    const double width = std::max(220.0, next_leaf);
    const double height = 150.0 + static_cast<double>(maximum_depth) * 150.0;

    std::ostringstream output;
    output << std::fixed << std::setprecision(1)
           << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << width << ' ' << height
           << "\" width=\"" << width << "\" height=\"" << height << "\" data-natural-width=\"" << width
           << "\" data-natural-height=\"" << height
           << "\" role=\"img\" aria-label=\"Sagan abstract syntax tree\">\n"
           << "  <style>.edge{fill:none;stroke:#94a3b8;stroke-width:2}.node rect{stroke-width:2}"
              ".label{font:14px ui-monospace,SFMono-Regular,Consolas,monospace;text-anchor:middle;fill:#0f172a}"
              ".label.primary{font-weight:700}</style>\n";
    write_edges(output, *root);
    write_nodes(output, *root);
    output << "</svg>\n";
    return output.str();
  }

  auto render_ast_html(std::string_view source, const program &tree, std::string_view title) -> std::string
  {
    std::ostringstream output;
    output << "<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
           << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n<title>"
           << escape_xml(title) << "</title>\n<style>" // LCOV_EXCL_LINE - gcov attributes this continued output expression inconsistently.
              ":root{color-scheme:light dark;font-family:Inter,system-ui,sans-serif}body{margin:0;background:#0f172a;color:#e2e8f0}"
              "header{padding:1.4rem 2rem;border-bottom:1px solid #334155}h1{font-size:1.25rem;margin:0}"
              ".grid{display:grid;grid-template-columns:minmax(18rem,34rem) minmax(30rem,1fr);gap:1rem;padding:1rem}"
              ".panel{background:#111827;border:1px solid #334155;border-radius:12px;overflow:hidden}"
              ".panel h2{font-size:.8rem;letter-spacing:.08em;text-transform:uppercase;margin:0;padding:.8rem 1rem;border-bottom:1px solid #334155;color:#93c5fd}"
              "pre{font:14px/1.6 ui-monospace,SFMono-Regular,Consolas,monospace;margin:0;padding:1rem;white-space:pre-wrap;overflow:auto}"
              ".tree-tools{display:flex;align-items:center;gap:.45rem;padding:.55rem .75rem;background:#e2e8f0;border-bottom:1px solid #cbd5e1;color:#0f172a}"
              ".tree-tools button{border:1px solid #94a3b8;border-radius:7px;background:white;color:#0f172a;padding:.35rem .65rem;font-weight:700;cursor:pointer}"
              ".tree-tools button:hover{background:#dbeafe}.zoom-value{min-width:4rem;text-align:center;font:13px ui-monospace,monospace}"
              ".tree-help{margin-left:auto;font-size:.78rem;color:#475569}.tree{height:78vh;min-height:34rem;overflow:auto;background:#f8fafc;cursor:grab}"
              ".tree.dragging{cursor:grabbing;user-select:none}.tree svg{display:block;max-width:none;height:auto;transform-origin:0 0}"
              "@media(max-width:850px){.grid{grid-template-columns:1fr}.tree-help{display:none}}"
              "</style>\n</head>\n<body>\n<header><h1>" << escape_xml(title)
           << "</h1></header>\n<main class=\"grid\">\n<section class=\"panel\"><h2>Input source</h2><pre>"
           << escape_xml(source) << "</pre></section>\n<section class=\"panel\"><h2>Abstract syntax tree</h2>"
              "<div class=\"tree-tools\"><button type=\"button\" data-action=\"out\" aria-label=\"Zoom out\">−</button>"
              "<button type=\"button\" data-action=\"in\" aria-label=\"Zoom in\">+</button>"
              "<button type=\"button\" data-action=\"fit\">Fit</button><button type=\"button\" data-action=\"reset\">100%</button>"
              "<span class=\"zoom-value\">100%</span><span class=\"tree-help\">Wheel to zoom · drag to pan</span></div>"
              "<div class=\"tree\" tabindex=\"0\">\n"
           << render_ast_svg(tree)
           << "</div></section>\n</main>\n<script>"
              "const viewport=document.querySelector('.tree'),svg=viewport.querySelector('svg'),label=document.querySelector('.zoom-value');"
              "const natural=Number(svg.dataset.naturalWidth);let scale=1,drag=false,lastX=0,lastY=0;"
              "function setZoom(next,cx=viewport.clientWidth/2,cy=viewport.clientHeight/2){"
              "next=Math.min(3,Math.max(.02,next));const x=(viewport.scrollLeft+cx)/scale,y=(viewport.scrollTop+cy)/scale;"
              "scale=next;svg.style.width=(natural*scale)+'px';label.textContent=Math.round(scale*100)+'%';"
              "viewport.scrollLeft=x*scale-cx;viewport.scrollTop=y*scale-cy;}"
              "function fit(){setZoom(Math.min(1,(viewport.clientWidth-24)/natural),0,0);viewport.scrollLeft=0;viewport.scrollTop=0;}"
              "document.querySelector('.tree-tools').addEventListener('click',event=>{const action=event.target.dataset.action;if(!action)return;"
              "if(action==='in')setZoom(scale*1.2);if(action==='out')setZoom(scale/1.2);if(action==='fit')fit();"
              "if(action==='reset'){setZoom(1,0,0);viewport.scrollLeft=0;viewport.scrollTop=0;}});"
              "viewport.addEventListener('wheel',event=>{event.preventDefault();const box=viewport.getBoundingClientRect();"
              "setZoom(scale*(event.deltaY<0?1.12:1/1.12),event.clientX-box.left,event.clientY-box.top);},{passive:false});"
              "viewport.addEventListener('pointerdown',event=>{drag=true;lastX=event.clientX;lastY=event.clientY;viewport.classList.add('dragging');viewport.setPointerCapture(event.pointerId);});"
              "viewport.addEventListener('pointermove',event=>{if(!drag)return;viewport.scrollLeft-=event.clientX-lastX;viewport.scrollTop-=event.clientY-lastY;lastX=event.clientX;lastY=event.clientY;});"
              "viewport.addEventListener('pointerup',()=>{drag=false;viewport.classList.remove('dragging');});fit();"
              "</script>\n</body>\n</html>\n";
    return output.str();
  }
}
