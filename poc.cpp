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

  class char_stream { 
    jute::view m_filename;
    jute::view m_src;
    unsigned m_line { 1 };
    unsigned m_col { 1 };

  public:
    constexpr explicit char_stream(jute::view fn, jute::view src) : m_filename {fn}, m_src {src} {}

    char peek() { return m_src.size() ? m_src[0] : 0; }
    char take() {
      if (m_src.size() == 0) return 0;
      auto [l, r] = m_src.subview(1);
      m_src = r;
      return l[0];
    }

    [[noreturn]] void fail(jute::view msg) {
      putln(m_filename, ":", m_line, ":", m_col, ": ", msg);
      throw failure {};
    }
  };
}
namespace yams::ast {
  enum class type {
    nil,
    list,
  };
  struct node {
    type type {};
    hai::sptr<hai::array<node>> children {};
  };

  static node do_list(char_stream & ts) {
    return { type::list };
  }
}
namespace yams {
  ast::node parse(jute::view file, jute::view src) {
    char_stream ts { file, src };
    switch (ts.peek()) {
      case 0: return ast::node { ast::type::nil };
      case '-': return ast::do_list(ts);
      default: ts.fail("unexpected char");
    }
  }
}

bool run_test(auto dir) try {
  auto in_yaml = (dir + "in.yaml").cstr();
  if (mtime::of(in_yaml.begin()) == 0) silog::die("invalid test file: %s", in_yaml.begin());

  auto yaml = jojo::read_cstr(in_yaml);
  yams::parse(in_yaml, yaml);

  auto in_json = (dir + "in.json").cstr();
  if (mtime::of(in_json.begin())) {
    auto json_src = jojo::read_cstr(in_json);
    auto view = jute::view { json_src };
    while (view.size()) {
      auto [ json, rest ] = jason::partial_parse(view);
      view = rest;
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

int main() {
  jute::view base = "yaml-test-suite/name/";
  recurse(base);
  silog::log(silog::info, "success: %d -- failed: %d", counts[0], counts[1]);
}

