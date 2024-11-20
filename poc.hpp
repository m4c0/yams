#pragma once

static bool bt(bool (*fn)()) { return false; }
static bool opt(auto && fn) { return false; }
static bool star(auto && fn) { return false; }
static bool plus(auto && fn) { return false; }
static bool excl(auto && fn) { return false; }

static bool empty() { return false; }
static bool sol() { return false; }

static bool match(char c) { return false; }
static bool range(char a, char b) { return false; }
