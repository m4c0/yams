#pragma leco tool

import hai;
import jojo;
import jute;

class cs {
  const hai::cstr & m_str;
  unsigned m_idx {};
public:
  explicit cs(const hai::cstr & str) : m_str { str } {}

  constexpr char take() {
    return m_idx >= m_str.size() ? 0 : m_str.data()[m_idx++]; 
  }
  constexpr char peek() const { 
    return m_idx >= m_str.size() ? 0 : m_str.data()[m_idx]; 
  }

  constexpr bool match(char c) {
    if (peek() != c) return false;
    take();
    return true;
  }

  constexpr char last_char() const {
    return m_idx > 0 ? m_str.data()[m_idx - 1] : 0;
  }

  constexpr bool backtrack(hai::fn<bool, cs &> fn) {
    auto i = m_idx;
    if (fn(*this)) return true;
    m_idx = i;
    return false;
  }
};

namespace ast {
class stream {};
}

static bool star(cs & cs, hai::fn<bool, ::cs &> fn) {
  while (fn(cs)) {}
  return true;
}
[[nodiscard]] static bool plus(cs & cs, hai::fn<bool, ::cs &> fn) {
  if (!fn(cs)) return false;
  while (fn(cs)) {}
  return true;
}
// Just for documenting the optionality
static bool opt(cs & cs, hai::fn<bool, ::cs &> fn) {
  fn(cs);
  return true;
}
// Documents the grouping, makes return mandatory and avoids uncalled lambdas
[[nodiscard]] static bool group(cs & cs, hai::fn<bool, ::cs &> fn) {
  return fn(cs);
}

// Trying to implement this as close as possible to the YAML specs

enum class context {
  block_in,
  block_out,
  block_key,
  flow_in,
  flow_out,
  flow_key,
};

// TBDs
static bool b_char(cs & cs) { return false; }
static bool c_byte_order_mark(cs & cs) { return false; }
static bool c_directives_end(cs & cs) { return false; }
static bool c_lp_folded(cs & cs, int indent) { return false; }
static bool c_lp_literal(cs & cs, int indent) { return false; }
static bool c_printable(cs & cs) { return false; }
static bool c_ns_properties(cs & cs, int indent, context k) { return false; }
static bool e_node(cs & cs) { return false; }
static bool l_directive(cs & cs) { return false; }
static bool l_document_prefix(cs & cs) { return false; }
static bool l_document_suffix(cs & cs) { return false; }
static bool lp_block_mapping(cs & cs, int indent) { return false; }
static bool ns_flow_node(cs & cs, int, context) { return false; }
static bool s_flow_line_prefix(cs & cs, int indent) { return false; }

static bool end_of_input(cs & cs) { return cs.peek() == 0; }
static bool start_of_line(cs & cs) {
  // Kinda annoying how YAML spec define this as:
  // <start-of-line>, which matches the empty string at the beginning of a line
  // So far, this is my interpretation: match a CR, a LF or the beginning of the file
  auto c = cs.last_char();
  return c == 0x10 || c == 0x13 || c == 0x0;
}

static bool s_space(cs & cs) { return cs.match(0x20); }
static bool s_tab(cs & cs) { return cs.match(0x09); }
static bool s_white(cs & cs) { return s_space(cs) || s_tab(cs); }

static bool s_separate_in_line(cs & cs) {
  return plus(cs, s_white)
      || start_of_line(cs);
}

static bool b_line_feed(cs & cs) { return cs.match(0x0A); }
static bool b_carriage_return(cs & cs) { return cs.match(0x0D); }
static bool b_break(cs & cs) { 
  return cs.backtrack([](auto & cs) {
        return b_carriage_return(cs)
            && b_line_feed(cs);
      })
      || b_carriage_return(cs)
      || b_line_feed(cs);
}

static bool b_non_content(cs & cs) { return b_break(cs); }

static bool nb_char(cs & cs) {
  // c-printable - b-char - c-byte-order-mark
  if (!cs.backtrack(c_printable)) return false;
  if (cs.backtrack(b_char)) return false;
  if (cs.backtrack(c_byte_order_mark)) return false;
  cs.take();
  return true;
}

static bool c_comment(cs & cs) { return cs.match('#'); }
static bool c_nb_comment_text(cs & cs) {
  return c_comment(cs)
      && star(cs, nb_char);
}

static bool b_comment(cs & cs) {
  return b_non_content(cs)
      || end_of_input(cs);
}
static bool s_b_comment(cs & cs) {
  opt(cs, [](auto & cs) {
    return s_separate_in_line(cs)
        && opt(cs, c_nb_comment_text);
  });
  return b_comment(cs);
}

static bool l_comment(cs & cs) {
  return cs.backtrack([](auto & cs) {
    return s_separate_in_line(cs)
        && opt(cs, c_nb_comment_text)
        && b_comment(cs);
  });
}

static bool s_l_comments(cs & cs) {
  return group(cs, [](auto & cs) {
        return s_b_comment(cs)
            || start_of_line(cs);
      })
      && star(cs, l_comment);
}

static bool s_separate_lines(cs & cs, int indent) { 
  return cs.backtrack([&](auto cs) {
        return s_l_comments(cs)
            && s_flow_line_prefix(cs, indent);
      })
      || s_separate_in_line(cs);
}

static bool s_separate(cs & cs, int indent, context k) {
  switch (k) {
    case context::block_out:
    case context::block_in:
    case context::flow_out:
    case context::flow_in:
      return s_separate_lines(cs, indent);

    case context::block_key:
    case context::flow_key:
      return s_separate_in_line(cs);
  }
}

static bool lp_block_sequence(cs & cs, int indent) {
  return plus(cs, [&](auto & cs) -> bool {
    throw 0;
  });
}

static bool seq_space(cs & cs, int indent, context k) {
  switch (k) {
    case context::block_out: return lp_block_sequence(cs, indent + 1);
    case context::block_in:  return lp_block_sequence(cs, indent);
    default: return false;
  }
}
static bool s_lp_block_collection(cs & cs, int indent, context k) {
  return cs.backtrack([&](auto & cs) {
    opt(cs, [&](auto & cs) {
      return s_separate(cs, indent + 1, k)
          && c_ns_properties(cs, indent + 1, k);
    });
    if (!s_l_comments(cs)) return false;
    return seq_space(cs, indent, k)
        || lp_block_mapping(cs, indent);
  });
}

static bool s_lp_block_scalar(cs & cs, int indent, context k) {
  return cs.backtrack([&](auto & cs) {
    return s_separate(cs, indent + 1, k)
        && opt(cs, [&](auto & cs) {
          return cs.backtrack([&](auto & cs) {
            return c_ns_properties(cs, indent + 1, k)
                && s_separate(cs, indent + 1, k);
          });
        })
        && group(cs, [&](auto & cs) {
          return c_lp_literal(cs, indent)
              || c_lp_folded(cs, indent);
        });
  });
}

static bool s_lp_block_in_block(cs & cs, int indent, context k) {
  return s_lp_block_scalar(cs, indent, k)
      || s_lp_block_collection(cs, indent, k);
}

static bool s_lp_flow_in_block(cs & cs, int indent) {
  return cs.backtrack([=](auto & cs) {
    return s_separate(cs, indent + 1, context::flow_out)
        && ns_flow_node(cs, indent + 1, context::flow_out)
        && s_l_comments(cs);
  });
}
static bool s_lp_block_node(cs & cs, int indent, context k) {
  return s_lp_block_in_block(cs, indent, k)
      || s_lp_flow_in_block(cs, indent);
}

static bool l_bare_document(cs & cs) {
  return s_lp_block_node(cs, -1, context::block_in);
}

static bool l_explicit_document(cs & cs) {
  return cs.backtrack([](auto & cs) {
    if (!c_directives_end(cs)) return false;
    return group(cs, [](auto & cs) {
      return l_bare_document(cs)
          || cs.backtrack([](auto & cs) {
              return e_node(cs)
                  && s_l_comments(cs);
          });
    });
  });
}

static bool l_directive_document(cs & cs) {
  if (!plus(cs, l_directive)) return false;
  return l_explicit_document(cs);
}

static bool l_any_document(cs & cs) {
  return l_directive_document(cs)
    || l_explicit_document(cs)
    || l_bare_document(cs);
}

static ast::stream l_yaml_stream(cs & cs) {
  ast::stream res {};
  star(cs, l_document_prefix);
  opt(cs, l_any_document);
  star(cs, [](auto & cs) {
    return group(cs, [](auto & cs) {
      if (!plus(cs, l_document_suffix)) return false;
      star(cs, l_document_prefix);
      opt(cs, l_any_document);
      return true;
    })
    || c_byte_order_mark(cs)
    || l_comment(cs)
    || l_explicit_document(cs);
  });
  return res;
}

static void parse(void *, hai::cstr & str) {
  cs s { str };
  l_yaml_stream(s);
}

int main(int argc, char ** argv) try {
  for (auto i = 1; i < argc; i++) {
    jojo::read(jute::view::unsafe(argv[i]), nullptr, parse);
  }
} catch (...) {
  return 1;
}
