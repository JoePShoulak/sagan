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
        const char *kind = collection->collection_kind == collection_expression::kind::array        ? "Array"
                           : collection->collection_kind == collection_expression::kind::vector     ? "Vector"
                                                                                                     : "Coordinate";
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
      throw std::runtime_error("AST renderer encountered an unsupported expression node");
    }

    auto make_tree(const program &tree) -> std::unique_ptr<visual_node>
    {
      auto root = std::make_unique<visual_node>(visual_node{"Program", "program"});
      for (const auto &statement : tree.statements)
      {
        if (const auto *declaration = dynamic_cast<const let_declaration *>(statement.get()))
        {
          std::string label = "Let\n" + declaration->name;
          if (declaration->type_name)
          {
            label += ": " + *declaration->type_name;
          }
          auto node = std::make_unique<visual_node>(visual_node{std::move(label), "declaration"});
          if (declaration->initializer)
          {
            node->children.push_back(make_expression_node(*declaration->initializer));
          }
          root->children.push_back(std::move(node));
          continue;
        }
        throw std::runtime_error("AST renderer encountered an unsupported statement node");
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
           << escape_xml(title) << "</title>\n<style>"
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
