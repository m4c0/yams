#pragma leco tool
/// Third time is the charm. Trying to use yaml-test-suite instead of specs.
///
/// Hypothesis: tests should contain and describe YAML by use-cases, which can
/// help prioritise feature support and lead to a parser MVP

import hai;
import jason;
import jojo;
import jute;
import mtime;
import pprent;
import print;
import silog;

namespace yams {
  struct failure {};
  [[noreturn]] static constexpr void fail(auto... msg) {
    putln(msg...);
    throw failure();
  }

  class char_stream { 
    jute::view m_filename;
    jute::view m_src;
    unsigned m_line { 1 };
    unsigned m_col { 1 };

  public:
    constexpr explicit char_stream(jute::view fn, jute::view src) : m_filename {fn}, m_src {src} {}

    constexpr const char * ptr() { return m_src.data(); }

    constexpr char peek() { return m_src.size() ? m_src[0] : 0; }
    constexpr char take() {
      if (m_src.size() == 0) return 0;
      auto [l, r] = m_src.subview(1);
      m_src = r;
      return l[0];
    }

    [[noreturn]] constexpr void fail(jute::view msg, auto... extra) {
      yams::fail(m_filename, ":", m_line, ":", m_col, ": ", msg, extra...);
    }
    constexpr void match(char c) {
      if (peek() != c) fail("mismatched char");
      else take();
    }
  };
}
namespace yams::ast {
  enum class type {
    nil,
    list,
    string,
  };
  struct node {
    using kids = hai::sptr<hai::chain<node>>;

    type type {};
    jute::view content {};
    kids children {};
  };

  static constexpr node do_nil() { return { type::nil }; }

  static constexpr node do_string(char_stream & ts) {
    auto start = ts.ptr();

    while (ts.peek() >= 32 && ts.peek() <= 127) {
      ts.take();
    }

    auto len = static_cast<unsigned>(ts.ptr() - start);

    ts.match('\n');
    return { .type = type::string, .content = jute::view { start, len } }; 
  }

  static constexpr node do_list(char_stream & ts) {
    node res { .type = type::list, .children = node::kids::make() };

    do {
      ts.match('-');

      while (ts.peek() == ' ') ts.take();

      res.children->push_back(do_string(ts));
    } while (ts.peek() == '-');

    return res;
  }
}
namespace yams {
  constexpr ast::node parse(jute::view file, jute::view src) {
    char_stream ts { file, src };
    switch (ts.peek()) {
      case 0: return ast::do_nil();
      case '-': return ast::do_list(ts);
      default: ts.fail("unexpected char ", ts.peek());
    }
  }
}

void compare(const yams::ast::node & yaml, const auto & json) {
  namespace j = jason::ast;
  namespace y = yams::ast;
  if (j::isa<j::nodes::array>(json)) {
    auto & jd = j::cast<j::nodes::array>(json);
    if (yaml.type != y::type::list) yams::fail("expecting list, got type ", static_cast<int>(yaml.type));
    if (jd.size() != yaml.children->size()) yams::fail("mismatched size: ", jd.size(), " v ", yaml.children->size());
    for (auto i = 0; i < jd.size(); i++) {
      compare(yaml.children->seek(i), jd[i]);
    }
    return;
  }
  if (j::isa<j::nodes::string>(json)) {
    auto & jd = j::cast<j::nodes::string>(json);
    if (yaml.type != y::type::string) yams::fail("expecting string, got type ", static_cast<int>(yaml.type));
    if (jd.str() != yaml.content) yams::fail("mismatched string: ", jd.str(), " v ", yaml.content);
    return;
  }
  yams::fail("unknown yaml type: ", static_cast<int>(yaml.type));
}
bool run_test(auto dir) try {
  auto in_yaml = (dir + "in.yaml").cstr();
  if (mtime::of(in_yaml.begin()) == 0) silog::die("invalid test file: %s", in_yaml.begin());

  auto yaml_src = jojo::read_cstr(in_yaml);
  auto yaml = yams::parse(in_yaml, yaml_src);

  auto in_json = (dir + "in.json").cstr();
  if (mtime::of(in_json.begin())) {
    auto json_src = jojo::read_cstr(in_json);
    auto view = jute::view { json_src };
    while (view.size()) {
      auto [ json, rest ] = jason::partial_parse(view);
      compare(yaml, json);
      view = rest;
      // TODO: deal with multiple docs
      return rest == "";
    }
    // TODO: deal with positive tests
    return false;
  }

  auto err_file = (dir + "error").cstr();
  if (mtime::of(err_file.begin())) {
    // TODO: deal with negative tests
    return false;
  }

  // TODO: how to test these? (example: M5DY)
  return false;
} catch (yams::failure) {
  return false;
} catch (...) {
  silog::whilst("running test [%s]", dir.cstr().begin());
}

static int counts[2] {};
void recurse(jute::view base) {
  for (auto dir : pprent::list(base.begin())) {
    if (dir[0] == '.') continue;
    auto base_dir = base + jute::view::unsafe(dir) + "/";

    if (mtime::of((base_dir + "/===").cstr().begin()) != 0) {
      auto success = run_test(base_dir);
      counts[success ? 0 : 1]++;
      if (!success) silog::log(silog::error, "test failed: %s", base_dir.cstr().begin());
    } else {
      recurse(base_dir.cstr());
    }
  }
}

int main() try {
  // return run_test(jute::view{"yaml-test-suite/FQ7F/"}) ? 0 : 1;
  jute::view base = "yaml-test-suite/name/";
  recurse(base);
  silog::log(silog::info, "success: %d -- failed: %d", counts[0], counts[1]);
} catch (...) {
  return 1;
}

