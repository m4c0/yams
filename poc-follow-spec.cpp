#pragma leco tool

import hai;
import jojo;
import jute;

class cs {
  const hai::cstr & m_str;
  unsigned m_idx {};
public:
  constexpr explicit cs(const hai::cstr & str) : m_str { str } {}

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

  constexpr bool backtrack(hai::fn<bool> fn) {
    auto i = m_idx;
    if (fn()) return true;
    m_idx = i;
    return false;
  }
};

namespace ast {
class stream {};
}

static cs * g_cs {};

static bool bt(hai::fn<bool> fn) {
  return g_cs->backtrack(fn);
}

static bool star(hai::fn<bool> fn) {
  while (bt(fn)) {}
  return true;
}
[[nodiscard]] static bool plus(hai::fn<bool> fn) {
  if (!bt(fn)) return false;
  while (bt(fn)) {}
  return true;
}
static bool opt(hai::fn<bool> fn) {
  bt(fn);
  return true;
}
[[nodiscard]] static bool group(hai::fn<bool> fn) {
  return bt(fn);
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
static bool b_char() { return false; }
static bool c_byte_order_mark() { return false; }
static bool c_directives_end() { return false; }
static bool c_lp_folded(int indent) { return false; }
static bool c_lp_literal(int indent) { return false; }
static bool c_printable() { return false; }
static bool c_ns_properties(int indent, context k) { return false; }
static bool e_node() { return false; }
static bool l_directive() { return false; }
static bool l_document_prefix() { return false; }
static bool l_document_suffix() { return false; }
static bool lp_block_mapping(int indent) { return false; }
static bool ns_flow_node(int, context) { return false; }
static bool s_flow_line_prefix(int indent) { return false; }

static bool end_of_input() { return g_cs->peek() == 0; }
static bool start_of_line() {
  // Kinda annoying how YAML spec define this as:
  // <start-of-line>, which matches the empty string at the beginning of a line
  // So far, this is my interpretation: match a CR, a LF or the beginning of the file
  auto c = g_cs->last_char();
  return c == 0x10 || c == 0x13 || c == 0x0;
}

static bool s_space() { return g_cs->match(0x20); }
static bool s_tab() { return g_cs->match(0x09); }
static bool s_white() { return s_space() || s_tab(); }

static bool s_separate_in_line() {
  return plus(s_white)
      || start_of_line();
}

static bool b_line_feed() { return g_cs->match(0x0A); }
static bool b_carriage_return() { return g_cs->match(0x0D); }
static bool b_break() { 
  return g_cs->backtrack([] {
        return b_carriage_return()
            && b_line_feed();
      })
      || b_carriage_return()
      || b_line_feed();
}

static bool b_non_content() { return b_break(); }

static bool nb_char() {
  // c-printable - b-char - c-byte-order-mark
  if (!g_cs->backtrack(c_printable)) return false;
  if (g_cs->backtrack(b_char)) return false;
  if (g_cs->backtrack(c_byte_order_mark)) return false;
  g_cs->take();
  return true;
}

static bool c_comment() { return g_cs->match('#'); }
static bool c_nb_comment_text() {
  return c_comment()
      && star(nb_char);
}

static bool b_comment() {
  return b_non_content()
      || end_of_input();
}
static bool s_b_comment() {
  opt([] {
    return s_separate_in_line()
        && opt(c_nb_comment_text);
  });
  return b_comment();
}

static bool l_comment() {
  return g_cs->backtrack([] {
    return s_separate_in_line()
        && opt(c_nb_comment_text)
        && b_comment();
  });
}

static bool s_l_comments() {
  return group([] {
        return s_b_comment()
            || start_of_line();
      })
      && star(l_comment);
}

static bool s_separate_lines(int indent) { 
  return g_cs->backtrack([&] {
        return s_l_comments()
            && s_flow_line_prefix(indent);
      })
      || s_separate_in_line();
}

static bool s_separate(int indent, context k) {
  switch (k) {
    case context::block_out:
    case context::block_in:
    case context::flow_out:
    case context::flow_in:
      return s_separate_lines(indent);

    case context::block_key:
    case context::flow_key:
      return s_separate_in_line();
  }
}

static bool lp_block_sequence(int indent) {
  return plus([&]() -> bool {
    throw 0;
  });
}

static bool seq_space(int indent, context k) {
  switch (k) {
    case context::block_out: return lp_block_sequence(indent + 1);
    case context::block_in:  return lp_block_sequence(indent);
    default: return false;
  }
}
static bool s_lp_block_collection(int indent, context k) {
  return g_cs->backtrack([&]() {
    opt([&]() {
      return s_separate(indent + 1, k)
          && c_ns_properties(indent + 1, k);
    });
    if (!s_l_comments()) return false;
    return seq_space(indent, k)
        || lp_block_mapping(indent);
  });
}

static bool s_lp_block_scalar(int indent, context k) {
  return g_cs->backtrack([&]() {
    return s_separate(indent + 1, k)
        && opt([&]() {
          return g_cs->backtrack([&]() {
            return c_ns_properties(indent + 1, k)
                && s_separate(indent + 1, k);
          });
        })
        && group([&]() {
          return c_lp_literal(indent)
              || c_lp_folded(indent);
        });
  });
}

static bool s_lp_block_in_block(int indent, context k) {
  return s_lp_block_scalar(indent, k)
      || s_lp_block_collection(indent, k);
}

static bool s_lp_flow_in_block(int indent) {
  return g_cs->backtrack([=]() {
    return s_separate(indent + 1, context::flow_out)
        && ns_flow_node(indent + 1, context::flow_out)
        && s_l_comments();
  });
}
static bool s_lp_block_node(int indent, context k) {
  return s_lp_block_in_block(indent, k)
      || s_lp_flow_in_block(indent);
}

static bool l_bare_document() {
  return s_lp_block_node(-1, context::block_in);
}

static bool l_explicit_document() {
  return g_cs->backtrack([] {
    if (!c_directives_end()) return false;
    return group([] {
      return l_bare_document()
          || g_cs->backtrack([] {
              return e_node()
                  && s_l_comments();
          });
    });
  });
}

static bool l_directive_document() {
  if (!plus(l_directive)) return false;
  return l_explicit_document();
}

static bool l_any_document() {
  return l_directive_document()
    || l_explicit_document()
    || l_bare_document();
}

static ast::stream l_yaml_stream() {
  ast::stream res {};
  star(l_document_prefix);
  opt(l_any_document);
  star([] {
    return group([] {
      if (!plus(l_document_suffix)) return false;
      star(l_document_prefix);
      opt(l_any_document);
      return true;
    })
    || c_byte_order_mark()
    || l_comment()
    || l_explicit_document();
  });
  return res;
}

static void parse(void *, hai::cstr & str) {
  cs c { str };
  g_cs = &c;
  l_yaml_stream();
}

int main(int argc, char ** argv) try {
  for (auto i = 1; i < argc; i++) {
    jojo::read(jute::view::unsafe(argv[i]), nullptr, parse);
  }
} catch (...) {
  return 1;
}
