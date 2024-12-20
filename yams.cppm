export module yams;
import hai;
import hashley;
import jute;
import print;

namespace yams {
  export struct failure {};
  export [[noreturn]] constexpr void fail_(auto... msg) {
    putln(msg...);
    throw failure();
  }

  struct file_info {
    jute::view filename;
    unsigned line { 1 };
    unsigned col { 1 };
  };
  export [[noreturn]] constexpr void fail(const file_info & n, jute::view msg, auto... extra) {
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

    [[nodiscard]] constexpr char peek() const { return m_src.size() ? m_src[0] : 0; }
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

    [[nodiscard]] constexpr auto lookahead(int n) const {
      if (m_src.size() < n) n = m_src.size();
      return m_src.subview(n).before;
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

  export struct node;
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
  static constexpr int take_spaces(char_stream & ts, int indent) {
    bool done = false;
    int count = 0;
    while (!done) {
      switch (ts.peek()) {
        case ' ':
        case '\t':
          count++;
          ts.take();
          break;
        case '#':
          while (ts.peek() && ts.peek() != '\n') ts.take();
          ts.take(); // Consumes CR
          for (count = 0; count < indent; count++)
            if (ts.peek() != ' ' && ts.peek() != '\t') break;
          break;
        default: done = true; break;
      }
    }
    return count;
  }

  static constexpr node do_inline(char_stream & ts, int indent);
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
      take_spaces(ts, indent);

      if (ts.peek() == '\n') {
        while (ts.peek() == '\n') ts.match('\n');
        res.children->push_back(do_value(ts, indent));
      } else {
        res.children->push_back(do_inline(ts, indent));
      }
      while (ts.peek() == '\n') ts.match('\n');
      (*res.index)[key] = res.children->size();
    } while (is_alpha(ts));

    return res;
  }

  static constexpr node do_string(char_stream & ts) {
    auto str = take_string(ts, is_alpha);
    ts.match('\n');
    return { .type = type::string, .content = str, .fileinfo = ts.fileinfo() }; 
  }
  static constexpr node do_dq_string(char_stream & ts) {
    auto ptr = ts.ptr();
    auto fi = ts.fileinfo();
    ts.match('"');
    while (ts.peek()) {
      // TODO: validate unicode and hex escapes (see test G4RS)
      if (ts.peek() == '\\') {
        ts.take();
        ts.take();
        continue;
      }
      if (ts.peek() == '"') break;
      ts.take();
    }
    ts.match('\"');
    jute::view str { ptr, static_cast<unsigned>(ts.ptr() - ptr) };
    ts.match('\n');
    return { .type = type::string, .content = str, .fileinfo = fi };
  }
  static constexpr node do_sq_string(char_stream & ts) {
    auto ptr = ts.ptr();
    auto fi = ts.fileinfo();
    ts.match('\'');
    while (ts.peek()) {
      if (ts.lookahead(2) == "''") {
        ts.take();
        ts.take();
        continue;
      }
      if (ts.peek() == '\'') break;
      ts.take();
    }
    ts.match('\'');
    jute::view str { ptr, static_cast<unsigned>(ts.ptr() - ptr) };
    ts.match('\n');
    return { .type = type::string, .content = str, .fileinfo = fi };
  }

  static constexpr node do_seq(char_stream & ts, int indent) {
    node res { .type = type::seq, .children = node::kids::make(), .fileinfo = ts.fileinfo() };

    do {
      ts.match('-');
      take_spaces(ts, indent);

      res.children->push_back(do_string(ts));
    } while (ts.peek() == '-');

    return res;
  }

  static constexpr node do_inline(char_stream & ts, int indent) {
    switch (ts.peek()) {
      case 0:    return do_nil();
      case '!':  ts.fail("TBD: tags");
      case '&':  ts.fail("TBD: value-anchors");
      case '|':  ts.fail("TBD: multi-line text");
      case '>':  ts.fail("TBD: multi-line text");
      case '\'': return do_sq_string(ts);
      case '"':  return do_dq_string(ts);
      case '[':  ts.fail("TBD: json-like seqs");
      case '{':  ts.fail("TBD: json-like maps");
      default:
        if (is_alpha(ts)) return do_string(ts);
        ts.fail("unexpected char [", ts.peek(), "]");
    }
  }

  static constexpr node do_seq_or_doc(char_stream & ts, int indent) {
    if (ts.lookahead(3) == "---") ts.fail("TBD: multiple docs");
    else return do_seq(ts, indent);
  }

  static constexpr node do_value(char_stream & ts, int indent) {
    auto ind = take_spaces(ts, indent);
    if (ind < indent) ts.fail("TBD: next indent is smaller: ", ind, " v ", indent);

    switch (ts.peek()) {
      case 0:    return do_nil();
      case '-':  return do_seq_or_doc(ts, indent);
      case '!':  ts.fail("TBD: flow tags");
      case '&':  ts.fail("TBD: prop-anchor");
      case '\'': return do_sq_string(ts);
      case '"':  return do_dq_string(ts);
      default: // TODO: do_string (':' do_value)?
        if (is_alpha(ts)) return do_map(ts, indent);
        return do_inline(ts, ind);
    }
  }
}
namespace yams {
  export [[noreturn]] constexpr void fail(const ast::node & n, jute::view msg, auto... extra) {
    yams::fail(n.fileinfo, msg, extra...);
  }

  export [[nodiscard]] constexpr auto unescape(const ast::node & n) {
    auto txt = n.content;
    hai::array<char> buffer { static_cast<unsigned>(txt.size()) };
    auto ptr = buffer.begin();
    unsigned i;
    if (txt[0] == '"') {
      for (i = 1; i < txt.size() - 1; i++, ptr++) {
        if (txt[i] != '\\') {
          *ptr = txt[i];
          continue;
        }
        if (i + 1 == txt.size()) fail(n, "escape without char");
        switch (auto c = txt[i + 1]) {
          case 'n': *ptr = '\n'; break;
          case 't': *ptr = '\t'; break;
          default: *ptr = c;
        }
        i++;
      }
    } else if (txt[0] == '\'') {
      for (i = 1; i < txt.size() - 1; i++, ptr++) {
        if (txt[i] != '\'') {
          *ptr = txt[i];
          continue;
        }
        i++;
      }
    } else {
      return jute::heap(txt);
    }
    unsigned len = ptr - buffer.begin();
    return jute::heap { jute::view { buffer.begin(), len } };
  }

  export constexpr ast::node parse(jute::view file, jute::view src) {
    char_stream ts { file, src };
    return ast::do_value(ts, 0);
  }

  export constexpr jute::view type_name(ast::type t) {
    switch (t) {
      case ast::type::nil:    return "nil";
      case ast::type::map:    return "map";
      case ast::type::seq:    return "seq";
      case ast::type::string: return "string";
    }
  }
  export constexpr jute::view type_name(const ast::node & n) { return type_name(n.type); }

  export class nil {
    const ast::node & m_n;
  public:
    static constexpr const auto type = ast::type::nil;

    explicit constexpr nil(const ast::node & n) : m_n { n } {}
    [[nodiscard]] constexpr const auto & node() const { return m_n; }
  };
  export class map {
    const ast::node & m_n;
  public:
    static constexpr const auto type = ast::type::map;

    explicit constexpr map(const ast::node & n) : m_n { n } {}
    [[nodiscard]] constexpr const auto & node() const { return m_n; }

    [[nodiscard]] constexpr auto size() const { return m_n.children->size(); }
    [[nodiscard]] constexpr bool has(jute::view key) const { return m_n.index->has(key); }
    [[nodiscard]] constexpr const auto & operator[](jute::view key) const {
      if (!has(key)) yams::fail(m_n, "missing key in map: ", key);
      return m_n.children->seek((*m_n.index)[key] - 1);
    }
  };
  export class seq {
    const ast::node & m_n;
  public:
    static constexpr const auto type = ast::type::seq;

    explicit constexpr seq(const ast::node & n) : m_n { n } {}
    [[nodiscard]] constexpr const auto & node() const { return m_n; }

    [[nodiscard]] constexpr auto size() const { return m_n.children->size(); }
    [[nodiscard]] constexpr const auto & operator[](unsigned i) const { return m_n.children->seek(i); }
  };
  export class string {
    const ast::node & m_n;
  public:
    static constexpr const auto type = ast::type::string;

    explicit constexpr string(const ast::node & n) : m_n { n } {}
    [[nodiscard]] constexpr const auto & node() const { return m_n; }
    [[nodiscard]] constexpr auto str() const { return yams::unescape(m_n); }
  };

  export template<typename T> constexpr bool isa(const ast::node & n) {
    return n.type == T::type;
  }
  export template<typename T> constexpr T cast(const ast::node & n) {
    if (yams::isa<T>(n)) return T { n };
    yams::fail(n, "expecting ", type_name(T::type), ", got type ", type_name(n));
  }
}
