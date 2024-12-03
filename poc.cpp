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

bool run_test(auto dir) try {
  auto in_yaml = (dir + "in.yaml").cstr();
  if (mtime::of(in_yaml.begin()) == 0) silog::die("invalid test file: %s", in_yaml.begin());

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
      if (!run_test(base_dir)) silog::log(silog::error, "test failed: %s", base_dir.cstr().begin());
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

