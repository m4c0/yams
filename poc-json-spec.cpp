#pragma leco tool

import hai;
import hashley;
import jason;
import jojo;
import jute;
import print;
import silog;
import traits;

namespace j = jason::ast::nodes;

static auto c_friendly_name(jute::view n) { return n; } // TODO

class fn {
public:
  virtual ~fn() {}
  virtual void emit_fwd_decl() const = 0;
  virtual void emit_impl() const = 0;
  virtual void emit_body() const = 0;
};
using fn_ptr = hai::uptr<fn>;

class wrap_fn : public fn {
  fn_ptr m_fn {};
public:
  constexpr explicit wrap_fn(fn_ptr fn) : m_fn { traits::move(fn) } {}
  virtual void emit_fwd_decl() const override { if (m_fn) m_fn->emit_fwd_decl(); }
  virtual void emit_impl() const override { if (m_fn) m_fn->emit_impl(); }
  virtual void emit_body() const override { if (m_fn) m_fn->emit_body(); }
};

class arr_fn : public fn {
  hai::array<fn_ptr> m_fns;

protected:
  void emit_fns(jute::view op) const {
    // bool first { true };
    for (auto & fn : m_fns) {
      if (!fn) continue;
      // if (!first) put(op);
      // put("(");
      fn->emit_body();
      // put(")");
      // first = false;
    }
  }
public:
  constexpr explicit arr_fn(hai::array<fn_ptr> fns) : m_fns { traits::move(fns) } {}
  virtual void emit_fwd_decl() const override { 
    for (auto & fn : m_fns) if (fn) fn->emit_fwd_decl(); 
  }
  virtual void emit_impl() const override { 
    for (auto & fn : m_fns) if (fn) fn->emit_impl(); 
  }
};
class term_fn : public fn {
public:
  virtual void emit_fwd_decl() const override {}
  virtual void emit_impl() const override {}
};

class all : public arr_fn {
public:
  using arr_fn::arr_fn;
  void emit_body() const override { emit_fns("&&"); }
};
class any : public arr_fn {
public:
  using arr_fn::arr_fn;
  void emit_body() const { emit_fns("||"); }
};
class sub : public arr_fn {
public:
  using arr_fn::arr_fn;
  void emit_body() const { putln(""); }
};

class plus : public wrap_fn {
public:
  using wrap_fn::wrap_fn;
  void emit_body() const { putln(""); }
};
class star : public wrap_fn {
public:
  using wrap_fn::wrap_fn;
};
class opt : public wrap_fn {
public:
  using wrap_fn::wrap_fn;
  void emit_body() const { putln(""); }
};
class excl : public wrap_fn {
public:
  using wrap_fn::wrap_fn;
  void emit_body() const { putln(""); }
};

class match : public term_fn {
  jute::heap m_c;
public:
  constexpr explicit match(jute::heap c) : m_c { c } {}
  void emit_body() const {
    if (m_c.size() == 0) silog::die("empty matcher");
    if (m_c.size() == 1) put("match('", m_c, "')");
    else if (m_c.size() == 3) put("match(0", m_c, ")");
    else if ((*m_c)[0] == 'x' && (m_c.size() % 2) == 1) {
      auto v = (*m_c).subview(1).after;
      put("bt([] {");
      while (v.size()) {
        auto [n, r] = v.subview(2);
        put("match(0x", n, ")&&");
        v = r;
      }
      put("true})");
    }
    else silog::die("invalid char matcher");
  }
};
class range : public term_fn {
  jute::heap m_min;
  jute::heap m_max;

  static constexpr auto to_wc(jute::view c) {
    // TODO: properly support wide-chars
    if (c.size() < 3) silog::die("found range with single char");
    return c.size() == 3 ? c : jute::view { "xFF" };
  }
public:
  constexpr explicit range(jute::heap mn, jute::heap mx) : m_min { mn }, m_max { mx } {}
  void emit_body() const {
    put("range(0", to_wc(*m_min), ", 0", to_wc(*m_max), ")");
  }
};

struct start_of_line : public term_fn {
  void emit_body() const { put("sol()"); }
};
struct end_of_stream : public term_fn {
  void emit_body() const { put("match(0)"); }
};
struct empty : public term_fn {
  void emit_body() const { put("empty()"); }
};

class rule : public wrap_fn {
  jute::heap m_name;

public:
  constexpr rule(jute::heap n, fn_ptr fn) : wrap_fn { traits::move(fn) }, m_name { n } {}

  void emit_fwd_decl() const override {
    wrap_fn::emit_fwd_decl();
    putln("static bool ", c_friendly_name(*m_name), "();");
  }
  void emit_impl() const override {
    wrap_fn::emit_impl();

    putln("static bool ", c_friendly_name(*m_name), "() { return bt([] {");
    wrap_fn::emit_body();
    putln("}); }");
  }

  void emit_body() const override {
    put(c_friendly_name(*m_name), "()");
    wrap_fn::emit_body();
  }
};

class parser {
  using node = jason::ast::node_ptr;

  node m_json;
  const j::dict & m_rules;
  hashley::niamh m_done { 113 };

  fn_ptr do_string(const node & n) {
    auto s = cast<j::string>(n).str();
    if (s.size() == 1 || (s.size() && (*s)[0] == 'x')) {
      return fn_ptr { new match { *s } };
    } else if (*s == "<start-of-line>") {
      return fn_ptr { new start_of_line() };
    } else if (*s == "<end-of-stream>") {
      return fn_ptr { new end_of_stream() };
    } else if (*s == "<empty>") {
      return fn_ptr { new empty() };
    } else {
      return do_rule(*s);
    }
  }

  fn_ptr do_array(const node & n) {
    auto & arr = cast<j::array>(n);
    if (arr.size() != 2) silog::die("invalid size for range");
    if (arr[0]->type() != jason::ast::string) silog::die("something inside array min");
    if (arr[1]->type() != jason::ast::string) silog::die("something inside array max");

    auto s = cast<j::string>(arr[0]).str();
    if (s.size() != 1 && !(s.size() && (*s)[0] == 'x')) silog::die("non-char inside array min");

    auto v = cast<j::string>(arr[1]).str();
    if (v.size() != 1 && !(v.size() && (*v)[0] == 'x')) silog::die("non-char inside array max");

    return fn_ptr { new range(s, v) };
  }

  template<typename T>
  fn_ptr do_arr_fn(const node & n) {
    hai::varray<fn_ptr> fns { 128 };
    for (auto & r : cast<j::array>(n)) fns.push_back(do_cond(r));
    return fn_ptr { new T { traits::move(fns) } };
  }
  template<typename T>
  fn_ptr do_wrap_fn(const node & n) {
    auto fn = do_cond(n);
    return fn_ptr { new T { traits::move(fn) } };
  }

  fn_ptr do_pair(jute::heap k, const node & v) {
    if      (*k == "(all)") return do_arr_fn<all>(v);
    else if (*k == "(any)") return do_arr_fn<any>(v);
    else if (*k == "(---)") return do_arr_fn<sub>(v);
    else if (*k == "(+++)") return do_wrap_fn<plus>(v);
    else if (*k == "(***)") return do_wrap_fn<star>(v);
    else if (*k == "(\?\?\?)") return do_wrap_fn<opt>(v);
    else if (*k == "(exclude)") return do_wrap_fn<excl>(v);
    else if (*k == "(case)") return {}; // TODO: TBD
    else if (*k == "(<<<)") {
      // TODO: check if parameter is non-zero???
      do_cond(v);
      return {};
    }
    else if (*k == "(!==)") return {}; // TODO
    else if (*k == "(<=)") return {}; // TODO
    else if (*k == "(<)") return {}; // TODO
    else if (*k == "({2})") return {}; // TODO
    else if (*k == "({4})") return {}; // TODO
    else if (*k == "({8})") return {}; // TODO
    else if (*k == "({n})") return {}; // TODO
    else if (*k == "(set)") return {}; // TODO
    else if (*k == "(max)") return {}; // TODO
    else {
      do_rule(*k);
      // TODO: parse parameters in `v`
      return {};
    }
  }
  fn_ptr do_dict(const node & n) {
    auto it = cast<j::dict>(n).begin();
    auto & [k, v] = *it;
    if (*k == "(...)") {
      // TODO: parse parameter name in `v`
      auto & [kk, vv] = *++it;
      return do_pair(kk, vv);
    }
    else if (*k == "(if)") {
      // TODO: eval condition in 'v'
      auto & [kk, vv] = *++it;
      return do_pair(kk, vv);
    }
    else return do_pair(k, v);
  }

  fn_ptr do_cond(const node & n) {
    if (n->type() == jason::ast::string) {
      return do_string(n);
    } else if (n->type() == jason::ast::array) {
      return do_array(n);
    } else if (n->type() == jason::ast::dict) {
      return do_dict(n);
    } else silog::die("unknown condition type: %d", n->type());
  }

public:
  constexpr explicit parser(jute::view src)
    : m_json { jason::parse(src) }
    , m_rules { cast<j::dict>(m_json) } {}

  fn_ptr do_rule(jute::view key) {
    // TODO: cache the result
    auto & k = m_done[key];
    if (k) return {};
    k = 1;
    return fn_ptr { new rule(key, do_cond(m_rules[key])) };
  }
};

static void parse(void *, hai::array<char> & data) {
  parser p { jute::view { data.begin(), data.size() } };
  auto fn = p.do_rule("l-yaml-stream");
  if (!fn) silog::die("something is not right");
  fn->emit_fwd_decl();
  fn->emit_impl();
}

int main() {
  jojo::read("yaml-spec-1.2.json", nullptr, parse);
}
