#pragma leco tool

import hai;
import jason;
import jojo;
import jute;
import silog;
import traits;

namespace j = jason::ast::nodes;

class fn {
};
using fn_ptr = hai::uptr<fn>;

class wrap_fn : public fn {
  fn_ptr m_fn;
public:
  constexpr explicit wrap_fn(fn_ptr fn) : m_fn { traits::move(fn) } {}
};

class arr_fn : public fn {
  hai::array<fn_ptr> m_fns;
public:
  constexpr explicit arr_fn(hai::array<fn_ptr> fns) : m_fns { traits::move(fns) } {}
};

class all : public arr_fn {
public:
  using arr_fn::arr_fn;
};
class any : public arr_fn {
public:
  using arr_fn::arr_fn;
};
class sub : public arr_fn {
public:
  using arr_fn::arr_fn;
};

class plus : public wrap_fn {
public:
  using wrap_fn::wrap_fn;
};
class star : public wrap_fn {
public:
  using wrap_fn::wrap_fn;
};
class opt : public wrap_fn {
public:
  using wrap_fn::wrap_fn;
};
class excl : public wrap_fn {
public:
  using wrap_fn::wrap_fn;
};

class match : public fn {
  jute::heap m_c;
public:
  constexpr explicit match(jute::heap c) : m_c { c } {}
};
class range : public fn {
  jute::heap m_min;
  jute::heap m_max;
public:
  constexpr explicit range(jute::heap mn, jute::heap mx) : m_min { mn }, m_max { mx } {}
};

class start_of_line : public fn {};
class end_of_stream : public fn {};
class empty : public fn {};

class parser {
  using node = jason::ast::node_ptr;

  node m_json;
  const j::dict & m_rules;

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
      return do_cond(m_rules[*s]);
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
    else {
      silog::trace("eval", *k);
      do_cond(m_rules[*k]);
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
