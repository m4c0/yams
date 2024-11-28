#pragma leco tool
/// Third time is the charm. Trying to use yaml-test-suite instead of specs.
///
/// Hypothesis: tests should contain and describe YAML by use-cases, which can
/// help prioritise feature support and lead to a parser MVP

import jason;
import jojo;
import jute;
import mtime;
import pprent;
import print;
import silog;

int main() {
  for (auto dir : pprent::list("yaml-test-suite")) {
    if (dir[0] == '.') continue;

    using namespace jute::literals;
    auto base_dir = "yaml-test-suite/"_s + jute::view::unsafe(dir) + "/";

    auto in_json = (base_dir + "in.json").cstr();
    if (mtime::of(in_json.begin())) {
      auto json = jason::parse(jojo::read_cstr(in_json));
      continue;
    }
    auto err_file = (base_dir + "error").cstr();
    if (mtime::of(err_file.begin())) {
      continue;
    }

    // TODO: how to test these? (example: M5DY)
  }
}

