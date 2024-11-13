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
    if (m_idx == m_str.size()) return 0;
    return m_str.data()[m_idx++];
  }
  constexpr char peek() const { return m_str.data()[m_idx]; }

  bool backtrack(hai::fn<bool, cs &> fn) {
    auto i = m_idx;
    if (fn(*this)) return true;
    m_idx = i;
    return false;
  }
};

namespace ast {
class stream {};
}

static void star(cs & cs, hai::fn<bool, ::cs &> fn) {
  while (fn(cs)) {}
}
[[nodiscard]] static bool plus(cs & cs, hai::fn<bool, ::cs &> fn) {
  if (!fn(cs)) return false;
  while (fn(cs)) {}
  return true;
}
// Just for documenting the optionality
static void opt(cs & cs, hai::fn<bool, ::cs &> fn) {
  fn(cs);
}
// Documents the grouping, makes return mandatory and avoids uncalled lambdas
[[nodiscard]] static bool group(cs & cs, hai::fn<bool, ::cs &> fn) {
  return fn(cs);
}

// Trying to implement this as close as possible to the YAML specs

enum k {
  block_in,
};

// TBDs
static bool c_byte_order_mark(cs & cs) { return false; }
static bool c_directives_end(cs & cs) { return false; }
static bool e_node(cs & cs) { return false; }
static bool l_comment(cs & cs) { return false; }
static bool l_directive(cs & cs) { return false; }
static bool l_document_prefix(cs & cs) { return false; }
static bool l_document_suffix(cs & cs) { return false; }
static bool s_l_block_node(cs & cs, int indent, k) { return false; }
static bool s_l_comments(cs & cs) { return false; }

static bool l_bare_document(cs & cs) {
  return s_l_block_node(cs, -1, k::block_in);
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
