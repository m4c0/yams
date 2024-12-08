#pragma leco tool
/// Third time is the charm. Trying to use yaml-test-suite instead of specs.
///
/// Hypothesis: tests should contain and describe YAML by use-cases, which can
/// help prioritise feature support and lead to a parser MVP

import hai;
import hashley;
import jason;
import jojo;
import jute;
import mtime;
import pprent;
import print;
import silog;

namespace yams {
  struct failure {};
  [[noreturn]] static constexpr void fail_(auto... msg) {
    putln(msg...);
    throw failure();
  }

  struct file_info {
    jute::view filename;
    unsigned line { 1 };
    unsigned col { 1 };
  };
  [[noreturn]] static constexpr void fail(const file_info & n, jute::view msg, auto... extra) {
    yams::fail_(n.filename, ":", n.line, ":", n.col, ": ", msg, extra...);
  }

  class char_stream { 
    file_info m_fileinfo {};
    jute::view m_src;

    constexpr jute::view esc_peek() const {
      if (m_src.size() == 0) return "EOF";
      switch (peek()) {
        case '\t': return "\\t";
        case '\n': return "\\n";
        default: return m_src.subview(1).before;
      }
    }

  public:
    constexpr explicit char_stream(jute::view fn, jute::view src) : m_fileinfo {fn}, m_src {src} {}

    constexpr const char * ptr() const { return m_src.data(); }
    constexpr const auto & fileinfo() const { return m_fileinfo; }

    constexpr char peek() const { return m_src.size() ? m_src[0] : 0; }
    constexpr char take() {
      if (m_src.size() == 0) return 0;
      auto [l, r] = m_src.subview(1);
      m_src = r;
      if (l[0] == '\n') {
        m_fileinfo.line++;
        m_fileinfo.col = 1;
      } else {
        m_fileinfo.col++;
      }
      return l[0];
    }

    [[noreturn]] constexpr void fail(jute::view msg, auto... extra) const {
      yams::fail(m_fileinfo, msg, extra...);
    }
    constexpr void match(char c) {
      if (peek() != c) fail("mismatched char - got: [", esc_peek(), "] exp: [", c, "]");
      else take();
    }
  };
}
namespace yams::ast {
  enum class type {
    nil,
    map,
    seq,
    string,
  };

  static constexpr jute::view type_name(type t) {
    switch (t) {
      case type::nil:    return "nil";
      case type::map:    return "map";
      case type::seq:    return "seq";
      case type::string: return "string";
    }
  }

  struct node {
    using kids = hai::sptr<hai::chain<node>>;
    using idx = hai::sptr<hashley::niamh>;

    type type {};
    jute::view content {};
    kids children {};
    idx index {};

    file_info fileinfo {};
  };

  static constexpr bool is_alpha(char_stream & ts) {
    return ts.peek() >= 32 && ts.peek() <= 127;
  }
  static constexpr jute::view take_string(char_stream & ts, auto && fn) {
    auto start = ts.ptr();
    while (fn(ts)) ts.take();
    auto len = static_cast<unsigned>(ts.ptr() - start);
    return jute::view { start, len };
  }
  static constexpr int take_spaces(char_stream & ts) {
    auto v = take_string(ts, [](auto & ts) { 
      return ts.peek() == ' ' || ts.peek() == '\t';
    });
    return v.size();
  }

  static constexpr node do_inline(char_stream & ts, int indent);
  static constexpr node do_string(char_stream & ts);
  static constexpr node do_value(char_stream & ts, int indent);

  static constexpr node do_nil() { return { type::nil }; }

  static constexpr node do_map(char_stream & ts, int indent) {
    node res {
      .type = type::map,
      .children = node::kids::make(),
      .index = node::idx::make(17U),
      .fileinfo = ts.fileinfo(),
    };

    do {
      auto key = take_string(ts, [](auto & ts) -> bool {
        return is_alpha(ts) && ts.peek() != ':';
      });
      ts.match(':');
      take_spaces(ts);

      if (ts.peek() == '\n') {
        while (ts.peek() == '\n') ts.match('\n');
        res.children->push_back(do_value(ts, indent));
      } else {
        res.children->push_back(do_inline(ts, indent));
      }
      (*res.index)[key] = res.children->size();
    } while (is_alpha(ts));

    return res;
  }

  static constexpr node do_string(char_stream & ts) {
    auto str = take_string(ts, is_alpha);
    ts.match('\n');
    return { .type = type::string, .content = str, .fileinfo = ts.fileinfo() }; 
  }

  static constexpr node do_seq(char_stream & ts) {
    node res { .type = type::seq, .children = node::kids::make(), .fileinfo = ts.fileinfo() };

    do {
      ts.match('-');
      take_spaces(ts);

      res.children->push_back(do_string(ts));
    } while (ts.peek() == '-');

    return res;
  }

  static constexpr node do_inline(char_stream & ts, int indent) {
    switch (ts.peek()) {
      case 0:    return do_nil();
      case '!':  ts.fail("TBD: tags");
      case '|':  ts.fail("TBD: multi-line text");
      case '>':  ts.fail("TBD: multi-line text");
      case '\'': ts.fail("TBD: single-quoted strings");
      case '"':  ts.fail("TBD: double-quoted strings");
      case '[':  ts.fail("TBD: json-like seqs");
      case '{':  ts.fail("TBD: json-like maps");
      default:
        if (is_alpha(ts)) return do_string(ts);
        ts.fail("unexpected char [", ts.peek(), "]");
    }
  }

  static constexpr node do_value(char_stream & ts, int indent) {
    auto ind = take_spaces(ts);
    if (ind < indent) ts.fail("TBD: next indent is smaller: ", ind, " v ", indent);

    switch (ts.peek()) {
      case 0:   return do_nil();
      case '-': return do_seq(ts);
      case '!': ts.fail("TBD: flow tags");
      default:
        if (is_alpha(ts)) return do_map(ts, indent);
        return do_inline(ts, ind);
    }
  }
}
namespace yams {
  [[noreturn]] constexpr void fail(const ast::node & n, jute::view msg, auto... extra) {
    yams::fail(n.fileinfo, msg, extra...);
  }

  constexpr ast::node parse(jute::view file, jute::view src) {
    char_stream ts { file, src };
    return ast::do_value(ts, 0);
  }
}

void compare(const yams::ast::node & yaml, const auto & json) {
  namespace j = jason::ast;
  namespace y = yams::ast;

  if (j::isa<j::nodes::array>(json)) {
    auto & jd = j::cast<j::nodes::array>(json);
    if (yaml.type != y::type::seq) yams::fail(yaml, "expecting sequence, got type ", type_name(yaml.type));
    if (jd.size() != yaml.children->size()) yams::fail(yaml, "mismatched size: ", jd.size(), " v ", yaml.children->size());
    for (auto i = 0; i < jd.size(); i++) {
      compare(yaml.children->seek(i), jd[i]);
    }
    return;
  }

  if (j::isa<j::nodes::dict>(json)) {
    auto & jd = j::cast<j::nodes::dict>(json);
    if (yaml.type != y::type::map) yams::fail(yaml, "expecting map, got type ", type_name(yaml.type));
    if (jd.size() != yaml.children->size()) yams::fail(yaml, "mismatched size: ", jd.size(), " v ", yaml.children->size());
    for (auto &[k, v] : jd) {
      if (!yaml.index->has(*k)) yams::fail(yaml, "missing key in map: ", k);
      compare(yaml.children->seek((*yaml.index)[*k] - 1), v);
    }
    return;
  }

  if (j::isa<j::nodes::string>(json)) {
    auto & jd = j::cast<j::nodes::string>(json);
    if (yaml.type != y::type::string) yams::fail(yaml, "expecting string, got ", type_name(yaml.type));
    if (jd.str() != yaml.content) yams::fail(yaml, "mismatched string - got: [", yaml.content, "] exp: [", jd.str(), "]");
    return;
  }

  yams::fail(yaml, "unknown yaml type: ", type_name(yaml.type));
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
    if (view.size() == 0) {
      if (yaml.type != yams::ast::type::nil) yams::fail(yaml, "expecing empty yaml");
      return true;
    }
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
      if (!success) putln(base_dir.cstr(), "in.yaml: failed");
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

