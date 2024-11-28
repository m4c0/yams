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

bool run_test(jute::view dir) try {
  using namespace jute::literals;
  auto base_dir = "yaml-test-suite/"_s + dir + "/";

  auto in_json = (base_dir + "in.json").cstr();
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
  auto err_file = (base_dir + "error").cstr();
  if (mtime::of(err_file.begin())) {
    // TODO: deal with negative tests
    return false;
  }

  // TODO: how to test these? (example: M5DY)
  return false;
} catch (...) {
  silog::whilst("running test [%s]", dir.begin());
}

int main() {
  for (auto dir : pprent::list("yaml-test-suite/name")) {
    if (dir[0] == '.') continue;
    if (!run_test(jute::view::unsafe(dir))) silog::log(silog::error, "test failed: %s", dir);
  }
}

