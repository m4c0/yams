#pragma leco tool
import jason;
import jojo;
import jute;
import mtime;
import pprent;
import print;
import silog;
import yams;

void compare(const yams::ast::node & yaml, const auto & json) {
  namespace j = jason::ast;
  namespace y = yams::ast;

  if (j::isa<j::nodes::array>(json)) {
    auto & jd = j::cast<j::nodes::array>(json);
    auto yd = yams::cast<yams::seq>(yaml);
    if (jd.size() != yd.size()) yams::fail(yaml, "mismatched size: ", jd.size(), " v ", yd.size());
    for (auto i = 0; i < jd.size(); i++) compare(yd[i], jd[i]);
    return;
  }

  if (j::isa<j::nodes::dict>(json)) {
    auto & jd = j::cast<j::nodes::dict>(json);
    auto yd = yams::cast<yams::map>(yaml);
    if (jd.size() != yd.size()) yams::fail(yaml, "mismatched size: ", jd.size(), " v ", yd.size());
    for (auto &[k, v] : jd) compare(yd[*k], v);
    return;
  }

  if (j::isa<j::nodes::string>(json)) {
    auto jd = j::cast<j::nodes::string>(json).str();
    auto yd = yams::cast<yams::string>(yaml).str();
    if (jd != yd) yams::fail(yaml, "mismatched string - got: [", yd, "] exp: [", jd, "]");
    return;
  }

  yams::fail(yaml, "unknown yaml type: ", yams::type_name(yaml));
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
      yams::cast<yams::nil>(yaml);
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

  // TODO: how to test these? (example: M5DY)
  return false;
} catch (yams::failure) {
  auto err_file = (dir + "error").cstr();
  if (mtime::of(err_file.begin())) return true;

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
      if (!success) putln(base_dir.cstr(), "in.yaml:1:1: test failed");
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

