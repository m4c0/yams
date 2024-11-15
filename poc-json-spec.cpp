#pragma leco tool

import hai;
import jason;
import jojo;
import jute;
import silog;

namespace j = jason::ast::nodes;

class parser {
  using node = jason::ast::node_ptr;

  node m_json;
  const j::dict & m_rules;

  void do_cond(const node & n) {
    if (n->type() == jason::ast::string) {
      auto s = cast<j::string>(n).str();
      if (s.size() == 1) {
        silog::trace("char", *s);
      } else if (s.size() && (*s)[0] == 'x') {
        silog::trace("x char", *s);
      } else if (*s == "<start-of-line>") {
        silog::trace("pseudo", "<sol>");
      } else if (*s == "<end-of-stream>") {
        silog::trace("pseudo", "<eos>");
      } else if (*s == "<empty>") {
        silog::trace("pseudo", "<empty>");
      } else {
        silog::trace("enter", *s);
        do_cond(m_rules[*s]);
        silog::trace("leave", *s);
      }
    } else if (n->type() == jason::ast::array) {
      for (auto & n : cast<j::array>(n)) {
        if (n->type() != jason::ast::string) silog::die("something inside array");
        do_cond(n);
      }
    } else if (n->type() == jason::ast::dict) {
      auto & [k, v] = *cast<j::dict>(n).begin();
      if (*k == "(all)") {
        for (auto & r : cast<j::array>(v)) do_cond(r);
      } else if (*k == "(any)") {
        for (auto & r : cast<j::array>(v)) do_cond(r);
      } else if (*k == "(---)") {
        for (auto & r : cast<j::array>(v)) do_cond(r);
      } else if (*k == "(+++)") {
        do_cond(v);
      } else if (*k == "(***)") {
        do_cond(v);
      } else if (*k == "(\?\?\?)") {
        do_cond(v);
      } else if (*k == "(exclude)") {
        do_cond(v);
      } else if (*k == "(...)") {
        // TODO: parse parameter name in `v`
      } else {
        silog::trace("eval", *k);
        do_cond(m_rules[*k]);
        // TODO: parse parameters in `v`
      }
    } else silog::die("unknown condition type: %d", n->type());
  }

public:
  constexpr explicit parser(jute::view src)
    : m_json { jason::parse(src) }
    , m_rules { cast<j::dict>(m_json) } {}

  void do_rule(jute::view key) {
    do_cond(m_rules[key]);
  }
};

static void parse(void *, hai::array<char> & data) {
  parser p { jute::view { data.begin(), data.size() } };
  p.do_rule("l-yaml-stream");
}

int main() {
  jojo::read("yaml-spec-1.2.json", nullptr, parse);
}
